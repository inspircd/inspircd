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

#ifdef GCC3
#define nspace __gnu_cxx
#else
#define nspace std
#endif

using namespace std;

extern int MODCOUNT;
extern vector<Module*> modules;
extern vector<ircd_module*> factory;

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
extern time_t nb_start;

extern std::vector<int> fd_reap;
extern std::vector<std::string> module_names;

extern char bannerBuffer[MAXBUF];
extern int boundPortCount;
extern int portCount;
extern int UDPportCount;
extern int ports[MAXSOCKS];
extern int defaultRoute;

extern std::vector<long> auth_cookies;
extern std::stringstream config_f;

extern serverrec* me[32];

extern FILE *log_file;

namespace nspace
{
	template<> struct nspace::hash<in_addr>
	{
		size_t operator()(const struct in_addr &a) const
		{
			size_t q;
			memcpy(&q,&a,sizeof(size_t));
			return q;
		}
	};

	template<> struct nspace::hash<string>
	{
		size_t operator()(const string &s) const
		{
			char a[MAXBUF];
			static struct hash<const char *> strhash;
			strcpy(a,s.c_str());
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
		strcpy(a,s1.c_str());
		strcpy(b,s2.c_str());
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
	
	if (!has_channel(u,Ptr))
	{
		WriteServ(user->fd,"442 %s %s :You're not on that channel!",user->nick, parameters[0]);
		return;
	}

	char reason[MAXBUF];
	
	if (pcnt > 2)
	{
		strncpy(reason,parameters[2],MAXBUF);
		if (strlen(reason)>MAXKICK)
		{
			reason[MAXKICK-1] = '\0';
		}

		kick_channel(user,u,Ptr,reason);
	}
	else
	{
		strcpy(reason,user->nick);
		kick_channel(user,u,Ptr,reason);
	}
	
	// this must be propogated so that channel membership is kept in step network-wide
	
	char buffer[MAXBUF];
	snprintf(buffer,MAXBUF,"k %s %s %s :%s",user->nick,u->nick,Ptr->name,reason);
	NetSendToAll(buffer);
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
	log(DEBUG,"restart: %s",user->nick);
	if (!strcmp(parameters[0],restartpass))
	{
		WriteOpers("*** RESTART command from %s!%s@%s, Pretending to restart till this is finished :D",user->nick,user->ident,user->host);
		sleep(DieDelay);
		Exit(ERROR);
		/* Will finish this later when i can be arsed :) */
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
		if (strcmp(ServerName,u->server))
		{
			// remote kill
			WriteOpers("*** Remote kill by %s: %s!%s@%s (%s)",user->nick,u->nick,u->ident,u->host,parameters[1]);
			sprintf(killreason,"[%s] Killed (%s (%s))",ServerName,user->nick,parameters[1]);
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
			WriteTo(user, u, "KILL %s :%s!%s!%s (%s)", u->nick, ServerName,user->dhost,user->nick,parameters[1]);
			WriteOpers("*** Local Kill by %s: %s!%s@%s (%s)",user->nick,u->nick,u->ident,u->host,parameters[1]);
			sprintf(killreason,"Killed (%s (%s))",user->nick,parameters[1]);
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
				if ((Ptr->topiclock) && (cstatus(user,Ptr)<STATUS_HOP))
				{
					WriteServ(user->fd,"482 %s %s :You must be at least a half-operator to change modes on this channel", user->nick, Ptr->name);
					return;
				}
				
				char topic[MAXBUF];
				strncpy(topic,parameters[1],MAXBUF);
				if (strlen(topic)>MAXTOPIC)
				{
					topic[MAXTOPIC-1] = '\0';
				}
					
				strcpy(Ptr->topic,topic);
				strcpy(Ptr->setby,user->nick);
				Ptr->topicset = time(NULL);
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

	if (loop_call(handle_names,parameters,pcnt,user,0,pcnt-1,0))
		return;
	c = FindChan(parameters[0]);
	if (c)
	{
		/*WriteServ(user->fd,"353 %s = %s :%s", user->nick, c->name,*/
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

	user->idle_lastmsg = time(NULL);
	
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

			FOREACH_RESULT(OnUserPreMessage(user,chan,TYPE_CHANNEL,std::string(parameters[1])));
			if (MOD_RESULT) {
				return;
			}
			
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
		
		FOREACH_RESULT(OnUserPreMessage(user,dest,TYPE_USER,std::string(parameters[1])));
		if (MOD_RESULT) {
			return;
		}



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

	user->idle_lastmsg = time(NULL);
	
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
		
			FOREACH_RESULT(OnUserPreNotice(user,chan,TYPE_CHANNEL,std::string(parameters[1])));
			if (MOD_RESULT) {
				return;
			}

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
		
		FOREACH_RESULT(OnUserPreNotice(user,dest,TYPE_USER,std::string(parameters[1])));
		if (MOD_RESULT) {
			return;
		}

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
			if (strchr(dest->modes,'o'))
			{
				WriteServ(user->fd,"313 %s %s :is an IRC operator",user->nick, dest->nick);
			}
			FOREACH_MOD OnWhois(user,dest);
			if (!strcasecmp(user->server,dest->server))
			{
				// idle time and signon line can only be sent if youre on the same server (according to RFC)
				WriteServ(user->fd,"317 %s %s %d %d :seconds idle, signon time",user->nick, dest->nick, abs((dest->idle_lastmsg)-time(NULL)), dest->signon);
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
	
	/* theres more to do here, but for now just close the socket */
	if (pcnt == 1)
	{
		if ((!strcmp(parameters[0],"0")) || (!strcmp(parameters[0],"*")))
		{
			if (user->chans[0].channel)
			{
				Ptr = user->chans[0].channel;
			  	for (user_hash::const_iterator i = clientlist.begin(); i != clientlist.end(); i++)
				{
					if ((common_channels(user,i->second)) && (isnick(i->second->nick)))
					{
						WriteServ(user->fd,"352 %s %s %s %s %s %s Hr@ :0 %s",user->nick, Ptr->name, i->second->ident, i->second->dhost, i->second->server, i->second->nick, i->second->fullname);
					}
				}
			}
			if (Ptr)
			{
				WriteServ(user->fd,"315 %s %s :End of /WHO list.",user->nick, Ptr->name);
			}
			else
			{
				WriteServ(user->fd,"315 %s %s :End of /WHO list.",user->nick, user->nick);
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
						WriteServ(user->fd,"352 %s %s %s %s %s %s Hr@ :0 %s",user->nick, Ptr->name, i->second->ident, i->second->dhost, i->second->server, i->second->nick, i->second->fullname);
					}
				}
				WriteServ(user->fd,"315 %s %s :End of /WHO list.",user->nick, Ptr->name);
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
				WriteServ(user->fd,"352 %s %s %s %s %s %s Hr@ :0 %s",user->nick, u->nick, u->ident, u->dhost, u->server, u->nick, u->fullname);
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
                                                WriteServ(user->fd,"352 %s %s %s %s %s %s Hr@ :0 %s",user->nick, user->nick, i->second->ident, i->second->dhost, i->second->server, i->second->nick, i->second->fullname);
                                        }
                                }
                        }
                        WriteServ(user->fd,"315 %s %s :End of /WHO list.",user->nick, user->nick);
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
	ReadConfig();
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
			strncat(user->ident,parameters[0],IDENTMAX);
			strncpy(user->fullname,parameters[3],128);
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
	sprintf(Return,"302 %s :",user->nick);
	for (int i = 0; i < pcnt; i++)
	{
		userrec *u = Find(parameters[i]);
		if (u)
		{
			if (strchr(u->modes,'o'))
			{
				sprintf(junk,"%s*=+%s@%s ",u->nick,u->ident,u->host);
				strcat(Return,junk);
			}
			else
			{
				sprintf(junk,"%s=+%s@%s ",u->nick,u->ident,u->host);
				strcat(Return,junk);
			}
		}
	}
	WriteServ(user->fd,Return);
}


void handle_ison(char **parameters, int pcnt, userrec *user)
{
	char Return[MAXBUF];
	sprintf(Return,"303 %s :",user->nick);
	for (int i = 0; i < pcnt; i++)
	{
		userrec *u = Find(parameters[i]);
		if (u)
		{
			strcat(Return,u->nick);
			strcat(Return," ");
		}
	}
	WriteServ(user->fd,Return);
}


void handle_away(char **parameters, int pcnt, userrec *user)
{
	if (pcnt)
	{
		strcpy(user->awaymsg,parameters[0]);
		WriteServ(user->fd,"306 %s :You have been marked as being away",user->nick);
	}
	else
	{
		strcpy(user->awaymsg,"");
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
		strcpy(b,asctime(timeinfo));
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
			strncpy(modulename,module_names[i].c_str(),256);
			WriteServ(user->fd,"900 %s :0x%08lx %d.%d.%d.%d %s",user->nick,modules[i],V.Major,V.Minor,V.Revision,V.Build,CleanFilename(modulename));
	}
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

	/* stats m (list number of times each command has been used, plus bytecount) */
	if (!strcasecmp(parameters[0],"m"))
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
	if (!strcasecmp(parameters[0],"z"))
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
	if (!strcasecmp(parameters[0],"o"))
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
	if (!strcasecmp(parameters[0],"l"))
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
	if (!strcasecmp(parameters[0],"u"))
	{
		time_t current_time = 0;
		current_time = time(NULL);
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
	WriteServ(user->fd,"%s%d (%.2f%%)",line,local_count(),(float)(((float)local_count()/(float)usercnt())*100));
	for (int j = 0; j < 32; j++)
 	{
		if (me[j] != NULL)
  		{
			for (int k = 0; k < me[j]->connectors.size(); k++)
			{
				snprintf(line,MAXBUF,"006 %s :%c-%s",user->nick,islast(me[j]->connectors[k].GetServerName().c_str()),me[j]->connectors[k].GetServerName().c_str());
				while (strlen(line) < 50)
					strcat(line," ");
				WriteServ(user->fd,"%s%d (%.2f%%)",line,map_count(me[j]->connectors[k].GetServerName().c_str()),(float)(((float)map_count(me[j]->connectors[k].GetServerName().c_str())/(float)usercnt())*100));
			}
		}
	}
	WriteServ(user->fd,"007 %s :End of /MAP",user->nick);
}


void handle_oper(char **parameters, int pcnt, userrec *user)
{
	char LoginName[MAXBUF];
	char Password[MAXBUF];
	char OperType[MAXBUF];
	char TypeName[MAXBUF];
	char Hostname[MAXBUF];
	int i,j;

	for (int i = 0; i < ConfValueEnum("oper",&config_f); i++)
	{
		ConfValue("oper","name",i,LoginName,&config_f);
		ConfValue("oper","password",i,Password,&config_f);
		if ((!strcmp(LoginName,parameters[0])) && (!strcmp(Password,parameters[1])))
		{
			/* correct oper credentials */
			ConfValue("oper","type",i,OperType,&config_f);
			WriteOpers("*** %s (%s@%s) is now an IRC operator of type %s",user->nick,user->ident,user->host,OperType);
			WriteServ(user->fd,"381 %s :You are now an IRC operator of type %s",user->nick,OperType);
			WriteServ(user->fd,"MODE %s :+o",user->nick);
			for (j =0; j < ConfValueEnum("type",&config_f); j++)
			{
				ConfValue("type","name",j,TypeName,&config_f);
				if (!strcmp(TypeName,OperType))
				{
					/* found this oper's opertype */
					ConfValue("type","host",j,Hostname,&config_f);
					ChangeDisplayedHost(user,Hostname);
				}
			}
			if (!strchr(user->modes,'o'))
			{
				strcat(user->modes,"o");
			}
			FOREACH_MOD OnOper(user);
			return;
		}
	}
	/* no such oper */
	WriteServ(user->fd,"491 %s :Invalid oper credentials",user->nick);
	WriteOpers("*** WARNING! Failed oper attempt by %s!%s@%s!",user->nick,user->ident,user->host);
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
		WriteCommon(user,"NICK %s",parameters[0]);
		
		// Q token must go to ALL servers!!!
		char buffer[MAXBUF];
		snprintf(buffer,MAXBUF,"n %s %s",user->nick,parameters[0]);
		NetSendToAll(buffer);
		
	}
	
	/* change the nick of the user in the users_hash */
	user = ReHashNick(user->nick, parameters[0]);
	/* actually change the nick within the record */
	if (!user) return;
	if (!user->nick) return;

	strncpy(user->nick, parameters[0],NICKMAX);

	log(DEBUG,"new nick set: %s",user->nick);
	
	if (user->registered < 3)
		user->registered = (user->registered | 2);
	if (user->registered == 3)
	{
		/* user is registered now, bit 0 = USER command, bit 1 = sent a NICK command */
		ConnectUser(user);
	}
	log(DEBUG,"exit nickchange: %s",user->nick);
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
		strncpy(c->topic,topic,MAXTOPIC);
		strncpy(c->setby,u->nick,NICKMAX);
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
			strncpy(c->topic,topic,MAXTOPIC);
			strncpy(c->setby,setby,NICKMAX);
		}
 	}	
}
	
void handle_M(char token,char* params,serverrec* source,serverrec* reply, char* tcp_host)
{
	char* pars[128];
	char original[MAXBUF],target[MAXBUF];
	strncpy(original,params,MAXBUF);
	int index = 0;
	char* parameter = strtok(params," ");
	strncpy(target,parameter,MAXBUF);
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
	strncpy(original,params,MAXBUF);
	
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
		user = ReHashNick(user->nick, newnick);
		if (!user) return;
		if (!user->nick) return;
		strncpy(user->nick, newnick,NICKMAX);
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


void handle_N(char token,char* params,serverrec* source,serverrec* reply, char* tcp_host)
{
	char* tm = strtok(params," ");
	char* nick = strtok(NULL," ");
	char* host = strtok(NULL," ");
	char* dhost = strtok(NULL," ");
	char* ident = strtok(NULL," ");
	char* modes = strtok(NULL," ");
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
	strncpy(clientlist[nick]->nick, nick,NICKMAX);
	strncpy(clientlist[nick]->host, host,160);
	strncpy(clientlist[nick]->dhost, dhost,160);
	strncpy(clientlist[nick]->server, server,256);
	strncpy(clientlist[nick]->ident, ident,10); // +1 char to compensate for tilde
	strncpy(clientlist[nick]->fullname, gecos,128);
	clientlist[nick]->signon = TS;
	clientlist[nick]->nping = 0; // this is ignored for a remote user anyway.
	clientlist[nick]->lastping = 1;
	clientlist[nick]->port = 0; // so is this...
	clientlist[nick]->registered = 7; // this however we need to set for them to receive messages and appear online
	clientlist[nick]->idle_lastmsg = time(NULL); // this is unrealiable and wont actually be used locally
	for (int i = 0; i < MAXCHANS; i++)
	{
 		clientlist[nick]->chans[i].channel = NULL;
 		clientlist[nick]->chans[i].uc_modes = 0;
 	}
}

void handle_F(char token,char* params,serverrec* source,serverrec* reply, char* tcp_host)
{
	long tdiff = time(NULL) - atoi(params);
	if (tdiff)
		WriteOpers("TS split for %s -> %s: %d",source->name,reply->name,tdiff);
}

void handle_a(char token,char* params,serverrec* source,serverrec* reply, char* tcp_host)
{
	char* nick = strtok(params," :");
	char* gecos = strtok(NULL,"\r\n");
	
	userrec* user = Find(nick);

	if (user)
		strncpy(user->fullname,gecos,MAXBUF);
}

void handle_b(char token,char* params,serverrec* source,serverrec* reply, char* tcp_host)
{
	char* nick = strtok(params," ");
	char* host = strtok(NULL," ");
	
	userrec* user = Find(nick);

	if (user)
		strncpy(user->dhost,host,160);
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


void process_restricted_commands(char token,char* params,serverrec* source,serverrec* reply, char* tcp_host,char* ipaddr,int port)
{
	char buffer[MAXBUF];

	switch(token)
	{
		// Y <TS>
  		// start netburst
		case 'Y':
			nb_start = time(NULL);
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
		// F <TS>
		// end netburst
		case 'F':
			WriteOpers("Server %s has completed netburst. (%d secs)",tcp_host,time(NULL)-nb_start);
			handle_F(token,params,source,reply,tcp_host);
			nb_start = 0;
			// tell all the other servers to use this authcookie to connect back again
			// got '+ test3.chatspike.net 7010 -2016508415' from test.chatspike.net
			snprintf(buffer,MAXBUF,"+ %s %s %d %d",tcp_host,ipaddr,port,authcookie);
			NetSendToAllExcept(tcp_host,buffer);
		break;
		// F <TS>
		// end netburst with no mesh creation
		case 'f':
			WriteOpers("Server %s has completed netburst. (%d secs)",tcp_host,time(NULL)-nb_start);
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
		strcpy(data,udp_msg);
		strcpy(source,src);
		strcpy(command,comd);
		udp_msg = old;
		if (!strcmp(command,"NOTICE"))
		{
			snprintf(udp_msg,MAXBUF,"V %s %s",source,data);
			log(DEBUG,"Rewrote NOTICE from services to: '%s'",udp_msg);
			token = udp_msg[0];
		}
		if (!strcmp(command,"SVSMODE"))
		{
			snprintf(udp_msg,MAXBUF,"M %s",data);
			log(DEBUG,"Rewrote SVSMODE from services to: '%s'",udp_msg);
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
		
	}


	char* params = udp_msg + 2;
	char finalparam[1024];
	strcpy(finalparam," :xxxx");
	if (strstr(udp_msg," :")) {
 		strncpy(finalparam,strstr(udp_msg," :"),1024);
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
			sprintf(buffer,"E :Server %s already exists!",servername);
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
			if (!strcasecmp(Link_ServerName,servername))
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
								me[j]->connectors[k].SetServerName(servername);
								me[j]->connectors[k].SetState(STATE_SERVICES);
								sprintf(buffer,"X 0");
								serv->SendPacket(buffer,servername);
								DoSync(me[j],servername);
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

