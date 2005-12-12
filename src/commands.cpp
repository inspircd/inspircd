/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  Inspire is copyright (C) 2002-2004 ChatSpike-Dev.
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

using namespace std;

#include "inspircd_config.h"
#include "inspircd.h"
#include "inspircd_io.h"
#include "inspircd_util.h"
#include <unistd.h>
#include <sys/errno.h>
#include <sys/ioctl.h>
#include <sys/utsname.h>
#include <cstdio>
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
#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>
#ifdef THREADED_DNS
#include <pthread.h>
#endif
#ifndef RUSAGE_SELF
#define   RUSAGE_SELF     0
#define   RUSAGE_CHILDREN     -1
#endif
#include "users.h"
#include "ctables.h"
#include "globals.h"
#include "modules.h"
#include "dynamic.h"
#include "wildcard.h"
#include "message.h"
#include "commands.h"
#include "mode.h"
#include "xline.h"
#include "inspstring.h"
#include "dnsqueue.h"
#include "helperfuncs.h"
#include "hashcomp.h"
#include "socketengine.h"

extern SocketEngine* SE;

extern int MODCOUNT;
extern std::vector<Module*> modules;
extern std::vector<ircd_module*> factory;

extern int LogLevel;
extern char ServerName[MAXBUF];
extern char Network[MAXBUF];
extern char ServerDesc[MAXBUF];
extern char AdminName[MAXBUF];
extern char AdminEmail[MAXBUF];
extern char AdminNick[MAXBUF];
extern char diepass[MAXBUF];
extern char restartpass[MAXBUF];
extern char motd[MAXBUF];
extern char rules[MAXBUF];
extern char list[MAXBUF];
extern char PrefixQuit[MAXBUF];
extern char DieValue[MAXBUF];

extern int debugging;
extern int WHOWAS_STALE;
extern int WHOWAS_MAX;
extern int DieDelay;
extern time_t startup_time;
extern int NetBufferSize;
extern int MaxWhoResults;
extern time_t nb_start;

extern bool nofork;

extern time_t TIME;

extern std::vector<std::string> module_names;

extern char MyExecutable[1024];
extern int boundPortCount;
extern int portCount;

extern int ports[MAXSOCKS];

extern std::stringstream config_f;



extern FILE *log_file;

extern ClassVector Classes;

const long duration_m = 60;
const long duration_h = duration_m * 60;
const long duration_d = duration_h * 24;
const long duration_w = duration_d * 7;
const long duration_y = duration_w * 52;

typedef nspace::hash_map<std::string, userrec*, nspace::hash<string>, irc::StrHashComp> user_hash;
typedef nspace::hash_map<std::string, chanrec*, nspace::hash<string>, irc::StrHashComp> chan_hash;
typedef nspace::hash_map<in_addr,string*, nspace::hash<in_addr>, irc::InAddr_HashComp> address_cache;
typedef nspace::hash_map<std::string, WhoWasUser*, nspace::hash<string>, irc::StrHashComp> whowas_hash;
typedef std::deque<command_t> command_table;


extern user_hash clientlist;
extern chan_hash chanlist;
extern whowas_hash whowas;
extern command_table cmdlist;
extern file_cache MOTD;
extern file_cache RULES;
extern address_cache IP;

extern std::vector<userrec*> all_opers;

// This table references users by file descriptor.
// its an array to make it VERY fast, as all lookups are referenced
// by an integer, meaning there is no need for a scan/search operation.
extern userrec* fd_ref_table[65536];

extern int statsAccept,statsRefused,statsUnknown,statsCollisions,statsDns,statsDnsGood,statsDnsBad,statsConnects,statsSent,statsRecv;

void handle_join(char **parameters, int pcnt, userrec *user)
{
	chanrec* Ptr;
	
	if (loop_call(handle_join,parameters,pcnt,user,0,0,1))
		return;
	if (parameters[0][0] == '#')
	{
		Ptr = add_channel(user,parameters[0],parameters[1],false);
	}
}


void handle_part(char **parameters, int pcnt, userrec *user)
{
	if (pcnt > 1)
	{
		if (loop_call(handle_part,parameters,pcnt,user,0,pcnt-2,0))
			return;
		del_channel(user,parameters[0],parameters[1],false);
	}
	else
	{
		if (loop_call(handle_part,parameters,pcnt,user,0,pcnt-1,0))
			return;
		del_channel(user,parameters[0],NULL,false);
	}
}

void handle_commands(char **parameters, int pcnt, userrec *user)
{
	for (unsigned int i = 0; i < cmdlist.size(); i++)
	{
		WriteServ(user->fd,"902 %s :%s %s %d",user->nick,cmdlist[i].command,cmdlist[i].source,cmdlist[i].min_params);
	}
	WriteServ(user->fd,"903 %s :End of COMMANDS list",user->nick);
}

void handle_kick(char **parameters, int pcnt, userrec *user)
{
	chanrec* Ptr = FindChan(parameters[0]);
	userrec* u   = Find(parameters[1]);

	if ((!u) || (!Ptr))
	{
		WriteServ(user->fd,"401 %s %s :No such nick/channel",user->nick, parameters[0]);
		return;
	}
	
	if ((!has_channel(user,Ptr)) && (!is_uline(user->server)))
	{
		WriteServ(user->fd,"442 %s %s :You're not on that channel!",user->nick, parameters[0]);
		return;
	}
	
	char reason[MAXBUF];
	
	if (pcnt > 2)
	{
		strlcpy(reason,parameters[2],MAXBUF);
		if (strlen(reason)>MAXKICK)
		{
			reason[MAXKICK-1] = '\0';
		}

		kick_channel(user,u,Ptr,reason);
	}
	else
	{
		strlcpy(reason,user->nick,MAXBUF);
		kick_channel(user,u,Ptr,reason);
	}
	
}

void handle_loadmodule(char **parameters, int pcnt, userrec *user)
{
	if (LoadModule(parameters[0]))
	{
		WriteOpers("*** NEW MODULE: %s",parameters[0]);
		WriteServ(user->fd,"975 %s %s :Module successfully loaded.",user->nick, parameters[0]);
	}
	else
	{
		WriteServ(user->fd,"974 %s %s :Failed to load module: %s",user->nick, parameters[0],ModuleError());
	}
}

void handle_unloadmodule(char **parameters, int pcnt, userrec *user)
{
        if (UnloadModule(parameters[0]))
        {
                WriteOpers("*** MODULE UNLOADED: %s",parameters[0]);
                WriteServ(user->fd,"973 %s %s :Module successfully unloaded.",user->nick, parameters[0]);
        }
        else
        {
                WriteServ(user->fd,"972 %s %s :Failed to unload module: %s",user->nick, parameters[0],ModuleError());
        }
}

void handle_die(char **parameters, int pcnt, userrec *user)
{
	log(DEBUG,"die: %s",user->nick);
	if (!strcmp(parameters[0],diepass))
	{
		WriteOpers("*** DIE command from %s!%s@%s, terminating...",user->nick,user->ident,user->host);
		sleep(DieDelay);
		Exit(ERROR);
	}
	else
	{
		WriteOpers("*** Failed DIE Command from %s!%s@%s.",user->nick,user->ident,user->host);
	}
}

void handle_restart(char **parameters, int pcnt, userrec *user)
{
	char *argv[32];
	log(DEFAULT,"Restart: %s",user->nick);
	if (!strcmp(parameters[0],restartpass))
	{
		WriteOpers("*** RESTART command from %s!%s@%s, restarting server.",user->nick,user->ident,user->host);

		argv[0] = MyExecutable;
		argv[1] = "-wait";
		if (nofork)
		{
			argv[2] = "-nofork";
		}
		else
		{
			argv[2] = NULL;
		}
		argv[3] = NULL;
		
		// close ALL file descriptors
		send_error("Server restarting.");
		sleep(1);
		for (int i = 0; i < 65536; i++)
		{
			shutdown(i,2);
    			close(i);
		}
		sleep(2);
		
		execv(MyExecutable,argv);

		exit(0);
	}
	else
	{
		WriteOpers("*** Failed RESTART Command from %s!%s@%s.",user->nick,user->ident,user->host);
	}
}

void handle_kill(char **parameters, int pcnt, userrec *user)
{
	userrec *u = Find(parameters[0]);
	char killreason[MAXBUF];

        log(DEBUG,"kill: %s %s",parameters[0],parameters[1]);
	if (u)
	{
		log(DEBUG,"into kill mechanism");
		int MOD_RESULT = 0;
                FOREACH_RESULT(OnKill(user,u,parameters[1]));
                if (MOD_RESULT) {
			log(DEBUG,"A module prevented the kill with result %d",MOD_RESULT);
                        return;
                }

		if (strcmp(ServerName,u->server))
		{
			// remote kill
			WriteOpers("*** Remote kill by %s: %s!%s@%s (%s)",user->nick,u->nick,u->ident,u->host,parameters[1]);
			snprintf(killreason,MAXBUF,"[%s] Killed (%s (%s))",ServerName,user->nick,parameters[1]);
			WriteCommonExcept(u,"QUIT :%s",killreason);

			FOREACH_MOD OnRemoteKill(user,u,killreason);
			
			user_hash::iterator iter = clientlist.find(u->nick);
			if (iter != clientlist.end())
			{
				log(DEBUG,"deleting user hash value %d",iter->second);
				clientlist.erase(iter);
			}
			if (u->registered == 7)
			{
				purge_empty_chans(u);
			}
	                if (u->fd > -1)
        	                fd_ref_table[u->fd] = NULL;
			delete u;
		}
		else
		{
			// local kill
			log(DEFAULT,"LOCAL KILL: %s :%s!%s!%s (%s)", u->nick, ServerName,user->dhost,user->nick,parameters[1]);
			WriteTo(user, u, "KILL %s :%s!%s!%s (%s)", u->nick, ServerName,user->dhost,user->nick,parameters[1]);
			WriteOpers("*** Local Kill by %s: %s!%s@%s (%s)",user->nick,u->nick,u->ident,u->host,parameters[1]);
			snprintf(killreason,MAXBUF,"Killed (%s (%s))",user->nick,parameters[1]);
			kill_link(u,killreason);
		}
	}
	else
	{
		WriteServ(user->fd,"401 %s %s :No such nick/channel",user->nick, parameters[0]);
	}
}

void handle_summon(char **parameters, int pcnt, userrec *user)
{
	WriteServ(user->fd,"445 %s :SUMMON has been disabled (depreciated command)",user->nick);
}

void handle_users(char **parameters, int pcnt, userrec *user)
{
	WriteServ(user->fd,"445 %s :USERS has been disabled (depreciated command)",user->nick);
}

void handle_pass(char **parameters, int pcnt, userrec *user)
{
	// Check to make sure they havnt registered -- Fix by FCS
	if (user->registered == 7)
	{
		WriteServ(user->fd,"462 %s :You may not reregister",user->nick);
		return;
	}
	strlcpy(user->password,parameters[0],MAXBUF);
	if (!strcasecmp(parameters[0],Passwd(user)))
	{
		user->haspassed = true;
	}
}

void handle_invite(char **parameters, int pcnt, userrec *user)
{
	if (pcnt == 2)
	{
		userrec* u = Find(parameters[0]);
		chanrec* c = FindChan(parameters[1]);

		if ((!c) || (!u))
		{
			if (!c)
			{
				WriteServ(user->fd,"401 %s %s :No such nick/channel",user->nick, parameters[1]);
			}
			else
			{
				if (c->binarymodes & CM_INVITEONLY)
				{
					WriteServ(user->fd,"401 %s %s :No such nick/channel",user->nick, parameters[0]);
				}
			}

			return;
		}

		if (c->binarymodes & CM_INVITEONLY)
		{
			if (cstatus(user,c) < STATUS_HOP)
			{
				WriteServ(user->fd,"482 %s %s :You must be at least a half-operator to change modes on this channel",user->nick, c->name);
				return;
			}
		}
		if (has_channel(u,c))
	 	{
	 		WriteServ(user->fd,"443 %s %s %s :Is already on channel %s",user->nick,u->nick,c->name,c->name);
	 		return;
		}
		if (!has_channel(user,c))
	 	{
			WriteServ(user->fd,"442 %s %s :You're not on that channel!",user->nick, c->name);
	  		return;
		}

		int MOD_RESULT = 0;
		FOREACH_RESULT(OnUserPreInvite(user,u,c));
		if (MOD_RESULT == 1) {
			return;
		}

		u->InviteTo(c->name);
		WriteFrom(u->fd,user,"INVITE %s :%s",u->nick,c->name);
		WriteServ(user->fd,"341 %s %s %s",user->nick,u->nick,c->name);
		FOREACH_MOD OnUserInvite(user,u,c);
	}
	else
	{
		// pinched from ircu - invite with not enough parameters shows channels
		// youve been invited to but haven't joined yet.
		InvitedList* il = user->GetInviteList();
		for (InvitedList::iterator i = il->begin(); i != il->end(); i++)
		{
		        if (i->channel) {
				WriteServ(user->fd,"346 %s :%s",user->nick,i->channel);
		        }
		}
		WriteServ(user->fd,"347 %s :End of INVITE list",user->nick);
	}
}

void handle_topic(char **parameters, int pcnt, userrec *user)
{
	chanrec* Ptr;

	if (pcnt == 1)
	{
		if (strlen(parameters[0]) <= CHANMAX)
		{
			Ptr = FindChan(parameters[0]);
			if (Ptr)
			{
				if (((Ptr) && (!has_channel(user,Ptr))) && (Ptr->binarymodes & CM_SECRET))
				{
					WriteServ(user->fd,"401 %s %s :No such nick/channel",user->nick, Ptr->name);
					return;
				}
				if (Ptr->topicset)
				{
					WriteServ(user->fd,"332 %s %s :%s", user->nick, Ptr->name, Ptr->topic);
					WriteServ(user->fd,"333 %s %s %s %d", user->nick, Ptr->name, Ptr->setby, Ptr->topicset);
				}
				else
				{
					WriteServ(user->fd,"331 %s %s :No topic is set.", user->nick, Ptr->name);
				}
			}
			else
			{
				WriteServ(user->fd,"401 %s %s :No such nick/channel",user->nick, parameters[0]);
			}
		}
		return;
	}
	else if (pcnt>1)
	{
		if (strlen(parameters[0]) <= CHANMAX)
		{
			Ptr = FindChan(parameters[0]);
			if (Ptr)
			{
				if ((Ptr) && (!has_channel(user,Ptr)))
				{
					WriteServ(user->fd,"442 %s %s :You're not on that channel!",user->nick, Ptr->name);
					return;
				}
				if ((Ptr->binarymodes & CM_TOPICLOCK) && (cstatus(user,Ptr)<STATUS_HOP))
				{
					WriteServ(user->fd,"482 %s %s :You must be at least a half-operator to change modes on this channel", user->nick, Ptr->name);
					return;
				}

				char topic[MAXBUF];
				strlcpy(topic,parameters[1],MAXBUF);
				if (strlen(topic)>MAXTOPIC)
				{
					topic[MAXTOPIC] = '\0';
				}

                                if (!strcasecmp(user->server,ServerName))
                                {
                                        int MOD_RESULT = 0;
                                        FOREACH_RESULT(OnLocalTopicChange(user,Ptr,topic));
                                        if (MOD_RESULT)
                                                return;
                                }

				strlcpy(Ptr->topic,topic,MAXTOPIC);
				strlcpy(Ptr->setby,user->nick,NICKMAX);
				Ptr->topicset = TIME;
				WriteChannel(Ptr,user,"TOPIC %s :%s",Ptr->name, Ptr->topic);
				if (!strcasecmp(user->server,ServerName))
				{
					FOREACH_MOD OnPostLocalTopicChange(user,Ptr,topic);
				}
			}
			else
			{
				WriteServ(user->fd,"401 %s %s :No such nick/channel",user->nick, parameters[0]);
			}
		}
	}
}

void handle_names(char **parameters, int pcnt, userrec *user)
{
	chanrec* c;

	if (!pcnt)
	{
		WriteServ(user->fd,"366 %s * :End of /NAMES list.",user->nick);
		return;
	}

	if (loop_call(handle_names,parameters,pcnt,user,0,pcnt-1,0))
		return;
	c = FindChan(parameters[0]);
	if (c)
	{
                if (((c) && (!has_channel(user,c))) && (c->binarymodes & CM_SECRET))
                {
                      WriteServ(user->fd,"401 %s %s :No such nick/channel",user->nick, c->name);
                      return;
                }
		userlist(user,c);
		WriteServ(user->fd,"366 %s %s :End of /NAMES list.", user->nick, c->name);
	}
	else
	{
		WriteServ(user->fd,"401 %s %s :No such nick/channel",user->nick, parameters[0]);
	}
}

void handle_privmsg(char **parameters, int pcnt, userrec *user)
{
	userrec *dest;
	chanrec *chan;

	user->idle_lastmsg = TIME;
	
	if (loop_call(handle_privmsg,parameters,pcnt,user,0,pcnt-2,0))
		return;
        if (parameters[0][0] == '$')
	{
		// notice to server mask
		char* servermask = parameters[0];
		servermask++;
		if (match(ServerName,servermask))
                {
			ServerPrivmsgAll("%s",parameters[1]);
                }
		return;
        }
        else if (parameters[0][0] == '#')
	{
		chan = FindChan(parameters[0]);
		if (chan)
		{
			if ((chan->binarymodes & CM_NOEXTERNAL) && (!has_channel(user,chan)))
			{
				WriteServ(user->fd,"404 %s %s :Cannot send to channel (no external messages)", user->nick, chan->name);
				return;
			}
			if ((chan->binarymodes & CM_MODERATED) && (cstatus(user,chan)<STATUS_VOICE))
			{
				WriteServ(user->fd,"404 %s %s :Cannot send to channel (+m)", user->nick, chan->name);
				return;
			}
			
			int MOD_RESULT = 0;

			std::string temp = parameters[1];
			FOREACH_RESULT(OnUserPreMessage(user,chan,TYPE_CHANNEL,temp));
			if (MOD_RESULT) {
				return;
			}
			parameters[1] = (char*)temp.c_str();

			if (temp == "")
			{
				WriteServ(user->fd,"412 %s No text to send", user->nick);
				return;
			}
			
			ChanExceptSender(chan, user, "PRIVMSG %s :%s", chan->name, parameters[1]);
			FOREACH_MOD OnUserMessage(user,chan,TYPE_CHANNEL,parameters[1]);
		}
		else
		{
			/* no such nick/channel */
			WriteServ(user->fd,"401 %s %s :No such nick/channel",user->nick, parameters[0]);
		}
		return;
	}
	
	log(DEBUG,"*** PRIVMSG HANDLER");
	dest = Find(parameters[0]);
	if (dest)
	{
		log(DEBUG,"*** FOUND NICK %s",dest->nick);
		if (strcmp(dest->awaymsg,""))
		{
			/* auto respond with aweh msg */
			WriteServ(user->fd,"301 %s %s :%s",user->nick,dest->nick,dest->awaymsg);
		}

		int MOD_RESULT = 0;
		
		std::string temp = parameters[1];
		FOREACH_RESULT(OnUserPreMessage(user,dest,TYPE_USER,temp));
		if (MOD_RESULT) {
			return;
		}
		parameters[1] = (char*)temp.c_str();

		if (!strcmp(dest->server,ServerName))
		{
			// direct write, same server
			log(DEBUG,"*** CALL WRITETO");
			WriteTo(user, dest, "PRIVMSG %s :%s", dest->nick, parameters[1]);
		}

		FOREACH_MOD OnUserMessage(user,dest,TYPE_USER,parameters[1]);
	}
	else
	{
		/* no such nick/channel */
		WriteServ(user->fd,"401 %s %s :No such nick/channel",user->nick, parameters[0]);
	}
}

void handle_notice(char **parameters, int pcnt, userrec *user)
{
	userrec *dest;
	chanrec *chan;

	user->idle_lastmsg = TIME;
	
	if (loop_call(handle_notice,parameters,pcnt,user,0,pcnt-2,0))
		return;
	if (parameters[0][0] == '$')
	{
		// notice to server mask
		char* servermask = parameters[0];
		servermask++;
		if (match(ServerName,servermask))
		{
			NoticeAll(user, true, "%s",parameters[1]);
		}
		return;
	}
	else if (parameters[0][0] == '#')
	{
		chan = FindChan(parameters[0]);
		if (chan)
		{
			if ((chan->binarymodes & CM_NOEXTERNAL) && (!has_channel(user,chan)))
			{
				WriteServ(user->fd,"404 %s %s :Cannot send to channel (no external messages)", user->nick, chan->name);
				return;
			}
			if ((chan->binarymodes & CM_MODERATED) && (cstatus(user,chan)<STATUS_VOICE))
			{
				WriteServ(user->fd,"404 %s %s :Cannot send to channel (+m)", user->nick, chan->name);
				return;
			}

			int MOD_RESULT = 0;

			std::string temp = parameters[1];
			FOREACH_RESULT(OnUserPreNotice(user,chan,TYPE_CHANNEL,temp));
			if (MOD_RESULT) {
				return;
			}
			parameters[1] = (char*)temp.c_str();

                        if (temp == "")
                        {
                                WriteServ(user->fd,"412 %s No text to send", user->nick);
                                return;
                        }

			ChanExceptSender(chan, user, "NOTICE %s :%s", chan->name, parameters[1]);

			FOREACH_MOD OnUserNotice(user,chan,TYPE_CHANNEL,parameters[1]);
		}
		else
		{
			/* no such nick/channel */
			WriteServ(user->fd,"401 %s %s :No such nick/channel",user->nick, parameters[0]);
		}
		return;
	}
	
	dest = Find(parameters[0]);
	if (dest)
	{
		int MOD_RESULT = 0;
		
		std::string temp = parameters[1];
		FOREACH_RESULT(OnUserPreNotice(user,dest,TYPE_USER,temp));
		if (MOD_RESULT) {
			return;
		}
		parameters[1] = (char*)temp.c_str();

		if (!strcmp(dest->server,ServerName))
		{
			// direct write, same server
			WriteTo(user, dest, "NOTICE %s :%s", dest->nick, parameters[1]);
		}

		FOREACH_MOD OnUserNotice(user,dest,TYPE_USER,parameters[1]);
	}
	else
	{
		/* no such nick/channel */
		WriteServ(user->fd,"401 %s %s :No such nick/channel",user->nick, parameters[0]);
	}
}

void handle_server(char **parameters, int pcnt, userrec *user)
{
	WriteServ(user->fd,"666 %s :You cannot identify as a server, you are a USER. IRC Operators informed.",user->nick);
	WriteOpers("*** WARNING: %s attempted to issue a SERVER command and is registered as a user!",user->nick);
}

void handle_info(char **parameters, int pcnt, userrec *user)
{
	WriteServ(user->fd,"371 %s :. o O (The Inspire Internet Relay Chat Server) O o .",user->nick);
        WriteServ(user->fd,"371 %s : ",user->nick);
        WriteServ(user->fd,"371 %s :Core developers: Craig Edwards (Brain)",user->nick);
        WriteServ(user->fd,"371 %s :                 Craig McLure",user->nick);
        WriteServ(user->fd,"371 %s : ",user->nick);
        WriteServ(user->fd,"371 %s :Contributors:    typobox43",user->nick);
        WriteServ(user->fd,"371 %s :                 w00t",user->nick);
        WriteServ(user->fd,"371 %s :                 Om",user->nick);
        WriteServ(user->fd,"371 %s :                 Jazza",user->nick);
        WriteServ(user->fd,"371 %s : ",user->nick);
        WriteServ(user->fd,"371 %s :Testers:         CC",user->nick);
	WriteServ(user->fd,"371 %s :                 Om",user->nick);
        WriteServ(user->fd,"371 %s :                 Piggles",user->nick);
        WriteServ(user->fd,"371 %s :                 Foamy",user->nick);
        WriteServ(user->fd,"371 %s :                 Hart",user->nick);
        WriteServ(user->fd,"371 %s :                 RageD",user->nick);
        WriteServ(user->fd,"371 %s :                 [ed]",user->nick);
        WriteServ(user->fd,"371 %s :                 Azhrarn",user->nick);
        WriteServ(user->fd,"371 %s :                 nenolod",user->nick);
        WriteServ(user->fd,"371 %s :                 luigiman",user->nick);
        WriteServ(user->fd,"371 %s :                 Chu",user->nick);
        WriteServ(user->fd,"371 %s :                 aquanight",user->nick);
        WriteServ(user->fd,"371 %s :                 xptek",user->nick);
        WriteServ(user->fd,"371 %s :                 Grantlinks",user->nick);
        WriteServ(user->fd,"371 %s :                 Rob",user->nick);
        WriteServ(user->fd,"371 %s :                 angelic",user->nick);
        WriteServ(user->fd,"371 %s :                 Jason",user->nick);
	WriteServ(user->fd,"371 %s :                 ThaPrince",user->nick);
        WriteServ(user->fd,"371 %s : ",user->nick);
        WriteServ(user->fd,"371 %s :Thanks to irc-junkie and searchirc",user->nick);
        WriteServ(user->fd,"371 %s :for the nice comments and the help",user->nick);
        WriteServ(user->fd,"371 %s :you gave us in attracting users to",user->nick);
        WriteServ(user->fd,"371 %s :this software.",user->nick);
        WriteServ(user->fd,"371 %s : ",user->nick);
        WriteServ(user->fd,"371 %s :Best experienced with: An IRC client.",user->nick);
	FOREACH_MOD OnInfo(user);
	WriteServ(user->fd,"374 %s :End of /INFO list",user->nick);
}

void handle_time(char **parameters, int pcnt, userrec *user)
{
	time_t rawtime;
	struct tm * timeinfo;

	time ( &rawtime );
	timeinfo = localtime ( &rawtime );
	WriteServ(user->fd,"391 %s %s :%s",user->nick,ServerName, asctime (timeinfo) );
  
}

void handle_whois(char **parameters, int pcnt, userrec *user)
{
	userrec *dest;
        if (loop_call(handle_whois,parameters,pcnt,user,0,pcnt-1,0))
                return;
	dest = Find(parameters[0]);
	if (dest)
	{
		do_whois(user,dest,0,0,parameters[0]);
	}
        else
        {
                /* no such nick/channel */
                WriteServ(user->fd,"401 %s %s :No such nick/channel",user->nick, parameters[0]);
                WriteServ(user->fd,"318 %s %s :End of /WHOIS list.",user->nick, parameters[0]);
	}
}

void do_whois(userrec* user, userrec* dest,unsigned long signon, unsigned long idle, char* nick)
{
	// bug found by phidjit - were able to whois an incomplete connection if it had sent a NICK or USER
	if (dest->registered == 7)
	{
		WriteServ(user->fd,"311 %s %s %s %s * :%s",user->nick, dest->nick, dest->ident, dest->dhost, dest->fullname);
		if ((user == dest) || (strchr(user->modes,'o')))
		{
			WriteServ(user->fd,"378 %s %s :is connecting from *@%s %s",user->nick, dest->nick, dest->host, dest->ip);
		}
		char* cl = chlist(dest,user);
		if (*cl)
		{
			WriteServ(user->fd,"319 %s %s :%s",user->nick, dest->nick, cl);
		}
		WriteServ(user->fd,"312 %s %s %s :%s",user->nick, dest->nick, dest->server, GetServerDescription(dest->server).c_str());
		if (*dest->awaymsg)
		{
			WriteServ(user->fd,"301 %s %s :%s",user->nick, dest->nick, dest->awaymsg);
		}
		if (strchr(dest->modes,'o'))
		{
			if (*dest->oper)
			{
				WriteServ(user->fd,"313 %s %s :is %s %s on %s",user->nick, dest->nick, (strchr("aeiou",dest->oper[0]) ? "an" : "a"),dest->oper, Network);
			}
			else
			{
				WriteServ(user->fd,"313 %s %s :is opered but has an unknown type",user->nick, dest->nick);
			}
		}
		if ((!signon) && (!idle))
		{
			FOREACH_MOD OnWhois(user,dest);
		}
		if (!strcasecmp(user->server,dest->server))
		{
			// idle time and signon line can only be sent if youre on the same server (according to RFC)
			WriteServ(user->fd,"317 %s %s %d %d :seconds idle, signon time",user->nick, dest->nick, abs((dest->idle_lastmsg)-TIME), dest->signon);
		}
		else
		{
			if ((idle) || (signon))
				WriteServ(user->fd,"317 %s %s %d %d :seconds idle, signon time",user->nick, dest->nick, idle, signon);
		}
		WriteServ(user->fd,"318 %s %s :End of /WHOIS list.",user->nick, dest->nick);
	}
	else
	{
		WriteServ(user->fd,"401 %s %s :No such nick/channel",user->nick, nick);
		WriteServ(user->fd,"318 %s %s :End of /WHOIS list.",user->nick, nick);
	}
}

void handle_quit(char **parameters, int pcnt, userrec *user)
{
	user_hash::iterator iter = clientlist.find(user->nick);
	char* reason;

	if (user->registered == 7)
	{
		/* theres more to do here, but for now just close the socket */
		if (pcnt == 1)
		{
			if (parameters[0][0] == ':')
			{
				*parameters[0]++;
			}
			reason = parameters[0];

			if (strlen(reason)>MAXQUIT)
			{
				reason[MAXQUIT-1] = '\0';
			}

			/* We should only prefix the quit for a local user. Remote users have
			 * already been prefixed, where neccessary, by the upstream server.
			 */
			if (!strcasecmp(user->server,ServerName))
			{
				Write(user->fd,"ERROR :Closing link (%s@%s) [%s%s]",user->ident,user->host,PrefixQuit,parameters[0]);
				WriteOpers("*** Client exiting: %s!%s@%s [%s%s]",user->nick,user->ident,user->host,PrefixQuit,parameters[0]);
				WriteCommonExcept(user,"QUIT :%s%s",PrefixQuit,parameters[0]);
			}
			else
			{
				WriteOpers("*** Client exiting at %s: %s!%s@%s [%s]",user->server,user->nick,user->ident,user->host,parameters[0]);
				WriteCommonExcept(user,"QUIT :%s",parameters[0]);
			}
			FOREACH_MOD OnUserQuit(user,std::string(PrefixQuit)+std::string(parameters[0]));

		}
		else
		{
			Write(user->fd,"ERROR :Closing link (%s@%s) [QUIT]",user->ident,user->host);
			WriteOpers("*** Client exiting: %s!%s@%s [Client exited]",user->nick,user->ident,user->host);
			WriteCommonExcept(user,"QUIT :Client exited");
			FOREACH_MOD OnUserQuit(user,"Client exited");

		}
		AddWhoWas(user);
	}

	FOREACH_MOD OnUserDisconnect(user);

	/* push the socket on a stack of sockets due to be closed at the next opportunity */
	if (user->fd > -1)
	{
		SE->DelFd(user->fd);
		user->CloseSocket();
	}
	
	if (iter != clientlist.end())
	{
		clientlist.erase(iter);
	}

	if (user->registered == 7) {
		purge_empty_chans(user);
	}
        if (user->fd > -1)
                fd_ref_table[user->fd] = NULL;
	delete user;
}

void handle_who(char **parameters, int pcnt, userrec *user)
{
	chanrec* Ptr = NULL;
	char tmp[10];
	
	/* theres more to do here, but for now just close the socket */
	if (pcnt == 1)
	{
		if ((!strcmp(parameters[0],"0")) || (!strcmp(parameters[0],"*")))
		{
			if (user->chans[0].channel)
			{
				int n_list = 0;
			  	for (user_hash::const_iterator i = clientlist.begin(); i != clientlist.end(); i++)
				{
					Ptr = i->second->chans[0].channel;
					// suggested by phidjit and FCS
					if ((!common_channels(user,i->second)) && (isnick(i->second->nick)))
					{
						// Bug Fix #29
						strcpy(tmp, "");
						if (strcmp(i->second->awaymsg, "")) {
							strlcat(tmp, "G", 9);
						} else {
							strlcat(tmp, "H", 9);
						}
						if (strchr(i->second->modes,'o')) { strlcat(tmp, "*", 9); }
						WriteServ(user->fd,"352 %s %s %s %s %s %s %s :0 %s",user->nick, Ptr ? Ptr->name : "*", i->second->ident, i->second->dhost, i->second->server, i->second->nick, tmp, i->second->fullname);
						n_list++;
						if (n_list > MaxWhoResults)
						{
							WriteServ(user->fd,"523 %s WHO :Command aborted: More results than configured limit",user->nick);
							break;
						}
					}
				}
			}
			if (Ptr)
			{
				WriteServ(user->fd,"315 %s %s :End of /WHO list.",user->nick , parameters[0]);
			}
			else
			{
				WriteServ(user->fd,"315 %s %s :End of /WHO list.",user->nick, parameters[0]);
			}
			return;
		}
		if (parameters[0][0] == '#')
		{
			Ptr = FindChan(parameters[0]);
			if (Ptr)
			{
				int n_list = 0;
			  	for (user_hash::const_iterator i = clientlist.begin(); i != clientlist.end(); i++)
				{
					if ((has_channel(i->second,Ptr)) && (isnick(i->second->nick)))
					{
						// Fix Bug #29 - Part 2..
						strcpy(tmp, "");
						if (strcmp(i->second->awaymsg, "")) {
							strlcat(tmp, "G", 9);
						} else {
							strlcat(tmp, "H", 9);
						}
						if (strchr(i->second->modes,'o')) { strlcat(tmp, "*", 9); }
						strlcat(tmp, cmode(i->second, Ptr),5);
						WriteServ(user->fd,"352 %s %s %s %s %s %s %s :0 %s",user->nick, Ptr->name, i->second->ident, i->second->dhost, i->second->server, i->second->nick, tmp, i->second->fullname);
                                                n_list++;
                                                if (n_list > MaxWhoResults)
                                                {
                                                        WriteServ(user->fd,"523 %s WHO :Command aborted: More results than configured limit",user->nick);
                                                        break;
                                                }

					}
				}
				WriteServ(user->fd,"315 %s %s :End of /WHO list.",user->nick, parameters[0]);
			}
			else
			{
				WriteServ(user->fd,"401 %s %s :No such nick/channel",user->nick, parameters[0]);
			}
		}
		else
		{
			userrec* u = Find(parameters[0]);
			if (u)
			{
				// Bug Fix #29 -- Part 29..
				strcpy(tmp, "");
				if (strcmp(u->awaymsg, "")) {
					strlcat(tmp, "G" ,9);
				} else {
					strlcat(tmp, "H" ,9);
				}
				if (strchr(u->modes,'o')) { strlcat(tmp, "*" ,9); }
				WriteServ(user->fd,"352 %s %s %s %s %s %s %s :0 %s",user->nick, u->chans[0].channel ? u->chans[0].channel->name
                                : "*", u->ident, u->dhost, u->server, u->nick, tmp, u->fullname);
			}
			WriteServ(user->fd,"315 %s %s :End of /WHO list.",user->nick, parameters[0]);
		}
	}
	if (pcnt == 2)
	{
                if ((!strcmp(parameters[0],"0")) || (!strcmp(parameters[0],"*")) && (!strcmp(parameters[1],"o")))
                {
		  	for (std::vector<userrec*>::iterator i = all_opers.begin(); i != all_opers.end(); i++)
                        {
				// If i were a rich man.. I wouldn't need to me making these bugfixes..
				// But i'm a poor bastard with nothing better to do.
				userrec* oper = *i;
				strcpy(tmp, "");
				if (strcmp(oper->awaymsg, "")) {
					strlcat(tmp, "G" ,9);
				} else {
					strlcat(tmp, "H" ,9);
				}
                                WriteServ(user->fd,"352 %s %s %s %s %s %s %s* :0 %s", user->nick, oper->chans[0].channel ? oper->chans[0].channel->name 
				: "*", oper->ident, oper->dhost, oper->server, oper->nick, tmp, oper->fullname);
                        }
                        WriteServ(user->fd,"315 %s %s :End of /WHO list.",user->nick, parameters[0]);
                        return;
                }
	}
}

void handle_wallops(char **parameters, int pcnt, userrec *user)
{
	WriteWallOps(user,false,"%s",parameters[0]);
	FOREACH_MOD OnWallops(user,parameters[0]);
}

void handle_list(char **parameters, int pcnt, userrec *user)
{
	WriteServ(user->fd,"321 %s Channel :Users Name",user->nick);
	for (chan_hash::const_iterator i = chanlist.begin(); i != chanlist.end(); i++)
	{
		// if the channel is not private/secret, OR the user is on the channel anyway
		if (((!(i->second->binarymodes & CM_PRIVATE)) && (!(i->second->binarymodes & CM_SECRET))) || (has_channel(user,i->second)))
		{
			WriteServ(user->fd,"322 %s %s %d :[+%s] %s",user->nick,i->second->name,usercount_i(i->second),chanmodes(i->second),i->second->topic);
		}
	}
	WriteServ(user->fd,"323 %s :End of channel list.",user->nick);
}


void handle_rehash(char **parameters, int pcnt, userrec *user)
{
	WriteServ(user->fd,"382 %s %s :Rehashing",user->nick,CleanFilename(CONFIG_FILE));
	std::string parameter = "";
	if (pcnt)
	{
		parameter = parameters[0];
	}
	else
	{
		WriteOpers("%s is rehashing config file %s",user->nick,CleanFilename(CONFIG_FILE));
		ReadConfig(false,user);
	}
	FOREACH_MOD OnRehash(parameter);
}

void handle_lusers(char **parameters, int pcnt, userrec *user)
{
	// this lusers command shows one server at all times because
	// a protocol module must override it to show those stats.
	WriteServ(user->fd,"251 %s :There are %d users and %d invisible on 1 server",user->nick,usercnt()-usercount_invisible(),usercount_invisible());
	WriteServ(user->fd,"252 %s %d :operator(s) online",user->nick,usercount_opers());
	WriteServ(user->fd,"253 %s %d :unknown connections",user->nick,usercount_unknown());
	WriteServ(user->fd,"254 %s %d :channels formed",user->nick,chancount());
	WriteServ(user->fd,"254 %s :I have %d clients and 0 servers",user->nick,local_count());
}

void handle_admin(char **parameters, int pcnt, userrec *user)
{
	WriteServ(user->fd,"256 %s :Administrative info for %s",user->nick,ServerName);
	WriteServ(user->fd,"257 %s :Name     - %s",user->nick,AdminName);
	WriteServ(user->fd,"258 %s :Nickname - %s",user->nick,AdminNick);
	WriteServ(user->fd,"258 %s :E-Mail   - %s",user->nick,AdminEmail);
}

void handle_ping(char **parameters, int pcnt, userrec *user)
{
	WriteServ(user->fd,"PONG %s :%s",ServerName,parameters[0]);
}

void handle_pong(char **parameters, int pcnt, userrec *user)
{
	// set the user as alive so they survive to next ping
	user->lastping = 1;
}

void handle_motd(char **parameters, int pcnt, userrec *user)
{
	ShowMOTD(user);
}

void handle_rules(char **parameters, int pcnt, userrec *user)
{
	ShowRULES(user);
}

void handle_user(char **parameters, int pcnt, userrec *user)
{
	if (user->registered < 3)
	{
		if (isident(parameters[0]) == 0) {
			// This kinda Sucks, According to the RFC thou, its either this,
			// or "You have already registered" :p -- Craig
			WriteServ(user->fd,"461 %s USER :Not enough parameters",user->nick);
		}
		else {
			/* We're not checking ident, but I'm not sure I like the idea of '~' prefixing.. */
			/* XXX - Should this IDENTMAX + 1 be IDENTMAX - 1? Ok, users.h has it defined as
			 * char ident[IDENTMAX+2]; - WTF?
			 */
			snprintf(user->ident, IDENTMAX+1, "~%s", parameters[0]);
			strlcpy(user->fullname,parameters[3],MAXGECOS);
			user->registered = (user->registered | 1);
		}
	}
	else
	{
		WriteServ(user->fd,"462 %s :You may not reregister",user->nick);
		return;
	}
	/* parameters 2 and 3 are local and remote hosts, ignored when sent by client connection */
	if (user->registered == 3)
	{
		/* user is registered now, bit 0 = USER command, bit 1 = sent a NICK command */
		FOREACH_MOD OnUserRegister(user);
		ConnectUser(user);
	}
}

void handle_userhost(char **parameters, int pcnt, userrec *user)
{
	char Return[MAXBUF],junk[MAXBUF];
	snprintf(Return,MAXBUF,"302 %s :",user->nick);
	for (int i = 0; i < pcnt; i++)
	{
		userrec *u = Find(parameters[i]);
		if (u)
		{
			if (strchr(u->modes,'o'))
			{
				snprintf(junk,MAXBUF,"%s*=+%s@%s ",u->nick,u->ident,u->host);
				strlcat(Return,junk,MAXBUF);
			}
			else
			{
				snprintf(junk,MAXBUF,"%s=+%s@%s ",u->nick,u->ident,u->host);
				strlcat(Return,junk,MAXBUF);
			}
		}
	}
	WriteServ(user->fd,Return);
}


void handle_ison(char **parameters, int pcnt, userrec *user)
{
	char Return[MAXBUF];
	snprintf(Return,MAXBUF,"303 %s :",user->nick);
	for (int i = 0; i < pcnt; i++)
	{
		userrec *u = Find(parameters[i]);
		if (u)
		{
			strlcat(Return,u->nick,MAXBUF);
			strlcat(Return," ",MAXBUF);
		}
	}
	WriteServ(user->fd,Return);
}


void handle_away(char **parameters, int pcnt, userrec *user)
{
	if (pcnt)
	{
		strlcpy(user->awaymsg,parameters[0],MAXAWAY);
		WriteServ(user->fd,"306 %s :You have been marked as being away",user->nick);
	}
	else
	{
		strlcpy(user->awaymsg,"",MAXAWAY);
		WriteServ(user->fd,"305 %s :You are no longer marked as being away",user->nick);
	}
}

void handle_whowas(char **parameters, int pcnt, userrec* user)
{
	whowas_hash::iterator i = whowas.find(parameters[0]);

	if (i == whowas.end())
	{
		WriteServ(user->fd,"406 %s %s :There was no such nickname",user->nick,parameters[0]);
		WriteServ(user->fd,"369 %s %s :End of WHOWAS",user->nick,parameters[0]);
	}
	else
	{
		time_t rawtime = i->second->signon;
		tm *timeinfo;
		char b[MAXBUF];
		
		timeinfo = localtime(&rawtime);
		strlcpy(b,asctime(timeinfo),MAXBUF);
		b[strlen(b)-1] = '\0';
		
		WriteServ(user->fd,"314 %s %s %s %s * :%s",user->nick,i->second->nick,i->second->ident,i->second->dhost,i->second->fullname);
		WriteServ(user->fd,"312 %s %s %s :%s",user->nick,i->second->nick,i->second->server,b);
		WriteServ(user->fd,"369 %s %s :End of WHOWAS",user->nick,parameters[0]);
	}

}

void handle_trace(char **parameters, int pcnt, userrec *user)
{
	for (user_hash::iterator i = clientlist.begin(); i != clientlist.end(); i++)
	{
		if (i->second)
		{
			if (isnick(i->second->nick))
			{
				if (strchr(i->second->modes,'o'))
				{
					WriteServ(user->fd,"205 %s :Oper 0 %s",user->nick,i->second->nick);
				}
				else
				{
					WriteServ(user->fd,"204 %s :User 0 %s",user->nick,i->second->nick);
				}
			}
			else
			{
				WriteServ(user->fd,"203 %s :???? 0 [%s]",user->nick,i->second->host);
			}
		}
	}
}

void handle_modules(char **parameters, int pcnt, userrec *user)
{
  	for (unsigned int i = 0; i < module_names.size(); i++)
	{
		Version V = modules[i]->GetVersion();
		char modulename[MAXBUF];
		char flagstate[MAXBUF];
		strcpy(flagstate,"");
		if (V.Flags & VF_STATIC)
			strlcat(flagstate,", static",MAXBUF);
		if (V.Flags & VF_VENDOR)
			strlcat(flagstate,", vendor",MAXBUF);
		if (V.Flags & VF_COMMON)
			strlcat(flagstate,", common",MAXBUF);
		if (V.Flags & VF_SERVICEPROVIDER)
			strlcat(flagstate,", service provider",MAXBUF);
		if (!flagstate[0])
			strcpy(flagstate,"  <no flags>");
		strlcpy(modulename,module_names[i].c_str(),256);
		if (strchr(user->modes,'o'))
		{
			WriteServ(user->fd,"900 %s :0x%08lx %d.%d.%d.%d %s (%s)",user->nick,modules[i],V.Major,V.Minor,V.Revision,V.Build,CleanFilename(modulename),flagstate+2);
		}
		else
		{
			WriteServ(user->fd,"900 %s :%s",user->nick,CleanFilename(modulename));
		}
	}
	WriteServ(user->fd,"901 %s :End of MODULES list",user->nick);
}

void handle_stats(char **parameters, int pcnt, userrec *user)
{
	if (pcnt != 1)
	{
		return;
	}
	if (strlen(parameters[0])>1)
	{
		/* make the stats query 1 character long */
		parameters[0][1] = '\0';
	}


	FOREACH_MOD OnStats(*parameters[0]);

	if (*parameters[0] == 'c')
	{
		/* This stats symbol must be handled by a linking module */
	}
	
	if (*parameters[0] == 'i')
	{
		int idx = 0;
		for (ClassVector::iterator i = Classes.begin(); i != Classes.end(); i++)
		{
			WriteServ(user->fd,"215 %s I * * * %d %d %s *",user->nick,MAXCLIENTS,idx,ServerName);
			idx++;
		}
	}
	
	if (*parameters[0] == 'y')
	{
		int idx = 0;
		for (ClassVector::iterator i = Classes.begin(); i != Classes.end(); i++)
		{
			WriteServ(user->fd,"218 %s Y %d %d 0 %d %d",user->nick,idx,120,i->flood,i->registration_timeout);
			idx++;
		}
	}

	if (*parameters[0] == 'U')
	{
		char ulined[MAXBUF];
		for (int i = 0; i < ConfValueEnum("uline",&config_f); i++)
		{
			ConfValue("uline","server",i,ulined,&config_f);
			WriteServ(user->fd,"248 %s U %s",user->nick,ulined);
		}
	}
	
	if (*parameters[0] == 'P')
	{
		int idx = 0;
	  	for (user_hash::iterator i = clientlist.begin(); i != clientlist.end(); i++)
		{
			if (strchr(i->second->modes,'o'))
			{
				WriteServ(user->fd,"249 %s :%s (%s@%s) Idle: %d",user->nick,i->second->nick,i->second->ident,i->second->dhost,(TIME - i->second->idle_lastmsg));
				idx++;
			}
		}
		WriteServ(user->fd,"249 %s :%d OPER(s)",user->nick,idx);
	}
 	
	if (*parameters[0] == 'k')
	{
		stats_k(user);
	}

	if (*parameters[0] == 'g')
	{
		stats_g(user);
	}

	if (*parameters[0] == 'q')
	{
		stats_q(user);
	}

	if (*parameters[0] == 'Z')
	{
		stats_z(user);
	}

	if (*parameters[0] == 'e')
	{
		stats_e(user);
	}

	/* stats m (list number of times each command has been used, plus bytecount) */
	if (*parameters[0] == 'm')
	{
		for (unsigned int i = 0; i < cmdlist.size(); i++)
		{
			if (cmdlist[i].handler_function)
			{
				if (cmdlist[i].use_count)
				{
					/* RPL_STATSCOMMANDS */
					WriteServ(user->fd,"212 %s %s %d %d",user->nick,cmdlist[i].command,cmdlist[i].use_count,cmdlist[i].total_bytes);
				}
			}
		}
			
	}

	/* stats z (debug and memory info) */
	if (*parameters[0] == 'z')
	{
		rusage R;
		WriteServ(user->fd,"249 %s :Users(HASH_MAP) %d (%d bytes, %d buckets)",user->nick,clientlist.size(),clientlist.size()*sizeof(userrec),clientlist.bucket_count());
		WriteServ(user->fd,"249 %s :Channels(HASH_MAP) %d (%d bytes, %d buckets)",user->nick,chanlist.size(),chanlist.size()*sizeof(chanrec),chanlist.bucket_count());
		WriteServ(user->fd,"249 %s :Commands(VECTOR) %d (%d bytes)",user->nick,cmdlist.size(),cmdlist.size()*sizeof(command_t));
		WriteServ(user->fd,"249 %s :MOTD(VECTOR) %d, RULES(VECTOR) %d",user->nick,MOTD.size(),RULES.size());
		WriteServ(user->fd,"249 %s :address_cache(HASH_MAP) %d (%d buckets)",user->nick,IP.size(),IP.bucket_count());
		WriteServ(user->fd,"249 %s :Modules(VECTOR) %d (%d)",user->nick,modules.size(),modules.size()*sizeof(Module));
		WriteServ(user->fd,"249 %s :ClassFactories(VECTOR) %d (%d)",user->nick,factory.size(),factory.size()*sizeof(ircd_module));
		WriteServ(user->fd,"249 %s :Ports(STATIC_ARRAY) %d",user->nick,boundPortCount);
		if (!getrusage(RUSAGE_SELF,&R))
		{
			WriteServ(user->fd,"249 %s :Total allocation: %luK (0x%lx)",user->nick,R.ru_maxrss,R.ru_maxrss);
			WriteServ(user->fd,"249 %s :Signals:          %lu  (0x%lx)",user->nick,R.ru_nsignals,R.ru_nsignals);
			WriteServ(user->fd,"249 %s :Page faults:      %lu  (0x%lx)",user->nick,R.ru_majflt,R.ru_majflt);
			WriteServ(user->fd,"249 %s :Swaps:            %lu  (0x%lx)",user->nick,R.ru_nswap,R.ru_nswap);
			WriteServ(user->fd,"249 %s :Context Switches: %lu  (0x%lx)",user->nick,R.ru_nvcsw+R.ru_nivcsw,R.ru_nvcsw+R.ru_nivcsw);
		}
	}

	if (*parameters[0] == 'T')
	{
		WriteServ(user->fd,"249 Brain :accepts %d refused %d",statsAccept,statsRefused);
		WriteServ(user->fd,"249 Brain :unknown commands %d",statsUnknown);
		WriteServ(user->fd,"249 Brain :nick collisions %d",statsCollisions);
		WriteServ(user->fd,"249 Brain :dns requests %d succeeded %d failed %d",statsDns,statsDnsGood,statsDnsBad);
		WriteServ(user->fd,"249 Brain :connections %d",statsConnects);
		WriteServ(user->fd,"249 Brain :bytes sent %dK recv %dK",(statsSent / 1024),(statsRecv / 1024));
	}
	
	/* stats o */
	if (*parameters[0] == 'o')
	{
		for (int i = 0; i < ConfValueEnum("oper",&config_f); i++)
		{
			char LoginName[MAXBUF];
			char HostName[MAXBUF];
			char OperType[MAXBUF];
			ConfValue("oper","name",i,LoginName,&config_f);
			ConfValue("oper","host",i,HostName,&config_f);
			ConfValue("oper","type",i,OperType,&config_f);
			WriteServ(user->fd,"243 %s O %s * %s %s 0",user->nick,HostName,LoginName,OperType);
		}
	}
	
	/* stats l (show user I/O stats) */
	if (*parameters[0] == 'l')
	{
		WriteServ(user->fd,"211 %s :server:port nick bytes_in cmds_in bytes_out cmds_out",user->nick);
	  	for (user_hash::iterator i = clientlist.begin(); i != clientlist.end(); i++)
		{
			if (isnick(i->second->nick))
			{
				WriteServ(user->fd,"211 %s :%s:%d %s %d %d %d %d",user->nick,i->second->server,i->second->port,i->second->nick,i->second->bytes_in,i->second->cmds_in,i->second->bytes_out,i->second->cmds_out);
			}
			else
			{
				WriteServ(user->fd,"211 %s :%s:%d (unknown@%d) %d %d %d %d",user->nick,i->second->server,i->second->port,i->second->fd,i->second->bytes_in,i->second->cmds_in,i->second->bytes_out,i->second->cmds_out);
			}
			
		}
	}
	
	/* stats u (show server uptime) */
	if (*parameters[0] == 'u')
	{
		time_t current_time = 0;
		current_time = TIME;
		time_t server_uptime = current_time - startup_time;
		struct tm* stime;
		stime = gmtime(&server_uptime);
		/* i dont know who the hell would have an ircd running for over a year nonstop, but
		 * Craig suggested this, and it seemed a good idea so in it went */
		if (stime->tm_year > 70)
		{
			WriteServ(user->fd,"242 %s :Server up %d years, %d days, %.2d:%.2d:%.2d",user->nick,(stime->tm_year-70),stime->tm_yday,stime->tm_hour,stime->tm_min,stime->tm_sec);
		}
		else
		{
			WriteServ(user->fd,"242 %s :Server up %d days, %.2d:%.2d:%.2d",user->nick,stime->tm_yday,stime->tm_hour,stime->tm_min,stime->tm_sec);
		}
	}

	WriteServ(user->fd,"219 %s %s :End of /STATS report",user->nick,parameters[0]);
	WriteOpers("*** Notice: Stats '%s' requested by %s (%s@%s)",parameters[0],user->nick,user->ident,user->host);
	
}


void handle_connect(char **parameters, int pcnt, userrec *user)
{
}

void handle_squit(char **parameters, int pcnt, userrec *user)
{
}

void handle_links(char **parameters, int pcnt, userrec *user)
{
	WriteServ(user->fd,"364 %s %s %s :0 %s",user->nick,ServerName,ServerName,ServerDesc);
	WriteServ(user->fd,"365 %s * :End of /LINKS list.",user->nick);
}

void handle_map(char **parameters, int pcnt, userrec *user)
{
	// as with /LUSERS this does nothing without a linking
	// module to override its behaviour and display something
	// better.
	WriteServ(user->fd,"006 %s :%s",user->nick,ServerName);
	WriteServ(user->fd,"007 %s :End of /MAP",user->nick);
}

bool is_uline(const char* server)
{
	char ServName[MAXBUF];

	if (!server)
		return false;
	if (!(*server))
		return true;

	for (int i = 0; i < ConfValueEnum("uline",&config_f); i++)
	{
		ConfValue("uline","server",i,ServName,&config_f);
		if (!strcasecmp(server,ServName))
		{
			return true;
		}
	}
	return false;
}
int operstrcmp(char* data,char* input)
{
	int MOD_RESULT = 0;
	FOREACH_RESULT(OnOperCompare(data,input))
	log(DEBUG,"operstrcmp: %d",MOD_RESULT);
	if (MOD_RESULT == 1)
		return 0;
	if (MOD_RESULT == -1)
		return 1;
	log(DEBUG,"strcmp fallback: '%s' '%s' %d",data,input,strcmp(data,input));
	return strcmp(data,input);
}

void handle_oper(char **parameters, int pcnt, userrec *user)
{
	char LoginName[MAXBUF];
	char Password[MAXBUF];
	char OperType[MAXBUF];
	char TypeName[MAXBUF];
	char HostName[MAXBUF];
	char TheHost[MAXBUF];
	int j;
	bool found = false;
	bool fail2 = false;

	snprintf(TheHost,MAXBUF,"%s@%s",user->ident,user->host);

	for (int i = 0; i < ConfValueEnum("oper",&config_f); i++)
	{
		ConfValue("oper","name",i,LoginName,&config_f);
		ConfValue("oper","password",i,Password,&config_f);
		ConfValue("oper","type",i,OperType,&config_f);
		ConfValue("oper","host",i,HostName,&config_f);
		if ((!strcmp(LoginName,parameters[0])) && (!operstrcmp(Password,parameters[1])) && (match(TheHost,HostName)))
		{
			fail2 = true;
			for (j =0; j < ConfValueEnum("type",&config_f); j++)
			{
				ConfValue("type","name",j,TypeName,&config_f);

				if (!strcmp(TypeName,OperType))
				{
					/* found this oper's opertype */
					ConfValue("type","host",j,HostName,&config_f);
					if (*HostName)
						ChangeDisplayedHost(user,HostName);
					strlcpy(user->oper,TypeName,NICKMAX);
					found = true;
					fail2 = false;
					break;
				}
			}
		}
		if (found)
			break;
	}
	if (found)
	{
                /* correct oper credentials */
                WriteOpers("*** %s (%s@%s) is now an IRC operator of type %s",user->nick,user->ident,user->host,OperType);
                WriteServ(user->fd,"381 %s :You are now an IRC operator of type %s",user->nick,OperType);
		if (!strchr(user->modes,'o'))
		{
			strcat(user->modes,"o");
			WriteServ(user->fd,"MODE %s :+o",user->nick);
			FOREACH_MOD OnOper(user,OperType);
			log(DEFAULT,"OPER: %s!%s@%s opered as type: %s",user->nick,user->ident,user->host,OperType);
			AddOper(user);
		}
	}
	else
	{
		if (!fail2)
		{
			WriteServ(user->fd,"491 %s :Invalid oper credentials",user->nick);
			WriteOpers("*** WARNING! Failed oper attempt by %s!%s@%s!",user->nick,user->ident,user->host);
			log(DEFAULT,"OPER: Failed oper attempt by %s!%s@%s: user, host or password did not match.",user->nick,user->ident,user->host);
		}
		else
		{
			WriteServ(user->fd,"491 %s :Your oper block does not have a valid opertype associated with it",user->nick);
			WriteOpers("*** CONFIGURATION ERROR! Oper block mismatch for OperType %s",OperType);
                        log(DEFAULT,"OPER: Failed oper attempt by %s!%s@%s: credentials valid, but oper type nonexistent.",user->nick,user->ident,user->host);
		}
	}
	return;
}

void handle_nick(char **parameters, int pcnt, userrec *user)
{
	if (pcnt < 1) 
	{
		log(DEBUG,"not enough params for handle_nick");
		return;
	}
	if (!parameters[0])
	{
		log(DEBUG,"invalid parameter passed to handle_nick");
		return;
	}
	if (!parameters[0][0])
	{
		log(DEBUG,"zero length new nick passed to handle_nick");
		return;
	}
	if (!user)
	{
		log(DEBUG,"invalid user passed to handle_nick");
		return;
	}
	if (!user->nick)
	{
		log(DEBUG,"invalid old nick passed to handle_nick");
		return;
	}
	if (!strcasecmp(user->nick,parameters[0]))
	{
		log(DEBUG,"old nick is new nick, skipping");
		return;
	}
	else
	{
		if (strlen(parameters[0]) > 1)
		{
			if (parameters[0][0] == ':')
			{
				*parameters[0]++;
			}
		}
		if (matches_qline(parameters[0]))
		{
			WriteOpers("*** Q-Lined nickname %s from %s!%s@%s: %s",parameters[0],user->nick,user->ident,user->host,matches_qline(parameters[0]));
			WriteServ(user->fd,"432 %s %s :Invalid nickname: %s",user->nick,parameters[0],matches_qline(parameters[0]));
			return;
		}
		if ((Find(parameters[0])) && (Find(parameters[0]) != user))
		{
			WriteServ(user->fd,"433 %s %s :Nickname is already in use.",user->nick,parameters[0]);
			return;
		}
	}
	if (isnick(parameters[0]) == 0)
	{
		WriteServ(user->fd,"432 %s %s :Erroneous Nickname",user->nick,parameters[0]);
		return;
	}

	if (user->registered == 7)
	{
		int MOD_RESULT = 0;
		FOREACH_RESULT(OnUserPreNick(user,parameters[0]));
		if (MOD_RESULT) {
			// if a module returns true, the nick change is silently forbidden.
			return;
		}

		WriteCommon(user,"NICK %s",parameters[0]);
		
	}
	
	char oldnick[NICKMAX];
	strlcpy(oldnick,user->nick,NICKMAX);

	/* change the nick of the user in the users_hash */
	user = ReHashNick(user->nick, parameters[0]);
	/* actually change the nick within the record */
	if (!user) return;
	if (!user->nick) return;

	strlcpy(user->nick, parameters[0],NICKMAX);

	log(DEBUG,"new nick set: %s",user->nick);
	
	if (user->registered < 3)
	{
		user->registered = (user->registered | 2);
		// dont attempt to look up the dns until they pick a nick... because otherwise their pointer WILL change
		// and unless we're lucky we'll get a duff one later on.
		//user->dns_done = (!lookup_dns(user->nick));
		//if (user->dns_done)
		//	log(DEBUG,"Aborting dns lookup of %s because dns server experienced a failure.",user->nick);

#ifdef THREADED_DNS
		// initialize their dns lookup thread
		if (pthread_create(&user->dnsthread, NULL, dns_task, (void *)user) != 0)
		{
			log(DEBUG,"Failed to create DNS lookup thread for user %s",user->nick);
		}
#else
		user->dns_done = (!lookup_dns(user->nick));
		if (user->dns_done)
			log(DEBUG,"Aborting dns lookup of %s because dns server experienced a failure.",user->nick);
#endif
	
	}
	if (user->registered == 3)
	{
		/* user is registered now, bit 0 = USER command, bit 1 = sent a NICK command */
		FOREACH_MOD OnUserRegister(user);
		ConnectUser(user);
	}
	if (user->registered == 7)
	{
		FOREACH_MOD OnUserPostNick(user,oldnick);
	}
}

long duration(const char* str)
{
	char n_field[MAXBUF];
	long total = 0;
	const char* str_end = str + strlen(str);
	n_field[0] = 0;

	if ((!strchr(str,'s')) && (!strchr(str,'m')) && (!strchr(str,'h')) && (!strchr(str,'d')) && (!strchr(str,'w')) && (!strchr(str,'y')))
	{
		std::string n = str;
		n = n + "s";
		return duration(n.c_str());
	}
	
	for (char* i = (char*)str; i < str_end; i++)
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

bool host_matches_everyone(std::string mask, userrec* user)
{
	char insanemasks[MAXBUF];
	char buffer[MAXBUF];
	char itrigger[MAXBUF];
	ConfValue("insane","hostmasks",0,insanemasks,&config_f);
	ConfValue("insane","trigger",0,itrigger,&config_f);
	if (*itrigger == 0)
		strlcpy(itrigger,"95.5",MAXBUF);
	if ((*insanemasks == 'y') || (*insanemasks == 't') || (*insanemasks == '1'))
		return false;
	long matches = 0;
	for (user_hash::iterator u = clientlist.begin(); u != clientlist.end(); u++)
	{
		strlcpy(buffer,u->second->ident,MAXBUF);
		strlcat(buffer,"@",MAXBUF);
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

bool ip_matches_everyone(std::string ip, userrec* user)
{
	char insanemasks[MAXBUF];
	char itrigger[MAXBUF];
	ConfValue("insane","ipmasks",0,insanemasks,&config_f);
	ConfValue("insane","trigger",0,itrigger,&config_f);
	if (*itrigger == 0)
		strlcpy(itrigger,"95.5",MAXBUF);
	if ((*insanemasks == 'y') || (*insanemasks == 't') || (*insanemasks == '1'))
		return false;
	long matches = 0;
	for (user_hash::iterator u = clientlist.begin(); u != clientlist.end(); u++)
	{
		if (match(u->second->ip,ip.c_str()))
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

bool nick_matches_everyone(std::string nick, userrec* user)
{
	char insanemasks[MAXBUF];
	char itrigger[MAXBUF];
	ConfValue("insane","nickmasks",0,insanemasks,&config_f);
	ConfValue("insane","trigger",0,itrigger,&config_f);
	if (*itrigger == 0)
		strlcpy(itrigger,"95.5",MAXBUF);
	if ((*insanemasks == 'y') || (*insanemasks == 't') || (*insanemasks == '1'))
		return false;
	long matches = 0;
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

void handle_kline(char **parameters, int pcnt, userrec *user)
{
	if (pcnt >= 3)
	{
		if (host_matches_everyone(parameters[0],user))
			return;
		add_kline(duration(parameters[1]),user->nick,parameters[2],parameters[0]);
		FOREACH_MOD OnAddKLine(duration(parameters[1]), user, parameters[2], parameters[0]);
		if (!duration(parameters[1]))
		{
			WriteOpers("*** %s added permenant K-line for %s.",user->nick,parameters[0]);
		}
		else
		{
			WriteOpers("*** %s added timed K-line for %s, expires in %d seconds.",user->nick,parameters[0],duration(parameters[1]));
		}
	}
	else
	{
		if (del_kline(parameters[0]))
		{
			FOREACH_MOD OnDelKLine(user, parameters[0]);
			WriteOpers("*** %s Removed K-line on %s.",user->nick,parameters[0]);
		}
		else
		{
			WriteServ(user->fd,"NOTICE %s :*** K-Line %s not found in list, try /stats k.",user->nick,parameters[0]);
		}
	}
	apply_lines();
}

void handle_eline(char **parameters, int pcnt, userrec *user)
{
        if (pcnt >= 3)
        {
		if (host_matches_everyone(parameters[0],user))
			return;
                add_eline(duration(parameters[1]),user->nick,parameters[2],parameters[0]);
		FOREACH_MOD OnAddELine(duration(parameters[1]), user, parameters[2], parameters[0]);
                if (!duration(parameters[1]))
                {
                        WriteOpers("*** %s added permenant E-line for %s.",user->nick,parameters[0]);
                }
                else
                {
                        WriteOpers("*** %s added timed E-line for %s, expires in %d seconds.",user->nick,parameters[0],duration(parameters[1]));
                }
        }
        else
        {
                if (del_eline(parameters[0]))
                {
			FOREACH_MOD OnDelELine(user, parameters[0]);
                        WriteOpers("*** %s Removed E-line on %s.",user->nick,parameters[0]);
                }
                else
                {
                        WriteServ(user->fd,"NOTICE %s :*** E-Line %s not found in list, try /stats e.",user->nick,parameters[0]);
                }
        }
	// no need to apply the lines for an eline
}

void handle_gline(char **parameters, int pcnt, userrec *user)
{
	if (pcnt >= 3)
	{
		if (host_matches_everyone(parameters[0],user))
			return;
		add_gline(duration(parameters[1]),user->nick,parameters[2],parameters[0]);
		FOREACH_MOD OnAddGLine(duration(parameters[1]), user, parameters[2], parameters[0]);
		if (!duration(parameters[1]))
		{
			WriteOpers("*** %s added permenant G-line for %s.",user->nick,parameters[0]);
		}
		else
		{
			WriteOpers("*** %s added timed G-line for %s, expires in %d seconds.",user->nick,parameters[0],duration(parameters[1]));
		}
	}
	else
	{
		if (del_gline(parameters[0]))
		{
			FOREACH_MOD OnDelGLine(user, parameters[0]);
			WriteOpers("*** %s Removed G-line on %s.",user->nick,parameters[0]);
		}
		else
		{
			WriteServ(user->fd,"NOTICE %s :*** G-Line %s not found in list, try /stats g.",user->nick,parameters[0]);
		}
	}
	apply_lines();
}

void handle_zline(char **parameters, int pcnt, userrec *user)
{
	if (pcnt >= 3)
	{
		if (ip_matches_everyone(parameters[0],user))
			return;
		add_zline(duration(parameters[1]),user->nick,parameters[2],parameters[0]);
		FOREACH_MOD OnAddZLine(duration(parameters[1]), user, parameters[2], parameters[0]);
		if (!duration(parameters[1]))
		{
			WriteOpers("*** %s added permenant Z-line for %s.",user->nick,parameters[0]);
		}
		else
		{
			WriteOpers("*** %s added timed Z-line for %s, expires in %d seconds.",user->nick,parameters[0],duration(parameters[1]));
		}
	}
	else
	{
		if (del_zline(parameters[0]))
		{
			FOREACH_MOD OnDelZLine(user, parameters[0]);
			WriteOpers("*** %s Removed Z-line on %s.",user->nick,parameters[0]);
		}
		else
		{
			WriteServ(user->fd,"NOTICE %s :*** Z-Line %s not found in list, try /stats Z.",user->nick,parameters[0]);
		}
	}
	apply_lines();
}

void handle_qline(char **parameters, int pcnt, userrec *user)
{
	if (pcnt >= 3)
	{
		if (nick_matches_everyone(parameters[0],user))
			return;
		add_qline(duration(parameters[1]),user->nick,parameters[2],parameters[0]);
		FOREACH_MOD OnAddQLine(duration(parameters[1]), user, parameters[2], parameters[0]);
		if (!duration(parameters[1]))
		{
			WriteOpers("*** %s added permenant Q-line for %s.",user->nick,parameters[0]);
		}
		else
		{
			WriteOpers("*** %s added timed Q-line for %s, expires in %d seconds.",user->nick,parameters[0],duration(parameters[1]));
		}
	}
	else
	{
		if (del_qline(parameters[0]))
		{
			FOREACH_MOD OnDelQLine(user, parameters[0]);
			WriteOpers("*** %s Removed Q-line on %s.",user->nick,parameters[0]);
		}
		else
		{
			WriteServ(user->fd,"NOTICE %s :*** Q-Line %s not found in list, try /stats k.",user->nick,parameters[0]);
		}
	}
	apply_lines();
}


