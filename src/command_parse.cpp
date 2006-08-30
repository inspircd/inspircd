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

#include "inspircd.h"
#include "configreader.h"
#include <algorithm>
#include "users.h"
#include "modules.h"
#include "wildcard.h"
#include "xline.h"
#include "socketengine.h"
#include "userprocess.h"
#include "socket.h"
#include "command_parse.h"
#define nspace __gnu_cxx

/*       XXX Serious WTFness XXX
 *
 * Well, unless someone invents a wildcard or
 * regexp #include, and makes it a standard,
 * we're stuck with this way of including all
 * the commands.
 */

#include "commands/cmd_admin.h"
#include "commands/cmd_away.h"
#include "commands/cmd_commands.h"
#include "commands/cmd_connect.h"
#include "commands/cmd_die.h"
#include "commands/cmd_eline.h"
#include "commands/cmd_gline.h"
#include "commands/cmd_info.h"
#include "commands/cmd_invite.h"
#include "commands/cmd_ison.h"
#include "commands/cmd_join.h"
#include "commands/cmd_kick.h"
#include "commands/cmd_kill.h"
#include "commands/cmd_kline.h"
#include "commands/cmd_links.h"
#include "commands/cmd_list.h"
#include "commands/cmd_loadmodule.h"
#include "commands/cmd_lusers.h"
#include "commands/cmd_map.h"
#include "commands/cmd_modules.h"
#include "commands/cmd_motd.h"
#include "commands/cmd_names.h"
#include "commands/cmd_nick.h"
#include "commands/cmd_notice.h"
#include "commands/cmd_oper.h"
#include "commands/cmd_part.h"
#include "commands/cmd_pass.h"
#include "commands/cmd_ping.h"
#include "commands/cmd_pong.h"
#include "commands/cmd_privmsg.h"
#include "commands/cmd_qline.h"
#include "commands/cmd_quit.h"
#include "commands/cmd_rehash.h"
#include "commands/cmd_restart.h"
#include "commands/cmd_rules.h"
#include "commands/cmd_server.h"
#include "commands/cmd_squit.h"
#include "commands/cmd_stats.h"
#include "commands/cmd_summon.h"
#include "commands/cmd_time.h"
#include "commands/cmd_topic.h"
#include "commands/cmd_trace.h"
#include "commands/cmd_unloadmodule.h"
#include "commands/cmd_user.h"
#include "commands/cmd_userhost.h"
#include "commands/cmd_users.h"
#include "commands/cmd_version.h"
#include "commands/cmd_wallops.h"
#include "commands/cmd_who.h"
#include "commands/cmd_whois.h"
#include "commands/cmd_whowas.h"
#include "commands/cmd_zline.h"

bool InspIRCd::ULine(const char* server)
{
	if (!server)
		return false;
	if (!*server)
		return true;

	return (find(Config->ulines.begin(),Config->ulines.end(),server) != Config->ulines.end());
}

int InspIRCd::OperPassCompare(const char* data,const char* input)
{
	int MOD_RESULT = 0;
	FOREACH_RESULT_I(this,I_OnOperCompare,OnOperCompare(data,input))
	Log(DEBUG,"OperPassCompare: %d",MOD_RESULT);
	if (MOD_RESULT == 1)
		return 0;
	if (MOD_RESULT == -1)
		return 1;
	Log(DEBUG,"strcmp fallback: '%s' '%s' %d",data,input,strcmp(data,input));
	return strcmp(data,input);
}

long InspIRCd::Duration(const char* str)
{
	char n_field[MAXBUF];
	long total = 0;
	n_field[0] = 0;

	if ((!strchr(str,'s')) && (!strchr(str,'m')) && (!strchr(str,'h')) && (!strchr(str,'d')) && (!strchr(str,'w')) && (!strchr(str,'y')))
	{
		std::string n = str;
		n += 's';
		return Duration(n.c_str());
	}
	
	for (char* i = (char*)str; *i; i++)
	{
		// if we have digits, build up a string for the value in n_field,
		// up to 10 digits in size.
		if ((*i >= '0') && (*i <= '9'))
		{
			strlcat(n_field,i,10);
		}
		else
		{
			// we dont have a digit, check for numeric tokens
			switch (tolower(*i))
			{
				case 's':
					total += atoi(n_field);
				break;

				case 'm':
					total += (atoi(n_field)*duration_m);
				break;

				case 'h':
					total += (atoi(n_field)*duration_h);
				break;

				case 'd':
					total += (atoi(n_field)*duration_d);
				break;

				case 'w':
					total += (atoi(n_field)*duration_w);
				break;

				case 'y':
					total += (atoi(n_field)*duration_y);
				break;
			}
			n_field[0] = 0;
		}
	}
	// add trailing seconds
	total += atoi(n_field);
	
	return total;
}

/* All other ircds when doing this check usually just look for a string of *@* or *. We're smarter than that, though. */

bool InspIRCd::HostMatchesEveryone(const std::string &mask, userrec* user)
{
	char buffer[MAXBUF];
	char itrigger[MAXBUF];
	long matches = 0;
	
	if (!Config->ConfValue(Config->config_data, "insane","trigger", 0, itrigger, MAXBUF))
		strlcpy(itrigger,"95.5",MAXBUF);
	
	if (Config->ConfValueBool(Config->config_data, "insane","hostmasks", 0))
		return false;
	
	for (user_hash::iterator u = clientlist.begin(); u != clientlist.end(); u++)
	{
		strlcpy(buffer,u->second->ident,MAXBUF);
		charlcat(buffer,'@',MAXBUF);
		strlcat(buffer,u->second->host,MAXBUF);
		if (match(buffer,mask.c_str()))
			matches++;
	}
	float percent = ((float)matches / (float)clientlist.size()) * 100;
	if (percent > (float)atof(itrigger))
	{
		WriteOpers("*** \2WARNING\2: %s tried to set a G/K/E line mask of %s, which covers %.2f%% of the network!",user->nick,mask.c_str(),percent);
		return true;
	}
	return false;
}

bool InspIRCd::IPMatchesEveryone(const std::string &ip, userrec* user)
{
	char itrigger[MAXBUF];
	long matches = 0;
	
	if (!Config->ConfValue(Config->config_data, "insane","trigger",0,itrigger,MAXBUF))
		strlcpy(itrigger,"95.5",MAXBUF);
	
	if (Config->ConfValueBool(Config->config_data, "insane","ipmasks",0))
		return false;
	
	for (user_hash::iterator u = clientlist.begin(); u != clientlist.end(); u++)
	{
		if (match(u->second->GetIPString(),ip.c_str(),true))
			matches++;
	}
	
	float percent = ((float)matches / (float)clientlist.size()) * 100;
	if (percent > (float)atof(itrigger))
	{
		WriteOpers("*** \2WARNING\2: %s tried to set a Z line mask of %s, which covers %.2f%% of the network!",user->nick,ip.c_str(),percent);
		return true;
	}
	return false;
}

bool InspIRCd::NickMatchesEveryone(const std::string &nick, userrec* user)
{
	char itrigger[MAXBUF];
	long matches = 0;
	
	if (!Config->ConfValue(Config->config_data, "insane","trigger",0,itrigger,MAXBUF))
		strlcpy(itrigger,"95.5",MAXBUF);
	
	if (Config->ConfValueBool(Config->config_data, "insane","nickmasks",0))
		return false;

	for (user_hash::iterator u = clientlist.begin(); u != clientlist.end(); u++)
	{
		if (match(u->second->nick,nick.c_str()))
			matches++;
	}
	
	float percent = ((float)matches / (float)clientlist.size()) * 100;
	if (percent > (float)atof(itrigger))
	{
		WriteOpers("*** \2WARNING\2: %s tried to set a Q line mask of %s, which covers %.2f%% of the network!",user->nick,nick.c_str(),percent);
		return true;
	}
	return false;
}





/* Special commands which may occur without registration of the user */
cmd_user* command_user;
cmd_nick* command_nick;
cmd_pass* command_pass;


/* LoopCall is used to call a command classes handler repeatedly based on the contents of a comma seperated list.
 * There are two overriden versions of this method, one of which takes two potential lists and the other takes one.
 * We need a version which takes two potential lists for JOIN, because a JOIN may contain two lists of items at once,
 * the channel names and their keys as follows:
 * JOIN #chan1,#chan2,#chan3 key1,,key3
 * Therefore, we need to deal with both lists concurrently. The first instance of this method does that by creating
 * two instances of irc::commasepstream and reading them both together until the first runs out of tokens.
 * The second version is much simpler and just has the one stream to read, and is used in NAMES, WHOIS, PRIVMSG etc.
 * Both will only parse until they reach ServerInstance->Config->MaxTargets number of targets, to stop abuse via spam.
 */
int CommandParser::LoopCall(userrec* user, command_t* CommandObj, const char** parameters, int pcnt, unsigned int splithere, unsigned int extra)
{
	/* First check if we have more than one item in the list, if we don't we return zero here and the handler
	 * which called us just carries on as it was.
	 */
	if (!strchr(parameters[splithere],','))
		return 0;

	/* Create two lists, one for channel names, one for keys
	 */
	irc::commasepstream items1(parameters[splithere]);
	irc::commasepstream items2(parameters[extra]);
	std::string item = "";
	unsigned int max = 0;

	/* Attempt to iterate these lists and call the command objech
	 * which called us, for every parameter pair until there are
	 * no more left to parse.
	 */
	while (((item = items1.GetToken()) != "") && (max++ < ServerInstance->Config->MaxTargets))
	{
		std::string extrastuff = items2.GetToken();
		parameters[splithere] = item.c_str();
		parameters[extra] = extrastuff.c_str();
		CommandObj->Handle(parameters,pcnt,user);
	}
	return 1;
}

int CommandParser::LoopCall(userrec* user, command_t* CommandObj, const char** parameters, int pcnt, unsigned int splithere)
{
	/* First check if we have more than one item in the list, if we don't we return zero here and the handler
	 * which called us just carries on as it was.
	 */
	if (!strchr(parameters[splithere],','))
		return 0;

	/* Only one commasepstream here */
	irc::commasepstream items1(parameters[splithere]);
	std::string item = "";
	unsigned int max = 0;

	/* Parse the commasepstream until there are no tokens remaining.
	 * Each token we parse out, call the command handler that called us
	 * with it
	 */
	while (((item = items1.GetToken()) != "") && (max++ < ServerInstance->Config->MaxTargets))
	{
		parameters[splithere] = item.c_str();
		CommandObj->Handle(parameters,pcnt,user);
	}
	/* By returning 1 we tell our caller that nothing is to be done,
	 * as all the previous calls handled the data. This makes the parent
	 * return without doing any processing.
	 */
	return 1;
}

bool CommandParser::IsValidCommand(const std::string &commandname, int pcnt, userrec * user)
{
	nspace::hash_map<std::string,command_t*>::iterator n = cmdlist.find(commandname);

	if (n != cmdlist.end())
	{
		if ((pcnt>=n->second->min_params) && (n->second->source != "<core>"))
		{
			if ((!n->second->flags_needed) || (user->modes[n->second->flags_needed-65]))
			{
				if (n->second->flags_needed)
				{
					return ((user->HasPermission(commandname)) || (ServerInstance->ULine(user->server)));
				}
				return true;
			}
		}
	}
	return false;
}

// calls a handler function for a command

bool CommandParser::CallHandler(const std::string &commandname,const char** parameters, int pcnt, userrec *user)
{
	nspace::hash_map<std::string,command_t*>::iterator n = cmdlist.find(commandname);

	if (n != cmdlist.end())
	{
		if (pcnt >= n->second->min_params)
		{
			if ((!n->second->flags_needed) || (user->modes[n->second->flags_needed-65]))
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

void CommandParser::ProcessCommand(userrec *user, std::string &cmd)
{
	const char *command_p[127];
	int items = 0;
	std::string para[127];
	irc::tokenstream tokens(cmd);
	std::string command = tokens.GetToken();

	while (((para[items] = tokens.GetToken()) != "") && (items < 127))
		command_p[items] = para[items++].c_str();

	std::transform(command.begin(), command.end(), command.begin(), ::toupper);
		
	int MOD_RESULT = 0;
	FOREACH_RESULT(I_OnPreCommand,OnPreCommand(command,command_p,items,user,false));
	if (MOD_RESULT == 1) {
		return;
	}

	nspace::hash_map<std::string,command_t*>::iterator cm = cmdlist.find(command);
	
	if (cm != cmdlist.end())
	{
		if (user)
		{
			/* activity resets the ping pending timer */
			user->nping = ServerInstance->Time() + user->pingmax;
			if (cm->second->flags_needed)
			{
				if (!user->IsModeSet(cm->second->flags_needed))
				{
					user->WriteServ("481 %s :Permission Denied- You do not have the required operator privilages",user->nick);
					return;
				}
				if (!user->HasPermission(command))
				{
					user->WriteServ("481 %s :Permission Denied- Oper type %s does not have access to command %s",user->nick,user->oper,command.c_str());
					return;
				}
			}
			if ((user->registered == REG_ALL) && (!*user->oper) && (cm->second->IsDisabled()))
			{
				/* command is disabled! */
				user->WriteServ("421 %s %s :This command has been disabled.",user->nick,command.c_str());
				return;
			}
			if (items < cm->second->min_params)
			{
				user->WriteServ("461 %s %s :Not enough parameters.", user->nick, command.c_str());
				/* If syntax is given, display this as the 461 reply */
				if ((ServerInstance->Config->SyntaxHints) && (cm->second->syntax.length()))
					user->WriteServ("304 %s :SYNTAX %s %s", user->nick, cm->second->command.c_str(), cm->second->syntax.c_str());
				return;
			}
			if ((user->registered == REG_ALL) || (cm->second == command_user) || (cm->second == command_nick) || (cm->second == command_pass))
			{
				/* ikky /stats counters */
				cm->second->use_count++;
				cm->second->total_bytes += cmd.length();

				int MOD_RESULT = 0;
				FOREACH_RESULT(I_OnPreCommand,OnPreCommand(command,command_p,items,user,true));
				if (MOD_RESULT == 1)
					return;

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
				user->WriteServ("451 %s :You have not registered",command.c_str());
				return;
			}
		}
	}
	else if (user)
	{
		ServerInstance->stats->statsUnknown++;
		user->WriteServ("421 %s %s :Unknown command",user->nick,command.c_str());
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
				ServerInstance->Log(DEBUG,"removecommands(%s) Removing dependent command: %s",x->source.c_str(),x->command.c_str());
				cmdlist.erase(i);
				go_again = true;
				break;
			}
		}
	}

	return true;
}

void CommandParser::ProcessBuffer(std::string &buffer,userrec *user)
{
	std::string::size_type a;

	if (!user)
		return;

	while ((a = buffer.rfind("\n")) != std::string::npos)
		buffer.erase(a);
	while ((a = buffer.rfind("\r")) != std::string::npos)
		buffer.erase(a);

	if (buffer.length())
	{
		ServerInstance->Log(DEBUG,"CMDIN: %s %s",user->nick,buffer.c_str());
		this->ProcessCommand(user,buffer);
	}
}

bool CommandParser::CreateCommand(command_t *f)
{
	/* create the command and push it onto the table */
	if (cmdlist.find(f->command) == cmdlist.end())
	{
		cmdlist[f->command] = f;
		ServerInstance->Log(DEBUG,"Added command %s (%lu parameters)",f->command.c_str(),(unsigned long)f->min_params);
		return true;
	}
	else return false;
}

CommandParser::CommandParser(InspIRCd* Instance) : ServerInstance(Instance)
{
	this->SetupCommandTable();
}

void CommandParser::SetupCommandTable()
{
	/* These three are special (can occur without
	 * full user registration) and so are saved
	 * for later use.
	 */
	command_user = new cmd_user(ServerInstance);
	command_nick = new cmd_nick(ServerInstance);
	command_pass = new cmd_pass(ServerInstance);
	this->CreateCommand(command_user);
	this->CreateCommand(command_nick);
	this->CreateCommand(command_pass);

	/* The rest of these arent special. boo hoo.
	 */
	this->CreateCommand(new cmd_quit(ServerInstance));
	this->CreateCommand(new cmd_version(ServerInstance));
	this->CreateCommand(new cmd_ping(ServerInstance));
	this->CreateCommand(new cmd_pong(ServerInstance));
	this->CreateCommand(new cmd_admin(ServerInstance));
	this->CreateCommand(new cmd_privmsg(ServerInstance));
	this->CreateCommand(new cmd_info(ServerInstance));
	this->CreateCommand(new cmd_time(ServerInstance));
	this->CreateCommand(new cmd_whois(ServerInstance));
	this->CreateCommand(new cmd_wallops(ServerInstance));
	this->CreateCommand(new cmd_notice(ServerInstance));
	this->CreateCommand(new cmd_join(ServerInstance));
	this->CreateCommand(new cmd_names(ServerInstance));
	this->CreateCommand(new cmd_part(ServerInstance));
	this->CreateCommand(new cmd_kick(ServerInstance));
	this->CreateCommand(new cmd_mode(ServerInstance));
	this->CreateCommand(new cmd_topic(ServerInstance));
	this->CreateCommand(new cmd_who(ServerInstance));
	this->CreateCommand(new cmd_motd(ServerInstance));
	this->CreateCommand(new cmd_rules(ServerInstance));
	this->CreateCommand(new cmd_oper(ServerInstance));
	this->CreateCommand(new cmd_list(ServerInstance));
	this->CreateCommand(new cmd_die(ServerInstance));
	this->CreateCommand(new cmd_restart(ServerInstance));
	this->CreateCommand(new cmd_kill(ServerInstance));
	this->CreateCommand(new cmd_rehash(ServerInstance));
	this->CreateCommand(new cmd_lusers(ServerInstance));
	this->CreateCommand(new cmd_stats(ServerInstance));
	this->CreateCommand(new cmd_userhost(ServerInstance));
	this->CreateCommand(new cmd_away(ServerInstance));
	this->CreateCommand(new cmd_ison(ServerInstance));
	this->CreateCommand(new cmd_summon(ServerInstance));
	this->CreateCommand(new cmd_users(ServerInstance));
	this->CreateCommand(new cmd_invite(ServerInstance));
	this->CreateCommand(new cmd_trace(ServerInstance));
	this->CreateCommand(new cmd_whowas(ServerInstance));
	this->CreateCommand(new cmd_connect(ServerInstance));
	this->CreateCommand(new cmd_squit(ServerInstance));
	this->CreateCommand(new cmd_modules(ServerInstance));
	this->CreateCommand(new cmd_links(ServerInstance));
	this->CreateCommand(new cmd_map(ServerInstance));
	this->CreateCommand(new cmd_kline(ServerInstance));
	this->CreateCommand(new cmd_gline(ServerInstance));
	this->CreateCommand(new cmd_zline(ServerInstance));
	this->CreateCommand(new cmd_qline(ServerInstance));
	this->CreateCommand(new cmd_eline(ServerInstance));
	this->CreateCommand(new cmd_loadmodule(ServerInstance));
	this->CreateCommand(new cmd_unloadmodule(ServerInstance));
	this->CreateCommand(new cmd_server(ServerInstance));
	this->CreateCommand(new cmd_commands(ServerInstance));
}

