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

/* Now with added unF! ;) */

using namespace std;

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

#ifdef GCC3
#define nspace __gnu_cxx
#else
#define nspace std
#endif

int LogLevel = DEFAULT;
char ServerName[MAXBUF];
char Network[MAXBUF];
char ServerDesc[MAXBUF];
char AdminName[MAXBUF];
char AdminEmail[MAXBUF];
char AdminNick[MAXBUF];
char diepass[MAXBUF];
char restartpass[MAXBUF];
char motd[MAXBUF];
char rules[MAXBUF];
char list[MAXBUF];
char PrefixQuit[MAXBUF];
char DieValue[MAXBUF];
int debugging =  0;
int WHOWAS_STALE = 48; // default WHOWAS Entries last 2 days before they go 'stale'
int WHOWAS_MAX = 100;  // default 100 people maximum in the WHOWAS list
int DieDelay  =  5;
time_t startup_time = time(NULL);
int NetBufferSize = 10240; // NetBufferSize used as the buffer size for all read() ops
time_t nb_start = 0;

extern vector<Module*> modules;
std::vector<std::string> module_names;
extern vector<ircd_module*> factory;
std::vector<int> fd_reap;

extern int MODCOUNT;

bool nofork = false;

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

serverrec* me[32];

FILE *log_file;

user_hash clientlist;
chan_hash chanlist;
user_hash whowas;
command_table cmdlist;
file_cache MOTD;
file_cache RULES;
address_cache IP;

ClassVector Classes;

struct linger linger = { 0 };
char bannerBuffer[MAXBUF];
int boundPortCount = 0;
int portCount = 0, UDPportCount = 0, ports[MAXSOCKS];
int defaultRoute = 0;

connection C;

long MyKey = C.GenKey();

/* prototypes */

int has_channel(userrec *u, chanrec *c);
int usercount(chanrec *c);
int usercount_i(chanrec *c);
void update_stats_l(int fd,int data_out);
char* Passwd(userrec *user);
bool IsDenied(userrec *user);
void AddWhoWas(userrec* u);

std::vector<long> auth_cookies;
std::stringstream config_f(stringstream::in | stringstream::out);

void safedelete(userrec *p)
{
	if (p)
	{
		log(DEBUG,"deleting %s %s %s %s",p->nick,p->ident,p->dhost,p->fullname);
		log(DEBUG,"safedelete(userrec*): pointer is safe to delete");
		delete p;
		p = NULL;
	}
	else
	{
		log(DEBUG,"safedelete(userrec*): unsafe pointer operation squished");
	}
}

void safedelete(chanrec *p)
{
	if (p)
	{
		delete p;
		p = NULL;
		log(DEBUG,"safedelete(chanrec*): pointer is safe to delete");
	}
	else
	{
		log(DEBUG,"safedelete(chanrec*): unsafe pointer operation squished");
	}
}


void tidystring(char* str)
{
	// strips out double spaces before a : parameter
	
	char temp[MAXBUF];
	bool go_again = true;
	
	if (!str)
	{
		return;
	}
	
	while ((str[0] == ' ') && (strlen(str)>0))
	{
		str++;
	}
	
	while (go_again)
	{
		bool noparse = false;
		int t = 0, a = 0;
		go_again = false;
		while (a < strlen(str))
		{
			if ((a<strlen(str)-1) && (noparse==false))
			{
				if ((str[a] == ' ') && (str[a+1] == ' '))
				{
					log(DEBUG,"Tidied extra space out of string: %s",str);
					go_again = true;
					a++;
				}
			}
			
			if (a<strlen(str)-1)
			{
				if ((str[a] == ' ') && (str[a+1] == ':'))
				{
					noparse = true;
				}
			}
			
			temp[t++] = str[a++];
		}
		temp[t] = '\0';
		strncpy(str,temp,MAXBUF);
	}
}

/* chop a string down to 512 characters and preserve linefeed (irc max
 * line length) */

void chop(char* str)
{
  if (!str)
  {
  	log(DEBUG,"ERROR! Null string passed to chop()!");
  	return;
  }
  string temp = str;
  FOREACH_MOD OnServerRaw(temp,false);
  const char* str2 = temp.c_str();
  sprintf(str,"%s",str2);
  

  if (strlen(str) >= 512)
  {
  	str[509] = '\r';
  	str[510] = '\n';
  	str[511] = '\0';
  }
}


long GetRevision()
{
	char Revision[] = "$Revision$";
	char *s1 = Revision;
	char *savept;
	char *v1 = strtok_r(s1," ",&savept);
	s1 = savept;
	char *v2 = strtok_r(s1," ",&savept);
	s1 = savept;
	return (long)(atof(v2)*10000);
}


std::string getservername()
{
	return ServerName;
}

std::string getserverdesc()
{
	return ServerDesc;
}

std::string getnetworkname()
{
	return Network;
}

std::string getadminname()
{
	return AdminName;
}

std::string getadminemail()
{
	return AdminEmail;
}

std::string getadminnick()
{
	return AdminNick;
}

void log(int level,char *text, ...)
{
	char textbuffer[MAXBUF];
	va_list argsPtr;
	time_t rawtime;
	struct tm * timeinfo;
	if (level < LogLevel)
		return;

	time(&rawtime);
	timeinfo = localtime (&rawtime);

	if (log_file)
	{
		char b[MAXBUF];
		va_start (argsPtr, text);
		vsnprintf(textbuffer, MAXBUF, text, argsPtr);
		va_end(argsPtr);
		strcpy(b,asctime(timeinfo));
		b[strlen(b)-1] = ':';
		fprintf(log_file,"%s %s\n",b,textbuffer);
		if (nofork)
		{
			// nofork enabled? display it on terminal too
			printf("%s %s\n",b,textbuffer);
		}
	}
}

void readfile(file_cache &F, const char* fname)
{
  FILE* file;
  char linebuf[MAXBUF];

  log(DEBUG,"readfile: loading %s",fname);
  F.clear();
  file =  fopen(fname,"r");
  if (file)
  {
  	while (!feof(file))
  	{
  		fgets(linebuf,sizeof(linebuf),file);
  		linebuf[strlen(linebuf)-1]='\0';
  		if (!strcmp(linebuf,""))
  		{
  			strcpy(linebuf,"  ");
  		}
  		if (!feof(file))
  		{
  			F.push_back(linebuf);
  		}
  	}
  	fclose(file);
  }
  else
  {
	  log(DEBUG,"readfile: failed to load file: %s",fname);
  }
  log(DEBUG,"readfile: loaded %s, %d lines",fname,F.size());
}

void ReadConfig(void)
{
  char dbg[MAXBUF],pauseval[MAXBUF],Value[MAXBUF],timeout[MAXBUF],NB[MAXBUF],flood[MAXBUF];
  ConnectClass c;

  LoadConf(CONFIG_FILE,&config_f);
  
  ConfValue("server","name",0,ServerName,&config_f);
  ConfValue("server","description",0,ServerDesc,&config_f);
  ConfValue("server","network",0,Network,&config_f);
  ConfValue("admin","name",0,AdminName,&config_f);
  ConfValue("admin","email",0,AdminEmail,&config_f);
  ConfValue("admin","nick",0,AdminNick,&config_f);
  ConfValue("files","motd",0,motd,&config_f);
  ConfValue("files","rules",0,rules,&config_f);
  ConfValue("power","diepass",0,diepass,&config_f);
  ConfValue("power","pause",0,pauseval,&config_f);
  ConfValue("power","restartpass",0,restartpass,&config_f);
  ConfValue("options","prefixquit",0,PrefixQuit,&config_f);
  ConfValue("die","value",0,DieValue,&config_f);
  ConfValue("options","loglevel",0,dbg,&config_f);
  ConfValue("options","netbuffersize",0,NB,&config_f);
  NetBufferSize = atoi(NB);
  if ((!NetBufferSize) || (NetBufferSize > 65535) || (NetBufferSize < 1024))
  {
  	log(DEFAULT,"No NetBufferSize specified or size out of range, setting to default of 10240.");
  	NetBufferSize = 10240;
  }
  if (!strcmp(dbg,"debug"))
  	LogLevel = DEBUG;
  if (!strcmp(dbg,"verbose"))
  	LogLevel = VERBOSE;
  if (!strcmp(dbg,"default"))
  	LogLevel = DEFAULT;
  if (!strcmp(dbg,"sparse"))
  	LogLevel = SPARSE;
  if (!strcmp(dbg,"none"))
  	LogLevel = NONE;
  readfile(MOTD,motd);
  log(DEBUG,"Reading message of the day");
  readfile(RULES,rules);
  log(DEBUG,"Reading connect classes");
  Classes.clear();
  for (int i = 0; i < ConfValueEnum("connect",&config_f); i++)
  {
	strcpy(Value,"");
	ConfValue("connect","allow",i,Value,&config_f);
	ConfValue("connect","timeout",i,timeout,&config_f);
	ConfValue("connect","flood",i,flood,&config_f);
	if (strcmp(Value,""))
	{
		strcpy(c.host,Value);
		c.type = CC_ALLOW;
		strcpy(Value,"");
		ConfValue("connect","password",i,Value,&config_f);
		strcpy(c.pass,Value);
		c.registration_timeout = 90; // default is 2 minutes
		c.flood = atoi(flood);
		if (atoi(timeout)>0)
		{
			c.registration_timeout = atoi(timeout);
		}
		Classes.push_back(c);
		log(DEBUG,"Read connect class type ALLOW, host=%s password=%s timeout=%d flood=%d",c.host,c.pass,c.registration_timeout,c.flood);
	}
	else
	{
		ConfValue("connect","deny",i,Value,&config_f);
                strcpy(c.host,Value);
                c.type = CC_DENY;
		Classes.push_back(c);
		log(DEBUG,"Read connect class type DENY, host=%s",c.host);
	}
	
  }
}

void Blocking(int s)
{
  int flags;
  log(DEBUG,"Blocking: %d",s);
  flags = fcntl(s, F_GETFL, 0);
  fcntl(s, F_SETFL, flags ^ O_NONBLOCK);
}

void NonBlocking(int s)
{
  int flags;
  log(DEBUG,"NonBlocking: %d",s);
  flags = fcntl(s, F_GETFL, 0);
  //fcntl(s, F_SETFL, O_NONBLOCK);
  fcntl(s, F_SETFL, flags | O_NONBLOCK);
}


int CleanAndResolve (char *resolvedHost, const char *unresolvedHost)
{
  struct hostent *hostPtr = NULL;
  struct in_addr addr;

  memset (resolvedHost, '\0',MAXBUF);
  if(unresolvedHost == NULL)
	return(ERROR);
  if ((inet_aton(unresolvedHost,&addr)) == 0)
	return(ERROR);
  hostPtr = gethostbyaddr ((char *)&addr.s_addr,sizeof(addr.s_addr),AF_INET);
  if (hostPtr != NULL)
  	snprintf(resolvedHost,MAXBUF,"%s",hostPtr->h_name);
  else
  	snprintf(resolvedHost,MAXBUF,"%s",unresolvedHost);
  return (TRUE);
}

/* write formatted text to a socket, in same format as printf */

void Write(int sock,char *text, ...)
{
  if (!text)
  {
  	log(DEFAULT,"*** BUG *** Write was given an invalid parameter");
  	return;
  }
  char textbuffer[MAXBUF];
  va_list argsPtr;
  char tb[MAXBUF];

  va_start (argsPtr, text);
  vsnprintf(textbuffer, MAXBUF, text, argsPtr);
  va_end(argsPtr);
  sprintf(tb,"%s\r\n",textbuffer);
  chop(tb);
  if (sock != -1)
  {
	write(sock,tb,strlen(tb));
	update_stats_l(sock,strlen(tb)); /* add one line-out to stats L for this fd */
  }
}

/* write a server formatted numeric response to a single socket */

void WriteServ(int sock, char* text, ...)
{
  if (!text)
  {
  	log(DEFAULT,"*** BUG *** WriteServ was given an invalid parameter");
	return;
  }
  char textbuffer[MAXBUF],tb[MAXBUF];
  va_list argsPtr;
  va_start (argsPtr, text);

  vsnprintf(textbuffer, MAXBUF, text, argsPtr);
  va_end(argsPtr);
  sprintf(tb,":%s %s\r\n",ServerName,textbuffer);
  chop(tb);
  if (sock != -1)
  {
	write(sock,tb,strlen(tb));
	update_stats_l(sock,strlen(tb)); /* add one line-out to stats L for this fd */
  }
}

/* write text from an originating user to originating user */

void WriteFrom(int sock, userrec *user,char* text, ...)
{
  if ((!text) || (!user))
  {
  	log(DEFAULT,"*** BUG *** WriteFrom was given an invalid parameter");
	return;
  }
  char textbuffer[MAXBUF],tb[MAXBUF];
  va_list argsPtr;
  va_start (argsPtr, text);

  vsnprintf(textbuffer, MAXBUF, text, argsPtr);
  va_end(argsPtr);
  sprintf(tb,":%s!%s@%s %s\r\n",user->nick,user->ident,user->dhost,textbuffer);
  chop(tb);
  if (sock != -1)
  {
	write(sock,tb,strlen(tb));
	update_stats_l(sock,strlen(tb)); /* add one line-out to stats L for this fd */
  }
}

/* write text to an destination user from a source user (e.g. user privmsg) */

void WriteTo(userrec *source, userrec *dest,char *data, ...)
{
	if ((!dest) || (!data))
	{
		log(DEFAULT,"*** BUG *** WriteTo was given an invalid parameter");
		return;
	}
	char textbuffer[MAXBUF],tb[MAXBUF];
	va_list argsPtr;
	va_start (argsPtr, data);
	vsnprintf(textbuffer, MAXBUF, data, argsPtr);
	va_end(argsPtr);
	chop(tb);

	// if no source given send it from the server.
	if (!source)
	{
		WriteServ(dest->fd,":%s %s",ServerName,textbuffer);
	}
	else
	{
		WriteFrom(dest->fd,source,"%s",textbuffer);
	}
}

/* write formatted text from a source user to all users on a channel
 * including the sender (NOT for privmsg, notice etc!) */

void WriteChannel(chanrec* Ptr, userrec* user, char* text, ...)
{
	if ((!Ptr) || (!user) || (!text))
	{
		log(DEFAULT,"*** BUG *** WriteChannel was given an invalid parameter");
		return;
	}
	char textbuffer[MAXBUF];
	va_list argsPtr;
	va_start (argsPtr, text);
	vsnprintf(textbuffer, MAXBUF, text, argsPtr);
	va_end(argsPtr);
	for (user_hash::const_iterator i = clientlist.begin(); i != clientlist.end(); i++)
	{
		if (has_channel(i->second,Ptr))
		{
			WriteTo(user,i->second,"%s",textbuffer);
		}
	}
}

/* write formatted text from a source user to all users on a channel
 * including the sender (NOT for privmsg, notice etc!) doesnt send to
 * users on remote servers */

void WriteChannelLocal(chanrec* Ptr, userrec* user, char* text, ...)
{
	if ((!Ptr) || (!text))
	{
		log(DEFAULT,"*** BUG *** WriteChannel was given an invalid parameter");
		return;
	}
	char textbuffer[MAXBUF];
	va_list argsPtr;
	va_start (argsPtr, text);
	vsnprintf(textbuffer, MAXBUF, text, argsPtr);
	va_end(argsPtr);
	for (user_hash::const_iterator i = clientlist.begin(); i != clientlist.end(); i++)
	{
		if (has_channel(i->second,Ptr))
		{
			if (i->second->fd != -1)
			{
				if (!user)
				{
					WriteServ(i->second->fd,"%s",textbuffer);
				}
				else
				{
					WriteTo(user,i->second,"%s",textbuffer);
				}
			}	
		}
	}
}


void WriteChannelWithServ(char* ServerName, chanrec* Ptr, userrec* user, char* text, ...)
{
	if ((!Ptr) || (!user) || (!text))
	{
		log(DEFAULT,"*** BUG *** WriteChannelWithServ was given an invalid parameter");
		return;
	}
	char textbuffer[MAXBUF];
	va_list argsPtr;
	va_start (argsPtr, text);
	vsnprintf(textbuffer, MAXBUF, text, argsPtr);
	va_end(argsPtr);
	for (user_hash::const_iterator i = clientlist.begin(); i != clientlist.end(); i++)
	{
		if (i->second)
		{
			if (has_channel(i->second,Ptr))
			{
				WriteServ(i->second->fd,"%s",textbuffer);
			}
		}
	}
}


/* write formatted text from a source user to all users on a channel except
 * for the sender (for privmsg etc) */

void ChanExceptSender(chanrec* Ptr, userrec* user, char* text, ...)
{
	if ((!Ptr) || (!user) || (!text))
	{
		log(DEFAULT,"*** BUG *** ChanExceptSender was given an invalid parameter");
		return;
	}
	char textbuffer[MAXBUF];
	va_list argsPtr;
	va_start (argsPtr, text);
	vsnprintf(textbuffer, MAXBUF, text, argsPtr);
	va_end(argsPtr);

	for (user_hash::const_iterator i = clientlist.begin(); i != clientlist.end(); i++)
	{
		if (i->second)
		{
			if (has_channel(i->second,Ptr) && (user != i->second))
			{
				WriteTo(user,i->second,"%s",textbuffer);
			}
		}
	}
}

int c_count(userrec* u)
{
	int z = 0;
	for (int i =0; i != MAXCHANS; i++)
		if (u->chans[i].channel != NULL)
			z++;
	return z;

}


std::string GetServerDescription(char* servername)
{
	for (int j = 0; j < 32; j++)
	{
		if (me[j] != NULL)
		{
			for (int k = 0; k < me[j]->connectors.size(); k++)
			{
				if (!strcasecmp(me[j]->connectors[k].GetServerName().c_str(),servername))
				{
					return me[j]->connectors[k].GetDescription();
				}
			}
		}
		return ServerDesc; // not a remote server that can be found, it must be me.
	}
}


/* return 0 or 1 depending if users u and u2 share one or more common channels
 * (used by QUIT, NICK etc which arent channel specific notices) */

int common_channels(userrec *u, userrec *u2)
{
	int i = 0;
	int z = 0;

	if ((!u) || (!u2))
	{
		log(DEFAULT,"*** BUG *** common_channels was given an invalid parameter");
		return 0;
	}
	for (int i = 0; i != MAXCHANS; i++)
	{
		for (z = 0; z != MAXCHANS; z++)
		{
			if ((u->chans[i].channel != NULL) && (u2->chans[z].channel != NULL))
			{
				if ((u->chans[i].channel == u2->chans[z].channel) && (u->chans[i].channel) && (u2->chans[z].channel) && (u->registered == 7) && (u2->registered == 7))
				{
					if ((c_count(u)) && (c_count(u2)))
					{
						return 1;
					}
				}
			}
		}
	}
	return 0;
}

/* write a formatted string to all users who share at least one common
 * channel, including the source user e.g. for use in NICK */

void WriteCommon(userrec *u, char* text, ...)
{
	if (!u)
	{
		log(DEFAULT,"*** BUG *** WriteCommon was given an invalid parameter");
		return;
	}

	if (u->registered != 7) {
		log(DEFAULT,"*** BUG *** WriteCommon on an unregistered user");
		return;
	}
	
	char textbuffer[MAXBUF];
	va_list argsPtr;
	va_start (argsPtr, text);
	vsnprintf(textbuffer, MAXBUF, text, argsPtr);
	va_end(argsPtr);

	WriteFrom(u->fd,u,"%s",textbuffer);

	for (user_hash::const_iterator i = clientlist.begin(); i != clientlist.end(); i++)
	{
		if (i->second)
		{
			if (common_channels(u,i->second) && (i->second != u))
			{
				WriteFrom(i->second->fd,u,"%s",textbuffer);
			}
		}
	}
}

/* write a formatted string to all users who share at least one common
 * channel, NOT including the source user e.g. for use in QUIT */

void WriteCommonExcept(userrec *u, char* text, ...)
{
	if (!u)
	{
		log(DEFAULT,"*** BUG *** WriteCommon was given an invalid parameter");
		return;
	}

	if (u->registered != 7) {
		log(DEFAULT,"*** BUG *** WriteCommon on an unregistered user");
		return;
	}

	char textbuffer[MAXBUF];
	va_list argsPtr;
	va_start (argsPtr, text);
	vsnprintf(textbuffer, MAXBUF, text, argsPtr);
	va_end(argsPtr);

	for (user_hash::const_iterator i = clientlist.begin(); i != clientlist.end(); i++)
	{
		if (i->second)
		{
			if ((common_channels(u,i->second)) && (u != i->second))
			{
				WriteFrom(i->second->fd,u,"%s",textbuffer);
			}
		}
	}
}

void WriteOpers(char* text, ...)
{
	if (!text)
	{
		log(DEFAULT,"*** BUG *** WriteOpers was given an invalid parameter");
		return;
	}

	char textbuffer[MAXBUF];
	va_list argsPtr;
	va_start (argsPtr, text);
	vsnprintf(textbuffer, MAXBUF, text, argsPtr);
	va_end(argsPtr);

	for (user_hash::const_iterator i = clientlist.begin(); i != clientlist.end(); i++)
	{
		if (i->second)
		{
			if (strchr(i->second->modes,'o'))
			{
				if (strchr(i->second->modes,'s'))
				{
					// send server notices to all with +s
					// (TODO: needs SNOMASKs)
					WriteServ(i->second->fd,"NOTICE %s :%s",i->second->nick,textbuffer);
				}
			}
		}
	}
}

// returns TRUE of any users on channel C occupy server 'servername'.

bool ChanAnyOnThisServer(chanrec *c,char* servername)
{
	log(DEBUG,"ChanAnyOnThisServer");
	for (user_hash::iterator i = clientlist.begin(); i != clientlist.end(); i++)
	{
		if (has_channel(i->second,c))
		{
			if (!strcasecmp(i->second->server,servername))
			{
				return true;
			}
		}
	}
	return false;
}

// returns true if user 'u' shares any common channels with any users on server 'servername'

bool CommonOnThisServer(userrec* u,const char* servername)
{
	log(DEBUG,"ChanAnyOnThisServer");
	for (user_hash::iterator i = clientlist.begin(); i != clientlist.end(); i++)
	{
		if ((common_channels(u,i->second)) && (u != i->second))
		{
			if (!strcasecmp(i->second->server,servername))
			{
				log(DEBUG,"%s is common to %s sharing with %s",i->second->nick,servername,u->nick);
				return true;
			}
		}
	}
	return false;
}


void NetSendToCommon(userrec* u, char* s)
{
	char buffer[MAXBUF];
	snprintf(buffer,MAXBUF,"%s",s);
	
	log(DEBUG,"NetSendToCommon: '%s' '%s'",u->nick,s);

	for (int j = 0; j < 32; j++)
	{
		if (me[j] != NULL)
		{
			for (int k = 0; k < me[j]->connectors.size(); k++)
			{
				if (CommonOnThisServer(u,me[j]->connectors[k].GetServerName().c_str()))
				{
					me[j]->SendPacket(buffer,me[j]->connectors[k].GetServerName().c_str());
				}
			}
		}
	}
}


void NetSendToAll(char* s)
{
	char buffer[MAXBUF];
	snprintf(buffer,MAXBUF,"%s",s);
	
	log(DEBUG,"NetSendToAll: '%s'",s);

	for (int j = 0; j < 32; j++)
	{
		if (me[j] != NULL)
		{
			for (int k = 0; k < me[j]->connectors.size(); k++)
			{
				me[j]->SendPacket(buffer,me[j]->connectors[k].GetServerName().c_str());
			}
		}
	}
}

void NetSendToAllAlive(char* s)
{
	char buffer[MAXBUF];
	snprintf(buffer,MAXBUF,"%s",s);
	
	log(DEBUG,"NetSendToAllAlive: '%s'",s);

	for (int j = 0; j < 32; j++)
	{
		if (me[j] != NULL)
		{
			for (int k = 0; k < me[j]->connectors.size(); k++)
			{
				if (me[j]->connectors[k].GetState() != STATE_DISCONNECTED)
				{
					me[j]->SendPacket(buffer,me[j]->connectors[k].GetServerName().c_str());
				}
				else
				{
					log(DEBUG,"%s is dead, not sending to it.",me[j]->connectors[k].GetServerName().c_str());
				}
			}
		}
	}
}


void NetSendToOne(char* target,char* s)
{
	char buffer[MAXBUF];
	snprintf(buffer,MAXBUF,"%s",s);
	
	log(DEBUG,"NetSendToOne: '%s' '%s'",target,s);

	for (int j = 0; j < 32; j++)
	{
		if (me[j] != NULL)
		{
			for (int k = 0; k < me[j]->connectors.size(); k++)
			{
				if (!strcasecmp(me[j]->connectors[k].GetServerName().c_str(),target))
				{
					me[j]->SendPacket(buffer,me[j]->connectors[k].GetServerName().c_str());
				}
			}
		}
	}
}

void NetSendToAllExcept(const char* target,char* s)
{
	char buffer[MAXBUF];
	snprintf(buffer,MAXBUF,"%s",s);
	
	log(DEBUG,"NetSendToAllExcept: '%s' '%s'",target,s);
	
	for (int j = 0; j < 32; j++)
	{
		if (me[j] != NULL)
		{
			for (int k = 0; k < me[j]->connectors.size(); k++)
			{
				if (strcasecmp(me[j]->connectors[k].GetServerName().c_str(),target))
				{
					me[j]->SendPacket(buffer,me[j]->connectors[k].GetServerName().c_str());
				}
			}
		}
	}
}


bool hasumode(userrec* user, char mode)
{
	if (user)
	{
		return (strchr(user->modes,mode)>0);
	}
	else return false;
}

void WriteMode(const char* modes, int flags, const char* text, ...)
{
	if ((!text) || (!modes) || (!flags))
	{
		log(DEFAULT,"*** BUG *** WriteMode was given an invalid parameter");
		return;
	}

	char textbuffer[MAXBUF];
	va_list argsPtr;
	va_start (argsPtr, text);
	vsnprintf(textbuffer, MAXBUF, text, argsPtr);
	va_end(argsPtr);

	for (user_hash::const_iterator i = clientlist.begin(); i != clientlist.end(); i++)
	{
		if (i->second)
		{
			bool send_to_user = false;
			
			if (flags == WM_AND)
			{
				send_to_user = true;
				for (int n = 0; n < strlen(modes); n++)
				{
					if (!hasumode(i->second,modes[n]))
					{
						send_to_user = false;
						break;
					}
				}
			}
			else if (flags == WM_OR)
			{
				send_to_user = false;
				for (int n = 0; n < strlen(modes); n++)
				{
					if (hasumode(i->second,modes[n]))
					{
						send_to_user = true;
						break;
					}
				}
			}

			if (send_to_user)
			{
				WriteServ(i->second->fd,"NOTICE %s :%s",i->second->nick,textbuffer);
			}
		}
	}
}

void ChangeName(userrec* user, const char* gecos)
{
	strncpy(user->fullname,gecos,MAXBUF);

	// TODO: replace these with functions:
	// NetSendToAll - to all
	// NetSendToCommon - to all that hold users sharing a common channel with another user
	// NetSendToOne - to one server
	// NetSendToAllExcept - send to all but one
	// all by servername

	char buffer[MAXBUF];
	snprintf(buffer,MAXBUF,"a %s :%s",user->nick,gecos);
	NetSendToAll(buffer);
}

void ChangeDisplayedHost(userrec* user, const char* host)
{
	strncpy(user->dhost,host,160);
	char buffer[MAXBUF];
	snprintf(buffer,MAXBUF,"b %s %s",user->nick,host);
	NetSendToAll(buffer);
}

void WriteWallOps(userrec *source, bool local_only, char* text, ...)  
{  
	if ((!text) || (!source))
	{
		log(DEFAULT,"*** BUG *** WriteOpers was given an invalid parameter");
		return;
	}

        int i = 0;  
        char textbuffer[MAXBUF];  
        va_list argsPtr;  
        va_start (argsPtr, text);  
        vsnprintf(textbuffer, MAXBUF, text, argsPtr);  
        va_end(argsPtr);  
  
  	for (user_hash::const_iterator i = clientlist.begin(); i != clientlist.end(); i++)
        {
        	if (i->second)
        	{
                	if (strchr(i->second->modes,'w'))
                	{
				WriteTo(source,i->second,"WALLOPS %s",textbuffer);
                	}
                }
	}

	if (!local_only)
	{
		char buffer[MAXBUF];
		snprintf(buffer,MAXBUF,"@ %s :%s",source->nick,textbuffer);
		NetSendToAll(buffer);
	}
}  

/* convert a string to lowercase. Note following special circumstances
 * taken from RFC 1459. Many "official" server branches still hold to this
 * rule so i will too;
 *
 *  Because of IRC's scandanavian origin, the characters {}| are
 *  considered to be the lower case equivalents of the characters []\,
 *  respectively. This is a critical issue when determining the
 *  equivalence of two nicknames.
 */

void strlower(char *n)
{
	if (!n)
	{
		return;
	}
	for (int i = 0; i != strlen(n); i++)
	{
		n[i] = tolower(n[i]);
		if (n[i] == '[')
			n[i] = '{';
		if (n[i] == ']')
			n[i] = '}';
		if (n[i] == '\\')
			n[i] = '|';
	}
}

/* verify that a user's ident and nickname is valid */

int isident(const char* n)
{
        char v[MAXBUF];
        if (!n)

        {
                return 0;
        }
        if (!strcmp(n,""))
        {
                return 0;
        }
        for (int i = 0; i != strlen(n); i++)
        {
                if ((n[i] < 33) || (n[i] > 125))
                {
                        return 0;
                }
                /* can't occur ANYWHERE in an Ident! */
                if (strchr("<>,./?:;@'~#=+()*&%$£ \"!",n[i]))
                {
                        return 0;
                }
        }
        return 1;
}


int isnick(const char* n)
{
	int i = 0;
	char v[MAXBUF];
	if (!n)
	{
		return 0;
	}
	if (!strcmp(n,""))
	{
		return 0;
	}
	if (strlen(n) > NICKMAX-1)
	{
		return 0;
	}
	for (int i = 0; i != strlen(n); i++)
	{
		if ((n[i] < 33) || (n[i] > 125))
		{
			return 0;
		}
		/* can't occur ANYWHERE in a nickname! */
		if (strchr("<>,./?:;@'~#=+()*&%$£ \"!",n[i]))
		{
			return 0;
		}
		/* can't occur as the first char of a nickname... */
		if ((strchr("0123456789",n[i])) && (!i))
		{
			return 0;
		}
	}
	return 1;
}

/* Find a user record by nickname and return a pointer to it */

userrec* Find(string nick)
{
	user_hash::iterator iter = clientlist.find(nick);

	if (iter == clientlist.end())
		/* Couldn't find it */
		return NULL;

	return iter->second;
}

void update_stats_l(int fd,int data_out) /* add one line-out to stats L for this fd */
{
	for (user_hash::const_iterator i = clientlist.begin(); i != clientlist.end(); i++)
	{
		if (i->second)
		{
			if (i->second->fd == fd)
			{
				i->second->bytes_out+=data_out;
				i->second->cmds_out++;
			}
		}
	}
}


/* find a channel record by channel name and return a pointer to it */

chanrec* FindChan(const char* chan)
{
	if (!chan)
	{
		log(DEFAULT,"*** BUG *** Findchan was given an invalid parameter");
		return NULL;
	}

	chan_hash::iterator iter = chanlist.find(chan);

	if (iter == chanlist.end())
		/* Couldn't find it */
		return NULL;

	return iter->second;
}


void purge_empty_chans(void)
{
	int go_again = 1, purge = 0;
	
	while (go_again)
	{
		go_again = 0;
		for (chan_hash::iterator i = chanlist.begin(); i != chanlist.end(); i++)
		{
			if (i->second) {
				if (!usercount(i->second))
				{
					/* kill the record */
					if (i != chanlist.end())
					{
						log(DEBUG,"del_channel: destroyed: %s",i->second->name);
						delete i->second;
						chanlist.erase(i);
						go_again = 1;
						purge++;
						break;
					}
				}
				else
				{
					log(DEBUG,"skipped purge for %s",i->second->name);
				}
			}
		}
	}
	log(DEBUG,"completed channel purge, killed %d",purge);
}

/* returns the status character for a given user on a channel, e.g. @ for op,
 * % for halfop etc. If the user has several modes set, the highest mode
 * the user has must be returned. */

char* cmode(userrec *user, chanrec *chan)
{
	if ((!user) || (!chan))
	{
		log(DEFAULT,"*** BUG *** cmode was given an invalid parameter");
		return "";
	}

	int i;
	for (int i = 0; i != MAXCHANS; i++)
	{
		if ((user->chans[i].channel == chan) && (chan != NULL))
		{
			if ((user->chans[i].uc_modes & UCMODE_OP) > 0)
			{
				return "@";
			}
			if ((user->chans[i].uc_modes & UCMODE_HOP) > 0)
			{
				return "%";
			}
			if ((user->chans[i].uc_modes & UCMODE_VOICE) > 0)
			{
				return "+";
			}
			return "";
		}
	}
}

char scratch[MAXBUF];
char sparam[MAXBUF];

char* chanmodes(chanrec *chan)
{
	if (!chan)
	{
		log(DEFAULT,"*** BUG *** chanmodes was given an invalid parameter");
		strcpy(scratch,"");
		return scratch;
	}

	strcpy(scratch,"");
	strcpy(sparam,"");
	if (chan->noexternal)
	{
		strncat(scratch,"n",MAXMODES);
	}
	if (chan->topiclock)
	{
		strncat(scratch,"t",MAXMODES);
	}
	if (strcmp(chan->key,""))
	{
		strncat(scratch,"k",MAXMODES);
	}
	if (chan->limit)
	{
		strncat(scratch,"l",MAXMODES);
	}
	if (chan->inviteonly)
	{
		strncat(scratch,"i",MAXMODES);
	}
	if (chan->moderated)
	{
		strncat(scratch,"m",MAXMODES);
	}
	if (chan->secret)
	{
		strncat(scratch,"s",MAXMODES);
	}
	if (chan->c_private)
	{
		strncat(scratch,"p",MAXMODES);
	}
	if (strcmp(chan->key,""))
	{
		strncat(sparam,chan->key,MAXBUF);
	}
	if (chan->limit)
	{
		char foo[24];
		sprintf(foo," %d",chan->limit);
		strncat(sparam,foo,MAXBUF);
	}
	if (strlen(chan->custom_modes))
	{
		strncat(scratch,chan->custom_modes,MAXMODES);
		for (int z = 0; z < strlen(chan->custom_modes); z++)
		{
			std::string extparam = chan->GetModeParameter(chan->custom_modes[z]);
			if (extparam != "")
			{
				strncat(sparam," ",MAXBUF);
				strncat(sparam,extparam.c_str(),MAXBUF);
			}
		}
	}
	log(DEBUG,"chanmodes: %s %s%s",chan->name,scratch,sparam);
	strncat(scratch,sparam,MAXMODES);
	return scratch;
}

/* returns the status value for a given user on a channel, e.g. STATUS_OP for
 * op, STATUS_VOICE for voice etc. If the user has several modes set, the
 * highest mode the user has must be returned. */

int cstatus(userrec *user, chanrec *chan)
{
	if ((!chan) || (!user))
	{
		log(DEFAULT,"*** BUG *** cstatus was given an invalid parameter");
		return 0;
	}

	for (int i = 0; i != MAXCHANS; i++)
	{
		if ((user->chans[i].channel == chan) && (chan != NULL))
		{
			if ((user->chans[i].uc_modes & UCMODE_OP) > 0)
			{
				return STATUS_OP;
			}
			if ((user->chans[i].uc_modes & UCMODE_HOP) > 0)
			{
				return STATUS_HOP;
			}
			if ((user->chans[i].uc_modes & UCMODE_VOICE) > 0)
			{
				return STATUS_VOICE;
			}
			return STATUS_NORMAL;
		}
	}
}


/* compile a userlist of a channel into a string, each nick seperated by
 * spaces and op, voice etc status shown as @ and + */

void userlist(userrec *user,chanrec *c)
{
	if ((!c) || (!user))
	{
		log(DEFAULT,"*** BUG *** userlist was given an invalid parameter");
		return;
	}

	sprintf(list,"353 %s = %s :", user->nick, c->name);
  	for (user_hash::const_iterator i = clientlist.begin(); i != clientlist.end(); i++)
	{
		if (has_channel(i->second,c))
		{
			if (isnick(i->second->nick))
			{
				if ((!has_channel(i->second,c)) && (strchr(i->second->modes,'i')))
				{
					/* user is +i, and source not on the channel, does not show
					 * nick in NAMES list */
					continue;
				}
				strcat(list,cmode(i->second,c));
				strcat(list,i->second->nick);
				strcat(list," ");
				if (strlen(list)>(480-NICKMAX))
				{
					/* list overflowed into
					 * multiple numerics */
					WriteServ(user->fd,list);
					sprintf(list,"353 %s = %s :", user->nick, c->name);
				}
			}
		}
	}
	/* if whats left in the list isnt empty, send it */
	if (list[strlen(list)-1] != ':')
	{
		WriteServ(user->fd,list);
	}
}

/* return a count of the users on a specific channel accounting for
 * invisible users who won't increase the count. e.g. for /LIST */

int usercount_i(chanrec *c)
{
	int i = 0;
	int count = 0;
	
	if (!c)
	{
		log(DEFAULT,"*** BUG *** usercount_i was given an invalid parameter");
		return 0;
	}

	strcpy(list,"");
  	for (user_hash::const_iterator i = clientlist.begin(); i != clientlist.end(); i++)
	{
		if (i->second)
		{
			if (has_channel(i->second,c))
			{
				if (isnick(i->second->nick))
				{
					if ((!has_channel(i->second,c)) && (strchr(i->second->modes,'i')))
					{
						/* user is +i, and source not on the channel, does not show
						 * nick in NAMES list */
						continue;
					}
					count++;
				}
			}
		}
	}
	log(DEBUG,"usercount_i: %s %d",c->name,count);
	return count;
}


int usercount(chanrec *c)
{
	int i = 0;
	int count = 0;
	
	if (!c)
	{
		log(DEFAULT,"*** BUG *** usercount was given an invalid parameter");
		return 0;
	}

	strcpy(list,"");
  	for (user_hash::const_iterator i = clientlist.begin(); i != clientlist.end(); i++)
	{
		if (i->second)
		{
			if (has_channel(i->second,c))
			{
				if ((isnick(i->second->nick)) && (i->second->registered == 7))
				{
					count++;
				}
			}
		}
	}
	log(DEBUG,"usercount: %s %d",c->name,count);
	return count;
}


/* add a channel to a user, creating the record for it if needed and linking
 * it to the user record */

chanrec* add_channel(userrec *user, const char* cn, const char* key, bool override)
{
	if ((!user) || (!cn))
	{
		log(DEFAULT,"*** BUG *** add_channel was given an invalid parameter");
		return 0;
	}

	int i = 0;
	chanrec* Ptr;
	int created = 0;
	char cname[MAXBUF];

	strncpy(cname,cn,MAXBUF);
	
	// we MUST declare this wherever we use FOREACH_RESULT
	int MOD_RESULT = 0;

	if (strlen(cname) > CHANMAX-1)
	{
		cname[CHANMAX-1] = '\0';
	}

	log(DEBUG,"add_channel: %s %s",user->nick,cname);
	
	if ((FindChan(cname)) && (has_channel(user,FindChan(cname))))
	{
		return NULL; // already on the channel!
	}


	if (!FindChan(cname))
	{
		FOREACH_RESULT(OnUserPreJoin(user,NULL,cname));
		if (MOD_RESULT) {
			return NULL;
		}

		/* create a new one */
		log(DEBUG,"add_channel: creating: %s",cname);
		{
			chanlist[cname] = new chanrec();

			strcpy(chanlist[cname]->name, cname);
			chanlist[cname]->topiclock = 1;
			chanlist[cname]->noexternal = 1;
			chanlist[cname]->created = time(NULL);
			strcpy(chanlist[cname]->topic, "");
			strncpy(chanlist[cname]->setby, user->nick,NICKMAX);
			chanlist[cname]->topicset = 0;
			Ptr = chanlist[cname];
			log(DEBUG,"add_channel: created: %s",cname);
			/* set created to 2 to indicate user
			 * is the first in the channel
			 * and should be given ops */
			created = 2;
		}
	}
	else
	{
		/* channel exists, just fish out a pointer to its struct */
		Ptr = FindChan(cname);
		if (Ptr)
		{
			log(DEBUG,"add_channel: joining to: %s",Ptr->name);
			
			// the override flag allows us to bypass channel modes
			// and bans (used by servers)
			if (!override)
			{
				FOREACH_RESULT(OnUserPreJoin(user,Ptr,cname));
				if (MOD_RESULT) {
					return NULL;
				}
				
				if (strcmp(Ptr->key,""))
				{
					log(DEBUG,"add_channel: %s has key %s",Ptr->name,Ptr->key);
					if (!key)
					{
						log(DEBUG,"add_channel: no key given in JOIN");
						WriteServ(user->fd,"475 %s %s :Cannot join channel (Requires key)",user->nick, Ptr->name);
						return NULL;
					}
					else
					{
						log(DEBUG,"key at %p is %s",key,key);
						if (strcasecmp(key,Ptr->key))
						{
							log(DEBUG,"add_channel: bad key given in JOIN");
							WriteServ(user->fd,"475 %s %s :Cannot join channel (Incorrect key)",user->nick, Ptr->name);
							return NULL;
						}
					}
				}
				log(DEBUG,"add_channel: no key");
	
				if (Ptr->inviteonly)
				{
					log(DEBUG,"add_channel: channel is +i");
					if (user->IsInvited(Ptr->name))
					{
						/* user was invited to channel */
						/* there may be an optional channel NOTICE here */
					}
					else
					{
						WriteServ(user->fd,"473 %s %s :Cannot join channel (Invite only)",user->nick, Ptr->name);
						return NULL;
					}
				}
				log(DEBUG,"add_channel: channel is not +i");
	
				if (Ptr->limit)
				{
					if (usercount(Ptr) == Ptr->limit)
					{
						WriteServ(user->fd,"471 %s %s :Cannot join channel (Channel is full)",user->nick, Ptr->name);
						return NULL;
					}
				}
				
				log(DEBUG,"add_channel: about to walk banlist");
	
				/* check user against the channel banlist */
				if (Ptr)
				{
					if (Ptr->bans.size())
					{
						for (BanList::iterator i = Ptr->bans.begin(); i != Ptr->bans.end(); i++)
						{
							if (match(user->GetFullHost(),i->data))
							{
								WriteServ(user->fd,"474 %s %s :Cannot join channel (You're banned)",user->nick, Ptr->name);
								return NULL;
							}
						}
					}
				}
				
				log(DEBUG,"add_channel: bans checked");
				

				if ((Ptr) && (user))
				{
					user->RemoveInvite(Ptr->name);
				}
	
				log(DEBUG,"add_channel: invites removed");

			}
			else
			{
				log(DEBUG,"Overridden checks");
			}

			
		}
		created = 1;
	}

	log(DEBUG,"Passed channel checks");
	
	for (int i =0; i != MAXCHANS; i++)
	{
		log(DEBUG,"Check location %d",i);
		if (user->chans[i].channel == NULL)
		{
			log(DEBUG,"Adding into their channel list at location %d",i);

			if (created == 2) 
			{
				/* first user in is given ops */
				user->chans[i].uc_modes = UCMODE_OP;
			}
			else
			{
				user->chans[i].uc_modes = 0;
			}
			user->chans[i].channel = Ptr;
			WriteChannel(Ptr,user,"JOIN :%s",Ptr->name);
			
			if (!override) // we're not overriding... so this isnt part of a netburst, broadcast it.
			{
				// use the stamdard J token with no privilages.
				char buffer[MAXBUF];
				snprintf(buffer,MAXBUF,"J %s :%s",user->nick,Ptr->name);
				NetSendToAll(buffer);
			}

			log(DEBUG,"Sent JOIN to client");

			if (Ptr->topicset)
			{
				WriteServ(user->fd,"332 %s %s :%s", user->nick, Ptr->name, Ptr->topic);
				WriteServ(user->fd,"333 %s %s %s %d", user->nick, Ptr->name, Ptr->setby, Ptr->topicset);
			}
			userlist(user,Ptr);
			WriteServ(user->fd,"366 %s %s :End of /NAMES list.", user->nick, Ptr->name);
			WriteServ(user->fd,"324 %s %s +%s",user->nick, Ptr->name,chanmodes(Ptr));
			WriteServ(user->fd,"329 %s %s %d", user->nick, Ptr->name, Ptr->created);
			FOREACH_MOD OnUserJoin(user,Ptr);
			return Ptr;
		}
	}
	log(DEBUG,"add_channel: user channel max exceeded: %s %s",user->nick,cname);
	WriteServ(user->fd,"405 %s %s :You are on too many channels",user->nick, cname);
	return NULL;
}

/* remove a channel from a users record, and remove the record from memory
 * if the channel has become empty */

chanrec* del_channel(userrec *user, const char* cname, const char* reason, bool local)
{
	if ((!user) || (!cname))
	{
		log(DEFAULT,"*** BUG *** del_channel was given an invalid parameter");
		return NULL;
	}

	chanrec* Ptr;
	int created = 0;

	if ((!cname) || (!user))
	{
		return NULL;
	}

	Ptr = FindChan(cname);
	
	if (!Ptr)
	{
		return NULL;
	}

	FOREACH_MOD OnUserPart(user,Ptr);
	log(DEBUG,"del_channel: removing: %s %s",user->nick,Ptr->name);
	
	for (int i =0; i != MAXCHANS; i++)
	{
		/* zap it from the channel list of the user */
		if (user->chans[i].channel == Ptr)
		{
			if (reason)
			{
				WriteChannel(Ptr,user,"PART %s :%s",Ptr->name, reason);

				if (!local)
				{
					char buffer[MAXBUF];
					snprintf(buffer,MAXBUF,"L %s %s :%s",user->nick,Ptr->name,reason);
					NetSendToAll(buffer);
				}

				
			}
			else
			{
				if (!local)
				{
					char buffer[MAXBUF];
					snprintf(buffer,MAXBUF,"L %s %s :",user->nick,Ptr->name);
					NetSendToAll(buffer);
				}
			
				WriteChannel(Ptr,user,"PART :%s",Ptr->name);
			}
			user->chans[i].uc_modes = 0;
			user->chans[i].channel = NULL;
			log(DEBUG,"del_channel: unlinked: %s %s",user->nick,Ptr->name);
			break;
		}
	}
	
	/* if there are no users left on the channel */
	if (!usercount(Ptr))
	{
		chan_hash::iterator iter = chanlist.find(Ptr->name);

		log(DEBUG,"del_channel: destroying channel: %s",Ptr->name);

		/* kill the record */
		if (iter != chanlist.end())
		{
			log(DEBUG,"del_channel: destroyed: %s",Ptr->name);
			delete iter->second;
			chanlist.erase(iter);
		}
	}
}


void kick_channel(userrec *src,userrec *user, chanrec *Ptr, char* reason)
{
	if ((!src) || (!user) || (!Ptr) || (!reason))
	{
		log(DEFAULT,"*** BUG *** kick_channel was given an invalid parameter");
		return;
	}

	int i = 0;
	int created = 0;

	if ((!Ptr) || (!user) || (!src))
	{
		return;
	}

	log(DEBUG,"kick_channel: removing: %s %s %s",user->nick,Ptr->name,src->nick);

	if (!has_channel(user,Ptr))
	{
		WriteServ(src->fd,"441 %s %s %s :They are not on that channel",src->nick, user->nick, Ptr->name);
		return;
	}
	if ((cstatus(src,Ptr) < STATUS_HOP) || (cstatus(src,Ptr) < cstatus(user,Ptr)))
	{
		if (cstatus(src,Ptr) == STATUS_HOP)
		{
			WriteServ(src->fd,"482 %s %s :You must be a channel operator",src->nick, Ptr->name);
		}
		else
		{
			WriteServ(src->fd,"482 %s %s :You must be at least a half-operator to change modes on this channel",src->nick, Ptr->name);
		}
		
		return;
	}
	
	for (int i =0; i != MAXCHANS; i++)
	{
		/* zap it from the channel list of the user */
		if (user->chans[i].channel)
		if (!strcasecmp(user->chans[i].channel->name,Ptr->name))
		{
			WriteChannel(Ptr,src,"KICK %s %s :%s",Ptr->name, user->nick, reason);
			user->chans[i].uc_modes = 0;
			user->chans[i].channel = NULL;
			log(DEBUG,"del_channel: unlinked: %s %s",user->nick,Ptr->name);
			break;
		}
	}
	
	/* if there are no users left on the channel */
	if (!usercount(Ptr))
	{
		chan_hash::iterator iter = chanlist.find(Ptr->name);

		log(DEBUG,"del_channel: destroying channel: %s",Ptr->name);

		/* kill the record */
		if (iter != chanlist.end())
		{
			log(DEBUG,"del_channel: destroyed: %s",Ptr->name);
			delete iter->second;
			chanlist.erase(iter);
		}
	}
}


/* returns 1 if user u has channel c in their record, 0 if not */

int has_channel(userrec *u, chanrec *c)
{
	if ((!u) || (!c))
	{
		log(DEFAULT,"*** BUG *** has_channel was given an invalid parameter");
		return 0;
	}
	for (int i =0; i != MAXCHANS; i++)
	{
		if (u->chans[i].channel == c)
		{
			return 1;
		}
	}
	return 0;
}

int give_ops(userrec *user,char *dest,chanrec *chan,int status)
{
	userrec *d;
	int i;
	
	if ((!user) || (!dest) || (!chan))
	{
		log(DEFAULT,"*** BUG *** give_ops was given an invalid parameter");
		return 0;
	}
	if (status < STATUS_OP)
	{
		log(DEBUG,"%s cant give ops to %s because they nave status %d and needs %d",user->nick,dest,status,STATUS_OP);
		WriteServ(user->fd,"482 %s %s :You're not a channel operator",user->nick, chan->name);
		return 0;
	}
	else
	{
		if (!isnick(dest))
		{
			log(DEFAULT,"the target nickname given to give_ops was invalid");
			WriteServ(user->fd,"401 %s %s :No suck nick/channel",user->nick, dest);
			return 0;
		}
		d = Find(dest);
		if (!d)
		{
			log(DEFAULT,"the target nickname given to give_ops couldnt be found");
			WriteServ(user->fd,"401 %s %s :No suck nick/channel",user->nick, dest);
			return 0;
		}
		else
		{
			for (int i = 0; i != MAXCHANS; i++)
			{
				if ((d->chans[i].channel != NULL) && (chan != NULL))
				if (!strcasecmp(d->chans[i].channel->name,chan->name))
				{
					if (d->chans[i].uc_modes & UCMODE_OP)
					{
						/* mode already set on user, dont allow multiple */
						log(DEFAULT,"The target user given to give_ops was already opped on the channel");
						return 0;
					}
					d->chans[i].uc_modes = d->chans[i].uc_modes | UCMODE_OP;
					log(DEBUG,"gave ops: %s %s",d->chans[i].channel->name,d->nick);
					return 1;
				}
			}
			log(DEFAULT,"The target channel given to give_ops was not in the users mode list");
		}
	}
	return 1;
}

int give_hops(userrec *user,char *dest,chanrec *chan,int status)
{
	userrec *d;
	int i;
	
	if ((!user) || (!dest) || (!chan))
	{
		log(DEFAULT,"*** BUG *** give_hops was given an invalid parameter");
		return 0;
	}
	if (status != STATUS_OP)
	{
		WriteServ(user->fd,"482 %s %s :You're not a channel operator",user->nick, chan->name);
		return 0;
	}
	else
	{
		d = Find(dest);
		if (!isnick(dest))
		{
			WriteServ(user->fd,"401 %s %s :No suck nick/channel",user->nick, dest);
			return 0;
		}
		if (!d)
		{
			WriteServ(user->fd,"401 %s %s :No suck nick/channel",user->nick, dest);
			return 0;
		}
		else
		{
			for (int i = 0; i != MAXCHANS; i++)
			{
				if ((d->chans[i].channel != NULL) && (chan != NULL))
				if (!strcasecmp(d->chans[i].channel->name,chan->name))
				{
					if (d->chans[i].uc_modes & UCMODE_HOP)
					{
						/* mode already set on user, dont allow multiple */
						return 0;
					}
					d->chans[i].uc_modes = d->chans[i].uc_modes | UCMODE_HOP;
					log(DEBUG,"gave h-ops: %s %s",d->chans[i].channel->name,d->nick);
					return 1;
				}
			}
		}
	}
	return 1;
}

int give_voice(userrec *user,char *dest,chanrec *chan,int status)
{
	userrec *d;
	int i;
	
	if ((!user) || (!dest) || (!chan))
	{
		log(DEFAULT,"*** BUG *** give_voice was given an invalid parameter");
		return 0;
	}
	if (status < STATUS_HOP)
	{
		WriteServ(user->fd,"482 %s %s :You must be at least a half-operator to change modes on this channel",user->nick, chan->name);
		return 0;
	}
	else
	{
		d = Find(dest);
		if (!isnick(dest))
		{
			WriteServ(user->fd,"401 %s %s :No suck nick/channel",user->nick, dest);
			return 0;
		}
		if (!d)
		{
			WriteServ(user->fd,"401 %s %s :No suck nick/channel",user->nick, dest);
			return 0;
		}
		else
		{
			for (int i = 0; i != MAXCHANS; i++)
			{
				if ((d->chans[i].channel != NULL) && (chan != NULL))
				if (!strcasecmp(d->chans[i].channel->name,chan->name))
				{
					if (d->chans[i].uc_modes & UCMODE_VOICE)
					{
						/* mode already set on user, dont allow multiple */
						return 0;
					}
					d->chans[i].uc_modes = d->chans[i].uc_modes | UCMODE_VOICE;
					log(DEBUG,"gave voice: %s %s",d->chans[i].channel->name,d->nick);
					return 1;
				}
			}
		}
	}
	return 1;
}

int take_ops(userrec *user,char *dest,chanrec *chan,int status)
{
	userrec *d;
	int i;
	
	if ((!user) || (!dest) || (!chan))
	{
		log(DEFAULT,"*** BUG *** take_ops was given an invalid parameter");
		return 0;
	}
	if (status < STATUS_OP)
	{
		log(DEBUG,"%s cant give ops to %s because they have status %d and needs %d",user->nick,dest,status,STATUS_OP);
		WriteServ(user->fd,"482 %s %s :You're not a channel operator",user->nick, chan->name);
		return 0;
	}
	else
	{
		d = Find(dest);
		if (!isnick(dest))
		{
			log(DEBUG,"take_ops was given an invalid target nickname of %s",dest);
			WriteServ(user->fd,"401 %s %s :No suck nick/channel",user->nick, dest);
			return 0;
		}
		if (!d)
		{
			log(DEBUG,"take_ops couldnt resolve the target nickname: %s",dest);
			WriteServ(user->fd,"401 %s %s :No suck nick/channel",user->nick, dest);
			return 0;
		}
		else
		{
			for (int i = 0; i != MAXCHANS; i++)
			{
				if ((d->chans[i].channel != NULL) && (chan != NULL))
				if (!strcasecmp(d->chans[i].channel->name,chan->name))
				{
					if ((d->chans[i].uc_modes & UCMODE_OP) == 0)
					{
						/* mode already set on user, dont allow multiple */
						return 0;
					}
					d->chans[i].uc_modes ^= UCMODE_OP;
					log(DEBUG,"took ops: %s %s",d->chans[i].channel->name,d->nick);
					return 1;
				}
			}
			log(DEBUG,"take_ops couldnt locate the target channel in the target users list");
		}
	}
	return 1;
}

int take_hops(userrec *user,char *dest,chanrec *chan,int status)
{
	userrec *d;
	int i;
	
	if ((!user) || (!dest) || (!chan))
	{
		log(DEFAULT,"*** BUG *** take_hops was given an invalid parameter");
		return 0;
	}
	if (status != STATUS_OP)
	{
		WriteServ(user->fd,"482 %s %s :You're not a channel operator",user->nick, chan->name);
		return 0;
	}
	else
	{
		d = Find(dest);
		if (!isnick(dest))
		{
			WriteServ(user->fd,"401 %s %s :No suck nick/channel",user->nick, dest);
			return 0;
		}
		if (!d)
		{
			WriteServ(user->fd,"401 %s %s :No suck nick/channel",user->nick, dest);
			return 0;
		}
		else
		{
			for (int i = 0; i != MAXCHANS; i++)
			{
				if ((d->chans[i].channel != NULL) && (chan != NULL))
				if (!strcasecmp(d->chans[i].channel->name,chan->name))
				{
					if ((d->chans[i].uc_modes & UCMODE_HOP) == 0)
					{
						/* mode already set on user, dont allow multiple */
						return 0;
					}
					d->chans[i].uc_modes ^= UCMODE_HOP;
					log(DEBUG,"took h-ops: %s %s",d->chans[i].channel->name,d->nick);
					return 1;
				}
			}
		}
	}
	return 1;
}

int take_voice(userrec *user,char *dest,chanrec *chan,int status)
{
	userrec *d;
	int i;
	
	if ((!user) || (!dest) || (!chan))
	{
		log(DEFAULT,"*** BUG *** take_voice was given an invalid parameter");
		return 0;
	}
	if (status < STATUS_HOP)
	{
		WriteServ(user->fd,"482 %s %s :You must be at least a half-operator to change modes on this channel",user->nick, chan->name);
		return 0;
	}
	else
	{
		d = Find(dest);
		if (!isnick(dest))
		{
			WriteServ(user->fd,"401 %s %s :No suck nick/channel",user->nick, dest);
			return 0;
		}
		if (!d)
		{
			WriteServ(user->fd,"401 %s %s :No suck nick/channel",user->nick, dest);
			return 0;
		}
		else
		{
			for (int i = 0; i != MAXCHANS; i++)
			{
				if ((d->chans[i].channel != NULL) && (chan != NULL))
				if (!strcasecmp(d->chans[i].channel->name,chan->name))
				{
					if ((d->chans[i].uc_modes & UCMODE_VOICE) == 0)
					{
						/* mode already set on user, dont allow multiple */
						return 0;
					}
					d->chans[i].uc_modes ^= UCMODE_VOICE;
					log(DEBUG,"took voice: %s %s",d->chans[i].channel->name,d->nick);
					return 1;
				}
			}
		}
	}
	return 1;
}

void TidyBan(char *ban)
{
	if (!ban) {
		log(DEFAULT,"*** BUG *** TidyBan was given an invalid parameter");
		return;
	}
	
	char temp[MAXBUF],NICK[MAXBUF],IDENT[MAXBUF],HOST[MAXBUF];

	strcpy(temp,ban);

	char* pos_of_pling = strchr(temp,'!');
	char* pos_of_at = strchr(temp,'@');

	pos_of_pling[0] = '\0';
	pos_of_at[0] = '\0';
	pos_of_pling++;
	pos_of_at++;

	strncpy(NICK,temp,NICKMAX);
	strncpy(IDENT,pos_of_pling,IDENTMAX+1);
	strncpy(HOST,pos_of_at,160);

	sprintf(ban,"%s!%s@%s",NICK,IDENT,HOST);
}

int add_ban(userrec *user,char *dest,chanrec *chan,int status)
{
	if ((!user) || (!dest) || (!chan)) {
		log(DEFAULT,"*** BUG *** add_ban was given an invalid parameter");
		return 0;
	}

	BanItem b;
	if ((!user) || (!dest) || (!chan))
		return 0;
	if (strchr(dest,'!')==0)
		return 0;
	if (strchr(dest,'@')==0)
		return 0;
	for (int i = 0; i < strlen(dest); i++)
		if (dest[i] < 32)
			return 0;
	for (int i = 0; i < strlen(dest); i++)
		if (dest[i] > 126)
			return 0;
	int c = 0;
	for (int i = 0; i < strlen(dest); i++)
		if (dest[i] == '!')
			c++;
	if (c>1)
		return 0;
	c = 0;
	for (int i = 0; i < strlen(dest); i++)
		if (dest[i] == '@')
			c++;
	if (c>1)
		return 0;
	log(DEBUG,"add_ban: %s %s",chan->name,user->nick);

	TidyBan(dest);
	for (BanList::iterator i = chan->bans.begin(); i != chan->bans.end(); i++)
	{
		if (!strcasecmp(i->data,dest))
		{
			// dont allow a user to set the same ban twice
			return 0;
		}
	}

	b.set_time = time(NULL);
	strncpy(b.data,dest,MAXBUF);
	strncpy(b.set_by,user->nick,NICKMAX);
	chan->bans.push_back(b);
	return 1;
}

int take_ban(userrec *user,char *dest,chanrec *chan,int status)
{
	if ((!user) || (!dest) || (!chan)) {
		log(DEFAULT,"*** BUG *** take_ban was given an invalid parameter");
		return 0;
	}

	log(DEBUG,"del_ban: %s %s",chan->name,user->nick);
	for (BanList::iterator i = chan->bans.begin(); i != chan->bans.end(); i++)
	{
		if (!strcasecmp(i->data,dest))
		{
			chan->bans.erase(i);
			return 1;
		}
	}
	return 0;
}

void process_modes(char **parameters,userrec* user,chanrec *chan,int status, int pcnt, bool servermode, bool silent, bool local)
{
	if (!parameters) {
		log(DEFAULT,"*** BUG *** process_modes was given an invalid parameter");
		return;
	}

	char modelist[MAXBUF];
	char outlist[MAXBUF];
	char outstr[MAXBUF];
	char outpars[32][MAXBUF];
	int param = 2;
	int pc = 0;
	int ptr = 0;
	int mdir = 1;
	int r = 0;
	bool k_set = false, l_set = false;

	if (pcnt < 2)
	{
		return;
	}

	log(DEBUG,"process_modes: start: parameters=%d",pcnt);

	strcpy(modelist,parameters[1]); /* mode list, e.g. +oo-o */
					/* parameters[2] onwards are parameters for
					 * modes that require them :) */
	strcpy(outlist,"+");
	mdir = 1;

	log(DEBUG,"process_modes: modelist: %s",modelist);

	for (ptr = 0; ptr < strlen(modelist); ptr++)
	{
		r = 0;

		{
			log(DEBUG,"process_modes: modechar: %c",modelist[ptr]);
			char modechar = modelist[ptr];
			switch (modelist[ptr])
			{
				case '-':
					if (mdir != 0)
					{
						if ((outlist[strlen(outlist)-1] == '+') || (outlist[strlen(outlist)-1] == '-'))
						{
							outlist[strlen(outlist)-1] = '-';
						}
						else
						{
							strcat(outlist,"-");
						}
					}
					mdir = 0;
					
				break;			

				case '+':
					if (mdir != 1)
					{
						if ((outlist[strlen(outlist)-1] == '+') || (outlist[strlen(outlist)-1] == '-'))
						{
							outlist[strlen(outlist)-1] = '+';
						}
						else
						{
							strcat(outlist,"+");
						}
					}
					mdir = 1;
				break;

				case 'o':
					log(DEBUG,"Ops");
					if ((param >= pcnt)) break;
					log(DEBUG,"Enough parameters left");
					if (mdir == 1)
					{
						log(DEBUG,"calling give_ops");
						r = give_ops(user,parameters[param++],chan,status);
					}
					else
					{
						log(DEBUG,"calling take_ops");
						r = take_ops(user,parameters[param++],chan,status);
					}
					if (r)
					{
						strcat(outlist,"o");
						strcpy(outpars[pc++],parameters[param-1]);
					}
				break;
			
				case 'h':
					if ((param >= pcnt)) break;
					if (mdir == 1)
					{
						r = give_hops(user,parameters[param++],chan,status);
					}
					else
					{
						r = take_hops(user,parameters[param++],chan,status);
					}
					if (r)
					{
						strcat(outlist,"h");
						strcpy(outpars[pc++],parameters[param-1]);
					}
				break;
			
				
				case 'v':
					if ((param >= pcnt)) break;
					if (mdir == 1)
					{
						r = give_voice(user,parameters[param++],chan,status);
					}
					else
					{
						r = take_voice(user,parameters[param++],chan,status);
					}
					if (r)
					{
						strcat(outlist,"v");
						strcpy(outpars[pc++],parameters[param-1]);
					}
				break;
				
				case 'b':
					if ((param >= pcnt)) break;
					if (mdir == 1)
					{
						r = add_ban(user,parameters[param++],chan,status);
					}
					else
					{
						r = take_ban(user,parameters[param++],chan,status);
					}
					if (r)
					{
						strcat(outlist,"b");
						strcpy(outpars[pc++],parameters[param-1]);
					}
				break;


				case 'k':
					if ((param >= pcnt))
						break;

					if (mdir == 1)
					{
						if (k_set)
							break;
						
						if (!strcmp(chan->key,""))
						{
							strcat(outlist,"k");
							char key[MAXBUF];
							strcpy(key,parameters[param++]);
							if (strlen(key)>32) {
								key[31] = '\0';
							}
							strcpy(outpars[pc++],key);
							strcpy(chan->key,key);
							k_set = true;
						}
					}
					else
					{
						/* checks on -k are case sensitive and only accurate to the
  						   first 32 characters */
						char key[MAXBUF];
						strcpy(key,parameters[param++]);
						if (strlen(key)>32) {
							key[31] = '\0';
						}
						/* only allow -k if correct key given */
						if (!strcmp(chan->key,key))
						{
							strcat(outlist,"k");
							strcpy(chan->key,"");
							strcpy(outpars[pc++],key);
						}
					}
				break;
				
				case 'l':
					if (mdir == 0)
					{
						if (chan->limit)
						{
							strcat(outlist,"l");
							chan->limit = 0;
						}
					}
					
					if ((param >= pcnt)) break;
					if (mdir == 1)
					{
						if (l_set)
							break;
						
						bool invalid = false;
						for (int i = 0; i < strlen(parameters[param]); i++)
						{
							if ((parameters[param][i] < '0') || (parameters[param][i] > '9'))
							{
								invalid = true;
							}
						}
						if (atoi(parameters[param]) < 1)
						{
							invalid = true;
						}

						if (invalid)
							break;
						
						chan->limit = atoi(parameters[param]);
						if (chan->limit)
						{
							strcat(outlist,"l");
							strcpy(outpars[pc++],parameters[param++]);
							l_set = true;
						}
					}
				break;
				
				case 'i':
					if (chan->inviteonly != mdir)
					{
						strcat(outlist,"i");
					}
					chan->inviteonly = mdir;
				break;
				
				case 't':
					if (chan->topiclock != mdir)
					{
						strcat(outlist,"t");
					}
					chan->topiclock = mdir;
				break;
				
				case 'n':
					if (chan->noexternal != mdir)
					{
						strcat(outlist,"n");
					}
					chan->noexternal = mdir;
				break;
				
				case 'm':
					if (chan->moderated != mdir)
					{
						strcat(outlist,"m");
					}
					chan->moderated = mdir;
				break;
				
				case 's':
					if (chan->secret != mdir)
					{
						strcat(outlist,"s");
						if (chan->c_private)
						{
							chan->c_private = 0;
							if (mdir)
							{
								strcat(outlist,"-p+");
							}
							else
							{
								strcat(outlist,"+p-");
							}
						}
					}
					chan->secret = mdir;
				break;
				
				case 'p':
					if (chan->c_private != mdir)
					{
						strcat(outlist,"p");
						if (chan->secret)
						{
							chan->secret = 0;
							if (mdir)
							{
								strcat(outlist,"-s+");
							}
							else
							{
								strcat(outlist,"+s-");
							}
						}
					}
					chan->c_private = mdir;
				break;
				
				default:
					log(DEBUG,"Preprocessing custom mode %c",modechar);
					string_list p;
					p.clear();
					if (((!strchr(chan->custom_modes,modechar)) && (!mdir)) || ((strchr(chan->custom_modes,modechar)) && (mdir)))
					{
						log(DEBUG,"Mode %c isnt set on %s but trying to remove!",modechar,chan->name);
						break;
					}
					if (ModeDefined(modechar,MT_CHANNEL))
					{
						log(DEBUG,"A module has claimed this mode");
						if (param<pcnt)
						{
     							if ((ModeDefinedOn(modechar,MT_CHANNEL)>0) && (mdir))
							{
      								p.push_back(parameters[param]);
  							}
							if ((ModeDefinedOff(modechar,MT_CHANNEL)>0) && (!mdir))
							{
      								p.push_back(parameters[param]);
  							}
  						}
  						bool handled = false;
  						if (param>=pcnt)
  						{
  							log(DEBUG,"Not enough parameters for module-mode %c",modechar);
  							// we're supposed to have a parameter, but none was given... so dont handle the mode.
  							if (((ModeDefinedOn(modechar,MT_CHANNEL)>0) && (mdir)) || ((ModeDefinedOff(modechar,MT_CHANNEL)>0) && (!mdir)))	
  							{
  								handled = true;
  								param++;
  							}
  						}
  						for (int i = 0; i <= MODCOUNT; i++)
						{
							if (!handled)
							{
								if (modules[i]->OnExtendedMode(user,chan,modechar,MT_CHANNEL,mdir,p))
								{
									log(DEBUG,"OnExtendedMode returned nonzero for a module");
									char app[] = {modechar, 0};
									if (ptr>0)
									{
										if ((modelist[ptr-1] == '+') || (modelist[ptr-1] == '-'))
										{
											strcat(outlist, app);
										}
										else if (!strchr(outlist,modechar))
										{
											strcat(outlist, app);
										}
									}
									chan->SetCustomMode(modechar,mdir);
									// include parameters in output if mode has them
									if ((ModeDefinedOn(modechar,MT_CHANNEL)>0) && (mdir))
									{
										chan->SetCustomModeParam(modelist[ptr],parameters[param],mdir);
										strcpy(outpars[pc++],parameters[param++]);
									}
									// break, because only one module can handle the mode.
									handled = true;
        		 					}
        	 					}
     						}
     					}
				break;
				
			}
		}
	}

	/* this ensures only the *valid* modes are sent out onto the network */
	while ((outlist[strlen(outlist)-1] == '-') || (outlist[strlen(outlist)-1] == '+'))
	{
		outlist[strlen(outlist)-1] = '\0';
	}
	if (strcmp(outlist,""))
	{
		strcpy(outstr,outlist);
		for (ptr = 0; ptr < pc; ptr++)
		{
			strcat(outstr," ");
			strcat(outstr,outpars[ptr]);
		}
		if (local)
		{
			log(DEBUG,"Local mode change");
			WriteChannelLocal(chan, user, "MODE %s %s",chan->name,outstr);
		}
		else
		{
			if (servermode)
			{
				if (!silent)
				{
					WriteChannelWithServ(ServerName,chan,user,"MODE %s %s",chan->name,outstr);
					// M token for a usermode must go to all servers
					char buffer[MAXBUF];
					snprintf(buffer,MAXBUF,"M %s %s",chan->name, outstr);
					NetSendToAll(buffer);
				}
					
			}
			else
			{
				if (!silent)
				{
					WriteChannel(chan,user,"MODE %s %s",chan->name,outstr);
					// M token for a usermode must go to all servers
					char buffer[MAXBUF];
					snprintf(buffer,MAXBUF,"m %s %s %s",user->nick,chan->name, outstr);
					NetSendToAll(buffer);
				}
			}
		}
	}
}

// based on sourcemodes, return true or false to determine if umode is a valid mode a user may set on themselves or others.

bool allowed_umode(char umode, char* sourcemodes,bool adding)
{
	log(DEBUG,"Allowed_umode: %c %s",umode,sourcemodes);
	// RFC1459 specified modes
	if ((umode == 'w') || (umode == 's') || (umode == 'i'))
	{
		log(DEBUG,"umode %c allowed by RFC1459 scemantics",umode);
		return true;
	}
	
	// user may not +o themselves or others, but an oper may de-oper other opers or themselves
	if ((strchr(sourcemodes,'o')) && (!adding))
	{
		log(DEBUG,"umode %c allowed by RFC1459 scemantics",umode);
		return true;
	}
	else if (umode == 'o')
	{
		log(DEBUG,"umode %c allowed by RFC1459 scemantics",umode);
		return false;
	}
	
	// process any module-defined modes that need oper
	if ((ModeDefinedOper(umode,MT_CLIENT)) && (strchr(sourcemodes,'o')))
	{
		log(DEBUG,"umode %c allowed by module handler (oper only mode)",umode);
		return true;
	}
	else
	if (ModeDefined(umode,MT_CLIENT))
	{
		// process any module-defined modes that don't need oper
		log(DEBUG,"umode %c allowed by module handler (non-oper mode)",umode);
		if ((ModeDefinedOper(umode,MT_CLIENT)) && (!strchr(sourcemodes,'o')))
		{
			// no, this mode needs oper, and this user 'aint got what it takes!
			return false;
		}
		return true;
	}

	// anything else - return false.
	log(DEBUG,"umode %c not known by any ruleset",umode);
	return false;
}

bool process_module_umode(char umode, userrec* source, void* dest, bool adding)
{
	userrec* s2;
	bool faked = false;
	if (!source)
	{
		s2 = new userrec;
		strncpy(s2->nick,ServerName,NICKMAX);
		strcpy(s2->modes,"o");
		s2->fd = -1;
		source = s2;
		faked = true;
	}
	string_list p;
	p.clear();
	if (ModeDefined(umode,MT_CLIENT))
	{
		for (int i = 0; i <= MODCOUNT; i++)
		{
			if (modules[i]->OnExtendedMode(source,(void*)dest,umode,MT_CLIENT,adding,p))
			{
				log(DEBUG,"Module claims umode %c",umode);
				return true;
			}
		}
		log(DEBUG,"No module claims umode %c",umode);
		if (faked)
		{
			delete s2;
			source = NULL;
		}
		return false;
	}
	else
	{
		if (faked)
		{
			delete s2;
			source = NULL;
		}
		return false;
	}
}

void handle_mode(char **parameters, int pcnt, userrec *user)
{
	chanrec* Ptr;
	userrec* dest;
	int can_change,i;
	int direction = 1;
	char outpars[MAXBUF];

	dest = Find(parameters[0]);

	if (!user)
	{
		return;
	}

	if ((dest) && (pcnt == 1))
	{
		WriteServ(user->fd,"221 %s :+%s",user->nick,user->modes);
		return;
	}

	if ((dest) && (pcnt > 1))
	{
		char dmodes[MAXBUF];
		strncpy(dmodes,dest->modes,MAXBUF);
		log(DEBUG,"pulled up dest user modes: %s",dmodes);
	
		can_change = 0;
		if (user != dest)
		{
			if (strchr(user->modes,'o'))
			{
				can_change = 1;
			}
		}
		else
		{
			can_change = 1;
		}
		if (!can_change)
		{
			WriteServ(user->fd,"482 %s :Can't change mode for other users",user->nick);
			return;
		}
		
		strcpy(outpars,"+");
		direction = 1;

		if ((parameters[1][0] != '+') && (parameters[1][0] != '-'))
			return;

		for (int i = 0; i < strlen(parameters[1]); i++)
		{
			if (parameters[1][i] == '+')
			{
				if (direction != 1)
				{
					if ((outpars[strlen(outpars)-1] == '+') || (outpars[strlen(outpars)-1] == '-'))
					{
						outpars[strlen(outpars)-1] = '+';
					}
					else
					{
						strcat(outpars,"+");
					}
				}
				direction = 1;
			}
			else
			if (parameters[1][i] == '-')
			{
				if (direction != 0)
				{
					if ((outpars[strlen(outpars)-1] == '+') || (outpars[strlen(outpars)-1] == '-'))
					{
						outpars[strlen(outpars)-1] = '-';
					}
					else
					{
						strcat(outpars,"-");
					}
				}
				direction = 0;
			}
			else
			{
				can_change = 0;
				if (strchr(user->modes,'o'))
				{
					can_change = 1;
				}
				else
				{
					if ((parameters[1][i] == 'i') || (parameters[1][i] == 'w') || (parameters[1][i] == 's') || (allowed_umode(parameters[1][i],user->modes,direction)))
					{
						can_change = 1;
					}
				}
				if (can_change)
				{
					if (direction == 1)
					{
						if ((!strchr(dmodes,parameters[1][i])) && (allowed_umode(parameters[1][i],user->modes,true)))
						{
							char umode = parameters[1][i];
							if ((process_module_umode(umode, user, dest, direction)) || (umode == 'i') || (umode == 's') || (umode == 'w') || (umode == 'o'))
							{
								dmodes[strlen(dmodes)+1]='\0';
								dmodes[strlen(dmodes)] = parameters[1][i];
								outpars[strlen(outpars)+1]='\0';
								outpars[strlen(outpars)] = parameters[1][i];
							}
						}
					}
					else
					{
						if ((allowed_umode(parameters[1][i],user->modes,false)) && (strchr(dmodes,parameters[1][i])))
						{
							char umode = parameters[1][i];
							if ((process_module_umode(umode, user, dest, direction)) || (umode == 'i') || (umode == 's') || (umode == 'w') || (umode == 'o'))
							{
								int q = 0;
								char temp[MAXBUF];	
								char moo[MAXBUF];	

								outpars[strlen(outpars)+1]='\0';
								outpars[strlen(outpars)] = parameters[1][i];
							
								strcpy(temp,"");
								for (q = 0; q < strlen(dmodes); q++)
								{
									if (dmodes[q] != parameters[1][i])
									{
										moo[0] = dmodes[q];
										moo[1] = '\0';
										strcat(temp,moo);
									}
								}
								strcpy(dmodes,temp);
							}
						}
					}
				}
			}
		}
		if (strlen(outpars))
		{
			char b[MAXBUF];
			strcpy(b,"");
			int z = 0;
			int i = 0;
			while (i < strlen (outpars))
			{
				b[z++] = outpars[i++];
				b[z] = '\0';
				if (i<strlen(outpars)-1)
				{
					if (((outpars[i] == '-') || (outpars[i] == '+')) && ((outpars[i+1] == '-') || (outpars[i+1] == '+')))
					{
						// someones playing silly buggers and trying
						// to put a +- or -+ into the line...
						i++;
					}
				}
				if (i == strlen(outpars)-1)
				{
					if ((outpars[i] == '-') || (outpars[i] == '+'))
					{
						i++;
					}
				}
			}

			z = strlen(b)-1;
			if ((b[z] == '-') || (b[z] == '+'))
				b[z] == '\0';

			if ((!strcmp(b,"+")) || (!strcmp(b,"-")))
				return;

			WriteTo(user, dest, "MODE %s :%s", dest->nick, b);

			// M token for a usermode must go to all servers
			char buffer[MAXBUF];
			snprintf(buffer,MAXBUF,"m %s %s %s",user->nick, dest->nick, b);
			NetSendToAll(buffer);

			if (strlen(dmodes)>MAXMODES)
			{
				dmodes[MAXMODES-1] = '\0';
			}
			log(DEBUG,"Stripped mode line");
			log(DEBUG,"Line dest is now %s",dmodes);
			strncpy(dest->modes,dmodes,MAXMODES);

		}

		return;
	}
	
	Ptr = FindChan(parameters[0]);
	if (Ptr)
	{
		if (pcnt == 1)
		{
			/* just /modes #channel */
			WriteServ(user->fd,"324 %s %s +%s",user->nick, Ptr->name, chanmodes(Ptr));
			WriteServ(user->fd,"329 %s %s %d", user->nick, Ptr->name, Ptr->created);
			return;
		}
		else
		if (pcnt == 2)
		{
			if ((!strcmp(parameters[1],"+b")) || (!strcmp(parameters[1],"b")))
			{

				for (BanList::iterator i = Ptr->bans.begin(); i != Ptr->bans.end(); i++)
				{
					WriteServ(user->fd,"367 %s %s %s %s %d",user->nick, Ptr->name, i->data, i->set_by, i->set_time);
				}
				WriteServ(user->fd,"368 %s %s :End of channel ban list",user->nick, Ptr->name);
				return;
			}
		}

		if ((cstatus(user,Ptr) < STATUS_HOP) && (Ptr))
		{
			WriteServ(user->fd,"482 %s %s :You must be at least a half-operator to change modes on this channel",user->nick, Ptr->name);
			return;
		}

		process_modes(parameters,user,Ptr,cstatus(user,Ptr),pcnt,false,false,false);
	}
	else
	{
		WriteServ(user->fd,"401 %s %s :No suck nick/channel",user->nick, parameters[0]);
	}
}




void server_mode(char **parameters, int pcnt, userrec *user)
{
	chanrec* Ptr;
	userrec* dest;
	int can_change,i;
	int direction = 1;
	char outpars[MAXBUF];

	dest = Find(parameters[0]);
	
	// fix: ChroNiCk found this - we cant use this as debug if its null!
	if (dest)
	{
		log(DEBUG,"server_mode on %s",dest->nick);
	}

	if ((dest) && (pcnt > 1))
	{
		log(DEBUG,"params > 1");

		char dmodes[MAXBUF];
		strncpy(dmodes,dest->modes,MAXBUF);

		strcpy(outpars,"+");
		direction = 1;

		if ((parameters[1][0] != '+') && (parameters[1][0] != '-'))
			return;

		for (int i = 0; i < strlen(parameters[1]); i++)
		{
			if (parameters[1][i] == '+')
			{
				if (direction != 1)
				{
					if ((outpars[strlen(outpars)-1] == '+') || (outpars[strlen(outpars)-1] == '-'))
					{
						outpars[strlen(outpars)-1] = '+';
					}
					else
					{
						strcat(outpars,"+");
					}
				}
				direction = 1;
			}
			else
			if (parameters[1][i] == '-')
			{
				if (direction != 0)
				{
					if ((outpars[strlen(outpars)-1] == '+') || (outpars[strlen(outpars)-1] == '-'))
					{
						outpars[strlen(outpars)-1] = '-';
					}
					else
					{
						strcat(outpars,"-");
					}
				}
				direction = 0;
			}
			else
			{
				log(DEBUG,"begin mode processing entry");
				can_change = 1;
				if (can_change)
				{
					if (direction == 1)
					{
						log(DEBUG,"umode %c being added",parameters[1][i]);
						if ((!strchr(dmodes,parameters[1][i])) && (allowed_umode(parameters[1][i],user->modes,true)))
						{
							char umode = parameters[1][i];
							log(DEBUG,"umode %c is an allowed umode",umode);
							if ((process_module_umode(umode, user, dest, direction)) || (umode == 'i') || (umode == 's') || (umode == 'w') || (umode == 'o'))
							{
								dmodes[strlen(dmodes)+1]='\0';
								dmodes[strlen(dmodes)] = parameters[1][i];
								outpars[strlen(outpars)+1]='\0';
								outpars[strlen(outpars)] = parameters[1][i];
							}
						}
					}
					else
					{
						// can only remove a mode they already have
						log(DEBUG,"umode %c being removed",parameters[1][i]);
						if ((allowed_umode(parameters[1][i],user->modes,false)) && (strchr(dmodes,parameters[1][i])))
						{
							char umode = parameters[1][i];
							log(DEBUG,"umode %c is an allowed umode",umode);
							if ((process_module_umode(umode, user, dest, direction)) || (umode == 'i') || (umode == 's') || (umode == 'w') || (umode == 'o'))
							{
								int q = 0;
								char temp[MAXBUF];
								char moo[MAXBUF];	

								outpars[strlen(outpars)+1]='\0';
								outpars[strlen(outpars)] = parameters[1][i];
							
								strcpy(temp,"");
								for (q = 0; q < strlen(dmodes); q++)
								{
									if (dmodes[q] != parameters[1][i])
									{
										moo[0] = dmodes[q];
										moo[1] = '\0';
										strcat(temp,moo);
									}
								}
								strcpy(dmodes,temp);
							}
						}
					}
				}
			}
		}
		if (strlen(outpars))
		{
			char b[MAXBUF];
			strcpy(b,"");
			int z = 0;
			int i = 0;
			while (i < strlen (outpars))
			{
				b[z++] = outpars[i++];
				b[z] = '\0';
				if (i<strlen(outpars)-1)
				{
					if (((outpars[i] == '-') || (outpars[i] == '+')) && ((outpars[i+1] == '-') || (outpars[i+1] == '+')))
					{
						// someones playing silly buggers and trying
						// to put a +- or -+ into the line...
						i++;
					}
				}
				if (i == strlen(outpars)-1)
				{
					if ((outpars[i] == '-') || (outpars[i] == '+'))
					{
						i++;
					}
				}
			}

			z = strlen(b)-1;
			if ((b[z] == '-') || (b[z] == '+'))
				b[z] == '\0';

			if ((!strcmp(b,"+")) || (!strcmp(b,"-")))
				return;

			WriteTo(user, dest, "MODE %s :%s", dest->nick, b);

			// M token for a usermode must go to all servers
			char buffer[MAXBUF];
			snprintf(buffer,MAXBUF,"m %s %s %s",user->nick, dest->nick, b);
			NetSendToAll(buffer);
			
			if (strlen(dmodes)>MAXMODES)
			{
				dmodes[MAXMODES-1] = '\0';
			}
			log(DEBUG,"Stripped mode line");
			log(DEBUG,"Line dest is now %s",dmodes);
			strncpy(dest->modes,dmodes,MAXMODES);

		}

		return;
	}
	
	Ptr = FindChan(parameters[0]);
	if (Ptr)
	{
		process_modes(parameters,user,Ptr,STATUS_OP,pcnt,true,false,false);
	}
	else
	{
		WriteServ(user->fd,"401 %s %s :No suck nick/channel",user->nick, parameters[0]);
	}
}



void merge_mode(char **parameters, int pcnt)
{
	chanrec* Ptr;
	userrec* dest;
	int can_change,i;
	int direction = 1;
	char outpars[MAXBUF];

	dest = Find(parameters[0]);
	
	// fix: ChroNiCk found this - we cant use this as debug if its null!
	if (dest)
	{
		log(DEBUG,"merge_mode on %s",dest->nick);
	}

	if ((dest) && (pcnt > 1))
	{
		log(DEBUG,"params > 1");

		char dmodes[MAXBUF];
		strncpy(dmodes,dest->modes,MAXBUF);

		strcpy(outpars,"+");
		direction = 1;

		if ((parameters[1][0] != '+') && (parameters[1][0] != '-'))
			return;

		for (int i = 0; i < strlen(parameters[1]); i++)
		{
			if (parameters[1][i] == '+')
			{
				if (direction != 1)
				{
					if ((outpars[strlen(outpars)-1] == '+') || (outpars[strlen(outpars)-1] == '-'))
					{
						outpars[strlen(outpars)-1] = '+';
					}
					else
					{
						strcat(outpars,"+");
					}
				}
				direction = 1;
			}
			else
			if (parameters[1][i] == '-')
			{
				if (direction != 0)
				{
					if ((outpars[strlen(outpars)-1] == '+') || (outpars[strlen(outpars)-1] == '-'))
					{
						outpars[strlen(outpars)-1] = '-';
					}
					else
					{
						strcat(outpars,"-");
					}
				}
				direction = 0;
			}
			else
			{
				log(DEBUG,"begin mode processing entry");
				can_change = 1;
				if (can_change)
				{
					if (direction == 1)
					{
						log(DEBUG,"umode %c being added",parameters[1][i]);
						if ((!strchr(dmodes,parameters[1][i])) && (allowed_umode(parameters[1][i],"o",true)))
						{
							char umode = parameters[1][i];
							log(DEBUG,"umode %c is an allowed umode",umode);
							if ((process_module_umode(umode, NULL, dest, direction)) || (umode == 'i') || (umode == 's') || (umode == 'w') || (umode == 'o'))
							{
								dmodes[strlen(dmodes)+1]='\0';
								dmodes[strlen(dmodes)] = parameters[1][i];
								outpars[strlen(outpars)+1]='\0';
								outpars[strlen(outpars)] = parameters[1][i];
							}
						}
					}
					else
					{
						// can only remove a mode they already have
						log(DEBUG,"umode %c being removed",parameters[1][i]);
						if ((allowed_umode(parameters[1][i],"o",false)) && (strchr(dmodes,parameters[1][i])))
						{
							char umode = parameters[1][i];
							log(DEBUG,"umode %c is an allowed umode",umode);
							if ((process_module_umode(umode, NULL, dest, direction)) || (umode == 'i') || (umode == 's') || (umode == 'w') || (umode == 'o'))
							{
								int q = 0;
								char temp[MAXBUF];
								char moo[MAXBUF];	

								outpars[strlen(outpars)+1]='\0';
								outpars[strlen(outpars)] = parameters[1][i];
							
								strcpy(temp,"");
								for (q = 0; q < strlen(dmodes); q++)
								{
									if (dmodes[q] != parameters[1][i])
									{
										moo[0] = dmodes[q];
										moo[1] = '\0';
										strcat(temp,moo);
									}
								}
								strcpy(dmodes,temp);
							}
						}
					}
				}
			}
		}
		if (strlen(outpars))
		{
			char b[MAXBUF];
			strcpy(b,"");
			int z = 0;
			int i = 0;
			while (i < strlen (outpars))
			{
				b[z++] = outpars[i++];
				b[z] = '\0';
				if (i<strlen(outpars)-1)
				{
					if (((outpars[i] == '-') || (outpars[i] == '+')) && ((outpars[i+1] == '-') || (outpars[i+1] == '+')))
					{
						// someones playing silly buggers and trying
						// to put a +- or -+ into the line...
						i++;
					}
				}
				if (i == strlen(outpars)-1)
				{
					if ((outpars[i] == '-') || (outpars[i] == '+'))
					{
						i++;
					}
				}
			}

			z = strlen(b)-1;
			if ((b[z] == '-') || (b[z] == '+'))
				b[z] == '\0';

			if ((!strcmp(b,"+")) || (!strcmp(b,"-")))
				return;

			if (strlen(dmodes)>MAXMODES)
			{
				dmodes[MAXMODES-1] = '\0';
			}
			log(DEBUG,"Stripped mode line");
			log(DEBUG,"Line dest is now %s",dmodes);
			strncpy(dest->modes,dmodes,MAXMODES);

		}

		return;
	}
	
	Ptr = FindChan(parameters[0]);
	if (Ptr)
	{
		userrec s2;
		strncpy(s2.nick,ServerName,NICKMAX);
		strcpy(s2.modes,"o");
		s2.fd = -1;
		process_modes(parameters,&s2,Ptr,STATUS_OP,pcnt,true,true,false);
	}
}


void merge_mode2(char **parameters, int pcnt, userrec* user)
{
	chanrec* Ptr;
	userrec* dest;
	int can_change,i;
	int direction = 1;
	char outpars[MAXBUF];

	dest = Find(parameters[0]);
	
	// fix: ChroNiCk found this - we cant use this as debug if its null!
	if (dest)
	{
		log(DEBUG,"merge_mode on %s",dest->nick);
	}

	if ((dest) && (pcnt > 1))
	{
		log(DEBUG,"params > 1");

		char dmodes[MAXBUF];
		strncpy(dmodes,dest->modes,MAXBUF);

		strcpy(outpars,"+");
		direction = 1;

		if ((parameters[1][0] != '+') && (parameters[1][0] != '-'))
			return;

		for (int i = 0; i < strlen(parameters[1]); i++)
		{
			if (parameters[1][i] == '+')
			{
				if (direction != 1)
				{
					if ((outpars[strlen(outpars)-1] == '+') || (outpars[strlen(outpars)-1] == '-'))
					{
						outpars[strlen(outpars)-1] = '+';
					}
					else
					{
						strcat(outpars,"+");
					}
				}
				direction = 1;
			}
			else
			if (parameters[1][i] == '-')
			{
				if (direction != 0)
				{
					if ((outpars[strlen(outpars)-1] == '+') || (outpars[strlen(outpars)-1] == '-'))
					{
						outpars[strlen(outpars)-1] = '-';
					}
					else
					{
						strcat(outpars,"-");
					}
				}
				direction = 0;
			}
			else
			{
				log(DEBUG,"begin mode processing entry");
				can_change = 1;
				if (can_change)
				{
					if (direction == 1)
					{
						log(DEBUG,"umode %c being added",parameters[1][i]);
						if ((!strchr(dmodes,parameters[1][i])) && (allowed_umode(parameters[1][i],user->modes,true)))
						{
							char umode = parameters[1][i];
							log(DEBUG,"umode %c is an allowed umode",umode);
							if ((process_module_umode(umode, NULL, dest, direction)) || (umode == 'i') || (umode == 's') || (umode == 'w') || (umode == 'o'))
							{
								dmodes[strlen(dmodes)+1]='\0';
								dmodes[strlen(dmodes)] = parameters[1][i];
								outpars[strlen(outpars)+1]='\0';
								outpars[strlen(outpars)] = parameters[1][i];
							}
						}
					}
					else
					{
						// can only remove a mode they already have
						log(DEBUG,"umode %c being removed",parameters[1][i]);
						if ((allowed_umode(parameters[1][i],user->modes,false)) && (strchr(dmodes,parameters[1][i])))
						{
							char umode = parameters[1][i];
							log(DEBUG,"umode %c is an allowed umode",umode);
							if ((process_module_umode(umode, NULL, dest, direction)) || (umode == 'i') || (umode == 's') || (umode == 'w') || (umode == 'o'))
							{
								int q = 0;
								char temp[MAXBUF];
								char moo[MAXBUF];	

								outpars[strlen(outpars)+1]='\0';
								outpars[strlen(outpars)] = parameters[1][i];
							
								strcpy(temp,"");
								for (q = 0; q < strlen(dmodes); q++)
								{
									if (dmodes[q] != parameters[1][i])
									{
										moo[0] = dmodes[q];
										moo[1] = '\0';
										strcat(temp,moo);
									}
								}
								strcpy(dmodes,temp);
							}
						}
					}
				}
			}
		}
		if (strlen(outpars))
		{
			char b[MAXBUF];
			strcpy(b,"");
			int z = 0;
			int i = 0;
			while (i < strlen (outpars))
			{
				b[z++] = outpars[i++];
				b[z] = '\0';
				if (i<strlen(outpars)-1)
				{
					if (((outpars[i] == '-') || (outpars[i] == '+')) && ((outpars[i+1] == '-') || (outpars[i+1] == '+')))
					{
						// someones playing silly buggers and trying
						// to put a +- or -+ into the line...
						i++;
					}
				}
				if (i == strlen(outpars)-1)
				{
					if ((outpars[i] == '-') || (outpars[i] == '+'))
					{
						i++;
					}
				}
			}

			z = strlen(b)-1;
			if ((b[z] == '-') || (b[z] == '+'))
				b[z] == '\0';

			if ((!strcmp(b,"+")) || (!strcmp(b,"-")))
				return;

			WriteTo(user,dest,"MODE :%s",b);

			if (strlen(dmodes)>MAXMODES)
			{
				dmodes[MAXMODES-1] = '\0';
			}
			log(DEBUG,"Stripped mode line");
			log(DEBUG,"Line dest is now %s",dmodes);
			strncpy(dest->modes,dmodes,MAXMODES);

		}

		return;
	}
	
	Ptr = FindChan(parameters[0]);
	if (Ptr)
	{
		if ((cstatus(user,Ptr) < STATUS_HOP) && (Ptr))
		{
			return;
		}

		process_modes(parameters,user,Ptr,cstatus(user,Ptr),pcnt,false,false,true);
	}
}



/* This function pokes and hacks at a parameter list like the following:
 *
 * PART #winbot, #darkgalaxy :m00!
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
	int i = 0, j = 0, q = 0, total = 0, t = 0, t2 = 0, total2 = 0;
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
			strcpy(keystr,parameters[1]);
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
			strcat(plist,parameters[i]);
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
			strcpy(blog[j++],param);
			param = plist+i+1;
			if (j>20)
			{
				WriteServ(u->fd,"407 %s %s :Too many targets in list, message not delivered.",u->nick,blog[j-1]);
				return 1;
			}
		}
	}
	strcpy(blog[j++],param);
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
					strcpy(blog2[j++],param);
					param = keystr+i+1;
				}
			}
			strcpy(blog2[j++],param);
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


void kill_link(userrec *user,const char* r)
{
	user_hash::iterator iter = clientlist.find(user->nick);
	
	char reason[MAXBUF];
	
	strncpy(reason,r,MAXBUF);

	if (strlen(reason)>MAXQUIT)
	{
		reason[MAXQUIT-1] = '\0';
	}

	log(DEBUG,"kill_link: %s '%s'",user->nick,reason);
	Write(user->fd,"ERROR :Closing link (%s@%s) [%s]",user->ident,user->host,reason);
	log(DEBUG,"closing fd %d",user->fd);

	/* bugfix, cant close() a nonblocking socket (sux!) */
	if (user->registered == 7) {
		FOREACH_MOD OnUserQuit(user);
		WriteCommonExcept(user,"QUIT :%s",reason);

		// Q token must go to ALL servers!!!
		char buffer[MAXBUF];
		snprintf(buffer,MAXBUF,"Q %s :%s",user->nick,reason);
		NetSendToAll(buffer);
	}

	/* push the socket on a stack of sockets due to be closed at the next opportunity
	 * 'Client exited' is an exception to this as it means the client side has already
	 * closed the socket, we don't need to do it.
	 */
	fd_reap.push_back(user->fd);
	
	bool do_purge = false;
	
	if (user->registered == 7) {
		WriteOpers("*** Client exiting: %s!%s@%s [%s]",user->nick,user->ident,user->host,reason);
		AddWhoWas(user);
	}

	if (iter != clientlist.end())
	{
		log(DEBUG,"deleting user hash value %d",iter->second);
		if ((iter->second) && (user->registered == 7)) {
			delete iter->second;
		}
		clientlist.erase(iter);
	}

	if (user->registered == 7) {
		purge_empty_chans();
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


// looks up a users password for their connection class (<ALLOW>/<DENY> tags)

char* Passwd(userrec *user)
{
	for (ClassVector::iterator i = Classes.begin(); i != Classes.end(); i++)
	{
		if (match(user->host,i->host) && (i->type == CC_ALLOW))
		{
			return i->pass;
		}
	}
	return "";
}

bool IsDenied(userrec *user)
{
	for (ClassVector::iterator i = Classes.begin(); i != Classes.end(); i++)
	{
		if (match(user->host,i->host) && (i->type == CC_DENY))
		{
			return true;
		}
	}
	return false;
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

/* sends out an error notice to all connected clients (not to be used
 * lightly!) */

void send_error(char *s)
{
	log(DEBUG,"send_error: %s",s);
  	for (user_hash::const_iterator i = clientlist.begin(); i != clientlist.end(); i++)
	{
		if (isnick(i->second->nick))
		{
			WriteServ(i->second->fd,"NOTICE %s :%s",i->second->nick,s);
		}
		else
		{
			// fix - unregistered connections receive ERROR, not NOTICE
			Write(i->second->fd,"ERROR :%s",s);
		}
	}
}

void Error(int status)
{
	signal (SIGALRM, SIG_IGN);
	signal (SIGPIPE, SIG_IGN);
	signal (SIGTERM, SIG_IGN);
	signal (SIGABRT, SIG_IGN);
	signal (SIGSEGV, SIG_IGN);
	signal (SIGURG, SIG_IGN);
	signal (SIGKILL, SIG_IGN);
	log(DEBUG,"*** fell down a pothole in the road to perfection ***");
	send_error("Error! Segmentation fault! save meeeeeeeeeeeeee *splat!*");
	exit(status);
}


int main(int argc, char *argv[])
{
	Start();
	srand(time(NULL));
	log(DEBUG,"*** InspIRCd starting up!");
	if (!FileExists(CONFIG_FILE))
	{
		printf("ERROR: Cannot open config file: %s\nExiting...\n",CONFIG_FILE);
		log(DEBUG,"main: no config");
		printf("ERROR: Your config file is missing, this IRCd will self destruct in 10 seconds!\n");
		Exit(ERROR);
	}
	if (argc > 1) {
		if (!strcmp(argv[1],"-nofork")) {
			nofork = true;
		}
	}
	if (InspIRCd() == ERROR)
	{
		log(DEBUG,"main: daemon function bailed");
		printf("ERROR: could not initialise. Shutting down.\n");
		Exit(ERROR);
	}
	Exit(TRUE);
	return 0;
}

template<typename T> inline string ConvToStr(const T &in)
{
	stringstream tmp;
	if (!(tmp << in)) return string();
	return tmp.str();
}

/* re-allocates a nick in the user_hash after they change nicknames,
 * returns a pointer to the new user as it may have moved */

userrec* ReHashNick(char* Old, char* New)
{
	user_hash::iterator newnick;
	user_hash::iterator oldnick = clientlist.find(Old);

	log(DEBUG,"ReHashNick: %s %s",Old,New);
	
	if (!strcasecmp(Old,New))
	{
		log(DEBUG,"old nick is new nick, skipping");
		return oldnick->second;
	}
	
	if (oldnick == clientlist.end()) return NULL; /* doesnt exist */

	log(DEBUG,"ReHashNick: Found hashed nick %s",Old);

	clientlist[New] = new userrec();
	clientlist[New] = oldnick->second;
	/*delete oldnick->second; */
	clientlist.erase(oldnick);

	log(DEBUG,"ReHashNick: Nick rehashed as %s",New);
	
	return clientlist[New];
}

/* adds or updates an entry in the whowas list */
void AddWhoWas(userrec* u)
{
	user_hash::iterator iter = whowas.find(u->nick);
	userrec *a = new userrec();
	strcpy(a->nick,u->nick);
	strcpy(a->ident,u->ident);
	strcpy(a->dhost,u->dhost);
	strcpy(a->host,u->host);
	strcpy(a->fullname,u->fullname);
	strcpy(a->server,u->server);
	a->signon = u->signon;

	/* MAX_WHOWAS:   max number of /WHOWAS items
	 * WHOWAS_STALE: number of hours before a WHOWAS item is marked as stale and
	 *		 can be replaced by a newer one
	 */
	
	if (iter == whowas.end())
	{
		if (whowas.size() == WHOWAS_MAX)
		{
			for (user_hash::iterator i = whowas.begin(); i != whowas.end(); i++)
			{
				// 3600 seconds in an hour ;)
				if ((i->second->signon)<(time(NULL)-(WHOWAS_STALE*3600)))
				{
					delete i->second;
					i->second = a;
					log(DEBUG,"added WHOWAS entry, purged an old record");
					return;
				}
			}
		}
		else
		{
			log(DEBUG,"added fresh WHOWAS entry");
			whowas[a->nick] = a;
		}
	}
	else
	{
		log(DEBUG,"updated WHOWAS entry");
		delete iter->second;
		iter->second = a;
	}
}


/* add a client connection to the sockets list */
void AddClient(int socket, char* host, int port, bool iscached)
{
	int i;
	int blocking = 1;
	char resolved[MAXBUF];
	string tempnick;
	char tn2[MAXBUF];
	user_hash::iterator iter;

	tempnick = ConvToStr(socket) + "-unknown";
	sprintf(tn2,"%d-unknown",socket);

	iter = clientlist.find(tempnick);

	if (iter != clientlist.end()) return;

	/*
	 * It is OK to access the value here this way since we know
	 * it exists, we just created it above.
	 *
	 * At NO other time should you access a value in a map or a
	 * hash_map this way.
	 */
	clientlist[tempnick] = new userrec();

	NonBlocking(socket);
	log(DEBUG,"AddClient: %d %s %d",socket,host,port);

	clientlist[tempnick]->fd = socket;
	strncpy(clientlist[tempnick]->nick, tn2,NICKMAX);
	strncpy(clientlist[tempnick]->host, host,160);
	strncpy(clientlist[tempnick]->dhost, host,160);
	strncpy(clientlist[tempnick]->server, ServerName,256);
	strncpy(clientlist[tempnick]->ident, "unknown",9);
	clientlist[tempnick]->registered = 0;
	clientlist[tempnick]->signon = time(NULL);
	clientlist[tempnick]->nping = time(NULL)+240;
	clientlist[tempnick]->lastping = 1;
	clientlist[tempnick]->port = port;

	if (iscached)
	{
		WriteServ(socket,"NOTICE Auth :Found your hostname (cached)...");
	}
	else
	{
		WriteServ(socket,"NOTICE Auth :Looking up your hostname...");
	}

	// set the registration timeout for this user
	unsigned long class_regtimeout = 90;
	for (ClassVector::iterator i = Classes.begin(); i != Classes.end(); i++)
	{
		if (match(clientlist[tempnick]->host,i->host) && (i->type == CC_ALLOW))
		{
			class_regtimeout = (unsigned long)i->registration_timeout;
			break;
		}
	}

	int class_flood = 0;
	for (ClassVector::iterator i = Classes.begin(); i != Classes.end(); i++)
	{
		if (match(clientlist[tempnick]->host,i->host) && (i->type == CC_ALLOW))
		{
			class_flood = i->flood;
			break;
		}
	}

	clientlist[tempnick]->timeout = time(NULL)+class_regtimeout;
	clientlist[tempnick]->flood = class_flood;

	for (int i = 0; i < MAXCHANS; i++)
	{
 		clientlist[tempnick]->chans[i].channel = NULL;
 		clientlist[tempnick]->chans[i].uc_modes = 0;
 	}

	if (clientlist.size() == MAXCLIENTS)
		kill_link(clientlist[tempnick],"No more connections allowed in this class");
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

char lst[MAXBUF];

char* chlist(userrec *user)
{
	int i = 0;
	char cmp[MAXBUF];

        log(DEBUG,"chlist: %s",user->nick);
	strcpy(lst,"");
	if (!user)
	{
		return lst;
	}
	for (int i = 0; i != MAXCHANS; i++)
	{
		if (user->chans[i].channel != NULL)
		{
			if (user->chans[i].channel->name)
			{
				strcpy(cmp,user->chans[i].channel->name);
				strcat(cmp," ");
				if (!strstr(lst,cmp))
				{
					if ((!user->chans[i].channel->c_private) && (!user->chans[i].channel->secret))
					{
						strcat(lst,cmode(user,user->chans[i].channel));
						strcat(lst,user->chans[i].channel->name);
						strcat(lst," ");
					}
				}
			}
		}
	}
	if (strlen(lst))
	{
		lst[strlen(lst)-1] = '\0'; // chop trailing space
	}
	return lst;
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

void send_network_quit(const char* nick, const char* reason)
{
	char buffer[MAXBUF];
	snprintf(buffer,MAXBUF,"Q %s :%s",nick,reason);
	NetSendToAll(buffer);
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
			WriteServ(user->fd,"352 %s %s %s %s %s %s Hr@ :0 %s",user->nick, u->nick, u->ident, u->dhost, u->server, u->nick, u->fullname);
		}
	}
	if (pcnt == 2)
	{
                if ((!strcmp(parameters[0],"0")) || (!strcmp(parameters[0],"*")) && (!strcmp(parameters[1],"o")))
                {
                        Ptr = user->chans[0].channel;
		  	for (user_hash::const_iterator i = clientlist.begin(); i != clientlist.end(); i++)
                        {
                                if ((common_channels(user,i->second)) && (isnick(i->second->nick)))
                                {
                                        if (strchr(i->second->modes,'o'))
                                        {
                                                WriteServ(user->fd,"352 %s %s %s %s %s %s Hr@ :0 %s",user->nick, Ptr->name, i->second->ident, i->second->dhost, i->second->server, i->second->nick, i->second->fullname);
                                        }
                                }
                        }
                        WriteServ(user->fd,"315 %s %s :End of /WHO list.",user->nick, Ptr->name);
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


int usercnt(void)
{
	return clientlist.size();
}


int usercount_invisible(void)
{
	int c = 0;

	for (user_hash::const_iterator i = clientlist.begin(); i != clientlist.end(); i++)
	{
		if ((i->second->fd) && (isnick(i->second->nick)) && (strchr(i->second->modes,'i'))) c++;
	}
	return c;
}

int usercount_opers(void)
{
	int c = 0;

	for (user_hash::const_iterator i = clientlist.begin(); i != clientlist.end(); i++)
	{
		if ((i->second->fd) && (isnick(i->second->nick)) && (strchr(i->second->modes,'o'))) c++;
	}
	return c;
}

int usercount_unknown(void)
{
	int c = 0;

	for (user_hash::const_iterator i = clientlist.begin(); i != clientlist.end(); i++)
	{
		if ((i->second->fd) && (i->second->registered != 7))
			c++;
	}
	return c;
}

long chancount(void)
{
	return chanlist.size();
}

long count_servs(void)
{
	int c = 0;
	//for (int j = 0; j < 255; j++)
	//{
	//	if (servers[j] != NULL)
	//		c++;
	//}
	return c;
}

long servercount(void)
{
	return count_servs()+1;
}

long local_count()
{
	int c = 0;
	for (user_hash::const_iterator i = clientlist.begin(); i != clientlist.end(); i++)
	{
		if ((i->second->fd) && (isnick(i->second->nick)) && (!strcasecmp(i->second->server,ServerName))) c++;
	}
	return c;
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

void ShowMOTD(userrec *user)
{
	if (!MOTD.size())
	{
		WriteServ(user->fd,"422 %s :Message of the day file is missing.",user->nick);
		return;
	}
  	WriteServ(user->fd,"375 %s :- %s message of the day",user->nick,ServerName);
	for (int i = 0; i != MOTD.size(); i++)
	{
				WriteServ(user->fd,"372 %s :- %s",user->nick,MOTD[i].c_str());
	}
	WriteServ(user->fd,"376 %s :End of %s message of the day.",user->nick,ServerName);
}

void ShowRULES(userrec *user)
{
	if (!RULES.size())
	{
		WriteServ(user->fd,"NOTICE %s :Rules file is missing.",user->nick);
		return;
	}
  	WriteServ(user->fd,"NOTICE %s :%s rules",user->nick,ServerName);
	for (int i = 0; i != RULES.size(); i++)
	{
				WriteServ(user->fd,"NOTICE %s :%s",user->nick,RULES[i].c_str());
	}
	WriteServ(user->fd,"NOTICE %s :End of %s rules.",user->nick,ServerName);
}

/* shows the message of the day, and any other on-logon stuff */
void ConnectUser(userrec *user)
{
	user->registered = 7;
	user->idle_lastmsg = time(NULL);
        log(DEBUG,"ConnectUser: %s",user->nick);

	if (strcmp(Passwd(user),"") && (!user->haspassed))
	{
		kill_link(user,"Invalid password");
		return;
	}
	if (IsDenied(user))
	{
		kill_link(user,"Unauthorised connection");
		return;
	}

	WriteServ(user->fd,"NOTICE Auth :Welcome to \002%s\002!",Network);
	WriteServ(user->fd,"001 %s :Welcome to the %s IRC Network %s!%s@%s",user->nick,Network,user->nick,user->ident,user->host);
	WriteServ(user->fd,"002 %s :Your host is %s, running version %s",user->nick,ServerName,VERSION);
	WriteServ(user->fd,"003 %s :This server was created %s %s",user->nick,__TIME__,__DATE__);
	WriteServ(user->fd,"004 %s :%s %s iowghraAsORVSxNCWqBzvdHtGI lvhopsmntikrRcaqOALQbSeKVfHGCuzN",user->nick,ServerName,VERSION);
	WriteServ(user->fd,"005 %s :MAP KNOCK SAFELIST HCN MAXCHANNELS=20 MAXBANS=60 NICKLEN=30 TOPICLEN=307 KICKLEN=307 MAXTARGETS=20 AWAYLEN=307 :are supported by this server",user->nick);
	WriteServ(user->fd,"005 %s :WALLCHOPS WATCH=128 SILENCE=5 MODES=13 CHANTYPES=# PREFIX=(ohv)@%c+ CHANMODES=ohvbeqa,kfL,l,psmntirRcOAQKVHGCuzN NETWORK=%s :are supported by this server",user->nick,'%',Network);
	ShowMOTD(user);
	FOREACH_MOD OnUserConnect(user);
	WriteOpers("*** Client connecting on port %d: %s!%s@%s",user->port,user->nick,user->ident,user->host);
	
	char buffer[MAXBUF];
	snprintf(buffer,MAXBUF,"N %d %s %s %s %s +%s %s :%s",user->age,user->nick,user->host,user->dhost,user->ident,user->modes,ServerName,user->fullname);
	NetSendToAll(buffer);
}

void handle_version(char **parameters, int pcnt, userrec *user)
{
	char Revision[] = "$Revision$";

	char *s1 = Revision;
	char *savept;
	char *v1 = strtok_r(s1," ",&savept);
	s1 = savept;
	char *v2 = strtok_r(s1," ",&savept);
	s1 = savept;
	
	WriteServ(user->fd,"351 %s :%s Rev. %s %s :%s (O=%d)",user->nick,VERSION,v2,ServerName,SYSTEM,OPTIMISATION);
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

// calls a handler function for a command

void call_handler(const char* commandname,char **parameters, int pcnt, userrec *user)
{
		for (int i = 0; i < cmdlist.size(); i++)
		{
			if (!strcasecmp(cmdlist[i].command,commandname))
			{
				if (cmdlist[i].handler_function)
				{
					if (pcnt>=cmdlist[i].min_params)
					{
						if (strchr(user->modes,cmdlist[i].flags_needed))
						{
							cmdlist[i].handler_function(parameters,pcnt,user);
						}
					}
				}
			}
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

void DoSplitEveryone()
{
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
					if (strcasecmp(j->GetServerName().c_str(),ServerName))
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
	while (go_again)
	{
		go_again = false;
		for (user_hash::const_iterator u = clientlist.begin(); u != clientlist.end(); u++)
		{
			if (strcasecmp(u->second->server,ServerName))
			{
				snprintf(reason,MAXBUF,"%s %s",ServerName,u->second->server);
				kill_link(u->second,reason);
				go_again = true;
				break;
			}
		}
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

char islast(const char* s)
{
	char c = '`';
	for (int j = 0; j < 32; j++)
 	{
 		if (me[j] != NULL)
 		{
			for (int k = 0; k < me[j]->connectors.size(); k++)
			{
				if (strcasecmp(me[j]->connectors[k].GetServerName().c_str(),s))
  				{
  					c = '|';
				}
				if (!strcasecmp(me[j]->connectors[k].GetServerName().c_str(),s))
  				{
  					c = '`';
				}
			}
		}
	}
	return c;
}

long map_count(const char* s)
{
	int c = 0;
	for (user_hash::const_iterator i = clientlist.begin(); i != clientlist.end(); i++)
	{
		if ((i->second->fd) && (isnick(i->second->nick)) && (!strcasecmp(i->second->server,s))) c++;
	}
	return c;
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

void force_nickchange(userrec* user,const char* newnick)
{
	char nick[MAXBUF];
	strcpy(nick,"");
	
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
	int i = 0;
	int j = 0;
	int q = 0;
	q = strlen(parameters);
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
	int i, j, items, cmd_found;

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
	if (!strcmp(cmd,""))
	{
		return;
	}
	
	int total_params = 0;
	if (strlen(cmd)>2)
	{
		for (int q = 0; q < strlen(cmd)-1; q++)
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
		kill_link(user,"Protocol violation (1)");
		return;
	}
	
	strcpy(temp,cmd);

	std::string tmp = cmd;
	for (int i = 0; i <= MODCOUNT; i++)
	{
		std::string oldtmp = tmp;
		modules[i]->OnServerRaw(tmp,true);
		if (oldtmp != tmp)
		{
			log(DEBUG,"A Module changed the input string!");
			log(DEBUG,"New string: %s",tmp.c_str());
			log(DEBUG,"Old string: %s",oldtmp.c_str());
			break;
		}
	}
  	strncpy(cmd,tmp.c_str(),MAXBUF);
	strcpy(temp,cmd);

	if (!strchr(cmd,' '))
	{
		/* no parameters, lets skip the formalities and not chop up
		 * the string */
		log(DEBUG,"About to preprocess command with no params");
		items = 0;
		command_p[0] = NULL;
		parameters = NULL;
		for (int i = 0; i <= strlen(cmd); i++)
		{
			cmd[i] = toupper(cmd[i]);
		}
		log(DEBUG,"Preprocess done length=%d",strlen(cmd));
		command = cmd;
	}
	else
	{
		strcpy(cmd,"");
		j = 0;
		/* strip out extraneous linefeeds through mirc's crappy pasting (thanks Craig) */
		for (int i = 0; i < strlen(temp); i++)
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
			for (int i = 0; i <= strlen(cmd); i++)
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
			for (int i = 0; i <= strlen(cmd); i++)
			{
				cmd[i] = toupper(cmd[i]);
			}
		}

	}
	cmd_found = 0;
	
	if (strlen(command)>MAXCOMMAND)
	{
		kill_link(user,"Protocol violation (2)");
		return;
	}
	
	for (int x = 0; x < strlen(command); x++)
	{
		if (((command[x] < 'A') || (command[x] > 'Z')) && (command[x] != '.'))
		{
			if (((command[x] < '0') || (command[x]> '9')) && (command[x] != '-'))
			{
				if (!strchr("@!\"$%^&*(){}[]_-=+;:'#~,.<>/?\\|`",command[x]))
				{
					kill_link(user,"Protocol violation (3)");
					return;
				}
			}
		}
	}

	for (int i = 0; i != cmdlist.size(); i++)
	{
		if (strcmp(cmdlist[i].command,""))
		{
			if (strlen(command)>=(strlen(cmdlist[i].command))) if (!strncmp(command, cmdlist[i].command,MAXCOMMAND))
			{
				log(DEBUG,"Found matching command");

				if (parameters)
				{
					if (strcmp(parameters,""))
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
					log(DEBUG,"Processing command");
					
					/* activity resets the ping pending timer */
					user->nping = time(NULL) + 120;
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
					if ((user->registered == 7) || (!strcmp(command,"USER")) || (!strcmp(command,"NICK")) || (!strcmp(command,"PASS")))
					{
					        log(DEBUG,"process_command: handler: %s %s %d",user->nick,command,items);
						if (cmdlist[i].handler_function)
						{
							/* ikky /stats counters */
							if (temp)
							{
								if (user)
								{
									user->bytes_in += strlen(temp);
									user->cmds_in++;
								}
								cmdlist[i].use_count++;
								cmdlist[i].total_bytes+=strlen(temp);
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
					        log(DEBUG,"process_command: not registered: %s %s",user->nick,command);
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
	        log(DEBUG,"process_command: not in table: %s %s",user->nick,command);
		WriteServ(user->fd,"421 %s %s :Unknown command",user->nick,command);
	}
}


void createcommand(char* cmd, handlerfunc f, char flags, int minparams)
{
	command_t comm;
	/* create the command and push it onto the table */	
	strcpy(comm.command,cmd);
	comm.handler_function = f;
	comm.flags_needed = flags;
	comm.min_params = minparams;
	comm.use_count = 0;
	comm.total_bytes = 0;
	cmdlist.push_back(comm);
	log(DEBUG,"Added command %s (%d parameters)",cmd,minparams);
}

void SetupCommandTable(void)
{
	createcommand("USER",handle_user,0,4);
	createcommand("NICK",handle_nick,0,1);
	createcommand("QUIT",handle_quit,0,0);
	createcommand("VERSION",handle_version,0,0);
	createcommand("PING",handle_ping,0,1);
	createcommand("PONG",handle_pong,0,1);
	createcommand("ADMIN",handle_admin,0,0);
	createcommand("PRIVMSG",handle_privmsg,0,2);
	createcommand("INFO",handle_info,0,0);
	createcommand("TIME",handle_time,0,0);
	createcommand("WHOIS",handle_whois,0,1);
	createcommand("WALLOPS",handle_wallops,'o',1);
	createcommand("NOTICE",handle_notice,0,2);
	createcommand("JOIN",handle_join,0,1);
	createcommand("NAMES",handle_names,0,1);
	createcommand("PART",handle_part,0,1);
	createcommand("KICK",handle_kick,0,2);
	createcommand("MODE",handle_mode,0,1);
	createcommand("TOPIC",handle_topic,0,1);
	createcommand("WHO",handle_who,0,1);
	createcommand("MOTD",handle_motd,0,0);
	createcommand("RULES",handle_rules,0,0);
	createcommand("OPER",handle_oper,0,2);
	createcommand("LIST",handle_list,0,0);
	createcommand("DIE",handle_die,'o',1);
	createcommand("RESTART",handle_restart,'o',1);
	createcommand("KILL",handle_kill,'o',2);
	createcommand("REHASH",handle_rehash,'o',0);
	createcommand("LUSERS",handle_lusers,0,0);
	createcommand("STATS",handle_stats,0,1);
	createcommand("USERHOST",handle_userhost,0,1);
	createcommand("AWAY",handle_away,0,0);
	createcommand("ISON",handle_ison,0,0);
	createcommand("SUMMON",handle_summon,0,0);
	createcommand("USERS",handle_users,0,0);
	createcommand("INVITE",handle_invite,0,2);
	createcommand("PASS",handle_pass,0,1);
	createcommand("TRACE",handle_trace,'o',0);
	createcommand("WHOWAS",handle_whowas,0,1);
	createcommand("CONNECT",handle_connect,'o',1);
	createcommand("SQUIT",handle_squit,'o',0);
	createcommand("MODULES",handle_modules,'o',0);
	createcommand("LINKS",handle_links,0,0);
	createcommand("MAP",handle_map,0,0);
}

void process_buffer(const char* cmdbuf,userrec *user)
{
	if (!user)
	{
		log(DEFAULT,"*** BUG *** process_buffer was given an invalid parameter");
		return;
	}
	char cmd[MAXBUF];
	int i;
	if (!cmdbuf)
	{
		log(DEFAULT,"*** BUG *** process_buffer was given an invalid parameter");
		return;
	}
	if (!strcmp(cmdbuf,""))
	{
		return;
	}
	strncpy(cmd,cmdbuf,MAXBUF);
	if (!strcmp(cmd,""))
	{
		return;
	}
	if ((cmd[strlen(cmd)-1] == 13) || (cmd[strlen(cmd)-1] == 10))
	{
		cmd[strlen(cmd)-1] = '\0';
	}
	if ((cmd[strlen(cmd)-1] == 13) || (cmd[strlen(cmd)-1] == 10))
	{
		cmd[strlen(cmd)-1] = '\0';
	}
	if (!strcmp(cmd,""))
	{
		return;
	}
        log(DEBUG,"InspIRCd: processing: %s %s",user->nick,cmd);
	tidystring(cmd);
	if ((user) && (cmd))
	{
		process_command(user,cmd);
	}
}

void DoSync(serverrec* serv, char* udp_host)
{
	char data[MAXBUF];
	// send start of sync marker: Y <timestamp>
	// at this point the ircd receiving it starts broadcasting this netburst to all ircds
	// except the ones its receiving it from.
	snprintf(data,MAXBUF,"Y %d",time(NULL));
	serv->SendPacket(data,udp_host);
	// send users and channels
	for (user_hash::iterator u = clientlist.begin(); u != clientlist.end(); u++)
	{
		snprintf(data,MAXBUF,"N %d %s %s %s %s +%s %s :%s",u->second->age,u->second->nick,u->second->host,u->second->dhost,u->second->ident,u->second->modes,u->second->server,u->second->fullname);
		serv->SendPacket(data,udp_host);
		if (strcmp(chlist(u->second),""))
		{
			snprintf(data,MAXBUF,"J %s %s",u->second->nick,chlist(u->second));
			serv->SendPacket(data,udp_host);
		}
	}
	// send channel modes, topics etc...
	for (chan_hash::iterator c = chanlist.begin(); c != chanlist.end(); c++)
	{
		snprintf(data,MAXBUF,"M %s +%s",c->second->name,chanmodes(c->second));
		serv->SendPacket(data,udp_host);
		if (strcmp(c->second->topic,""))
		{
			snprintf(data,MAXBUF,"T %d %s %s :%s",c->second->topicset,c->second->setby,c->second->name,c->second->topic);
			serv->SendPacket(data,udp_host);
		}
		// send current banlist
		
		for (BanList::iterator b = c->second->bans.begin(); b != c->second->bans.end(); b++)
		{
			snprintf(data,MAXBUF,"M %s +b %s",b->set_time,c->second->name,b->data);
			serv->SendPacket(data,udp_host);
		}
	}
	// send end of sync marker: E <timestamp>
	snprintf(data,MAXBUF,"F %d",time(NULL));
	serv->SendPacket(data,udp_host);
	// ircd sends its serverlist after the end of sync here
}


void handle_V(char token,char* params,serverrec* source,serverrec* reply, char* udp_host)
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


void handle_P(char token,char* params,serverrec* source,serverrec* reply, char* udp_host)
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

void handle_i(char token,char* params,serverrec* source,serverrec* reply, char* udp_host)
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

void handle_t(char token,char* params,serverrec* source,serverrec* reply, char* udp_host)
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
	

void handle_T(char token,char* params,serverrec* source,serverrec* reply, char* udp_host)
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
	
void handle_M(char token,char* params,serverrec* source,serverrec* reply, char* udp_host)
{
	char* pars[128];
	char original[MAXBUF],target[MAXBUF];
	strncpy(original,params,MAXBUF);
	int index = 0;
	char* parameter = strtok(params," ");
	strncpy(target,parameter,MAXBUF);
	while (parameter)
	{
		pars[index++] = parameter;
		parameter = strtok(NULL," ");
	}
	merge_mode(pars,index);
	if (FindChan(target))
	{
		WriteChannelLocal(FindChan(target), NULL, "MODE %s",original);
	}
	if (Find(target))
	{
		WriteTo(NULL,Find(target),"MODE %s",original);
	}
}

// m is modes set by users only (not servers) valid targets are channels or users.

void handle_m(char token,char* params,serverrec* source,serverrec* reply, char* udp_host)
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


void handle_L(char token,char* params,serverrec* source,serverrec* reply, char* udp_host)
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

void handle_K(char token,char* params,serverrec* source,serverrec* reply, char* udp_host)
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

void handle_Q(char token,char* params,serverrec* source,serverrec* reply, char* udp_host)
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

void handle_n(char token,char* params,serverrec* source,serverrec* reply, char* udp_host)
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
void handle_k(char token,char* params,serverrec* source,serverrec* reply, char* udp_host)
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

void handle_AT(char token,char* params,serverrec* source,serverrec* reply, char* udp_host)
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


void handle_N(char token,char* params,serverrec* source,serverrec* reply, char* udp_host)
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

void handle_F(char token,char* params,serverrec* source,serverrec* reply, char* udp_host)
{
	long tdiff = time(NULL) - atoi(params);
	if (tdiff)
		WriteOpers("TS split for %s -> %s: %d",source->name,reply->name,tdiff);
}

void handle_a(char token,char* params,serverrec* source,serverrec* reply, char* udp_host)
{
	char* nick = strtok(params," :");
	char* gecos = strtok(NULL,"\r\n");
	
	userrec* user = Find(nick);

	if (user)
		strncpy(user->fullname,gecos,MAXBUF);
}

void handle_b(char token,char* params,serverrec* source,serverrec* reply, char* udp_host)
{
	char* nick = strtok(params," ");
	char* host = strtok(NULL," ");
	
	userrec* user = Find(nick);

	if (user)
		strncpy(user->dhost,host,160);
}

void handle_plus(char token,char* params,serverrec* source,serverrec* reply, char* udp_host)
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

void handle_R(char token,char* params,serverrec* source,serverrec* reply, char* udp_host)
{
	char* server = strtok(params," ");
	char* data = strtok(NULL,"\r\n");
	log(DEBUG,"Forwarded packet '%s' to '%s'",data,server);
	NetSendToOne(server,data);
}

void handle_J(char token,char* params,serverrec* source,serverrec* reply, char* udp_host)
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

void NetSendMyRoutingTable()
{
	// send out a line saying what is reachable to us.
	// E.g. if A is linked to B C and D, send out:
	// $ A B C D
	// if its only linked to B and D send out:
	// $ A B D
	// if it has no links, dont even send out the line at all.
	char buffer[MAXBUF];
	sprintf(buffer,"$ %s",ServerName);
	bool sendit = false;
	for (int i = 0; i < 32; i++)
	{
		if (me[i] != NULL)
		{
			for (int j = 0; j < me[i]->connectors.size(); j++)
			{
				if (me[i]->connectors[j].GetState() != STATE_DISCONNECTED)
				{
					strncat(buffer," ",MAXBUF);
					strncat(buffer,me[i]->connectors[j].GetServerName().c_str(),MAXBUF);
					sendit = true;
				}
			}
		}
	}
	if (sendit)
		NetSendToAll(buffer);
}

void handle_dollar(char token,char* params,serverrec* source,serverrec* reply, char* udp_host)
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


void DoSplit(const char* params)
{
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

// removes a server. Will NOT remove its users!

void RemoveServer(const char* name)
{
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
					if (!strcasecmp(j->GetServerName().c_str(),name))
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
}

void handle_amp(char token,char* params,serverrec* source,serverrec* reply, char* udp_host)
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


void process_restricted_commands(char token,char* params,serverrec* source,serverrec* reply, char* udp_host,char* ipaddr,int port)
{
	char buffer[MAXBUF];

	switch(token)
	{
		// Y <TS>
  		// start netburst
		case 'Y':
			nb_start = time(NULL);
			WriteOpers("Server %s is starting netburst.",udp_host);
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
			handle_plus(token,params,source,reply,udp_host);
		break;
		// routing table
		case '$':
			handle_dollar(token,params,source,reply,udp_host);
		break;
		// node unreachable - we cant route to a server, sooooo we slit it off.
		// servers can generate these for themselves for an squit.
		case '&':
			handle_amp(token,params,source,reply,udp_host);
		break;
		// R <server> <data>
		// redirect token, send all of <data> along to the given 
		// server as this server has been found to still have a route to it
		case 'R':
			handle_R(token,params,source,reply,udp_host);
		break;
		// ?
  		// ping
		case '?':
			reply->SendPacket("!",udp_host);
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
			handle_N(token,params,source,reply,udp_host);
		break;
		// a <NICK> :<GECOS>
		// change GECOS (SETNAME)
		case 'a':
			handle_a(token,params,source,reply,udp_host);
		break;
		// b <NICK> :<HOST>
		// change displayed host (SETHOST)
		case 'b':
			handle_b(token,params,source,reply,udp_host);
		break;
		// t <NICK> <CHANNEL> :<TOPIC>
		// change a channel topic
		case 't':
			handle_t(token,params,source,reply,udp_host);
		break;
		// i <NICK> <CHANNEL>
		// invite a user to a channel
		case 'i':
			handle_i(token,params,source,reply,udp_host);
		break;
		// k <SOURCE> <DEST> <CHANNEL> :<REASON>
		// kick a user from a channel
		case 'k':
			handle_k(token,params,source,reply,udp_host);
		break;
		// n <NICK> <NEWNICK>
		// change nickname of client -- a server should only be able to
		// change the nicknames of clients that reside on it unless
		// they are ulined.
		case 'n':
			handle_n(token,params,source,reply,udp_host);
		break;
		// J <NICK> <CHANLIST>
		// Join user to channel list, merge channel permissions
		case 'J':
			handle_J(token,params,source,reply,udp_host);
		break;
		// T <TS> <CHANNEL> <TOPICSETTER> :<TOPIC>
		// change channel topic (netburst only)
		case 'T':
			handle_T(token,params,source,reply,udp_host);
		break;
		// M <TARGET> <MODES> [MODE-PARAMETERS]
		// Server setting modes on an object
		case 'M':
			handle_M(token,params,source,reply,udp_host);
		break;
		// m <SOURCE> <TARGET> <MODES> [MODE-PARAMETERS]
		// User setting modes on an object
		case 'm':
			handle_m(token,params,source,reply,udp_host);
		break;
		// P <SOURCE> <TARGET> :<TEXT>
		// Send a private/channel message
		case 'P':
			handle_P(token,params,source,reply,udp_host);
		break;
		// V <SOURCE> <TARGET> :<TEXT>
		// Send a private/channel notice
		case 'V':
			handle_V(token,params,source,reply,udp_host);
		break;
		// L <SOURCE> <CHANNEL> :<REASON>
		// User parting a channel
		case 'L':
			handle_L(token,params,source,reply,udp_host);
		break;
		// Q <SOURCE> :<REASON>
		// user quitting
		case 'Q':
			handle_Q(token,params,source,reply,udp_host);
		break;
		// K <SOURCE> <DEST> :<REASON>
		// remote kill
		case 'K':
			handle_K(token,params,source,reply,udp_host);
		break;
		// @ <SOURCE> :<TEXT>
		// wallops
		case '@':
			handle_AT(token,params,source,reply,udp_host);
		break;
		// F <TS>
		// end netburst
		case 'F':
			WriteOpers("Server %s has completed netburst. (%d secs)",udp_host,time(NULL)-nb_start);
			handle_F(token,params,source,reply,udp_host);
			nb_start = 0;
			// tell all the other servers to use this authcookie to connect back again
			// got '+ test3.chatspike.net 7010 -2016508415' from test.chatspike.net
			snprintf(buffer,MAXBUF,"+ %s %s %d %d",udp_host,ipaddr,port,authcookie);
			NetSendToAllExcept(udp_host,buffer);
		break;
		// X <reserved>
		// Send netburst now
		case 'X':
			WriteOpers("Sending my netburst to %s",udp_host);
			DoSync(source,udp_host);
			WriteOpers("Send of netburst to %s completed",udp_host);
			NetSendMyRoutingTable();
		break;
		// anything else
		default:
			WriteOpers("WARNING! Unknown datagram type '%c'",token);
		break;
	}
}


void handle_link_packet(char* udp_msg, char* udp_host, serverrec *serv)
{
	char response[10240];
	char token = udp_msg[0];
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

		WriteOpers("AuthCookie CONNECT from %s (%s)",servername,udp_host);

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
							if (!strcasecmp(me[j]->connectors[k].GetServerName().c_str(),udp_host))
      							{
      								me[j]->connectors[k].SetServerName(servername);
								me[j]->connectors[k].SetDescription(serverdesc);
								me[j]->connectors[k].SetState(STATE_CONNECTED);
								NetSendMyRoutingTable();
      								return;
							}
						}
					}
					WriteOpers("\2WARNING!\2 %s sent us an authentication packet but we are not authenticating with this server right now! Possible intrusion attempt!",udp_host);
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

		WriteOpers("CONNECT from %s (%s) (their port: %d)",servername,udp_host,atoi(myport));
		
		ircd_connector* cn = serv->FindHost(servername);
		
		if (cn)
		{
			WriteOpers("CONNECT aborted: Server %s already exists from %s",servername,ServerName);
			char buffer[MAXBUF];
			sprintf(buffer,"E :Server %s already exists!",servername);
			serv->SendPacket(buffer,udp_host);
			RemoveServer(udp_host);
			return;
		}

		if (atoi(revision) != GetRevision())
		{
			WriteOpers("CONNECT aborted: Could not link to %s, is an incompatible version %s, our version is %d",servername,revision,GetRevision());
			char buffer[MAXBUF];
			sprintf(buffer,"E :Version number mismatch");
			serv->SendPacket(buffer,udp_host);
			RemoveServer(udp_host);
			RemoveServer(servername);
			return;
		}

		for (int j = 0; j < serv->connectors.size(); j++)
		{
			if (!strcasecmp(serv->connectors[j].GetServerName().c_str(),udp_host))
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
		serv->SendPacket(buffer,udp_host);
		WriteOpers("CONNECT from %s denied, no matching link block",servername);
		RemoveServer(udp_host);
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
							if (!strcasecmp(me[j]->connectors[k].GetServerName().c_str(),udp_host))
      							{
								char buffer[MAXBUF];
								me[j]->connectors[k].SetDescription(serverdesc);
								me[j]->connectors[k].SetState(STATE_CONNECTED);
								sprintf(buffer,"X 0");
								serv->SendPacket(buffer,udp_host);
								DoSync(me[j],udp_host);
								NetSendMyRoutingTable();
								return;
							}
						}
					}
					WriteOpers("\2WARNING!\2 %s sent us an authentication packet but we are not authenticating with this server right noe! Possible intrusion attempt!",udp_host);
					return;
				}
			}
			else {
				log(DEBUG,"Server names '%s' and '%s' don't match",Link_ServerName,servername);
			}
		}
		char buffer[MAXBUF];
		sprintf(buffer,"E :Access is denied (no matching link block)");
		serv->SendPacket(buffer,udp_host);
		WriteOpers("CONNECT from %s denied, no matching link block",servername);
		RemoveServer(udp_host);
		RemoveServer(servername);
		return;
	}
	else
	if (token == 'E') {
		char* error_message = finalparam+2;
		WriteOpers("ERROR from %s: %s",udp_host,error_message);
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
    					log(DEBUG,"Servers are: '%s' '%s'",udp_host,me[j]->connectors[x].GetServerName().c_str());
    					if (!strcasecmp(me[j]->connectors[x].GetServerName().c_str(),udp_host))
    					{
    						if (me[j]->connectors[x].GetState() == STATE_CONNECTED)
    						{
    							// found a valid ircd_connector.
      							process_restricted_commands(token,params,me[j],serv,udp_host,me[j]->connectors[x].GetServerIP(),me[j]->connectors[x].GetServerPort());
							return;
						}
					}
				}
			}
		}

		log(DEBUG,"Unrecognised token or unauthenticated host in datagram from %s: %c",udp_host,token);
	}
}

int reap_counter = 0;

int InspIRCd(void)
{
	struct sockaddr_in client, server;
	char addrs[MAXBUF][255];
	int openSockfd[MAXSOCKS], incomingSockfd, result = TRUE;
	socklen_t length;
	int count = 0, scanDetectTrigger = TRUE, showBanner = FALSE;
	int selectResult = 0, selectResult2 = 0;
	char *temp, configToken[MAXBUF], stuff[MAXBUF], Addr[MAXBUF], Type[MAXBUF];
	char resolvedHost[MAXBUF];
	fd_set selectFds;
	struct timeval tv;

	log_file = fopen("ircd.log","a+");
	if (!log_file)
	{
		printf("ERROR: Could not write to logfile ircd.log, bailing!\n\n");
		Exit(ERROR);
	}

	log(DEBUG,"InspIRCd: startup: begin");
	log(DEBUG,"$Id$");
	if (geteuid() == 0)
	{
		printf("WARNING!!! You are running an irc server as ROOT!!! DO NOT DO THIS!!!\n\n");
		Exit(ERROR);
		log(DEBUG,"InspIRCd: startup: not starting with UID 0!");
	}
	SetupCommandTable();
	log(DEBUG,"InspIRCd: startup: default command table set up");
	
	ReadConfig();
	if (strcmp(DieValue,"")) 
	{ 
		printf("WARNING: %s\n\n",DieValue);
		exit(0); 
	}  
	log(DEBUG,"InspIRCd: startup: read config");
	  
	int count2 = 0, count3 = 0;

	for (count = 0; count < ConfValueEnum("bind",&config_f); count++)
	{
		ConfValue("bind","port",count,configToken,&config_f);
		ConfValue("bind","address",count,Addr,&config_f);
		ConfValue("bind","type",count,Type,&config_f);
		if (!strcmp(Type,"servers"))
		{
			char Default[MAXBUF];
			strcpy(Default,"no");
			ConfValue("bind","default",count,Default,&config_f);
			if (strchr(Default,'y'))
			{
				defaultRoute = count3;
				log(DEBUG,"InspIRCd: startup: binding '%s:%s' is default server route",Addr,configToken);
			}
			me[count3] = new serverrec(ServerName,100L,false);
			me[count3]->CreateListener(Addr,atoi(configToken));
			count3++;
		}
		else
		{
			ports[count2] = atoi(configToken);
			strcpy(addrs[count2],Addr);
			count2++;
		}
		log(DEBUG,"InspIRCd: startup: read binding %s:%s [%s] from config",Addr,configToken, Type);
	}
	portCount = count2;
	UDPportCount = count3;
	  
	log(DEBUG,"InspIRCd: startup: read %d total client ports and %d total server ports",portCount,UDPportCount);
	
	log(DEBUG,"InspIRCd: startup: InspIRCd is now running!");
	
	printf("\n");
	
	/* BugFix By Craig! :p */
	count = 0;
	for (count2 = 0; count2 < ConfValueEnum("module",&config_f); count2++)
	{
		char modfile[MAXBUF];
		ConfValue("module","name",count2,configToken,&config_f);
		sprintf(modfile,"%s/%s",MOD_PATH,configToken,&config_f);
		printf("Loading module... \033[1;37m%s\033[0;37m\n",modfile);
		log(DEBUG,"InspIRCd: startup: Loading module: %s",modfile);
		/* If The File Doesnt exist, Trying to load it
	 	 * Will Segfault the IRCd.. So, check to see if
		 * it Exists, Before Proceeding. */
		if (FileExists(modfile))
		{
			factory[count] = new ircd_module(modfile);
			if (factory[count]->LastError())
			{
				log(DEBUG,"Unable to load %s: %s",modfile,factory[count]->LastError());
				sprintf("Unable to load %s: %s\nExiting...\n",modfile,factory[count]->LastError());
				Exit(ERROR);
			}
			if (factory[count]->factory)
			{
				modules[count] = factory[count]->factory->CreateModule();
				/* save the module and the module's classfactory, if
				 * this isnt done, random crashes can occur :/ */
				module_names.push_back(modfile);	
			}
			else
			{
				log(DEBUG,"Unable to load %s",modfile);
				sprintf("Unable to load %s\nExiting...\n",modfile);
				Exit(ERROR);
			}
			/* Increase the Count */
			count++;
		}
		else
		{
			log(DEBUG,"InspIRCd: startup: Module Not Found %s",modfile);
			printf("Module Not Found: \033[1;37m%s\033[0;37m, Skipping\n",modfile);
		}
	}
	MODCOUNT = count - 1;
	log(DEBUG,"Total loaded modules: %d",MODCOUNT+1);
	
	printf("\nInspIRCd is now running!\n");
	
	startup_time = time(NULL);
	  
	if (nofork)
	{
		log(VERBOSE,"Not forking as -nofork was specified");
	}
	else
	{
		if (DaemonSeed() == ERROR)
		{
			log(DEBUG,"InspIRCd: startup: can't daemonise");
	  		printf("ERROR: could not go into daemon mode. Shutting down.\n");
			Exit(ERROR);
	  	}
	}
	  
	  
	/* setup select call */
	FD_ZERO(&selectFds);
	log(DEBUG,"InspIRCd: startup: zero selects");
	log(VERBOSE,"InspIRCd: startup: portCount = %d", portCount);
	
	for (count = 0; count < portCount; count++)
	{
		if ((openSockfd[boundPortCount] = OpenTCPSocket()) == ERROR)
		{
			log(DEBUG,"InspIRCd: startup: bad fd %d",openSockfd[boundPortCount]);
			return(ERROR);
		}
		if (BindSocket(openSockfd[boundPortCount],client,server,ports[count],addrs[count]) == ERROR)
		{
			log(DEBUG,"InspIRCd: startup: failed to bind port %d",ports[count]);
		}
		else	/* well we at least bound to one socket so we'll continue */
		{
			boundPortCount++;
		}
	}
	
	log(DEBUG,"InspIRCd: startup: total bound ports %d",boundPortCount);
	  
	/* if we didn't bind to anything then abort */
	if (boundPortCount == 0)
	{
		log(DEBUG,"InspIRCd: startup: no ports bound, bailing!");
		return (ERROR);
	}
	

	length = sizeof (client);
	char udp_msg[MAXBUF], udp_host[MAXBUF];
	  
	/* main loop, this never returns */
	for (;;)
	{
#ifdef _POSIX_PRIORITY_SCHEDULING
		sched_yield();
#endif

		fd_set sfd;
		timeval tval;
		FD_ZERO(&sfd);

		user_hash::iterator count2 = clientlist.begin();

		// *FIX* Instead of closing sockets in kill_link when they receive the ERROR :blah line, we should queue
		// them in a list, then reap the list every second or so.
		if (reap_counter>300)
  		{
			if (fd_reap.size() > 0)
   			{
				for( int n = 0; n < fd_reap.size(); n++)
				{
					Blocking(fd_reap[n]);
					close(fd_reap[n]);
					NonBlocking(fd_reap[n]);
				}
			}
			fd_reap.clear();
			reap_counter=0;
		}
		reap_counter++;

		fd_set serverfds;
		FD_ZERO(&serverfds);
		timeval tvs;
		
		for (int x = 0; x != UDPportCount; x++)
		{
			FD_SET(me[x]->fd, &serverfds);
		}
		
		tvs.tv_usec = 0;		
		tvs.tv_sec = 0;
		
		int servresult = select(32767, &serverfds, NULL, NULL, &tvs);
		if (servresult > 0)
		{
			for (int x = 0; x != UDPportCount; x++)
			{
				if (FD_ISSET (me[x]->fd, &serverfds))
				{
					char remotehost[MAXBUF],resolved[MAXBUF];
					length = sizeof (client);
					incomingSockfd = accept (me[x]->fd, (sockaddr *) &client, &length);
					strncpy(remotehost,(char *)inet_ntoa(client.sin_addr),MAXBUF);
					if(CleanAndResolve(resolved, remotehost) != TRUE)
					{
						strncpy(resolved,remotehost,MAXBUF);
					}
					// add to this connections ircd_connector vector
					// *FIX* - we need the LOCAL port not the remote port in &client!
					me[x]->AddIncoming(incomingSockfd,resolved,me[x]->port);
				}
			}
		}
     
		for (int x = 0; x < UDPportCount; x++)
		{
			std::deque<std::string> msgs;
			msgs.clear();
			if (me[x]->RecvPacket(msgs, udp_host))
			{
				for (int ctr = 0; ctr < msgs.size(); ctr++)
				{
					char udp_msg[MAXBUF];
					strncpy(udp_msg,msgs[ctr].c_str(),MAXBUF);
					if (strlen(udp_msg)<1)
    						{
						log(DEBUG,"Invalid string from %s [route%d]",udp_host,x);
						break;
					}
					// during a netburst, send all data to all other linked servers
					if ((nb_start>0) && (udp_msg[0] != 'Y') && (udp_msg[0] != 'X') && (udp_msg[0] != 'F'))
					{
						NetSendToAllExcept(udp_host,udp_msg);
					}
					FOREACH_MOD OnPacketReceive(udp_msg);
					handle_link_packet(udp_msg, udp_host, me[x]);
				}
				goto label;
			}
		}
	

	while (count2 != clientlist.end())
	{
		char data[10240];
		tval.tv_usec = tval.tv_sec = 0;
		FD_ZERO(&sfd);
		int total_in_this_set = 0;

		user_hash::iterator xcount = count2;
		user_hash::iterator endingiter = count2;

		if (!count2->second) break;
		
		if (count2->second)
		if (count2->second->fd != 0)
		{
			// assemble up to 64 sockets into an fd_set
			// to implement a pooling mechanism.
			//
			// This should be up to 64x faster than the
			// old implementation.
			while (total_in_this_set < 64)
			{
				if (count2 != clientlist.end())
				{
					// we don't check the state of remote users.
					if (count2->second->fd > 0)
					{
						FD_SET (count2->second->fd, &sfd);

						// registration timeout -- didnt send USER/NICK/HOST in the time specified in
						// their connection class.
						if ((time(NULL) > count2->second->timeout) && (count2->second->registered != 7)) 
						{
						  	log(DEBUG,"InspIRCd: registration timeout: %s",count2->second->nick);
							kill_link(count2->second,"Registration timeout");
							goto label;
						}
						if (((time(NULL)) > count2->second->nping) && (isnick(count2->second->nick)) && (count2->second->registered == 7))
						{
							if ((!count2->second->lastping) && (count2->second->registered == 7))
							{
							  	log(DEBUG,"InspIRCd: ping timeout: %s",count2->second->nick);
								kill_link(count2->second,"Ping timeout");
								goto label;
							}
							Write(count2->second->fd,"PING :%s",ServerName);
						  	log(DEBUG,"InspIRCd: pinging: %s",count2->second->nick);
							count2->second->lastping = 0;
							count2->second->nping = time(NULL)+120;
						}
					}
					count2++;
					total_in_this_set++;
				}
				else break;
			}
   
	       		endingiter = count2;
       			count2 = xcount; // roll back to where we were
        
        		int v = 0;

			tval.tv_usec = 0;
			tval.tv_sec = 0;
			selectResult2 = select(65535, &sfd, NULL, NULL, &tval);
			
			// now loop through all of the items in this pool if any are waiting
			//if (selectResult2 > 0)
			for (user_hash::iterator count2a = xcount; count2a != endingiter; count2a++)
			{

#ifdef _POSIX_PRIORITY_SCHEDULING
				sched_yield();
#endif

				result = EAGAIN;
				if ((count2a->second->fd != -1) && (FD_ISSET (count2a->second->fd, &sfd)))
				{
					log(DEBUG,"Reading fd %d",count2a->second->fd);
					memset(data, 0, 10240);
					result = read(count2a->second->fd, data, 10240);
					
					if (result)
					{
						if (result > 0)
							log(DEBUG,"Read %d characters from socket",result);
						userrec* current = count2a->second;
						int currfd = current->fd;
						char* l = strtok(data,"\n");
						int floodlines = 0;
						while (l)
						{
							floodlines++;
							if ((floodlines > current->flood) && (current->flood != 0))
							{
							  	log(DEFAULT,"Excess flood from: %s!%s@%s",current->nick,current->ident,current->host);
							  	WriteOpers("*** Excess flood from: %s!%s@%s",current->nick,current->ident,current->host);
								kill_link(current,"Excess flood");
								goto label;
							}
							char sanitized[NetBufferSize];
							memset(sanitized, 0, NetBufferSize);
							int ptt = 0;
							for (int pt = 0; pt < strlen(l); pt++)
							{
								if (l[pt] != '\r')
								{
									sanitized[ptt++] = l[pt];
								}
							}
							sanitized[ptt] = '\0';
							if (strlen(sanitized))
							{


								// we're gonna re-scan to check if the nick is gone, after every
								// command - if it has, we're gonna bail
								bool find_again = false;
								process_buffer(sanitized,current);
	
								// look for the user's record in case it's changed
								for (user_hash::iterator c2 = clientlist.begin(); c2 != clientlist.end(); c2++)
								{
									if (c2->second->fd == currfd)
									{
										// found again, update pointer
										current == c2->second;
										find_again = true;
										break;
									}
								}
								if (!find_again)
									goto label;

							}
							l = strtok(NULL,"\n");
						}
						goto label;
					}

					if ((result == -1) && (errno != EAGAIN) && (errno != EINTR))
					{
						log(DEBUG,"killing: %s",count2a->second->nick);
						kill_link(count2a->second,strerror(errno));
						goto label;
					}
				}
				// result EAGAIN means nothing read
				if (result == EAGAIN)
				{
				}
				else
				if (result == 0)
				{
				  	if (count2->second)
				  	{
					  	log(DEBUG,"InspIRCd: Exited: %s",count2a->second->nick);
						kill_link(count2a->second,"Client exited");
						// must bail here? kill_link removes the hash, corrupting the iterator
						log(DEBUG,"Bailing from client exit");
						goto label;
					}
				}
				else if (result > 0)
				{
				}
			}
		}
		for (int q = 0; q < total_in_this_set; q++)
		{
			// there is no iterator += operator :(
			//if (count2 != clientlist.end())
			//{
				count2++;
			//}
		}
	}
	
	// set up select call
	for (count = 0; count < boundPortCount; count++)
	{
		FD_SET (openSockfd[count], &selectFds);
	}

	tv.tv_usec = 1;
	selectResult = select(MAXSOCKS, &selectFds, NULL, NULL, &tv);

	/* select is reporting a waiting socket. Poll them all to find out which */
	if (selectResult > 0)
	{
		char target[MAXBUF], resolved[MAXBUF];
		for (count = 0; count < boundPortCount; count++)		
		{
			if (FD_ISSET (openSockfd[count], &selectFds))
			{
				length = sizeof (client);
				incomingSockfd = accept (openSockfd[count], (struct sockaddr *) &client, &length);
			      
				address_cache::iterator iter = IP.find(client.sin_addr);
				bool iscached = false;
				if (iter == IP.end())
				{
					/* ip isn't in cache, add it */
					strncpy (target, (char *) inet_ntoa (client.sin_addr), MAXBUF);
					if(CleanAndResolve(resolved, target) != TRUE)
					{
						strncpy(resolved,target,MAXBUF);
					}
					/* hostname now in 'target' */
					IP[client.sin_addr] = new string(resolved);
					/* hostname in cache */
				}
				else
				{
					/* found ip (cached) */
					strncpy(resolved, iter->second->c_str(), MAXBUF);
					iscached = true;
				}
			
				if (incomingSockfd < 0)
				{
					WriteOpers("*** WARNING: Accept failed on port %d (%s)", ports[count],target);
					log(DEBUG,"InspIRCd: accept failed: %d",ports[count]);
				}
				else
				{
					AddClient(incomingSockfd, resolved, ports[count], iscached);
					log(DEBUG,"InspIRCd: adding client on port %d fd=%d",ports[count],incomingSockfd);
				}
				goto label;
			}
		}
	}
	label:
	if(0) {}; // "Label must be followed by a statement"... so i gave it one.
}
/* not reached */
close (incomingSockfd);
}

