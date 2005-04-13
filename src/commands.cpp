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

#include "inspircd.h"
#include "inspircd_io.h"
#include "inspircd_util.h"
#include "inspircd_config.h"
#include <unistd.h>
#include <fcntl.h>
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
#include <errno.h>
#include <deque>
#include <errno.h>
#include <unistd.h>
#include <sched.h>
#include "connection.h"
#include "users.h"
#include "servers.h"
#include "ctables.h"
#include "globals.h"
#include "modules.h"
#include "dynamic.h"
#include "wildcard.h"
#include "message.h"
#include "mode.h"
#include "xline.h"
#include "inspstring.h"
#include "dnsqueue.h"

#ifdef GCC3
#define nspace __gnu_cxx
#else
#define nspace std
#endif

using namespace std;

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

extern std::vector<int> fd_reap;
extern std::vector<std::string> module_names;

extern char MyExecutable[1024];
extern int boundPortCount;
extern int portCount;
extern int UDPportCount;
extern int ports[MAXSOCKS];
extern int defaultRoute;

extern std::vector<long> auth_cookies;
extern std::stringstream config_f;

extern serverrec* me[32];

extern FILE *log_file;

extern ClassVector Classes;

const long duration_m = 60;
const long duration_h = duration_m * 60;
const long duration_d = duration_h * 24;
const long duration_w = duration_d * 7;
const long duration_y = duration_w * 52;

namespace nspace
{
#ifdef GCC34
        template<> struct hash<in_addr>
#else
        template<> struct nspace::hash<in_addr>
#endif
        {
                size_t operator()(const struct in_addr &a) const
                {
                        size_t q;
                        memcpy(&q,&a,sizeof(size_t));
                        return q;
                }
        };
#ifdef GCC34
        template<> struct hash<string>
#else
        template<> struct nspace::hash<string>
#endif
        {
                size_t operator()(const string &s) const
                {
                        char a[MAXBUF];
                        static struct hash<const char *> strhash;
                        strlcpy(a,s.c_str(),MAXBUF);
                        strlower(a);
                        return strhash(a);
                }
        };
}


struct StrHashComp
{

	bool operator()(const string& s1, const string& s2) const
	{
		char a[MAXBUF],b[MAXBUF];
		strlcpy(a,s1.c_str(),MAXBUF);
		strlcpy(b,s2.c_str(),MAXBUF);
		return (strcasecmp(a,b) == 0);
	}

};

struct InAddr_HashComp
{

	bool operator()(const in_addr &s1, const in_addr &s2) const
	{
		size_t q;
		size_t p;
		
		memcpy(&q,&s1,sizeof(size_t));
		memcpy(&p,&s2,sizeof(size_t));
		
		return (q == p);
	}

};


typedef nspace::hash_map<std::string, userrec*, nspace::hash<string>, StrHashComp> user_hash;
typedef nspace::hash_map<std::string, chanrec*, nspace::hash<string>, StrHashComp> chan_hash;
typedef nspace::hash_map<in_addr,string*, nspace::hash<in_addr>, InAddr_HashComp> address_cache;
typedef std::deque<command_t> command_table;


extern user_hash clientlist;
extern chan_hash chanlist;
extern user_hash whowas;
extern command_table cmdlist;
extern file_cache MOTD;
extern file_cache RULES;
extern address_cache IP;


void handle_join(char **parameters, int pcnt, userrec *user)
{
	chanrec* Ptr;
	int i = 0;
	
	if (loop_call(handle_join,parameters,pcnt,user,0,0,1))
		return;
	if (parameters[0][0] == '#')
	{
		Ptr = add_channel(user,parameters[0],parameters[1],false);
	}
}


void handle_part(char **parameters, int pcnt, userrec *user)
{
	chanrec* Ptr;

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

void handle_kick(char **parameters, int pcnt, userrec *user)
{
	chanrec* Ptr = FindChan(parameters[0]);
	userrec* u   = Find(parameters[1]);

	if ((!u) || (!Ptr))
	{
		WriteServ(user->fd,"401 %s %s :No suck nick/channel",user->nick, parameters[0]);
		return;
	}
	
	if (!has_channel(user,Ptr))
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
	
	// this must be propogated so that channel membership is kept in step network-wide
	char buffer[MAXBUF];
	snprintf(buffer,MAXBUF,"k %s %s %s :%s",user->nick,u->nick,Ptr->name,reason);
	NetSendToAll(buffer);
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
	char restart[1024];
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
			int on = 0;
			struct linger linger = { 0 };
			setsockopt(i, SOL_SOCKET, SO_REUSEADDR, (const char*)&on, sizeof(on));
			linger.l_onoff = 1;
			linger.l_linger = 0;
			setsockopt(i, SOL_SOCKET, SO_LINGER, (const char*)&linger,sizeof(linger));
			Blocking(i);
    			close(i);
		}
		sleep(5);
		
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
			// K token must go to ALL servers!!!
			char buffer[MAXBUF];
			snprintf(buffer,MAXBUF,"K %s %s :%s",user->nick,u->nick,killreason);
			NetSendToAll(buffer);
			
			user_hash::iterator iter = clientlist.find(u->nick);
			if (iter != clientlist.end())
			{
				log(DEBUG,"deleting user hash value %d",iter->second);
				if ((iter->second) && (user->registered == 7)) {
					delete iter->second;
				}
				clientlist.erase(iter);
			}
			purge_empty_chans();
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
		WriteServ(user->fd,"401 %s %s :No suck nick/channel",user->nick, parameters[0]);
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
	if (!strcasecmp(parameters[0],Passwd(user)))
	{
		user->haspassed = true;
	}
}

void handle_invite(char **parameters, int pcnt, userrec *user)
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
			if (c->inviteonly)
			{
				WriteServ(user->fd,"401 %s %s :No such nick/channel",user->nick, parameters[0]);
			}
		}

		return;
	}

	if (c->inviteonly)
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
	
	// i token must go to ALL servers!!!
	char buffer[MAXBUF];
	snprintf(buffer,MAXBUF,"i %s %s %s",u->nick,user->nick,c->name);
	NetSendToAll(buffer);
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
				if (((Ptr) && (!has_channel(user,Ptr))) && (Ptr->secret))
				{
					WriteServ(user->fd,"442 %s %s :You're not on that channel!",user->nick, Ptr->name);
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
				WriteServ(user->fd,"401 %s %s :No suck nick/channel",user->nick, parameters[0]);
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
				if ((Ptr->topiclock) && (cstatus(user,Ptr)<STATUS_HOP))
				{
					WriteServ(user->fd,"482 %s %s :You must be at least a half-operator to change modes on this channel", user->nick, Ptr->name);
					return;
				}
				
				char topic[MAXBUF];
				strlcpy(topic,parameters[1],MAXBUF);
				if (strlen(topic)>MAXTOPIC)
				{
					topic[MAXTOPIC-1] = '\0';
				}
					
				strlcpy(Ptr->topic,topic,MAXBUF);
				strlcpy(Ptr->setby,user->nick,NICKMAX);
				Ptr->topicset = TIME;
				WriteChannel(Ptr,user,"TOPIC %s :%s",Ptr->name, Ptr->topic);

				// t token must go to ALL servers!!!
				char buffer[MAXBUF];
				snprintf(buffer,MAXBUF,"t %s %s :%s",user->nick,Ptr->name,topic);
				NetSendToAll(buffer);
			}
			else
			{
				WriteServ(user->fd,"401 %s %s :No suck nick/channel",user->nick, parameters[0]);
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
                if (((c) && (!has_channel(user,c))) && (c->secret))
                {
                      WriteServ(user->fd,"442 %s %s :You're not on that channel!",user->nick, c->name);
                      return;
                }
		userlist(user,c);
		WriteServ(user->fd,"366 %s %s :End of /NAMES list.", user->nick, c->name);
	}
	else
	{
		WriteServ(user->fd,"401 %s %s :No suck nick/channel",user->nick, parameters[0]);
	}
}

void handle_privmsg(char **parameters, int pcnt, userrec *user)
{
	userrec *dest;
	chanrec *chan;

	user->idle_lastmsg = TIME;
	
	if (loop_call(handle_privmsg,parameters,pcnt,user,0,pcnt-2,0))
		return;
	if (parameters[0][0] == '#')
	{
		chan = FindChan(parameters[0]);
		if (chan)
		{
			if ((chan->noexternal) && (!has_channel(user,chan)))
			{
				WriteServ(user->fd,"404 %s %s :Cannot send to channel (no external messages)", user->nick, chan->name);
				return;
			}
			if ((chan->moderated) && (cstatus(user,chan)<STATUS_VOICE))
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
			
			ChanExceptSender(chan, user, "PRIVMSG %s :%s", chan->name, parameters[1]);
			
			// if any users of this channel are on remote servers, broadcast the packet
			char buffer[MAXBUF];
			snprintf(buffer,MAXBUF,"P %s %s :%s",user->nick,chan->name,parameters[1]);
			NetSendToCommon(user,buffer);
		}
		else
		{
			/* no such nick/channel */
			WriteServ(user->fd,"401 %s %s :No suck nick/channel",user->nick, parameters[0]);
		}
		return;
	}
	
	dest = Find(parameters[0]);
	if (dest)
	{
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



		if (!strcmp(dest->server,user->server))
		{
			// direct write, same server
			WriteTo(user, dest, "PRIVMSG %s :%s", dest->nick, parameters[1]);
		}
		else
		{
			char buffer[MAXBUF];
			snprintf(buffer,MAXBUF,"P %s %s :%s",user->nick,dest->nick,parameters[1]);
			NetSendToOne(dest->server,buffer);
		}
	}
	else
	{
		/* no such nick/channel */
		WriteServ(user->fd,"401 %s %s :No suck nick/channel",user->nick, parameters[0]);
	}
}

void handle_notice(char **parameters, int pcnt, userrec *user)
{
	userrec *dest;
	chanrec *chan;

	user->idle_lastmsg = TIME;
	
	if (loop_call(handle_notice,parameters,pcnt,user,0,pcnt-2,0))
		return;
	if (parameters[0][0] == '#')
	{
		chan = FindChan(parameters[0]);
		if (chan)
		{
			if ((chan->noexternal) && (!has_channel(user,chan)))
			{
				WriteServ(user->fd,"404 %s %s :Cannot send to channel (no external messages)", user->nick, chan->name);
				return;
			}
			if ((chan->moderated) && (cstatus(user,chan)<STATUS_VOICE))
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

			ChanExceptSender(chan, user, "NOTICE %s :%s", chan->name, parameters[1]);

			// if any users of this channel are on remote servers, broadcast the packet
			char buffer[MAXBUF];
			snprintf(buffer,MAXBUF,"V %s %s :%s",user->nick,chan->name,parameters[1]);
			NetSendToCommon(user,buffer);
		}
		else
		{
			/* no such nick/channel */
			WriteServ(user->fd,"401 %s %s :No suck nick/channel",user->nick, parameters[0]);
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

		if (!strcmp(dest->server,user->server))
		{
			// direct write, same server
			WriteTo(user, dest, "NOTICE %s :%s", dest->nick, parameters[1]);
		}
		else
		{
			char buffer[MAXBUF];
			snprintf(buffer,MAXBUF,"V %s %s :%s",user->nick,dest->nick,parameters[1]);
			NetSendToOne(dest->server,buffer);
		}
	}
	else
	{
		/* no such nick/channel */
		WriteServ(user->fd,"401 %s %s :No suck nick/channel",user->nick, parameters[0]);
	}
}

void handle_server(char **parameters, int pcnt, userrec *user)
{
	WriteServ(user->fd,"666 %s :You cannot identify as a server, you are a USER. IRC Operators informed.",user->nick);
	WriteOpers("*** WARNING: %s attempted to issue a SERVER command and is registered as a user!",user->nick);
}

void handle_info(char **parameters, int pcnt, userrec *user)
{
	WriteServ(user->fd,"371 %s :The Inspire IRCd Project Has been brought to you by the following people..",user->nick);
	WriteServ(user->fd,"371 %s :Craig Edwards, Craig McLure, and Others..",user->nick);
	WriteServ(user->fd,"371 %s :Will finish this later when i can be arsed :p",user->nick);
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
	char *t;

	if (loop_call(handle_whois,parameters,pcnt,user,0,pcnt-1,0))
		return;
	dest = Find(parameters[0]);
	if (dest)
	{
		// bug found by phidjit - were able to whois an incomplete connection if it had sent a NICK or USER
		if (dest->registered == 7)
		{
			WriteServ(user->fd,"311 %s %s %s %s * :%s",user->nick, dest->nick, dest->ident, dest->dhost, dest->fullname);
			if ((user == dest) || (strchr(user->modes,'o')))
			{
				WriteServ(user->fd,"378 %s %s :is connecting from *@%s",user->nick, dest->nick, dest->host);
			}
			if (strcmp(chlist(dest),""))
			{
				WriteServ(user->fd,"319 %s %s :%s",user->nick, dest->nick, chlist(dest));
			}
			WriteServ(user->fd,"312 %s %s %s :%s",user->nick, dest->nick, dest->server, GetServerDescription(dest->server).c_str());
			if (strcmp(dest->awaymsg,""))
			{
				WriteServ(user->fd,"301 %s %s :%s",user->nick, dest->nick, dest->awaymsg);
			}
			if ((strchr(dest->modes,'o')) && (strcmp(dest->oper,"")))
			{
				WriteServ(user->fd,"313 %s %s :is %s %s on %s",user->nick, dest->nick,
    				(strchr("aeiou",dest->oper[0]) ? "an" : "a"),dest->oper, Network);
			}
			FOREACH_MOD OnWhois(user,dest);
			if (!strcasecmp(user->server,dest->server))
			{
				// idle time and signon line can only be sent if youre on the same server (according to RFC)
				WriteServ(user->fd,"317 %s %s %d %d :seconds idle, signon time",user->nick, dest->nick, abs((dest->idle_lastmsg)-TIME), dest->signon);
			}
			
			WriteServ(user->fd,"318 %s %s :End of /WHOIS list.",user->nick, dest->nick);
		}
		else
		{
			WriteServ(user->fd,"401 %s %s :No suck nick/channel",user->nick, parameters[0]);
		}
	}
	else
	{
		/* no such nick/channel */
		WriteServ(user->fd,"401 %s %s :No suck nick/channel",user->nick, parameters[0]);
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

			Write(user->fd,"ERROR :Closing link (%s@%s) [%s]",user->ident,user->host,parameters[0]);
			WriteOpers("*** Client exiting: %s!%s@%s [%s]",user->nick,user->ident,user->host,parameters[0]);
			WriteCommonExcept(user,"QUIT :%s%s",PrefixQuit,parameters[0]);

			char buffer[MAXBUF];
			snprintf(buffer,MAXBUF,"Q %s :%s%s",user->nick,PrefixQuit,parameters[0]);
			NetSendToAll(buffer);
		}
		else
		{
			Write(user->fd,"ERROR :Closing link (%s@%s) [QUIT]",user->ident,user->host);
			WriteOpers("*** Client exiting: %s!%s@%s [Client exited]",user->nick,user->ident,user->host);
			WriteCommonExcept(user,"QUIT :Client exited");

			char buffer[MAXBUF];
			snprintf(buffer,MAXBUF,"Q %s :Client exited",user->nick);
			NetSendToAll(buffer);
		}
		FOREACH_MOD OnUserQuit(user);
		AddWhoWas(user);
	}

	/* push the socket on a stack of sockets due to be closed at the next opportunity */
	fd_reap.push_back(user->fd);
	
	if (iter != clientlist.end())
	{
		clientlist.erase(iter);
		log(DEBUG,"deleting user hash value %d",iter->second);
		//if ((user) && (user->registered == 7)) {
			//delete user;
		//}
	}

	if (user->registered == 7) {
		purge_empty_chans();
	}
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
							strncat(tmp, "G", 9);
						} else {
							strncat(tmp, "H", 9);
						}
						if (strchr(i->second->modes,'o')) { strncat(tmp, "*", 9); }
						WriteServ(user->fd,"352 %s %s %s %s %s %s %s :0 %s",user->nick, Ptr ? Ptr->name : "*", i->second->ident, i->second->dhost, i->second->server, i->second->nick, tmp, i->second->fullname);
						n_list++;
						if (n_list > MaxWhoResults)
							break;
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
			  	for (user_hash::const_iterator i = clientlist.begin(); i != clientlist.end(); i++)
				{
					if ((has_channel(i->second,Ptr)) && (isnick(i->second->nick)))
					{
						// Fix Bug #29 - Part 2..
						strcpy(tmp, "");
						if (strcmp(i->second->awaymsg, "")) {
							strncat(tmp, "G", 9);
						} else {
							strncat(tmp, "H", 9);
						}
						if (strchr(i->second->modes,'o')) { strncat(tmp, "*", 9); }
						strcat(tmp, cmode(i->second, Ptr));
						WriteServ(user->fd,"352 %s %s %s %s %s %s %s :0 %s",user->nick, Ptr->name, i->second->ident, i->second->dhost, i->second->server, i->second->nick, tmp, i->second->fullname);
					}
				}
				WriteServ(user->fd,"315 %s %s :End of /WHO list.",user->nick, parameters[0]);
			}
			else
			{
				WriteServ(user->fd,"401 %s %s :No suck nick/channel",user->nick, parameters[0]);
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
					strncat(tmp, "G" ,9);
				} else {
					strncat(tmp, "H" ,9);
				}
				if (strchr(u->modes,'o')) { strncat(tmp, "*" ,9); }
				WriteServ(user->fd,"352 %s %s %s %s %s %s %s :0 %s",user->nick, u->nick, u->ident, u->dhost, u->server, u->nick, tmp, u->fullname);
			}
			WriteServ(user->fd,"315 %s %s :End of /WHO list.",user->nick, parameters[0]);
		}
	}
	if (pcnt == 2)
	{
                if ((!strcmp(parameters[0],"0")) || (!strcmp(parameters[0],"*")) && (!strcmp(parameters[1],"o")))
                {
		  	for (user_hash::const_iterator i = clientlist.begin(); i != clientlist.end(); i++)
                        {
                                if ((common_channels(user,i->second)) && (isnick(i->second->nick)))
                                {
                                        if (strchr(i->second->modes,'o'))
                                        {
						// If i were a rich man.. I wouldn't need to me making these bugfixes..
						// But i'm a poor bastard with nothing better to do.
						strcpy(tmp, "");
						if (strcmp(i->second->awaymsg, "")) {
							strncat(tmp, "G" ,9);
						} else {
							strncat(tmp, "H" ,9);
						}

                                                WriteServ(user->fd,"352 %s %s %s %s %s %s %s* :0 %s",user->nick, user->nick, i->second->ident, i->second->dhost, i->second->server, i->second->nick, tmp, i->second->fullname);
                                        }
                                }
                        }
                        WriteServ(user->fd,"315 %s %s :End of /WHO list.",user->nick, parameters[0]);
                        return;
                }
	}
}

void handle_wallops(char **parameters, int pcnt, userrec *user)
{
	WriteWallOps(user,false,"%s",parameters[0]);
}

void handle_list(char **parameters, int pcnt, userrec *user)
{
	chanrec* Ptr;
	
	WriteServ(user->fd,"321 %s Channel :Users Name",user->nick);
	for (chan_hash::const_iterator i = chanlist.begin(); i != chanlist.end(); i++)
	{
		if ((!i->second->c_private) && (!i->second->secret))
		{
			WriteServ(user->fd,"322 %s %s %d :[+%s] %s",user->nick,i->second->name,usercount_i(i->second),chanmodes(i->second),i->second->topic);
		}
	}
	WriteServ(user->fd,"323 %s :End of channel list.",user->nick);
}


void handle_rehash(char **parameters, int pcnt, userrec *user)
{
	WriteServ(user->fd,"382 %s %s :Rehashing",user->nick,CleanFilename(CONFIG_FILE));
	ReadConfig(false,user);
	FOREACH_MOD OnRehash();
	WriteOpers("%s is rehashing config file %s",user->nick,CleanFilename(CONFIG_FILE));
}

void handle_lusers(char **parameters, int pcnt, userrec *user)
{
	WriteServ(user->fd,"251 %s :There are %d users and %d invisible on %d servers",user->nick,usercnt()-usercount_invisible(),usercount_invisible(),servercount());
	WriteServ(user->fd,"252 %s %d :operator(s) online",user->nick,usercount_opers());
	WriteServ(user->fd,"253 %s %d :unknown connections",user->nick,usercount_unknown());
	WriteServ(user->fd,"254 %s %d :channels formed",user->nick,chancount());
	WriteServ(user->fd,"254 %s :I have %d clients and %d servers",user->nick,local_count(),count_servs());
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
			WriteServ(user->fd,"NOTICE Auth :No ident response, ident prefixed with ~");
			strcpy(user->ident,"~"); /* we arent checking ident... but these days why bother anyway? */
			strlcat(user->ident,parameters[0],IDENTMAX);
			strlcpy(user->fullname,parameters[3],128);
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
		strlcpy(user->awaymsg,parameters[0],512);
		WriteServ(user->fd,"306 %s :You have been marked as being away",user->nick);
	}
	else
	{
		strlcpy(user->awaymsg,"",512);
		WriteServ(user->fd,"305 %s :You are no longer marked as being away",user->nick);
	}
}

void handle_whowas(char **parameters, int pcnt, userrec* user)
{
	user_hash::iterator i = whowas.find(parameters[0]);

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
  	for (int i = 0; i < module_names.size(); i++)
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
		if (!strlen(flagstate))
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
}

void handle_stats(char **parameters, int pcnt, userrec *user)
{
	char Link_ServerName[MAXBUF],Link_IPAddr[MAXBUF],Link_Port[MAXBUF];
	if (pcnt != 1)
	{
		return;
	}
	if (strlen(parameters[0])>1)
	{
		/* make the stats query 1 character long */
		parameters[0][1] = '\0';
	}


	if (!strcasecmp(parameters[0],"c"))
	{
		for (int i = 0; i < ConfValueEnum("link",&config_f); i++)
		{
			ConfValue("link","name",i,Link_ServerName,&config_f);
			ConfValue("link","ipaddr",i,Link_IPAddr,&config_f);
			ConfValue("link","port",i,Link_Port,&config_f);
			WriteServ(user->fd,"213 %s C *@%s * %s %s 0 M",user->nick,Link_IPAddr,Link_ServerName,Link_Port);
			WriteServ(user->fd,"244 %s H * * %s",user->nick,Link_ServerName);
		}
	}
	
	if (!strcasecmp(parameters[0],"i"))
	{
		int idx = 0;
		for (ClassVector::iterator i = Classes.begin(); i != Classes.end(); i++)
		{
			WriteServ(user->fd,"215 %s I * * * %d %d %s *",user->nick,MAXCLIENTS,idx,ServerName);
			idx++;
		}
	}
	
	if (!strcasecmp(parameters[0],"y"))
	{
		int idx = 0;
		for (ClassVector::iterator i = Classes.begin(); i != Classes.end(); i++)
		{
			WriteServ(user->fd,"218 %s Y %d %d 0 %d %d",user->nick,idx,120,i->flood,i->registration_timeout);
			idx++;
		}
	}

	if (!strcmp(parameters[0],"U"))
	{
		for (int i = 0; i < ConfValueEnum("uline",&config_f); i++)
		{
			ConfValue("uline","server",i,Link_ServerName,&config_f);
			WriteServ(user->fd,"248 %s U %s",user->nick,Link_ServerName);
		}
	}
	
	if (!strcmp(parameters[0],"P"))
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
	//249 [Brain] :bwoadway-monitor (~wgmon@204.152.186.58) Idle: 18
	}
 	
	if (!strcmp(parameters[0],"k"))
	{
		stats_k(user);
	}

	if (!strcmp(parameters[0],"g"))
	{
		stats_g(user);
	}

	if (!strcmp(parameters[0],"q"))
	{
		stats_q(user);
	}

	if (!strcmp(parameters[0],"Z"))
	{
		stats_z(user);
	}

	if (!strcmp(parameters[0],"e"))
	{
		stats_e(user);
	}

	/* stats m (list number of times each command has been used, plus bytecount) */
	if (!strcmp(parameters[0],"m"))
	{
		for (int i = 0; i < cmdlist.size(); i++)
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
	if (!strcmp(parameters[0],"z"))
	{
		WriteServ(user->fd,"249 %s :Users(HASH_MAP) %d (%d bytes, %d buckets)",user->nick,clientlist.size(),clientlist.size()*sizeof(userrec),clientlist.bucket_count());
		WriteServ(user->fd,"249 %s :Channels(HASH_MAP) %d (%d bytes, %d buckets)",user->nick,chanlist.size(),chanlist.size()*sizeof(chanrec),chanlist.bucket_count());
		WriteServ(user->fd,"249 %s :Commands(VECTOR) %d (%d bytes)",user->nick,cmdlist.size(),cmdlist.size()*sizeof(command_t));
		WriteServ(user->fd,"249 %s :MOTD(VECTOR) %d, RULES(VECTOR) %d",user->nick,MOTD.size(),RULES.size());
		WriteServ(user->fd,"249 %s :address_cache(HASH_MAP) %d (%d buckets)",user->nick,IP.size(),IP.bucket_count());
		WriteServ(user->fd,"249 %s :Modules(VECTOR) %d (%d)",user->nick,modules.size(),modules.size()*sizeof(Module));
		WriteServ(user->fd,"249 %s :ClassFactories(VECTOR) %d (%d)",user->nick,factory.size(),factory.size()*sizeof(ircd_module));
		WriteServ(user->fd,"249 %s :Ports(STATIC_ARRAY) %d",user->nick,boundPortCount);
	}
	
	/* stats o */
	if (!strcmp(parameters[0],"o"))
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
	if (!strcmp(parameters[0],"l"))
	{
		WriteServ(user->fd,"211 %s :server:port nick bytes_in cmds_in bytes_out cmds_out",user->nick);
	  	for (user_hash::iterator i = clientlist.begin(); i != clientlist.end(); i++)
		{
			if (isnick(i->second->nick))
			{
				WriteServ(user->fd,"211 %s :%s:%d %s %d %d %d %d",user->nick,ServerName,i->second->port,i->second->nick,i->second->bytes_in,i->second->cmds_in,i->second->bytes_out,i->second->cmds_out);
			}
			else
			{
				WriteServ(user->fd,"211 %s :%s:%d (unknown@%d) %d %d %d %d",user->nick,ServerName,i->second->port,i->second->fd,i->second->bytes_in,i->second->cmds_in,i->second->bytes_out,i->second->cmds_out);
			}
			
		}
	}
	
	/* stats u (show server uptime) */
	if (!strcmp(parameters[0],"u"))
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
	char Link_ServerName[1024];
	char Link_IPAddr[1024];
	char Link_Port[1024];
	char Link_Pass[1024];
	int LinkPort;
	bool found = false;
	
	for (int i = 0; i < ConfValueEnum("link",&config_f); i++)
	{
		if (!found)
		{
			ConfValue("link","name",i,Link_ServerName,&config_f);
			ConfValue("link","ipaddr",i,Link_IPAddr,&config_f);
			ConfValue("link","port",i,Link_Port,&config_f);
			ConfValue("link","sendpass",i,Link_Pass,&config_f);
			log(DEBUG,"(%d) Comparing against name='%s', ipaddr='%s', port='%s', recvpass='%s'",i,Link_ServerName,Link_IPAddr,Link_Port,Link_Pass);
			LinkPort = atoi(Link_Port);
			if (match(Link_ServerName,parameters[0])) {
				found = true;
				break;
			}
		}
	}
	
	if (!found) {
		WriteServ(user->fd,"NOTICE %s :*** Failed to connect to %s: No servers matching this pattern are configured for linking.",user->nick,parameters[0]);
		return;
	}
	
	// TODO: Perform a check here to stop a server being linked twice!

	WriteServ(user->fd,"NOTICE %s :*** Connecting to %s (%s) port %s...",user->nick,Link_ServerName,Link_IPAddr,Link_Port);

	if (me[defaultRoute])
	{
		me[defaultRoute]->BeginLink(Link_IPAddr,LinkPort,Link_Pass,Link_ServerName,me[defaultRoute]->port);
		return;
	}
	else
	{
		WriteServ(user->fd,"NOTICE %s :No default route is defined for server connections on this server. You must define a server connection to be default route so that sockets can be bound to it.",user->nick);
	}
}

void handle_squit(char **parameters, int pcnt, userrec *user)
{
	// send out an squit across the mesh and then clear the server list (for local squit)
	if (!pcnt)
	{
		WriteOpers("SQUIT command issued by %s",user->nick);
		char buffer[MAXBUF];
		snprintf(buffer,MAXBUF,"& %s",ServerName);
		NetSendToAll(buffer);
		DoSplitEveryone();
	}
	else
	{
		WriteServ(user->fd,"NOTICE :*** Remote SQUIT not supported yet.");
	}
}

void handle_links(char **parameters, int pcnt, userrec *user)
{
	WriteServ(user->fd,"364 %s %s %s :0 %s",user->nick,ServerName,ServerName,ServerDesc);
	for (int j = 0; j < 32; j++)
 	{
		if (me[j] != NULL)
  		{
			for (int k = 0; k < me[j]->connectors.size(); k++)
			{
				WriteServ(user->fd,"364 %s %s %s :1 %s",user->nick,me[j]->connectors[k].GetServerName().c_str(),ServerName,me[j]->connectors[k].GetDescription().c_str());
			}
		}
	}
	WriteServ(user->fd,"365 %s * :End of /LINKS list.",user->nick);
}

void handle_map(char **parameters, int pcnt, userrec *user)
{
	char line[MAXBUF];
	snprintf(line,MAXBUF,"006 %s :%s",user->nick,ServerName);
	while (strlen(line) < 50)
		strcat(line," ");
	WriteServ(user->fd,"%s%d (%.2f%%)",line,local_count(),(float)(((float)local_count()/(float)registered_usercount())*100));
	for (int j = 0; j < 32; j++)
 	{
		if (me[j] != NULL)
  		{
			for (int k = 0; k < me[j]->connectors.size(); k++)
			{
				snprintf(line,MAXBUF,"006 %s :%c-%s",user->nick,islast(me[j]->connectors[k].GetServerName().c_str()),me[j]->connectors[k].GetServerName().c_str());
				while (strlen(line) < 50)
					strcat(line," ");
				WriteServ(user->fd,"%s%d (%.2f%%)",line,map_count(me[j]->connectors[k].GetServerName().c_str()),(float)(((float)map_count(me[j]->connectors[k].GetServerName().c_str())/(float)registered_usercount())*100));
			}
		}
	}
	WriteServ(user->fd,"007 %s :End of /MAP",user->nick);
}

bool is_uline(const char* server)
{
	char ServName[MAXBUF];
	int i,j;

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


void handle_oper(char **parameters, int pcnt, userrec *user)
{
	char LoginName[MAXBUF];
	char Password[MAXBUF];
	char OperType[MAXBUF];
	char TypeName[MAXBUF];
	char HostName[MAXBUF];
	char TheHost[MAXBUF];
	int i,j;
	bool found = false;
	bool fail2 = false;
	char global[MAXBUF];

	snprintf(TheHost,MAXBUF,"%s@%s",user->ident,user->host);

	for (int i = 0; i < ConfValueEnum("oper",&config_f); i++)
	{
		ConfValue("oper","name",i,LoginName,&config_f);
		ConfValue("oper","password",i,Password,&config_f);
		ConfValue("oper","type",i,OperType,&config_f);
		ConfValue("oper","host",i,HostName,&config_f);
		if ((!strcmp(LoginName,parameters[0])) && (!strcmp(Password,parameters[1])) && (match(TheHost,HostName)))
		{
			fail2 = true;
			for (j =0; j < ConfValueEnum("type",&config_f); j++)
			{
				ConfValue("type","name",j,TypeName,&config_f);

				if (!strcmp(TypeName,OperType))
				{
					/* found this oper's opertype */
					snprintf(global,MAXBUF,"| %s %s",user->nick,TypeName);
					NetSendToAll(global);
					ConfValue("type","host",j,HostName,&config_f);
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
                        snprintf(global,MAXBUF,"M %s +o",user->nick);
                        NetSendToAll(global);
			FOREACH_MOD OnOper(user);
			log(DEFAULT,"OPER: %s!%s@%s opered as type: %s",user->nick,user->ident,user->host,OperType);
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
	if (!strlen(parameters[0]))
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
		
		// Q token must go to ALL servers!!!
		char buffer[MAXBUF];
		snprintf(buffer,MAXBUF,"n %s %s",user->nick,parameters[0]);
		NetSendToAll(buffer);
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
		user->dns_done = (!lookup_dns(user->nick));
		if (user->dns_done)
			log(DEBUG,"Aborting dns lookup of %s because dns server experienced a failure.",user->nick);
	}
	if (user->registered == 3)
	{
		/* user is registered now, bit 0 = USER command, bit 1 = sent a NICK command */
		ConnectUser(user);
	}
	if (user->registered == 7)
	{
		FOREACH_MOD OnUserPostNick(user,oldnick);
	}
}



void handle_V(char token,char* params,serverrec* source,serverrec* reply, char* tcp_host)
{
	char* src = strtok(params," ");
	char* dest = strtok(NULL," :");
	char* text = strtok(NULL,"\r\n");
	text++;
	
	userrec* user = Find(src);
	if (user)
	{
		userrec* dst = Find(dest);
		
		if (dst)
		{
			WriteTo(user, dst, "NOTICE %s :%s", dst->nick, text);
		}
		else
		{
			chanrec* d = FindChan(dest);
			if (d)
			{
				ChanExceptSender(d, user, "NOTICE %s :%s", d->name, text);
			}
		}
	}
	
}


void handle_P(char token,char* params,serverrec* source,serverrec* reply, char* tcp_host)
{
	char* src = strtok(params," ");
	char* dest = strtok(NULL," :");
	char* text = strtok(NULL,"\r\n");
	text++;
	
	userrec* user = Find(src);
	if (user)
	{
		userrec* dst = Find(dest);
		
		if (dst)
		{
			WriteTo(user, dst, "PRIVMSG %s :%s", dst->nick, text);
		}
		else
		{
			chanrec* d = FindChan(dest);
			if (d)
			{
				ChanExceptSender(d, user, "PRIVMSG %s :%s", d->name, text);
			}
		}
	}
	
}

void handle_i(char token,char* params,serverrec* source,serverrec* reply, char* tcp_host)
{
	char* nick = strtok(params," ");
	char* from = strtok(NULL," ");
	char* channel = strtok(NULL," ");
	userrec* u = Find(nick);
	userrec* user = Find(from);
	chanrec* c = FindChan(channel);
	if ((c) && (u) && (user))
	{
		u->InviteTo(c->name);
		WriteFrom(u->fd,user,"INVITE %s :%s",u->nick,c->name);
	}
}

void handle_t(char token,char* params,serverrec* source,serverrec* reply, char* tcp_host)
{
	char* setby = strtok(params," ");
	char* channel = strtok(NULL," :");
	char* topic = strtok(NULL,"\r\n");
	topic++;
	userrec* u = Find(setby);
	chanrec* c = FindChan(channel);
	if ((c) && (u))
	{
		WriteChannelLocal(c,u,"TOPIC %s :%s",c->name,topic);
		strlcpy(c->topic,topic,MAXTOPIC);
		strlcpy(c->setby,u->nick,NICKMAX);
		c->topicset = TIME;
 	}	
}
	

void handle_T(char token,char* params,serverrec* source,serverrec* reply, char* tcp_host)
{
	char* tm = strtok(params," ");
	char* setby = strtok(NULL," ");
	char* channel = strtok(NULL," :");
	char* topic = strtok(NULL,"\r\n");
	topic++;
	time_t TS = atoi(tm);
	chanrec* c = FindChan(channel);
	if (c)
	{
		// in the case of topics and TS, the *NEWER* 
		if (TS <= c->topicset)
		{
			WriteChannelLocal(c,NULL,"TOPIC %s :%s",c->name,topic);
			strlcpy(c->topic,topic,MAXTOPIC);
			strlcpy(c->setby,setby,NICKMAX);
		}
 	}	
}
	
void handle_M(char token,char* params,serverrec* source,serverrec* reply, char* tcp_host)
{
	char* pars[128];
	char original[MAXBUF],target[MAXBUF];
	strlcpy(original,params,MAXBUF);
	int index = 0;
	char* parameter = strtok(params," ");
	strlcpy(target,parameter,MAXBUF);
	while (parameter)
	{
		if (parameter[0] == ':')
			parameter++;
		pars[index++] = parameter;
		parameter = strtok(NULL," ");
	}
	log(DEBUG,"*** MODE: %s %s",pars[0],pars[1]);
	merge_mode(pars,index);
	if (FindChan(target))
	{
		WriteChannelLocal(FindChan(target), NULL, "MODE %s",original);
	}
	if (Find(target))
	{
		Write(Find(target)->fd,":%s MODE %s",ServerName,original);
	}
}

// m is modes set by users only (not servers) valid targets are channels or users.

void handle_m(char token,char* params,serverrec* source,serverrec* reply, char* tcp_host)
{
	// m blah #chatspike +b *!test@*4
	char* pars[128];
	char original[MAXBUF];
	strlcpy(original,params,MAXBUF);
	
	if (!strchr(params,' '))
	{
		WriteOpers("WARNING! 'm' token in data stream without any parameters! Something fishy is going on!");
		return;
	}
	
	int index = 0;
	
	char* src = strtok(params," ");
	userrec* user = Find(src);
	
	if (user)
	{
		log(DEBUG,"Found user: %s",user->nick);
		char* parameter = strtok(NULL," ");
		while (parameter)
		{
			pars[index++] = parameter;
			parameter = strtok(NULL," ");
		}
		
		log(DEBUG,"Calling merge_mode2");
		merge_mode2(pars,index,user);
	}
}


void handle_L(char token,char* params,serverrec* source,serverrec* reply, char* tcp_host)
{
	char* nick = strtok(params," ");
	char* channel = strtok(NULL," :");
	char* reason = strtok(NULL,"\r\n");
	userrec* user = Find(nick);
	reason++;
	if (user)
	{
		if (strcmp(reason,""))
		{
			del_channel(user,channel,reason,true);
		}
		else
		{
			del_channel(user,channel,NULL,true);
		}
	}
}

void handle_K(char token,char* params,serverrec* source,serverrec* reply, char* tcp_host)
{
	char* src = strtok(params," ");
	char* nick = strtok(NULL," :");
	char* reason = strtok(NULL,"\r\n");
	char kreason[MAXBUF];
	reason++;

	userrec* u = Find(nick);
	userrec* user = Find(src);
	
	if ((user) && (u))
	{
		WriteTo(user, u, "KILL %s :%s!%s!%s!%s (%s)", u->nick, source->name, ServerName, user->dhost,user->nick,reason);
		WriteOpers("*** Remote kill from %s by %s: %s!%s@%s (%s)",source->name,user->nick,u->nick,u->ident,u->host,reason);
		snprintf(kreason,MAXBUF,"[%s] Killed (%s (%s))",source->name,user->nick,reason);
		kill_link(u,kreason);
	}
}

void handle_Q(char token,char* params,serverrec* source,serverrec* reply, char* tcp_host)
{
	char* nick = strtok(params," :");
	char* reason = strtok(NULL,"\r\n");
	reason++;

	userrec* user = Find(nick);
	
	if (user)
	{
		if (strlen(reason)>MAXQUIT)
		{
			reason[MAXQUIT-1] = '\0';
		}


		WriteCommonExcept(user,"QUIT :%s",reason);

		user_hash::iterator iter = clientlist.find(user->nick);
	
		if (iter != clientlist.end())
		{
			log(DEBUG,"deleting user hash value %d",iter->second);
			if ((iter->second) && (user->registered == 7)) {
				delete iter->second;
			}
			clientlist.erase(iter);
		}

		purge_empty_chans();
	}
}

void handle_n(char token,char* params,serverrec* source,serverrec* reply, char* tcp_host)
{
	char* oldnick = strtok(params," ");
	char* newnick = strtok(NULL," ");
	
	userrec* user = Find(oldnick);
	
	if (user)
	{
		WriteCommon(user,"NICK %s",newnick);
		if (is_uline(tcp_host))
		{
			int MOD_RESULT = 0;
			FOREACH_RESULT(OnUserPreNick(user,newnick));
			if (MOD_RESULT) {
				// if a module returns true, the nick change couldnt be allowed
				kill_link(user,"Nickname collision");
				return;
			}
			if (matches_qline(newnick))
			{
				kill_link(user,"Nickname collision");
				return;
			}
	
			// broadcast this because its a services thingy
			char buffer[MAXBUF];
			snprintf(buffer,MAXBUF,"n %s %s",user->nick,newnick);
			NetSendToAll(buffer);
		}
		user = ReHashNick(user->nick, newnick);
		if (!user) return;
		if (!user->nick) return;
		strlcpy(user->nick, newnick,NICKMAX);
		log(DEBUG,"new nick set: %s",user->nick);
	}
}

// k <SOURCE> <DEST> <CHANNEL> :<REASON>
void handle_k(char token,char* params,serverrec* source,serverrec* reply, char* tcp_host)
{
	char* src = strtok(params," ");
	char* dest = strtok(NULL," ");
	char* channel = strtok(NULL," :");
	char* reason = strtok(NULL,"\r\n");
	reason++;
	userrec* s = Find(src);
	userrec* d = Find(dest);
	chanrec* c = FindChan(channel);
	if ((s) && (d) && (c))
	{
		kick_channel(s,d,c,reason);
		return;
	}
	d = Find(channel);
	c = FindChan(dest);
	if ((s) && (d) && (c))
	{
		kick_channel(s,d,c,reason);
		return;
	}
}

void handle_AT(char token,char* params,serverrec* source,serverrec* reply, char* tcp_host)
{
	char* who = strtok(params," :");
	char* text = strtok(NULL,"\r\n");
	text++;
	userrec* s = Find(who);
	if (s)
	{
		WriteWallOps(s,true,text);
	}
}

void handle_H(char token,char* params,serverrec* source,serverrec* reply, char* tcp_host)
{
	log(DEBUG,"Adding ULined server %s to my map",params);
	ircd_connector s;
	s.SetState(STATE_DISCONNECTED);
	s.SetServerName(params);

	for (int j = 0; j < 32; j++)
	{
		if (me[j] != NULL)
		{
			for (int k = 0; k < me[j]->connectors.size(); k++)
			{
				if (!strcasecmp(me[j]->connectors[k].GetServerName().c_str(),params))
				{
					// dont allow a server to be added twice
					log(DEBUG,"ULined server %s already in the map!",params);
					return;
				}
			}
		}
	}

	source->connectors.push_back(s);
	WriteOpers("Non-Mesh server %s has joined the network",params);
}

void handle_N(char token,char* params,serverrec* source,serverrec* reply, char* tcp_host)
{
	char* tm = strtok(params," ");
	char* nick = strtok(NULL," ");
	char* host = strtok(NULL," ");
	char* dhost = strtok(NULL," ");
	char* ident = strtok(NULL," ");
	char* modes = strtok(NULL," ");
	char* ipaddr = strtok(NULL," ");
	char* server = strtok(NULL," :");
	char* gecos = strtok(NULL,"\r\n");
	gecos++;
	modes++;
	time_t TS = atoi(tm);
	user_hash::iterator iter = clientlist.find(nick);
	if (iter != clientlist.end())
	{
		// nick collision
		WriteOpers("Nickname collision: %s@%s != %s@%s",nick,server,iter->second->nick,iter->second->server);
		char str[MAXBUF];
		snprintf(str,MAXBUF,"Killed (Nick Collision (%s@%s < %s@%s))",nick,server,iter->second->nick,iter->second->server);
		WriteServ(iter->second->fd, "KILL %s :%s",iter->second->nick,str);
		kill_link(iter->second,str);
	}
	clientlist[nick] = new userrec();
	// remote users have an fd of -1. This is so that our Write abstraction
	// routines know to route any messages to this record away to whatever server
	// theyre on.
	clientlist[nick]->fd = -1;
	strlcpy(clientlist[nick]->nick, nick,NICKMAX);
	strlcpy(clientlist[nick]->host, host,160);
	strlcpy(clientlist[nick]->dhost, dhost,160);
	strlcpy(clientlist[nick]->server, server,256);
	strlcpy(clientlist[nick]->ident, ident,10); // +1 char to compensate for tilde
	strlcpy(clientlist[nick]->fullname, gecos,128);
	clientlist[nick]->signon = TS;
	clientlist[nick]->nping = 0; // this is ignored for a remote user anyway.
	clientlist[nick]->lastping = 1;
	clientlist[nick]->port = 0; // so is this...
	clientlist[nick]->registered = 7; // this however we need to set for them to receive messages and appear online
	clientlist[nick]->idle_lastmsg = TIME; // this is unrealiable and wont actually be used locally
	for (int i = 0; i < MAXCHANS; i++)
	{
 		clientlist[nick]->chans[i].channel = NULL;
 		clientlist[nick]->chans[i].uc_modes = 0;
 	}
}

void handle_F(char token,char* params,serverrec* source,serverrec* reply, char* tcp_host)
{
	long tdiff = TIME - atoi(params);
	if (tdiff)
		WriteOpers("TS split for %s -> %s: %d",source->name,reply->name,tdiff);
}

void handle_a(char token,char* params,serverrec* source,serverrec* reply, char* tcp_host)
{
	char* nick = strtok(params," :");
	char* gecos = strtok(NULL,"\r\n");
	
	userrec* user = Find(nick);

	if (user)
		strlcpy(user->fullname,gecos,MAXBUF);
}

void handle_b(char token,char* params,serverrec* source,serverrec* reply, char* tcp_host)
{
	char* nick = strtok(params," ");
	char* host = strtok(NULL," ");
	
	userrec* user = Find(nick);

	if (user)
		strlcpy(user->dhost,host,160);
}

void handle_plus(char token,char* params,serverrec* source,serverrec* reply, char* tcp_host)
{
	// %s %s %d %d
	// + test3.chatspike.net 7010 -2016508415
	char* servername = strtok(params," ");
	char* ipaddr = strtok(NULL," ");
	char* ipport = strtok(NULL," ");
	char* cookie = strtok(NULL," ");
	log(DEBUG,"*** Connecting back to %s:%d",ipaddr,atoi(ipport));


	bool conn_already = false;
	for (int i = 0; i < 32; i++)
	{
		if (me[i] != NULL)
		{
			for (int j = 0; j < me[i]->connectors.size(); j++)
			{
				if (!strcasecmp(me[i]->connectors[j].GetServerName().c_str(),servername))
				{
					if (me[i]->connectors[j].GetServerPort() == atoi(ipport))
					{
						log(DEBUG,"Already got a connection to %s:%d, ignoring +",ipaddr,atoi(ipport));
						conn_already = true;
					}
				}
			}
		}
	}
	if (!conn_already)
		me[defaultRoute]->MeshCookie(ipaddr,atoi(ipport),atoi(cookie),servername);
}

void handle_R(char token,char* params,serverrec* source,serverrec* reply, char* tcp_host)
{
	char* server = strtok(params," ");
	char* data = strtok(NULL,"\r\n");
	if ((!data) || (!server))
	{
		log(DEBUG,"Someones playing silly buggers, attempting to send to a null server or send a null message (BUG?)");
		return;
	}
		
	log(DEBUG,"Forwarded packet '%s' to '%s'",data,server);
	NetSendToOne(server,data);
}

void handle_J(char token,char* params,serverrec* source,serverrec* reply, char* tcp_host)
{
	// IMPORTANT NOTE
	// The J token currently has no timestamp - this needs looking at
	// because it will allow splitriding.
	char* nick = strtok(params," ");
	char* channel = strtok(NULL," ");
	userrec* user = Find(nick);
	while (channel)
	{
		if ((user != NULL) && (strcmp(channel,"")))
		{
			char privilage = '\0';
			if (channel[0] != '#')
			{
				privilage = channel[0];
				channel++;
			}
			add_channel(user,channel,"",true);

			// now work out the privilages they should have on each channel
			// and send the appropriate servermodes.
			for (int i = 0; i != MAXCHANS; i++)
			{
				if (user->chans[i].channel)
				{
					if (!strcasecmp(user->chans[i].channel->name,channel))
					{
						if (privilage == '@')
						{
							user->chans[i].uc_modes = user->chans[i].uc_modes | UCMODE_OP;
							WriteChannelLocal(user->chans[i].channel, NULL, "MODE %s +o %s",channel,user->nick);
						}
						if (privilage == '%')
						{
							user->chans[i].uc_modes = user->chans[i].uc_modes | UCMODE_HOP;
							WriteChannelLocal(user->chans[i].channel, NULL, "MODE %s +h %s",channel,user->nick);
						}
						if (privilage == '+')
						{
							user->chans[i].uc_modes = user->chans[i].uc_modes | UCMODE_VOICE;
							WriteChannelLocal(user->chans[i].channel, NULL, "MODE %s +v %s",channel,user->nick);
						}
					}
				}
			}

		}
		channel = strtok(NULL," ");
	}
}

void handle_dollar(char token,char* params,serverrec* source,serverrec* reply, char* tcp_host)
{
	log(DEBUG,"Storing routing table...");
	char* sourceserver = strtok(params," ");
	char* server = strtok(NULL," ");
	for (int i = 0; i < 32; i++)
	{
		if (me[i] != NULL)
		{
			for (int j = 0; j < me[i]->connectors.size(); j++)
			{
				if (!strcasecmp(me[i]->connectors[j].GetServerName().c_str(),sourceserver))
				{
					me[i]->connectors[j].routes.clear();
					log(DEBUG,"Found entry for source server.");
					while (server)
					{
						// store each route
						me[i]->connectors[j].routes.push_back(server);
						log(DEBUG,"*** Stored route: %s -> %s -> %s",ServerName,sourceserver,server);
						server = strtok(NULL," ");
					}
					return;
				}
			}
		}
	}
	log(DEBUG,"Warning! routing table received from nonexistent server!");
}

void handle_amp(char token,char* params,serverrec* source,serverrec* reply, char* tcp_host)
{
	log(DEBUG,"Netsplit! %s split from mesh, removing!",params);
	WriteOpers("*** NOTICE - Controlled netsplit: %s split from %s",params,ServerName);
	bool go_again = true;
	while (go_again)
	{
		go_again = false;
		for (int i = 0; i < 32; i++)
		{
			if (me[i] != NULL)
			{
				for (vector<ircd_connector>::iterator j = me[i]->connectors.begin(); j != me[i]->connectors.end(); j++)
				{
					if (!strcasecmp(j->GetServerName().c_str(),params))
					{
						j->routes.clear();
						j->CloseConnection();
						me[i]->connectors.erase(j);
						go_again = true;
						break;
					}
				}
			}
		}
	}
	log(DEBUG,"Removed server. Will remove clients...");
	// iterate through the userlist and remove all users on this server.
	// because we're dealing with a mesh, we dont have to deal with anything
	// "down-route" from this server (nice huh)
	go_again = true;
	char reason[MAXBUF];
	snprintf(reason,MAXBUF,"%s %s",ServerName,params);
	while (go_again)
	{
		go_again = false;
		for (user_hash::const_iterator u = clientlist.begin(); u != clientlist.end(); u++)
		{
			if (!strcasecmp(u->second->server,params))
			{
				kill_link(u->second,reason);
				go_again = true;
				break;
			}
		}
	}
}

long authcookie;

void handle_hash(char token,char* params,serverrec* source,serverrec* reply, char* tcp_host)
{
	// # <mask> <who-set-it> <time-set> <duration> :<reason>
	log(DEBUG,"Adding G-line");
	char* mask = strtok(params," ");
	char* who = strtok(NULL," ");
	char* create_time = strtok(NULL," ");
	char* duration = strtok(NULL," :");
	char* reason = strtok(NULL,"\r\n");
	add_gline(atoi(duration),(const char*)who,(const char*)reason,(const char*)mask);
	// we must update the creation time on this gline
	// now that we've added it, or it wont expire at the right time.
	gline_set_creation_time(mask,atoi(create_time));
	if (!atoi(duration))
	{
		WriteOpers("*** %s Added permenant G-Line on %s.",who,mask);
	}
	else
	{
		WriteOpers("*** %s Added timed G-Line on %s to expire in %d seconds.",who,mask,atoi(duration));
	}
	apply_lines();
}

void handle_dot(char token,char* params,serverrec* source,serverrec* reply, char* tcp_host)
{
	log(DEBUG,"Removing G-line");
	char* mask = strtok(params," ");
	char* who = strtok(NULL," ");
	if (mask)
	{
		if (del_gline((const char*)mask))
		{
			if (who)
			{
				WriteOpers("*** %s Removed G-line on %s.",who,mask);
			}
		}
	}
}

void handle_add_sqline(char token,char* params,serverrec* source,serverrec* reply, char* tcp_host)
{
	// { <mask> <who-set-it> <time-set> <duration> :<reason>
	log(DEBUG,"Adding Q-line");
	char* mask = strtok(params," ");
	char* who = strtok(NULL," ");
	char* create_time = strtok(NULL," ");
	char* duration = strtok(NULL," :");
	char* reason = strtok(NULL,"\r\n");
	add_qline(atoi(duration),(const char*)who,(const char*)reason,(const char*)mask);
	// we must update the creation time on this gline
	// now that we've added it, or it wont expire at the right time.
	qline_set_creation_time(mask,atoi(create_time));
	qline_make_global(mask);
	if (!atoi(duration))
	{
		WriteOpers("*** %s Added permenant Q-Line on %s.",who,mask);
	}
	else
	{
		WriteOpers("*** %s Added timed Q-Line on %s to expire in %d seconds.",who,mask,atoi(duration));
	}
	apply_lines();
}

void handle_del_sqline(char token,char* params,serverrec* source,serverrec* reply, char* tcp_host)
{
	log(DEBUG,"Removing Q-line");
	char* mask = strtok(params," ");
	char* who = strtok(NULL," ");
	if (mask)
	{
		if (del_qline((const char*)mask))
		{
			if (who)
			{
				WriteOpers("*** %s Removed Q-line on %s.",who,mask);
			}
		}
	}
}

void handle_add_szline(char token,char* params,serverrec* source,serverrec* reply, char* tcp_host)
{
	// } <mask> <who-set-it> <time-set> <duration> :<reason>
	log(DEBUG,"Adding Z-line");
	char* mask = strtok(params," ");
	char* who = strtok(NULL," ");
	char* create_time = strtok(NULL," ");
	char* duration = strtok(NULL," :");
	char* reason = strtok(NULL,"\r\n");
	add_zline(atoi(duration),(const char*)who,(const char*)reason,(const char*)mask);
	// we must update the creation time on this gline
	// now that we've added it, or it wont expire at the right time.
	zline_set_creation_time(mask,atoi(create_time));
	zline_make_global(mask);
	if (!atoi(duration))
	{
		WriteOpers("*** %s Added permenant Z-Line on %s.",who,mask);
	}
	else
	{
		WriteOpers("*** %s Added timed Z-Line on %s to expire in %d seconds.",who,mask,atoi(duration));
	}
	apply_lines();
}

void handle_del_szline(char token,char* params,serverrec* source,serverrec* reply, char* tcp_host)
{
	log(DEBUG,"Removing Z-line");
	char* mask = strtok(params," ");
	char* who = strtok(NULL," ");
	if (mask)
	{
		if (del_zline((const char*)mask))
		{
			if (who)
			{
				WriteOpers("*** %s Removed Q-line on %s.",who,mask);
			}
		}
	}
}

void handle_pipe(char token,char* params,serverrec* source,serverrec* reply, char* tcp_host)
{
	char* nick = strtok(params," ");
	char* type = strtok(params," ");
	userrec* u = Find(nick);
	if (u)
	{
		strlcpy(u->oper,type,NICKMAX);
	}
}


void process_restricted_commands(char token,char* params,serverrec* source,serverrec* reply, char* tcp_host,char* ipaddr,int port)
{
	char buffer[MAXBUF];

	switch(token)
	{
		// Y <TS>
  		// start netburst
		case 'Y':
			nb_start = TIME;
			WriteOpers("Server %s is starting netburst.",tcp_host);
			// now broadcast this new servers address out to all servers that are linked to us,
			// except the newcomer. They'll all attempt to connect back to it.
			authcookie = rand()*rand();
			snprintf(buffer,MAXBUF,"~ %d",authcookie);
			NetSendToAll(buffer);
		break;
		// ~
  		// Store authcookie
  		// once stored, this authcookie permits other servers to log in
  		// without user or password, using it.
		case '~':
			auth_cookies.push_back(atoi(params));
			log(DEBUG,"*** Stored auth cookie, will permit servers with auth-cookie %d",atoi(params));
		break;
		// connect back to a server using an authcookie
		case '+':
			handle_plus(token,params,source,reply,tcp_host);
		break;
		// routing table
		case '$':
			handle_dollar(token,params,source,reply,tcp_host);
		break;
		// node unreachable - we cant route to a server, sooooo we slit it off.
		// servers can generate these for themselves for an squit.
		case '&':
			handle_amp(token,params,source,reply,tcp_host);
		break;
		// R <server> <data>
		// redirect token, send all of <data> along to the given 
		// server as this server has been found to still have a route to it
		case 'R':
			handle_R(token,params,source,reply,tcp_host);
		break;
		// ?
  		// ping
		case '?':
			reply->SendPacket("!",tcp_host);
		break;
		// ?
  		// pong
		case '!':
		break;
		// *
  		// no operation
		case '*':
		break;
		// N <TS> <NICK> <HOST> <DHOST> <IDENT> <MODES> <SERVER> :<GECOS>
		// introduce remote client
		case 'N':
			handle_N(token,params,source,reply,tcp_host);
		break;
		// a <NICK> :<GECOS>
		// change GECOS (SETNAME)
		case 'a':
			handle_a(token,params,source,reply,tcp_host);
		break;
		// b <NICK> :<HOST>
		// change displayed host (SETHOST)
		case 'b':
			handle_b(token,params,source,reply,tcp_host);
		break;
		// t <NICK> <CHANNEL> :<TOPIC>
		// change a channel topic
		case 't':
			handle_t(token,params,source,reply,tcp_host);
		break;
		// i <NICK> <CHANNEL>
		// invite a user to a channel
		case 'i':
			handle_i(token,params,source,reply,tcp_host);
		break;
		// k <SOURCE> <DEST> <CHANNEL> :<REASON>
		// kick a user from a channel
		case 'k':
			handle_k(token,params,source,reply,tcp_host);
		break;
		// n <NICK> <NEWNICK>
		// change nickname of client -- a server should only be able to
		// change the nicknames of clients that reside on it unless
		// they are ulined.
		case 'n':
			handle_n(token,params,source,reply,tcp_host);
		break;
		// J <NICK> <CHANLIST>
		// Join user to channel list, merge channel permissions
		case 'J':
			handle_J(token,params,source,reply,tcp_host);
		break;
		// T <TS> <CHANNEL> <TOPICSETTER> :<TOPIC>
		// change channel topic (netburst only)
		case 'T':
			handle_T(token,params,source,reply,tcp_host);
		break;
		// M <TARGET> <MODES> [MODE-PARAMETERS]
		// Server setting modes on an object
		case 'M':
			handle_M(token,params,source,reply,tcp_host);
		break;
		// m <SOURCE> <TARGET> <MODES> [MODE-PARAMETERS]
		// User setting modes on an object
		case 'm':
			handle_m(token,params,source,reply,tcp_host);
		break;
		// P <SOURCE> <TARGET> :<TEXT>
		// Send a private/channel message
		case 'P':
			handle_P(token,params,source,reply,tcp_host);
		break;
		// V <SOURCE> <TARGET> :<TEXT>
		// Send a private/channel notice
		case 'V':
			handle_V(token,params,source,reply,tcp_host);
		break;
		// L <SOURCE> <CHANNEL> :<REASON>
		// User parting a channel
		case 'L':
			handle_L(token,params,source,reply,tcp_host);
		break;
		// Q <SOURCE> :<REASON>
		// user quitting
		case 'Q':
			handle_Q(token,params,source,reply,tcp_host);
		break;
		// H <SERVER>
		// introduce non-meshable server (such as a services server)
		case 'H':
			handle_H(token,params,source,reply,tcp_host);
		break;
		// K <SOURCE> <DEST> :<REASON>
		// remote kill
		case 'K':
			handle_K(token,params,source,reply,tcp_host);
		break;
		// @ <SOURCE> :<TEXT>
		// wallops
		case '@':
			handle_AT(token,params,source,reply,tcp_host);
		break;
		// # <mask> <who-set-it> <time-set> <duration> :<reason>
		// add gline
		case '#':
			handle_hash(token,params,source,reply,tcp_host);
		break;
		// . <mask> <who>
		// remove gline
		case '.':
			handle_dot(token,params,source,reply,tcp_host);
		break;
		// # <mask> <who-set-it> <time-set> <duration> :<reason>
		// add gline
		case '{':
			handle_add_sqline(token,params,source,reply,tcp_host);
		break;
		// . <mask> <who>
		// remove gline
		case '[':
			handle_del_sqline(token,params,source,reply,tcp_host);
		break;
		// # <mask> <who-set-it> <time-set> <duration> :<reason>
		// add gline
		case '}':
			handle_add_szline(token,params,source,reply,tcp_host);
		break;
		// . <mask> <who>
		// remove gline
		case ']':
			handle_del_szline(token,params,source,reply,tcp_host);
		break;
		// | <nick> <opertype>
		// set opertype
		case '|':
			handle_pipe(token,params,source,reply,tcp_host);
		break;
		// F <TS>
		// end netburst
		case 'F':
			WriteOpers("Server %s has completed netburst. (%d secs)",tcp_host,TIME-nb_start);
			handle_F(token,params,source,reply,tcp_host);
			nb_start = 0;
			// tell all the other servers to use this authcookie to connect back again
			// got '+ test3.chatspike.net 7010 -2016508415' from test.chatspike.net
			snprintf(buffer,MAXBUF,"+ %s %s %d %d",tcp_host,ipaddr,port,authcookie);
			NetSendToAllExcept(tcp_host,buffer);
		break;
		case '/':
			WriteOpers("Server %s is IRCServices-based server (assumes-SVSMODE) - Nickname Services: %s",tcp_host,params);
			strlcpy(source->nickserv,params,NICKMAX);
		break;
		// F <TS>
		// end netburst with no mesh creation
		case 'f':
			WriteOpers("Server %s has completed netburst. (%d secs)",tcp_host,TIME-nb_start);
			handle_F(token,params,source,reply,tcp_host);
			nb_start = 0;
			// tell everyone else about the new server name so they just add it in the disconnected
			// state
			snprintf(buffer,MAXBUF,"u %s :%s",tcp_host,GetServerDescription(tcp_host).c_str());
			NetSendToAllExcept(tcp_host,buffer);
		break;
		// X <reserved>
		// Send netburst now
		case 'X':
			WriteOpers("Sending my netburst to %s",tcp_host);
			DoSync(source,tcp_host);
			WriteOpers("Send of netburst to %s completed",tcp_host);
			NetSendMyRoutingTable();
		break;
		// anything else
		default:
			WriteOpers("WARNING! Unknown datagram type '%c'",token);
		break;
	}
}


void handle_link_packet(char* udp_msg, char* tcp_host, serverrec *serv)
{
	if ((!strncmp(udp_msg,"USER ",5)) || (!strncmp(udp_msg,"NICK ",5)))
	{
		// a user on a server port, just close their connection.
		RemoveServer(tcp_host);
		return;
	}

	char response[10240];
	char token = udp_msg[0];
	char* old = udp_msg;

	if (token == ':') // leading :servername or details - strip them off (services does this, sucky)
	{
		char* src = udp_msg+1;
		while (udp_msg[0] != ' ')
			udp_msg++;
		udp_msg[0] = 0;
		udp_msg++;
		char* comd = udp_msg;
		while (udp_msg[0] != ' ')
			udp_msg++;
		udp_msg[0] = 0;
		udp_msg++;
		char data[MAXBUF];
		char source[MAXBUF];
		char command[MAXBUF];
		strlcpy(data,udp_msg,512);
		strlcpy(source,src,MAXBUF);
		strlcpy(command,comd,MAXBUF);
		udp_msg = old;
		
		// unused numeric:
		// :services-dev.chatspike.net 433 Craig Craig :Nickname is registered to someone else
		if (!strcmp(command,"433"))
		{
			token = '*';
		}
		if (!strcmp(command,"432"))
		{
			token = '*';
		}
		if (!strcmp(command,"PING"))
		{
			token = '*';
		}
		if (!strcmp(command,"NOTICE"))
		{
			snprintf(udp_msg,MAXBUF,"V %s %s",source,data);
			log(DEBUG,"Rewrote NOTICE from services to: '%s'",udp_msg);
			token = udp_msg[0];
		}
		if (!strcmp(command,"QUIT"))
		{
			if ((!udp_msg) || (!strcmp(data,"")) || (strcmp(data,":")))
			{
				strcpy(data,":No reason");
			}
			if (!strcmp(data,":"))
			{
				strcpy(data,":No reason");
			}
			snprintf(udp_msg,MAXBUF,"Q %s %s",source,data);
			log(DEBUG,"Rewrote QUIT from services to: '%s'",udp_msg);
			token = udp_msg[0];
		}
		if (!strcmp(command,"SQUIT"))
		{
			snprintf(udp_msg,MAXBUF,"& %s",source);
			log(DEBUG,"Rewrote SQUIT from services to: '%s'",udp_msg);
			token = udp_msg[0];
		}
		if (!strcmp(command,"SVSMODE"))
		{
			snprintf(udp_msg,MAXBUF,"m %s %s",source,data);
			log(DEBUG,"Rewrote SVSMODE from services to: '%s'",udp_msg);
			token = udp_msg[0];
		}
		if (!strcmp(command,"SVS2MODE"))
		{
			snprintf(udp_msg,MAXBUF,"m %s %s",source,data);
			log(DEBUG,"Rewrote SVS2MODE from services to: '%s'",udp_msg);
			token = udp_msg[0];
		}
		// todo: this wont work without u:lines
		// in give_ops etc allow nick on a u:lined serv to do just about anything
		if (!strcmp(command,"MODE"))
		{
			snprintf(udp_msg,MAXBUF,"m %s %s",source,data);
			log(DEBUG,"Rewrote MODE from services to: '%s'",udp_msg);
			token = udp_msg[0];
		}
		if (!strcmp(command,"KICK"))
		{
			snprintf(udp_msg,MAXBUF,"k %s %s",source,data);
			log(DEBUG,"Rewrote KICK from services to: '%s'",udp_msg);
			token = udp_msg[0];
		}
		if (!strcmp(command,"KILL"))
		{
			snprintf(udp_msg,MAXBUF,"K %s %s",source,data);
			log(DEBUG,"Rewrote KILL from services to: '%s'",udp_msg);
			token = udp_msg[0];
		}
		if (!strcmp(command,"SVSJOIN"))
		{
			snprintf(udp_msg,MAXBUF,"J %s",data);
			NetSendToOne(tcp_host,udp_msg);
			char* nick = strtok(data," ");
			char* chan = strtok(NULL," ");
			log(DEBUG,"Rewrote SVSJOIN from services to: '%s'",udp_msg);
			userrec* u = Find(nick);
			if (u)
			{
				add_channel(u,chan,"",true);
			}
			token = udp_msg[0];
		}
		
	}


	char* params = udp_msg + 2;
	char finalparam[1024];
	strcpy(finalparam," :xxxx");
	if (strstr(udp_msg," :")) {
 		strlcpy(finalparam,strstr(udp_msg," :"),1024);
	}
	
	
  	if (token == '-') {
  		char* cookie = strtok(params," ");
		char* servername = strtok(NULL," ");
		char* serverdesc = finalparam+2;

		WriteOpers("AuthCookie CONNECT from %s (%s)",servername,tcp_host);

		for (int u = 0; u < auth_cookies.size(); u++)
		{
			if (auth_cookies[u] == atoi(cookie))
			{
				WriteOpers("Allowed cookie from %s, is now part of the mesh",servername);


				for (int j = 0; j < 32; j++)
    				{
					if (me[j] != NULL)
     					{
     						for (int k = 0; k < me[j]->connectors.size(); k++)
     						{
							if (!strcasecmp(me[j]->connectors[k].GetServerName().c_str(),tcp_host))
      							{
      								me[j]->connectors[k].SetServerName(servername);
								me[j]->connectors[k].SetDescription(serverdesc);
								me[j]->connectors[k].SetState(STATE_CONNECTED);
								NetSendMyRoutingTable();
      								return;
							}
						}
					}
					WriteOpers("\2WARNING!\2 %s sent us an authentication packet but we are not authenticating with this server right now! Possible intrusion attempt!",tcp_host);
					return;
				}


				return;
			}
		}
		// bad cookie, bad bad! go sit in the corner!
		WriteOpers("Bad cookie from %s!",servername);
		return;
  	}
  	else
  	if (token == 'S') {
		// S test.chatspike.net password portn :ChatSpike InspIRCd test server
		char* servername = strtok(params," ");
		char* password = strtok(NULL," ");
		char* myport = strtok(NULL," ");
		char* revision = strtok(NULL," ");
		char* serverdesc = finalparam+2;

		WriteOpers("CONNECT from %s (%s) (their port: %d)",servername,tcp_host,atoi(myport));
		
		ircd_connector* cn = serv->FindHost(servername);
		
		if (cn)
		{
			WriteOpers("CONNECT aborted: Server %s already exists from %s",servername,ServerName);
			char buffer[MAXBUF];
			snprintf(buffer,MAXBUF,"E :Server %s already exists!",servername);
			serv->SendPacket(buffer,tcp_host);
			RemoveServer(tcp_host);
			return;
		}

		if (atoi(revision) != GetRevision())
		{
			WriteOpers("CONNECT aborted: Could not link to %s, is an incompatible version %s, our version is %d",servername,revision,GetRevision());
			char buffer[MAXBUF];
			sprintf(buffer,"E :Version number mismatch");
			serv->SendPacket(buffer,tcp_host);
			RemoveServer(tcp_host);
			RemoveServer(servername);
			return;
		}

		for (int j = 0; j < serv->connectors.size(); j++)
		{
			if (!strcasecmp(serv->connectors[j].GetServerName().c_str(),tcp_host))
			{
				serv->connectors[j].SetServerName(servername);
				serv->connectors[j].SetDescription(serverdesc);
				serv->connectors[j].SetServerPort(atoi(myport));
			}
		}
		
		
		char Link_ServerName[1024];
		char Link_IPAddr[1024];
		char Link_Port[1024];
		char Link_Pass[1024];
		char Link_SendPass[1024];
		int LinkPort = 0;
		
		// search for a corresponding <link> block in the config files
		for (int i = 0; i < ConfValueEnum("link",&config_f); i++)
		{
			ConfValue("link","name",i,Link_ServerName,&config_f);
			ConfValue("link","ipaddr",i,Link_IPAddr,&config_f);
			ConfValue("link","port",i,Link_Port,&config_f);
			ConfValue("link","recvpass",i,Link_Pass,&config_f);
			ConfValue("link","sendpass",i,Link_SendPass,&config_f);
			log(DEBUG,"(%d) Comparing against name='%s', ipaddr='%s', port='%s', recvpass='%s'",i,Link_ServerName,Link_IPAddr,Link_Port,Link_Pass);
			LinkPort = atoi(Link_Port);
			if ((!strcasecmp(Link_ServerName,servername)) && (!strcmp(Link_Pass,password)))
  			{
				// we have a matching link line -
				// send a 'diminutive' server message back...
				snprintf(response,10240,"s %s %s :%s",ServerName,Link_SendPass,ServerDesc);
				serv->SendPacket(response,servername);

				for (int t = 0; t < serv->connectors.size(); t++)
				{
					if (!strcasecmp(serv->connectors[t].GetServerName().c_str(),servername))
					{
						serv->connectors[t].SetState(STATE_CONNECTED);
					}
				}
		
				return;
			}
		}
		char buffer[MAXBUF];
		sprintf(buffer,"E :Access is denied (no matching link block)");
		serv->SendPacket(buffer,tcp_host);
		WriteOpers("CONNECT from %s denied, no matching link block",servername);
		RemoveServer(tcp_host);
		RemoveServer(servername);
		return;
	}
	else
	if (token == 's') {
		// S test.chatspike.net password :ChatSpike InspIRCd test server
		char* servername = strtok(params," ");
		char* password = strtok(NULL," ");
		char* serverdesc = finalparam+2;
		
		// TODO: we should do a check here to ensure that this server is one we recently initiated a
		// link with, and didnt hear an 's' or 'E' back from yet (these are the only two valid responses
		// to an 'S' command. If we didn't recently send an 'S' to this server, theyre trying to spoof
		// a connect, so put out an oper alert!
		
		// for now, just accept all, we'll fix that later.
		WriteOpers("%s accepted our link credentials ",servername);
		
		char Link_ServerName[1024];
		char Link_IPAddr[1024];
		char Link_Port[1024];
		char Link_Pass[1024];
		char Link_SendPass[1024];
		int LinkPort = 0;
		
		// search for a corresponding <link> block in the config files
		for (int i = 0; i < ConfValueEnum("link",&config_f); i++)
		{
			ConfValue("link","name",i,Link_ServerName,&config_f);
			ConfValue("link","ipaddr",i,Link_IPAddr,&config_f);
			ConfValue("link","port",i,Link_Port,&config_f);
			ConfValue("link","recvpass",i,Link_Pass,&config_f);
			ConfValue("link","sendpass",i,Link_SendPass,&config_f);
			log(DEBUG,"(%d) Comparing against name='%s', ipaddr='%s', port='%s', recvpass='%s'",i,Link_ServerName,Link_IPAddr,Link_Port,Link_Pass);
			LinkPort = atoi(Link_Port);
			if (!strcasecmp(Link_ServerName,servername))
   			{
				// matching link at this end too, we're all done!
				// at this point we must begin key exchange and insert this
				// server into our 'active' table.
				for (int j = 0; j < 32; j++)
				{
					if (me[j] != NULL)
     					{
     						for (int k = 0; k < me[j]->connectors.size(); k++)
     						{
							if (!strcasecmp(me[j]->connectors[k].GetServerName().c_str(),tcp_host))
      							{
								char buffer[MAXBUF];
								me[j]->connectors[k].SetDescription(serverdesc);
								me[j]->connectors[k].SetState(STATE_CONNECTED);
								sprintf(buffer,"X 0");
								serv->SendPacket(buffer,tcp_host);
								DoSync(me[j],tcp_host);
								NetSendMyRoutingTable();
								return;
							}
						}
					}
					WriteOpers("\2WARNING!\2 %s sent us an authentication packet but we are not authenticating with this server right noe! Possible intrusion attempt!",tcp_host);
					return;
				}
			}
			else {
				log(DEBUG,"Server names '%s' and '%s' don't match",Link_ServerName,servername);
			}
		}
		char buffer[MAXBUF];
		sprintf(buffer,"E :Access is denied (no matching link block)");
		serv->SendPacket(buffer,tcp_host);
		WriteOpers("CONNECT from %s denied, no matching link block",servername);
		RemoveServer(tcp_host);
		RemoveServer(servername);
		return;
	}
	else
	if (token == 'U') {
		// U services.chatspike.net password :ChatSpike Services
		//
		// non-meshed link, used by services. Everything coming from a non-meshed link is auto broadcasted.
		char* servername = strtok(params," ");
		char* password = strtok(NULL," ");
		char* serverdesc = finalparam+2;
		
		char Link_ServerName[1024];
		char Link_IPAddr[1024];
		char Link_Port[1024];
		char Link_Pass[1024];
		char Link_SendPass[1024];
		int LinkPort = 0;

		log(DEBUG,"U-token linked server detected.");
		
		// search for a corresponding <link> block in the config files
		for (int i = 0; i < ConfValueEnum("link",&config_f); i++)
		{
			ConfValue("link","name",i,Link_ServerName,&config_f);
			ConfValue("link","ipaddr",i,Link_IPAddr,&config_f);
			ConfValue("link","port",i,Link_Port,&config_f);
			ConfValue("link","recvpass",i,Link_Pass,&config_f);
			ConfValue("link","sendpass",i,Link_SendPass,&config_f);
			log(DEBUG,"(%d) Comparing against name='%s', ipaddr='%s', port='%s', recvpass='%s'",i,Link_ServerName,Link_IPAddr,Link_Port,Link_Pass);
			LinkPort = atoi(Link_Port);
			if ((!strcasecmp(Link_ServerName,servername)) && (!strcmp(Link_Pass,password)))
   			{
				// matching link at this end too, we're all done!
				// at this point we must begin key exchange and insert this
				// server into our 'active' table.
				for (int j = 0; j < 32; j++)
				{
					if (me[j] != NULL)
     					{
     						for (int k = 0; k < me[j]->connectors.size(); k++)
     						{
							if (!strcasecmp(me[j]->connectors[k].GetServerName().c_str(),tcp_host))
      							{
          							char buffer[MAXBUF];
								me[j]->connectors[k].SetDescription(serverdesc);
								me[j]->connectors[k].SetServerName(servername);
								me[j]->connectors[k].SetState(STATE_SERVICES);
								sprintf(buffer,"X 0");
								serv->SendPacket(buffer,servername);
								DoSync(me[j],servername);
								snprintf(buffer,MAXBUF,"H %s",servername);
								NetSendToAllExcept(servername,buffer);
								WriteOpers("Non-Mesh server %s has joined the network",servername);
								log(DEBUG,"******** SENDING MY ROUTING TABLE! *******");
								NetSendMyRoutingTable();
								return;
							}
						}
					}
					WriteOpers("\2WARNING!\2 %s sent us an authentication packet but we are not authenticating with this server right noe! Possible intrusion attempt!",tcp_host);
					return;
				}
			}
			else {
				log(DEBUG,"Server names '%s' and '%s' don't match",Link_ServerName,servername);
			}
		}
		char buffer[MAXBUF];
		sprintf(buffer,"E :Access is denied (no matching link block)");
		serv->SendPacket(buffer,tcp_host);
		WriteOpers("CONNECT from %s denied, no matching link block",servername);
		RemoveServer(tcp_host);
		RemoveServer(servername);
		return;
	}
	else
	if (token == 'E') {
		char* error_message = finalparam+2;
		WriteOpers("ERROR from %s: %s",tcp_host,error_message);
		return;
	}
	else {

		serverrec* source_server = NULL;

		for (int j = 0; j < 32; j++)
  		{
			if (me[j] != NULL)
   			{
				for (int x = 0; x < me[j]->connectors.size(); x++)
    				{
    					log(DEBUG,"Servers are: '%s' '%s'",tcp_host,me[j]->connectors[x].GetServerName().c_str());
    					if (!strcasecmp(me[j]->connectors[x].GetServerName().c_str(),tcp_host))
    					{
    						if ((me[j]->connectors[x].GetState() == STATE_CONNECTED) || (me[j]->connectors[x].GetState() == STATE_SERVICES))
    						{
    							// found a valid ircd_connector.
      							process_restricted_commands(token,params,me[j],serv,tcp_host,me[j]->connectors[x].GetServerIP(),me[j]->connectors[x].GetServerPort());
							return;
						}
					}
				}
			}
		}

		log(DEBUG,"Unrecognised token or unauthenticated host in datagram from %s: %c",tcp_host,token);
	}
}

long duration(const char* str)
{
	char n_field[MAXBUF];
	long total = 0;
	const char* str_end = str + strlen(str);
	n_field[0] = 0;
	
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


void handle_kline(char **parameters, int pcnt, userrec *user)
{
	if (pcnt >= 3)
	{
		add_kline(duration(parameters[1]),user->nick,parameters[2],parameters[0]);
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
                add_eline(duration(parameters[1]),user->nick,parameters[2],parameters[0]);
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
	char netdata[MAXBUF];
	if (pcnt >= 3)
	{
		add_gline(duration(parameters[1]),user->nick,parameters[2],parameters[0]);
		// # <mask> <who-set-it> <time-set> <duration> :<reason>
		snprintf(netdata,MAXBUF,"# %s %s %ld %ld :%s",parameters[0],user->nick,TIME,duration(parameters[1]),parameters[2]);
		NetSendToAll(netdata);
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
			// . <mask> <who-removed-it>
			snprintf(netdata,MAXBUF,". %s %s",parameters[0],user->nick);
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
		add_zline(duration(parameters[1]),user->nick,parameters[2],parameters[0]);
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
		add_qline(duration(parameters[1]),user->nick,parameters[2],parameters[0]);
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
			WriteOpers("*** %s Removed Q-line on %s.",user->nick,parameters[0]);
		}
		else
		{
			WriteServ(user->fd,"NOTICE %s :*** Q-Line %s not found in list, try /stats k.",user->nick,parameters[0]);
		}
	}
	apply_lines();
}


