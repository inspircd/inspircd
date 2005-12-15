/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  Inspire is copyright (C) 2002-2005 ChatSpike-Dev.
 *                       E-mail:
 *                <brain@chatspike.net>
 *           	  <Craig@chatspike.net>
 *     
 * Written by Craig Edwards, Craig McLure, and others.
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

/* Now with added unF! ;) */

using namespace std;

#include "inspircd_config.h"
#include "inspircd.h"
#include "inspircd_io.h"
#include "inspircd_util.h"
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
#include "ctables.h"
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

InspIRCd* ServerInstance;

int WHOWAS_STALE = 48; // default WHOWAS Entries last 2 days before they go 'stale'
int WHOWAS_MAX = 100;  // default 100 people maximum in the WHOWAS list

extern std::vector<Module*> modules;
extern std::vector<ircd_module*> factory;
std::vector<InspSocket*> module_sockets;

extern int MODCOUNT;
int openSockfd[MAXSOCKS];
sockaddr_in client,server;
socklen_t length;

extern InspSocket* socket_ref[65535];

time_t TIME = time(NULL), OLDTIME = time(NULL);

SocketEngine* SE = NULL;

// This table references users by file descriptor.
// its an array to make it VERY fast, as all lookups are referenced
// by an integer, meaning there is no need for a scan/search operation.
userrec* fd_ref_table[65536];

serverstats* stats = new serverstats;
Server* MyServer = new Server;
ServerConfig *Config = new ServerConfig;

user_hash clientlist;
chan_hash chanlist;
whowas_hash whowas;
command_table cmdlist;
servernamelist servernames;
int BoundPortCount = 0;
char lowermap[255];

void AddServerName(std::string servername)
{
	log(DEBUG,"Adding server name: %s",servername.c_str());
	for (servernamelist::iterator a = servernames.begin(); a < servernames.end(); a++)
	{
		if (*a == servername)
			return;
	}
	servernames.push_back(servername);
}

const char* FindServerNamePtr(std::string servername)
{
	for (servernamelist::iterator a = servernames.begin(); a < servernames.end(); a++)
	{
		if (*a == servername)
			return a->c_str();
	}
	AddServerName(servername);
	return FindServerNamePtr(servername);
}

std::string InspIRCd::GetRevision()
{
	/* w00t got me to replace a bunch of strtok_r
	 * with something nicer, so i did this. Its the
	 * same thing really, only in C++. It places the
	 * text into a std::stringstream which is a readable
	 * and writeable buffer stream, and then pops two
	 * words off it, space delimited. Because it reads
	 * into the same variable twice, the first word
	 * is discarded, and the second one returned.
	 */
	std::stringstream Revision("$Revision$");
	std::string single;
	Revision >> single >> single;
	return single;
}


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

int loop_call(handlerfunc fn, char **parameters, int pcnt, userrec *u, int start, int end, int joins)
{
	char plist[MAXBUF];
	char *param;
	char *pars[32];
	char blog[32][MAXBUF];
	char blog2[32][MAXBUF];
	int j = 0, q = 0, total = 0, t = 0, t2 = 0, total2 = 0;
	char keystr[MAXBUF];
	char moo[MAXBUF];

	for (int i = 0; i <32; i++)
		strcpy(blog[i],"");

	for (int i = 0; i <32; i++)
		strcpy(blog2[i],"");

	strcpy(moo,"");
	for (int i = 0; i <10; i++)
	{
		if (!parameters[i])
		{
			parameters[i] = moo;
		}
	}
	if (joins)
	{
		if (pcnt > 1) /* we have a key to copy */
		{
			strlcpy(keystr,parameters[1],MAXBUF);
		}
	}

	if (!parameters[start])
	{
		return 0;
	}
	if (!strchr(parameters[start],','))
	{
		return 0;
	}
	strcpy(plist,"");
	for (int i = start; i <= end; i++)
	{
		if (parameters[i])
		{
			strlcat(plist,parameters[i],MAXBUF);
		}
	}
	
	j = 0;
	param = plist;

	t = strlen(plist);
	for (int i = 0; i < t; i++)
	{
		if (plist[i] == ',')
		{
			plist[i] = '\0';
			strlcpy(blog[j++],param,MAXBUF);
			param = plist+i+1;
			if (j>20)
			{
				WriteServ(u->fd,"407 %s %s :Too many targets in list, message not delivered.",u->nick,blog[j-1]);
				return 1;
			}
		}
	}
	strlcpy(blog[j++],param,MAXBUF);
	total = j;

	if ((joins) && (keystr) && (total>0)) // more than one channel and is joining
	{
		strcat(keystr,",");
	}
	
	if ((joins) && (keystr))
	{
		if (strchr(keystr,','))
		{
			j = 0;
			param = keystr;
			t2 = strlen(keystr);
			for (int i = 0; i < t2; i++)
			{
				if (keystr[i] == ',')
				{
					keystr[i] = '\0';
					strlcpy(blog2[j++],param,MAXBUF);
					param = keystr+i+1;
				}
			}
			strlcpy(blog2[j++],param,MAXBUF);
			total2 = j;
		}
	}

	for (j = 0; j < total; j++)
	{
		if (blog[j])
		{
			pars[0] = blog[j];
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
				pars[1] = blog2[j];
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
				// pars[1] already set up and containing key from blog2[j]
				fn(pars,2,u);
			}
			else
			{
				pars[1] = parameters[1];
				fn(pars,2,u);
			}
		}
		else
		{
			fn(pars,pcnt-(end-start),u);
		}
	}

	return 1;
}



InspIRCd::InspIRCd(int argc, char** argv)
{
	Start();
	module_sockets.clear();
	this->startup_time = time(NULL);
	srand(time(NULL));
	log(DEBUG,"*** InspIRCd starting up!");
	if (!FileExists(CONFIG_FILE))
	{
		printf("ERROR: Cannot open config file: %s\nExiting...\n",CONFIG_FILE);
		log(DEFAULT,"main: no config");
		printf("ERROR: Your config file is missing, this IRCd will self destruct in 10 seconds!\n");
		Exit(ERROR);
	}
	if (argc > 1) {
		for (int i = 1; i < argc; i++)
		{
			if (!strcmp(argv[i],"-nofork")) {
				Config->nofork = true;
			}
			if (!strcmp(argv[i],"-wait")) {
				sleep(6);
			}
			if (!strcmp(argv[i],"-nolimit")) {
				Config->unlimitcore = true;
			}
		}
	}

	strlcpy(Config->MyExecutable,argv[0],MAXBUF);
	
	// initialize the lowercase mapping table
	for (unsigned int cn = 0; cn < 256; cn++)
		lowermap[cn] = cn;
	// lowercase the uppercase chars
	for (unsigned int cn = 65; cn < 91; cn++)
		lowermap[cn] = tolower(cn);
	// now replace the specific chars for scandanavian comparison
	lowermap[(unsigned)'['] = '{';
	lowermap[(unsigned)']'] = '}';
	lowermap[(unsigned)'\\'] = '|';


        OpenLog(argv, argc);
        Config->ClearStack();
        Config->Read(true,NULL);
        CheckRoot();
        SetupCommandTable();
        AddServerName(Config->ServerName);
        CheckDie();
        BoundPortCount = BindPorts();

        printf("\n");
        if (!Config->nofork)
        {
                if (DaemonSeed() == ERROR)
                {
                        printf("ERROR: could not go into daemon mode. Shutting down.\n");
                        Exit(ERROR);
                }
        }

        /* Because of limitations in kqueue on freebsd, we must fork BEFORE we
         * initialize the socket engine.
         */
        SE = new SocketEngine();

        /* We must load the modules AFTER initializing the socket engine, now */
        LoadAllModules();

        printf("\nInspIRCd is now running!\n");

	return;
}

/* re-allocates a nick in the user_hash after they change nicknames,
 * returns a pointer to the new user as it may have moved */

userrec* ReHashNick(char* Old, char* New)
{
	//user_hash::iterator newnick;
	user_hash::iterator oldnick = clientlist.find(Old);

	log(DEBUG,"ReHashNick: %s %s",Old,New);
	
	if (!strcasecmp(Old,New))
	{
		log(DEBUG,"old nick is new nick, skipping");
		return oldnick->second;
	}
	
	if (oldnick == clientlist.end()) return NULL; /* doesnt exist */

	log(DEBUG,"ReHashNick: Found hashed nick %s",Old);

	userrec* olduser = oldnick->second;
	clientlist[New] = olduser;
	clientlist.erase(oldnick);

	log(DEBUG,"ReHashNick: Nick rehashed as %s",New);
	
	return clientlist[New];
}

#ifdef THREADED_DNS
void* dns_task(void* arg)
{
	userrec* u = (userrec*)arg;
	log(DEBUG,"DNS thread for user %s",u->nick);
	DNS dns1;
	DNS dns2;
	std::string host;
	std::string ip;
	if (dns1.ReverseLookup(u->ip))
	{
		log(DEBUG,"DNS Step 1");
		while (!dns1.HasResult())
		{
			usleep(100);
		}
		host = dns1.GetResult();
		if (host != "")
		{
			log(DEBUG,"DNS Step 2: '%s'",host.c_str());
			if (dns2.ForwardLookup(host))
			{
				while (!dns2.HasResult())
				{
					usleep(100);
				}
				ip = dns2.GetResultIP();
				log(DEBUG,"DNS Step 3 '%s'(%d) '%s'(%d)",ip.c_str(),ip.length(),u->ip,strlen(u->ip));
				if (ip == std::string(u->ip))
				{
					log(DEBUG,"DNS Step 4");
					if (host.length() < 160)
					{
						log(DEBUG,"DNS Step 5");
						strcpy(u->host,host.c_str());
						strcpy(u->dhost,host.c_str());
					}
				}
			}
		}
	}
	u->dns_done = true;
	return NULL;
}
#endif

std::string InspIRCd::GetVersionString()
{
	char versiondata[MAXBUF];
#ifdef THREADED_DNS
	char dnsengine[] = "multithread";
#else
	char dnsengine[] = "singlethread";
#endif
	snprintf(versiondata,MAXBUF,"%s Rev. %s %s :%s [FLAGS=%lu,%s,%s]",VERSION,GetRevision().c_str(),Config->ServerName,SYSTEM,(unsigned long)OPTIMISATION,SE->GetName().c_str(),dnsengine);
	return versiondata;
}


bool is_valid_cmd(std::string &commandname, int pcnt, userrec * user)
{
	for (unsigned int i = 0; i < cmdlist.size(); i++)
	{
		if (!strcasecmp(cmdlist[i].command,commandname.c_str()))
		{
			if (cmdlist[i].handler_function)
			{
				if ((pcnt>=cmdlist[i].min_params) && (strcasecmp(cmdlist[i].source,"<core>")))
				{
					if ((strchr(user->modes,cmdlist[i].flags_needed)) || (!cmdlist[i].flags_needed))
					{
						if (cmdlist[i].flags_needed)
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
		}
	}
	return false;
}

// calls a handler function for a command

void call_handler(std::string &commandname,char **parameters, int pcnt, userrec *user)
{
	for (unsigned int i = 0; i < cmdlist.size(); i++)
	{
		if (!strcasecmp(cmdlist[i].command,commandname.c_str()))
		{
			if (cmdlist[i].handler_function)
			{
				if (pcnt>=cmdlist[i].min_params)
				{
					if ((strchr(user->modes,cmdlist[i].flags_needed)) || (!cmdlist[i].flags_needed))
					{
						if (cmdlist[i].flags_needed)
						{
							if ((user->HasPermission(commandname)) || (is_uline(user->server)))
							{
								cmdlist[i].handler_function(parameters,pcnt,user);
							}
						}
						else
						{
							cmdlist[i].handler_function(parameters,pcnt,user);
						}
					}
				}
			}
		}
	}
}


void force_nickchange(userrec* user,const char* newnick)
{
	char nick[MAXBUF];
	int MOD_RESULT = 0;
	
	strcpy(nick,"");

	FOREACH_RESULT(OnUserPreNick(user,newnick));
	if (MOD_RESULT) {
		stats->statsCollisions++;
		kill_link(user,"Nickname collision");
		return;
	}
	if (matches_qline(newnick))
	{
		stats->statsCollisions++;
		kill_link(user,"Nickname collision");
		return;
	}
	
	if (user)
	{
		if (newnick)
		{
			strncpy(nick,newnick,MAXBUF);
		}
		if (user->registered == 7)
		{
			char* pars[1];
			pars[0] = nick;
			handle_nick(pars,1,user);
		}
	}
}
				

int process_parameters(char **command_p,char *parameters)
{
	int j = 0;
	int q = strlen(parameters);
	if (!q)
	{
		/* no parameters, command_p invalid! */
		return 0;
	}
	if (parameters[0] == ':')
	{
		command_p[0] = parameters+1;
		return 1;
	}
	if (q)
	{
		if ((strchr(parameters,' ')==NULL) || (parameters[0] == ':'))
		{
			/* only one parameter */
			command_p[0] = parameters;
			if (parameters[0] == ':')
			{
				if (strchr(parameters,' ') != NULL)
				{
					command_p[0]++;
				}
			}
			return 1;
		}
	}
	command_p[j++] = parameters;
	for (int i = 0; i <= q; i++)
	{
		if (parameters[i] == ' ')
		{
			command_p[j++] = parameters+i+1;
			parameters[i] = '\0';
			if (command_p[j-1][0] == ':')
			{
				*command_p[j-1]++; /* remove dodgy ":" */
				break;
				/* parameter like this marks end of the sequence */
			}
		}
	}
	return j; /* returns total number of items in the list */
}

void process_command(userrec *user, char* cmd)
{
	char *parameters;
	char *command;
	char *command_p[127];
	char p[MAXBUF], temp[MAXBUF];
	int j, items, cmd_found;

	for (int i = 0; i < 127; i++)
		command_p[i] = NULL;

	if (!user)
	{
		return;
	}
	if (!cmd)
	{
		return;
	}
	if (!cmd[0])
	{
		return;
	}
	
	int total_params = 0;
	if (strlen(cmd)>2)
	{
		for (unsigned int q = 0; q < strlen(cmd)-1; q++)
		{
			if ((cmd[q] == ' ') && (cmd[q+1] == ':'))
			{
				total_params++;
				// found a 'trailing', we dont count them after this.
				break;
			}
			if (cmd[q] == ' ')
				total_params++;
		}
	}

	// another phidjit bug...
	if (total_params > 126)
	{
		*(strchr(cmd,' ')) = '\0';
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

	if (!strchr(cmd,' '))
	{
		/* no parameters, lets skip the formalities and not chop up
		 * the string */
		log(DEBUG,"About to preprocess command with no params");
		items = 0;
		command_p[0] = NULL;
		parameters = NULL;
		for (unsigned int i = 0; i <= strlen(cmd); i++)
		{
			cmd[i] = toupper(cmd[i]);
		}
		command = cmd;
	}
	else
	{
		strcpy(cmd,"");
		j = 0;
		/* strip out extraneous linefeeds through mirc's crappy pasting (thanks Craig) */
		for (unsigned int i = 0; i < strlen(temp); i++)
		{
			if ((temp[i] != 10) && (temp[i] != 13) && (temp[i] != 0) && (temp[i] != 7))
			{
				cmd[j++] = temp[i];
				cmd[j] = 0;
			}
		}
		/* split the full string into a command plus parameters */
		parameters = p;
		strcpy(p," ");
		command = cmd;
		if (strchr(cmd,' '))
		{
			for (unsigned int i = 0; i <= strlen(cmd); i++)
			{
				/* capitalise the command ONLY, leave params intact */
				cmd[i] = toupper(cmd[i]);
				/* are we nearly there yet?! :P */
				if (cmd[i] == ' ')
				{
					command = cmd;
					parameters = cmd+i+1;
					cmd[i] = '\0';
					break;
				}
			}
		}
		else
		{
			for (unsigned int i = 0; i <= strlen(cmd); i++)
			{
				cmd[i] = toupper(cmd[i]);
			}
		}

	}
	cmd_found = 0;
	
	if (strlen(command)>MAXCOMMAND)
	{
		WriteServ(user->fd,"421 %s %s :Command too long",user->nick,command);
		return;
	}
	
	for (unsigned int x = 0; x < strlen(command); x++)
	{
		if (((command[x] < 'A') || (command[x] > 'Z')) && (command[x] != '.'))
		{
			if (((command[x] < '0') || (command[x]> '9')) && (command[x] != '-'))
			{
				if (strchr("@!\"$%^&*(){}[]_=+;:'#~,<>/?\\|`",command[x]))
				{
					stats->statsUnknown++;
					WriteServ(user->fd,"421 %s %s :Unknown command",user->nick,command);
					return;
				}
			}
		}
	}

	std::string xcommand = command;
	for (unsigned int i = 0; i != cmdlist.size(); i++)
	{
		if (cmdlist[i].command[0])
		{
			if (strlen(command)>=(strlen(cmdlist[i].command))) if (!strncmp(command, cmdlist[i].command,MAXCOMMAND))
			{
				if (parameters)
				{
					if (parameters[0])
					{
						items = process_parameters(command_p,parameters);
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
				
				if (user)
				{
					/* activity resets the ping pending timer */
					user->nping = TIME + user->pingmax;
					if ((items) < cmdlist[i].min_params)
					{
					        log(DEBUG,"process_command: not enough parameters: %s %s",user->nick,command);
						WriteServ(user->fd,"461 %s %s :Not enough parameters",user->nick,command);
						return;
					}
					if ((!strchr(user->modes,cmdlist[i].flags_needed)) && (cmdlist[i].flags_needed))
					{
					        log(DEBUG,"process_command: permission denied: %s %s",user->nick,command);
						WriteServ(user->fd,"481 %s :Permission Denied- You do not have the required operator privilages",user->nick);
						cmd_found = 1;
						return;
					}
					if ((cmdlist[i].flags_needed) && (!user->HasPermission(xcommand)))
					{
					        log(DEBUG,"process_command: permission denied: %s %s",user->nick,command);
						WriteServ(user->fd,"481 %s :Permission Denied- Oper type %s does not have access to command %s",user->nick,user->oper,command);
						cmd_found = 1;
						return;
					}
					/* if the command isnt USER, PASS, or NICK, and nick is empty,
					 * deny command! */
					if ((strncmp(command,"USER",4)) && (strncmp(command,"NICK",4)) && (strncmp(command,"PASS",4)))
					{
						if ((!isnick(user->nick)) || (user->registered != 7))
						{
						        log(DEBUG,"process_command: not registered: %s %s",user->nick,command);
							WriteServ(user->fd,"451 %s :You have not registered",command);
							return;
						}
					}
					if ((user->registered == 7) && (!strchr(user->modes,'o')))
					{
						std::stringstream dcmds(Config->DisabledCommands);
						while (!dcmds.eof())
						{
							std::string thiscmd;
							dcmds >> thiscmd;
							if (!strcasecmp(thiscmd.c_str(),command))
							{
								// command is disabled!
								WriteServ(user->fd,"421 %s %s :This command has been disabled.",user->nick,command);
								return;
							}
						}
					}
					if ((user->registered == 7) || (!strncmp(command,"USER",4)) || (!strncmp(command,"NICK",4)) || (!strncmp(command,"PASS",4)))
					{
						if (cmdlist[i].handler_function)
						{
							
							/* ikky /stats counters */
							if (temp)
							{
								cmdlist[i].use_count++;
								cmdlist[i].total_bytes+=strlen(temp);
							}

							int MOD_RESULT = 0;
							FOREACH_RESULT(OnPreCommand(command,command_p,items,user));
							if (MOD_RESULT == 1) {
								return;
							}

							/* WARNING: nothing may come after the
							 * command handler call, as the handler
							 * may free the user structure! */

							cmdlist[i].handler_function(command_p,items,user);
						}
						return;
					}
					else
					{
						WriteServ(user->fd,"451 %s :You have not registered",command);
						return;
					}
				}
				cmd_found = 1;
			}
		}
	}
	if ((!cmd_found) && (user))
	{
		stats->statsUnknown++;
		WriteServ(user->fd,"421 %s %s :Unknown command",user->nick,command);
	}
}

bool removecommands(const char* source)
{
	bool go_again = true;
	while (go_again)
	{
		go_again = false;
		for (std::deque<command_t>::iterator i = cmdlist.begin(); i != cmdlist.end(); i++)
		{
			if (!strcmp(i->source,source))
			{
				log(DEBUG,"removecommands(%s) Removing dependent command: %s",i->source,i->command);
				cmdlist.erase(i);
				go_again = true;
				break;
			}
		}
	}
	return true;
}


void process_buffer(const char* cmdbuf,userrec *user)
{
	if (!user)
	{
		log(DEFAULT,"*** BUG *** process_buffer was given an invalid parameter");
		return;
	}
	char cmd[MAXBUF];
	if (!cmdbuf)
	{
		log(DEFAULT,"*** BUG *** process_buffer was given an invalid parameter");
		return;
	}
	if (!cmdbuf[0])
	{
		return;
	}
	while (*cmdbuf == ' ') cmdbuf++; // strip leading spaces

	strlcpy(cmd,cmdbuf,MAXBUF);
	if (!cmd[0])
	{
		return;
	}
	int sl = strlen(cmd)-1;
	if ((cmd[sl] == 13) || (cmd[sl] == 10))
	{
		cmd[sl] = '\0';
	}
	sl = strlen(cmd)-1;
	if ((cmd[sl] == 13) || (cmd[sl] == 10))
	{
		cmd[sl] = '\0';
	}
	sl = strlen(cmd)-1;
	while (cmd[sl] == ' ') // strip trailing spaces
	{
		cmd[sl] = '\0';
		sl = strlen(cmd)-1;
	}

	if (!cmd[0])
	{
		return;
	}
        log(DEBUG,"CMDIN: %s %s",user->nick,cmd);
	tidystring(cmd);
	if ((user) && (cmd))
	{
		process_command(user,cmd);
	}
}

char* InspIRCd::ModuleError()
{
	return MODERR;
}

void InspIRCd::erase_factory(int j)
{
	int v = 0;
	for (std::vector<ircd_module*>::iterator t = factory.begin(); t != factory.end(); t++)
	{
		if (v == j)
		{
                	factory.erase(t);
                 	factory.push_back(NULL);
                 	return;
           	}
		v++;
     	}
}

void InspIRCd::erase_module(int j)
{
	int v1 = 0;
	for (std::vector<Module*>::iterator m = modules.begin(); m!= modules.end(); m++)
        {
                if (v1 == j)
                {
			delete *m;
                        modules.erase(m);
                        modules.push_back(NULL);
			break;
                }
		v1++;
        }
	int v2 = 0;
        for (std::vector<std::string>::iterator v = Config->module_names.begin(); v != Config->module_names.end(); v++)
        {
                if (v2 == j)
                {
                       Config->module_names.erase(v);
                       break;
                }
		v2++;
        }

}

bool InspIRCd::UnloadModule(const char* filename)
{
	std::string filename_str = filename;
	for (unsigned int j = 0; j != Config->module_names.size(); j++)
	{
		if (Config->module_names[j] == filename_str)
		{
			if (modules[j]->GetVersion().Flags & VF_STATIC)
			{
				log(DEFAULT,"Failed to unload STATIC module %s",filename);
				snprintf(MODERR,MAXBUF,"Module not unloadable (marked static)");
				return false;
			}
			/* Give the module a chance to tidy out all its metadata */
			for (chan_hash::iterator c = chanlist.begin(); c != chanlist.end(); c++)
			{
				modules[j]->OnCleanup(TYPE_CHANNEL,c->second);
			}
			for (user_hash::iterator u = clientlist.begin(); u != clientlist.end(); u++)
			{
				modules[j]->OnCleanup(TYPE_USER,u->second);
			}
			FOREACH_MOD OnUnloadModule(modules[j],Config->module_names[j]);
			// found the module
			log(DEBUG,"Deleting module...");
			erase_module(j);
			log(DEBUG,"Erasing module entry...");
			erase_factory(j);
                        log(DEBUG,"Removing dependent commands...");
                        removecommands(filename);
			log(DEFAULT,"Module %s unloaded",filename);
			MODCOUNT--;
			return true;
		}
	}
	log(DEFAULT,"Module %s is not loaded, cannot unload it!",filename);
	snprintf(MODERR,MAXBUF,"Module not loaded");
	return false;
}

bool InspIRCd::LoadModule(const char* filename)
{
	char modfile[MAXBUF];
#ifdef STATIC_LINK
	snprintf(modfile,MAXBUF,"%s",filename);
#else
	snprintf(modfile,MAXBUF,"%s/%s",Config->ModPath,filename);
#endif
	std::string filename_str = filename;
#ifndef STATIC_LINK
	if (!DirValid(modfile))
	{
		log(DEFAULT,"Module %s is not within the modules directory.",modfile);
		snprintf(MODERR,MAXBUF,"Module %s is not within the modules directory.",modfile);
		return false;
	}
#endif
	log(DEBUG,"Loading module: %s",modfile);
#ifndef STATIC_LINK
        if (FileExists(modfile))
        {
#endif
		for (unsigned int j = 0; j < Config->module_names.size(); j++)
		{
			if (Config->module_names[j] == filename_str)
			{
				log(DEFAULT,"Module %s is already loaded, cannot load a module twice!",modfile);
				snprintf(MODERR,MAXBUF,"Module already loaded");
				return false;
			}
		}
		ircd_module* a = new ircd_module(modfile);
                factory[MODCOUNT+1] = a;
                if (factory[MODCOUNT+1]->LastError())
                {
                        log(DEFAULT,"Unable to load %s: %s",modfile,factory[MODCOUNT+1]->LastError());
			snprintf(MODERR,MAXBUF,"Loader/Linker error: %s",factory[MODCOUNT+1]->LastError());
			MODCOUNT--;
			return false;
                }
                if (factory[MODCOUNT+1]->factory)
                {
			Module* m = factory[MODCOUNT+1]->factory->CreateModule(MyServer);
                        modules[MODCOUNT+1] = m;
                        /* save the module and the module's classfactory, if
                         * this isnt done, random crashes can occur :/ */
                        Config->module_names.push_back(filename);
                }
		else
                {
                        log(DEFAULT,"Unable to load %s",modfile);
			snprintf(MODERR,MAXBUF,"Factory function failed!");
			return false;
                }
#ifndef STATIC_LINK
        }
        else
        {
                log(DEFAULT,"InspIRCd: startup: Module Not Found %s",modfile);
		snprintf(MODERR,MAXBUF,"Module file could not be found");
		return false;
        }
#endif
	MODCOUNT++;
	FOREACH_MOD OnLoadModule(modules[MODCOUNT],filename_str);
	return true;
}

int InspIRCd::Run()
{
	bool expire_run = false;
	std::vector<int> activefds;
	int incomingSockfd;
	int in_port;
	userrec* cu = NULL;
	InspSocket* s = NULL;
	InspSocket* s_del = NULL;
	char* target;
	unsigned int numberactive;
        sockaddr_in sock_us;     // our port number
        socklen_t uslen;         // length of our port number

	if (!Config->nofork)
	{
		freopen("/dev/null","w",stdout);
		freopen("/dev/null","w",stderr);
	}

	/* Add the listening sockets used for client inbound connections
	 * to the socket engine
	 */
	for (int count = 0; count < BoundPortCount; count++)
		SE->AddFd(openSockfd[count],true,X_LISTEN);

	WritePID(Config->PID);

	/* main loop, this never returns */
	for (;;)
	{
		/* time() seems to be a pretty expensive syscall, so avoid calling it too much.
		 * Once per loop iteration is pleanty.
		 */
		OLDTIME = TIME;
		TIME = time(NULL);

		/* Run background module timers every few seconds
		 * (the docs say modules shouldnt rely on accurate
		 * timing using this event, so we dont have to
		 * time this exactly).
		 */
		if (((TIME % 8) == 0) && (!expire_run))
		{
			expire_lines();
			FOREACH_MOD OnBackgroundTimer(TIME);
			expire_run = true;
			continue;
		}
		if ((TIME % 8) == 1)
			expire_run = false;
		
		/* Once a second, do the background processing */
		if (TIME != OLDTIME)
			while (DoBackgroundUserStuff(TIME));

		/* Call the socket engine to wait on the active
		 * file descriptors. The socket engine has everything's
		 * descriptors in its list... dns, modules, users,
		 * servers... so its nice and easy, just one call.
		 */
		SE->Wait(activefds);

		/**
		 * Now process each of the fd's. For users, we have a fast
		 * lookup table which can find a user by file descriptor, so
		 * processing them by fd isnt expensive. If we have a lot of
		 * listening ports or module sockets though, things could get
		 * ugly.
		 */
		numberactive = activefds.size();
		for (unsigned int activefd = 0; activefd < numberactive; activefd++)
		{
			int socket_type = SE->GetType(activefds[activefd]);
			switch (socket_type)
			{
				case X_ESTAB_CLIENT:

					cu = fd_ref_table[activefds[activefd]];
					if (cu)
						ProcessUser(cu);

				break;

				case X_ESTAB_MODULE:

					/* Process module-owned sockets.
					 * Modules are encouraged to inherit their sockets from
					 * InspSocket so we can process them neatly like this.
					 */
					s = socket_ref[activefds[activefd]];

					if ((s) && (!s->Poll()))
					{
						log(DEBUG,"Socket poll returned false, close and bail");
						SE->DelFd(s->GetFd());
						for (std::vector<InspSocket*>::iterator a = module_sockets.begin(); a < module_sockets.end(); a++)
						{
							s_del = (InspSocket*)*a;
							if ((s_del) && (s_del->GetFd() == activefds[activefd]))
							{
								module_sockets.erase(a);
								break;
							}
						}
						s->Close();
						delete s;
					}

				break;

				case X_ESTAB_DNS:

					/* When we are using single-threaded dns,
					 * the sockets for dns end up in our mainloop.
					 * When we are using multi-threaded dns,
					 * each thread has its own basic poll() loop
					 * within it, making them 'fire and forget'
					 * and independent of the mainloop.
					 */
#ifndef THREADED_DNS
					dns_poll(activefds[activefd]);
#endif
				break;
				
				case X_LISTEN:

					/* It's a listener */
					uslen = sizeof(sock_us);
					length = sizeof(client);
					incomingSockfd = accept (activefds[activefd],(struct sockaddr*)&client,&length);
					if (!getsockname(incomingSockfd,(sockaddr*)&sock_us,&uslen))
					{
						in_port = ntohs(sock_us.sin_port);
						log(DEBUG,"Accepted socket %d",incomingSockfd);
						target = (char*)inet_ntoa(client.sin_addr);
						/* Years and years ago, we used to resolve here
						 * using gethostbyaddr(). That is sucky and we
						 * don't do that any more...
						 */
						if (incomingSockfd >= 0)
						{
							FOREACH_MOD OnRawSocketAccept(incomingSockfd, target, in_port);
							stats->statsAccept++;
							AddClient(incomingSockfd, target, in_port, false, target);
							log(DEBUG,"Adding client on port %lu fd=%lu",(unsigned long)in_port,(unsigned long)incomingSockfd);
						}
						else
						{
							WriteOpers("*** WARNING: accept() failed on port %lu (%s)",(unsigned long)in_port,target);
							log(DEBUG,"accept failed: %lu",(unsigned long)in_port);
							stats->statsRefused++;
						}
					}
					else
					{
						log(DEBUG,"Couldnt look up the port number for fd %lu (OS BROKEN?!)",incomingSockfd);
						shutdown(incomingSockfd,2);
						close(incomingSockfd);
					}
				break;

				default:
					/* Something went wrong if we're in here.
					 * In fact, so wrong, im not quite sure
					 * what we would do, so for now, its going
					 * to safely do bugger all.
					 */
				break;
			}
		}

	}
	/* This is never reached -- we hope! */
	return 0;
}

/**********************************************************************************/

/**
 * An ircd in four lines! bwahahaha. ahahahahaha. ahahah *cough*.
 */

int main(int argc, char** argv)
{
        ServerInstance = new InspIRCd(argc, argv);
        ServerInstance->Run();
        delete ServerInstance;
        return 0;
}

