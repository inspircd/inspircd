/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd is copyright (C) 2002-2006 ChatSpike-Dev.
 *                       E-mail:
 *                <brain@chatspike.net>
 *                <Craig@chatspike.net>
 *
 * Written by Craig Edwards, Craig McLure, and others.
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

using namespace std;

#include "inspircd_config.h"
#include "inspircd.h"
#include "inspircd_io.h"
#include <unistd.h>
#include <fcntl.h>
#include <sys/errno.h>
#include <sys/ioctl.h>
#include <sys/utsname.h>
#include <time.h>
#include <string>
#ifdef GCC3
#include <ext/hash_map>
#else
#include <hash_map>
#endif
#include <map>
#include <sstream>
#include <vector>
#include <deque>
#include <sched.h>
#ifdef THREADED_DNS
#include <pthread.h>
#endif
#include "users.h"
#include "globals.h"
#include "modules.h"
#include "dynamic.h"
#include "wildcard.h"
#include "message.h"
#include "mode.h"
#include "commands.h"
#include "xline.h"
#include "inspstring.h"
#include "dnsqueue.h"
#include "helperfuncs.h"
#include "hashcomp.h"
#include "socketengine.h"
#include "userprocess.h"
#include "socket.h"
#include "dns.h"
#include "typedefs.h"
#include "command_parse.h"
#include "ctables.h"

#ifdef GCC3
#define nspace __gnu_cxx
#else
#define nspace std
#endif


extern InspIRCd* ServerInstance;

extern std::vector<Module*> modules;
extern std::vector<ircd_module*> factory;
extern std::vector<InspSocket*> module_sockets;
extern std::vector<userrec*> local_users;

extern int MODCOUNT;
extern InspSocket* socket_ref[MAX_DESCRIPTORS];
extern time_t TIME;

// This table references users by file descriptor.
// its an array to make it VERY fast, as all lookups are referenced
// by an integer, meaning there is no need for a scan/search operation.
extern userrec* fd_ref_table[MAX_DESCRIPTORS];

extern Server* MyServer;
extern ServerConfig *Config;

extern user_hash clientlist;
extern chan_hash chanlist;

/* Special commands which may occur without registration of the user */
cmd_user* command_user;
cmd_nick* command_nick;
cmd_pass* command_pass;

/* This function pokes and hacks at a parameter list like the following:
 *
 * PART #winbot,#darkgalaxy :m00!
 *
 * to turn it into a series of individual calls like this:
 *
 * PART #winbot :m00!
 * PART #darkgalaxy :m00!
 *
 * The seperate calls are sent to a callback function provided by the caller
 * (the caller will usually call itself recursively). The callback function
 * must be a command handler. Calling this function on a line with no list causes
 * no action to be taken. You must provide a starting and ending parameter number
 * where the range of the list can be found, useful if you have a terminating
 * parameter as above which is actually not part of the list, or parameters
 * before the actual list as well. This code is used by many functions which
 * can function as "one to list" (see the RFC) */

int CommandParser::LoopCall(command_t* fn, char **parameters, int pcnt, userrec *u, int start, int end, int joins)
{
	/* Copy of the parameter list, because like strltok, we make a bit of
	 * a mess of the parameter string we're given, and we want to keep this
	 * private.
	 */
	char paramlist[MAXBUF];
	/* Temporary variable used to hold one split parameter
	 */
	char *param;
	/* Parameter list, we can have up to 32 of these
	 */
	char *pars[32];
	/* Seperated items, e.g. holds the #one and #two from "#one,#two"
	 */
	char *sep_items[32];
	/* Seperated keys, holds the 'two' and 'three' of "two,three"
	 */
	char *sep_keys[32];
	/* Misc. counters, the total values hold the total number of
	 * seperated items in sep_items (total) and the total number of
	 * seperated items in sep_keys (total2)
	 */
	int j = 0, q = 0, total = 0, total2 = 0;
	/* A temporary copy of the comma seperated key list (see the description
	 * of the paramlist variable)
	 */
	char keylist[MAXBUF];
	/* Exactly what it says. Its nothing. We point invalid parameters at this.
	 */
	char nothing = 0;

	/* First, initialize our arrays */
	for (int i = 0; i < 32; i++)
		sep_items[i] = sep_keys[i] = NULL;

	/* Now find all parameters that are NULL, maybe above pcnt,
	 * and for safety, point them at 'nothing'
	 */
	for (int i = 0; i < 10; i++)
	{
		if (!parameters[i])
		{
			parameters[i] = &nothing;
		}
	}
	/* Check if we're doing JOIN handling. JOIN has two lists in
	 * it, potentially, if we have keys, so this is a special-case
	 * to handle the keys if they are provided.
	 */
	if (joins)
	{
		if (pcnt > 1) /* we have a key to copy */
		{
			strlcpy(keylist,parameters[1],MAXBUF);
		}
	}

	/* There's nothing to split! We don't do anything
	 */
	if (!parameters[start] || (!strchr(parameters[start],',')))
	{
		return 0;
	}

	/* This function can handle multiple comma seperated
	 * lists as one, which is a feature no actual commands
	 * have yet -- this is futureproofing in case we encounter
	 * a command that  does.
	 */
	*paramlist = 0;

	for (int i = start; i <= end; i++)
	{
		if (parameters[i])
		{
			strlcat(paramlist,parameters[i],MAXBUF);
		}
	}

	/* Now we split off paramlist into seperate parameters using
	 * pointer voodoo, this parameter list goes into sep_items
	 */
	j = 0;
	param = paramlist;

	for (char* i = paramlist; *i; i++)
	{
		/* Found an item */
		if (*i == ',')
		{
			*i = '\0';
			sep_items[j++] = param;
			/* Iterate along to next item, if there is one */
			param = i+1;
			if ((unsigned int)j > Config->MaxTargets)
			{
				/* BZZT! Too many items */
				WriteServ(u->fd,"407 %s %s :Too many targets in list, message not delivered.",u->nick,sep_items[j-1]);
				return 1;
			}
		}
	}
	sep_items[j++] = param;
	total = j;

	/* We add this extra comma here just to ensure we get all the keys
	 * in the event the user gave a malformed key string (yes, you guessed
	 * it, this is a mirc-ism)
	 */
	if ((joins) && (*keylist) && (total>0)) // more than one channel and is joining
	{
		charlcat(keylist,',',MAXBUF);
	}

	/* If we're doing JOIN handling (e.g. we've had to kludge for two
	 * lists being handled at once, real smart by the way JRO </sarcasm>)
	 * and we also have keys, we must seperate out this key list into
	 * our char** list sep_keys for later use.
	 */
	if ((joins) && (*keylist))
	{
		/* There is more than one key */
		if (strchr(keylist,','))
		{
			j = 0;
			param = keylist;
			for (char* i = keylist; *i; i++)
			{
				if (*i == ',')
				{
					*i = '\0';
					sep_keys[j++] = param;
					param = i+1;
				}
			}

			sep_keys[j++] = param;
			total2 = j;
		}
	}

	/* Now the easier bit. We call the command class's Handle function
	 * X times, where x is the number of parameters we've 'collated'.
	 */
	for (j = 0; j < total; j++)
	{
		if (sep_items[j])
		{
			pars[0] = sep_items[j];
		}

		for (q = end; q < pcnt-1; q++)
		{
			if (parameters[q+1])
			{
				pars[q-end+1] = parameters[q+1];
			}
		}

		if ((joins) && (parameters[1]))
		{
			if (pcnt > 1)
			{
				pars[1] = sep_keys[j];
			}
			else
			{
				pars[1] = NULL;
			}
		}

		/* repeatedly call the function with the hacked parameter list */
		if ((joins) && (pcnt > 1))
		{
			if (pars[1])
			{
				/* pars[1] already set up and containing key from sep_keys[j] */
				fn->Handle(pars,2,u);
			}
			else
			{
				pars[1] = parameters[1];
				fn->Handle(pars,2,u);
			}
		}
		else
		{
			fn->Handle(pars,pcnt-(end-start),u);
		}
	}

	return 1;
}

bool CommandParser::IsValidCommand(std::string &commandname, int pcnt, userrec * user)
{
	nspace::hash_map<std::string,command_t*>::iterator n = cmdlist.find(commandname);

	if (n != cmdlist.end())
	{
		if ((pcnt>=n->second->min_params) && (n->second->source != "<core>"))
		{
			if ((strchr(user->modes,n->second->flags_needed)) || (!n->second->flags_needed))
			{
				if (n->second->flags_needed)
				{
					if ((user->HasPermission(commandname)) || (is_uline(user->server)))
					{
						return true;
					}
					else
					{
						return false;
					}
				}

				return true;
			}
		}
	}

	return false;
}

// calls a handler function for a command

bool CommandParser::CallHandler(std::string &commandname,char **parameters, int pcnt, userrec *user)
{
	nspace::hash_map<std::string,command_t*>::iterator n = cmdlist.find(commandname);

	if (n != cmdlist.end())
	{
		if (pcnt >= n->second->min_params)
		{
			if ((strchr(user->modes,n->second->flags_needed)) || (!n->second->flags_needed))
			{
				if (n->second->flags_needed)
				{
					if ((user->HasPermission(commandname)) || (!IS_LOCAL(user)))
					{
						n->second->Handle(parameters,pcnt,user);
						return true;
					}
				}
				else
				{
					n->second->Handle(parameters,pcnt,user);
					return true;
				}
			}
		}
	}
	return false;
}

int CommandParser::ProcessParameters(char **command_p,char *parameters)
{
	int j = 0;

	if (!*parameters)
	{
		/* no parameters, command_p invalid! */
		return 0;
	}

	if (*parameters == ':')
	{
		command_p[0] = parameters+1;
		return 1;
	}

	if (*parameters)
	{
		char* n = strchr(parameters,' ');
		if ((!n) || (*parameters == ':'))
		{
			/* only one parameter */
			command_p[0] = parameters;
			if (*parameters == ':')
			{
				if (n)
				{
					command_p[0]++;
				}
			}

			return 1;
		}
	}

	command_p[j++] = parameters;

	for (char* i = parameters; *i; i++)
	{
		if (*i == ' ')
		{
			command_p[j++] = i+1;
			*i = '\0';

			if (*command_p[j-1] == ':')
			{
				*command_p[j-1]++; /* remove dodgy ":" */
				break;
				/* parameter like this marks end of the sequence */
			}
		}
	}
	return j; /* returns total number of items in the list */
}

void CommandParser::ProcessCommand(userrec *user, char* cmd)
{
	char *parameters;
	char *command;
	char *command_p[127];
	char p[MAXBUF], temp[MAXBUF];
	int j, items, cmd_found;
	int total_params = 0;

	for (int i = 0; i < 127; i++)
		command_p[i] = NULL;

	if (!user || !cmd || !*cmd)
	{
		return;
	}

	char* first_space = NULL;

	/* If the command is > 2 characters (quick and dirty way to find out) */
	if (*cmd && *(cmd+1) && *(cmd+2))
	{
		for (char* q = cmd; *q; q++)
		{
			if (*q == ' ')
			{
				first_space = q;
				if (*(q+1) == ':')
				{
					total_params++;
					// found a 'trailing', we dont count them after this.
					break;
				}
				else
					total_params++;
			}
		}
	}

	// another phidjit bug...
	if (total_params > 126)
	{
		*first_space = 0;
		WriteServ(user->fd,"421 %s %s :Too many parameters given",user->nick,cmd);
		return;
	}

	strlcpy(temp,cmd,MAXBUF);

	std::string tmp = cmd;

	for (int i = 0; i <= MODCOUNT; i++)
	{
		std::string oldtmp = tmp;
		modules[i]->OnServerRaw(tmp,true,user);
		if (oldtmp != tmp)
		{
			log(DEBUG,"A Module changed the input string!");
			log(DEBUG,"New string: %s",tmp.c_str());
			log(DEBUG,"Old string: %s",oldtmp.c_str());
			break;
		}
	}

	strlcpy(cmd,tmp.c_str(),MAXBUF);
	strlcpy(temp,cmd,MAXBUF);

	char* has_space = strchr(cmd,' ');
	int cm_length = 0;
	if (!has_space)
	{
		/*
		 * no parameters, lets skip the formalities and not chop up
		 * the string
		 */
		log(DEBUG,"About to preprocess command with no params");
		items = 0;
		command_p[0] = NULL;
		parameters = NULL;
		for (char* i = cmd; *i; i++,cm_length++)
			*i = toupper(*i);
		command = cmd;
	}
	else
	{
		*cmd = 0;
		j = 0;

		/* strip out extraneous linefeeds through mirc's crappy pasting (thanks Craig) */
		for (char* i = temp; *i; i++)
		{
			if ((*i != 10) && (*i != 13) && (*i != 0) && (*i != 7))
			{
				cmd[j++] = *i;
			}
		}
		cmd[j] = 0;

		/* split the full string into a command plus parameters */
		parameters = p;
		p[0] = ' ';
		p[1] = 0;

		command = cmd;

		if (has_space)
		{
			for (char* i = cmd; *i; i++)
			{
				/* capitalise the command ONLY, leave params intact */
				*i = toupper(*i);
				/* are we nearly there yet?! :P */
				if (*i == ' ')
				{
					command = cmd;
					parameters = i+1;
					*i = 0;
					break;
				}
			}
		}
		else
		{
			for (char* i = cmd; *i; i++,cm_length++)
			{
				*i = toupper(*i);
			}
		}
	}

	if (cm_length > MAXCOMMAND)
	{
		WriteServ(user->fd,"421 %s %s :Command too long",user->nick,command);
		return;
	}

	for (char* x = command; *x; x++)
	{
		if (((*x < 'A') || (*x > 'Z')) && (*x != '.'))
		{
			if (((*x < '0') || (*x> '9')) && (*x != '-'))
			{
				if (strchr("@!\"$%^&*(){}[]_=+;:'#~,<>/?\\|`",*x))
				{
					ServerInstance->stats->statsUnknown++;
					WriteServ(user->fd,"421 %s %s :Unknown command",user->nick,command);
					return;
				}
			}
		}
	}

	std::string xcommand = command;
	if ((user->registered != 7) && (xcommand == "SERVER"))
	{
		kill_link(user,"Server connection to non-server port");
		return;
	}
	
	/* Tweak by brain - why was this INSIDE the mainloop? */
	if (parameters)
	{
		 if (parameters[0])
		 {
			 items = this->ProcessParameters(command_p,parameters);
		 }
		 else
		 {
			 items = 0;
			 command_p[0] = NULL;
		 }
	}
	else
	{
		items = 0;
		command_p[0] = NULL;
	}

	int MOD_RESULT = 0;
	FOREACH_RESULT(I_OnPreCommand,OnPreCommand(command,command_p,items,user,false));
	if (MOD_RESULT == 1) {
		return;
	}
	
	nspace::hash_map<std::string,command_t*>::iterator cm = cmdlist.find(xcommand);
	
	if (cm != cmdlist.end())
	{
		if (user)
		{
			/* activity resets the ping pending timer */
			user->nping = TIME + user->pingmax;
			if ((items) < cm->second->min_params)
			{
				log(DEBUG,"not enough parameters: %s %s",user->nick,command);
				WriteServ(user->fd,"461 %s %s :Not enough parameters",user->nick,command);
				return;
			}
			if ((!strchr(user->modes,cm->second->flags_needed)) && (cm->second->flags_needed))
			{
				log(DEBUG,"permission denied: %s %s",user->nick,command);
				WriteServ(user->fd,"481 %s :Permission Denied- You do not have the required operator privilages",user->nick);
				cmd_found = 1;
				return;
			}
			if ((cm->second->flags_needed) && (!user->HasPermission(xcommand)))
			{
				log(DEBUG,"permission denied: %s %s",user->nick,command);
				WriteServ(user->fd,"481 %s :Permission Denied- Oper type %s does not have access to command %s",user->nick,user->oper,command);
				if (!IS_LOCAL(user))
					WriteOpers("*** \2WARNING\2: Command '%s' not allowed for oper '%s', dropped.",command,user->nick);
				cmd_found = 1;
				return;
			}
			/* if the command isnt USER, PASS, or NICK, and nick is empty,
			 * deny command! */
			if ((cm->second != command_user) && (cm->second != command_nick) && (cm->second != command_pass))
			{
				if ((!isnick(user->nick)) || (user->registered != 7))
				{
					log(DEBUG,"not registered: %s %s",user->nick,command);
					WriteServ(user->fd,"451 %s :You have not registered",command);
					return;
				}
			}
			if ((user->registered == 7) && (!*user->oper) && (*Config->DisabledCommands))
			{
				std::stringstream dcmds(Config->DisabledCommands);
				std::string thiscmd;
				while (dcmds >> thiscmd)
				{
					if (!strcasecmp(thiscmd.c_str(),command))
					{
						// command is disabled!
						WriteServ(user->fd,"421 %s %s :This command has been disabled.",user->nick,command);
						return;
					}
				}
			}
			if ((user->registered == 7) || (cm->second == command_user) || (cm->second == command_nick) || (cm->second == command_pass))
			{
				/* ikky /stats counters */
				if (temp)
				{
					cm->second->use_count++;
					cm->second->total_bytes+=strlen(temp);
				}

				int MOD_RESULT = 0;
				FOREACH_RESULT(I_OnPreCommand,OnPreCommand(command,command_p,items,user,true));
				if (MOD_RESULT == 1)
				{
					return;
				}

				/*
				 * WARNING: nothing may come after the
				 * command handler call, as the handler
				 * may free the user structure!
				 */

				cm->second->Handle(command_p,items,user);
				return;
			}
			else
			{
				WriteServ(user->fd,"451 %s :You have not registered",command);
				return;
			}
		}
	}
	else if (user)
	{
		ServerInstance->stats->statsUnknown++;
		WriteServ(user->fd,"421 %s %s :Unknown command",user->nick,command);
	}
}

bool CommandParser::RemoveCommands(const char* source)
{
	bool go_again = true;

	while (go_again)
	{
		go_again = false;

		for (nspace::hash_map<std::string,command_t*>::iterator i = cmdlist.begin(); i != cmdlist.end(); i++)
		{
			command_t* x = i->second;
			if (x->source == std::string(source))
			{
				log(DEBUG,"removecommands(%s) Removing dependent command: %s",x->source.c_str(),x->command.c_str());
				cmdlist.erase(i);
				go_again = true;
				break;
			}
		}
	}

	return true;
}

void CommandParser::ProcessBuffer(const char* cmdbuf,userrec *user)
{
	char cmd[MAXBUF];

	if (!user || !cmdbuf || !*cmdbuf)
	{
		log(DEFAULT,"*** BUG *** process_buffer was given an invalid parameter");
		return;
	}

	while (*cmdbuf == ' ') cmdbuf++; // strip leading spaces

	if (!*cmdbuf)
		return;
	
	strlcpy(cmd,cmdbuf,MAXBUF);

	while (charremove(cmd,10));
	while (charremove(cmd,13));

	int sl = strlen(cmd)-1;
	while (sl && (cmd[sl] == ' ')) // strip trailing spaces
	{
		cmd[sl--] = 0;
	}

	if (!sl)
		return;

	log(DEBUG,"CMDIN: %s %s",user->nick,cmd);

	tidystring(cmd);

	if (user && cmd)
	{
		this->ProcessCommand(user,cmd);
	}
}

bool CommandParser::CreateCommand(command_t *f)
{
	/* create the command and push it onto the table */
	if (cmdlist.find(f->command) == cmdlist.end())
	{
		cmdlist[f->command] = f;
		log(DEBUG,"Added command %s (%lu parameters)",f->command.c_str(),(unsigned long)f->min_params);
		return true;
	}
	else return false;
}

CommandParser::CommandParser()
{
	this->SetupCommandTable();
}

void CommandParser::SetupCommandTable()
{
	/* These three are special (can occur without
	 * full user registration) and so are saved
	 * for later use.
	 */
	command_user = new cmd_user;
	command_nick = new cmd_nick;
	command_pass = new cmd_pass;
	this->CreateCommand(command_user);
	this->CreateCommand(command_nick);
	this->CreateCommand(command_pass);

	/* The rest of these arent special. boo hoo.
	 */
	this->CreateCommand(new cmd_quit);
	this->CreateCommand(new cmd_version);
	this->CreateCommand(new cmd_ping);
	this->CreateCommand(new cmd_pong);
	this->CreateCommand(new cmd_admin);
	this->CreateCommand(new cmd_privmsg);
	this->CreateCommand(new cmd_info);
	this->CreateCommand(new cmd_time);
	this->CreateCommand(new cmd_whois);
	this->CreateCommand(new cmd_wallops);
	this->CreateCommand(new cmd_notice);
	this->CreateCommand(new cmd_join);
	this->CreateCommand(new cmd_names);
	this->CreateCommand(new cmd_part);
	this->CreateCommand(new cmd_kick);
	this->CreateCommand(new cmd_mode);
	this->CreateCommand(new cmd_topic);
	this->CreateCommand(new cmd_who);
	this->CreateCommand(new cmd_motd);
	this->CreateCommand(new cmd_rules);
	this->CreateCommand(new cmd_oper);
	this->CreateCommand(new cmd_list);
	this->CreateCommand(new cmd_die);
	this->CreateCommand(new cmd_restart);
	this->CreateCommand(new cmd_kill);
	this->CreateCommand(new cmd_rehash);
	this->CreateCommand(new cmd_lusers);
	this->CreateCommand(new cmd_stats);
	this->CreateCommand(new cmd_userhost);
	this->CreateCommand(new cmd_away);
	this->CreateCommand(new cmd_ison);
	this->CreateCommand(new cmd_summon);
	this->CreateCommand(new cmd_users);
	this->CreateCommand(new cmd_invite);
	this->CreateCommand(new cmd_trace);
	this->CreateCommand(new cmd_whowas);
	this->CreateCommand(new cmd_connect);
	this->CreateCommand(new cmd_squit);
	this->CreateCommand(new cmd_modules);
	this->CreateCommand(new cmd_links);
	this->CreateCommand(new cmd_map);
	this->CreateCommand(new cmd_kline);
	this->CreateCommand(new cmd_gline);
	this->CreateCommand(new cmd_zline);
	this->CreateCommand(new cmd_qline);
	this->CreateCommand(new cmd_eline);
	this->CreateCommand(new cmd_loadmodule);
	this->CreateCommand(new cmd_unloadmodule);
	this->CreateCommand(new cmd_server);
	this->CreateCommand(new cmd_commands);
}

