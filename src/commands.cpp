/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd is copyright (C) 2002-2006 ChatSpike-Dev.
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
#include "typedefs.h"
#include "command_parse.h"

extern ServerConfig* Config;
extern InspIRCd* ServerInstance;

extern int MODCOUNT;
extern std::vector<Module*> modules;
extern std::vector<ircd_module*> factory;
extern time_t TIME;

const long duration_m = 60;
const long duration_h = duration_m * 60;
const long duration_d = duration_h * 24;
const long duration_w = duration_d * 7;
const long duration_y = duration_w * 52;

extern user_hash clientlist;
extern chan_hash chanlist;

extern std::vector<userrec*> all_opers;
extern std::vector<userrec*> local_users;

// This table references users by file descriptor.
// its an array to make it VERY fast, as all lookups are referenced
// by an integer, meaning there is no need for a scan/search operation.
extern userrec* fd_ref_table[MAX_DESCRIPTORS];


void split_chlist(userrec* user, userrec* dest, const std::string &cl)
{
	std::string line;
	std::ostringstream prefix;
	std::string::size_type start, pos, length;
	
	prefix << ":" << Config->ServerName << " 319 " << user->nick << " " << dest->nick << " :";
	line = prefix.str();
	
	for (start = 0; pos = cl.find(' ', start); start = pos+1)
	{
		length = (pos == std::string::npos) ? cl.length() : pos;
		
		if (line.length() + length - start > 510)
		{
			Write_NoFormat(user->fd, line.c_str());
			line = prefix.str();
		}
		
		if(pos == std::string::npos)
		{
			line += cl.substr(start, length - start);
			break;
		}
		else
		{
			line += cl.substr(start, length - start + 1);
		}
	}
	
	if (line.length())
	{
		Write_NoFormat(user->fd, line.c_str());
	}
}

/* XXX - perhaps this should be in cmd_whois? -- w00t */
void do_whois(userrec* user, userrec* dest,unsigned long signon, unsigned long idle, char* nick)
{
	// bug found by phidjit - were able to whois an incomplete connection if it had sent a NICK or USER
	if (dest->registered == 7)
	{
		WriteServ(user->fd,"311 %s %s %s %s * :%s",user->nick, dest->nick, dest->ident, dest->dhost, dest->fullname);
		if ((user == dest) || (*user->oper))
		{
			WriteServ(user->fd,"378 %s %s :is connecting from *@%s %s",user->nick, dest->nick, dest->host, inet_ntoa(dest->ip4));
		}
		std::string cl = chlist(dest,user);
		if (cl.length())
		{
			if (cl.length() > 400)
			{
				split_chlist(user,dest,cl);
			}
			else
			{
				WriteServ(user->fd,"319 %s %s :%s",user->nick, dest->nick, cl.c_str());
			}
		}
		if (*Config->HideWhoisServer && !(*user->oper))
		{
			WriteServ(user->fd,"312 %s %s %s :%s",user->nick, dest->nick, Config->HideWhoisServer, Config->Network);
		}
		else
		{
			WriteServ(user->fd,"312 %s %s %s :%s",user->nick, dest->nick, dest->server, GetServerDescription(dest->server).c_str());
		}
		if (*dest->awaymsg)
		{
			WriteServ(user->fd,"301 %s %s :%s",user->nick, dest->nick, dest->awaymsg);
		}
		if (*dest->oper)
		{
			WriteServ(user->fd,"313 %s %s :is %s %s on %s",user->nick, dest->nick, (strchr("aeiou",*dest->oper) ? "an" : "a"),dest->oper, Config->Network);
		}
		if ((!signon) && (!idle))
		{
			FOREACH_MOD(I_OnWhois,OnWhois(user,dest));
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


/* XXX - these really belong in helperfuncs perhaps -- w00t */
bool is_uline(const char* server)
{
	if (!server)
		return false;
	if (!*server)
		return true;

	return (find(Config->ulines.begin(),Config->ulines.end(),server) != Config->ulines.end());
}

int operstrcmp(char* data,char* input)
{
	int MOD_RESULT = 0;
	FOREACH_RESULT(I_OnOperCompare,OnOperCompare(data,input))
	log(DEBUG,"operstrcmp: %d",MOD_RESULT);
	if (MOD_RESULT == 1)
		return 0;
	if (MOD_RESULT == -1)
		return 1;
	log(DEBUG,"strcmp fallback: '%s' '%s' %d",data,input,strcmp(data,input));
	return strcmp(data,input);
}

long duration(const char* str)
{
	char n_field[MAXBUF];
	long total = 0;
	n_field[0] = 0;

	if ((!strchr(str,'s')) && (!strchr(str,'m')) && (!strchr(str,'h')) && (!strchr(str,'d')) && (!strchr(str,'w')) && (!strchr(str,'y')))
	{
		std::string n = str;
		n += 's';
		return duration(n.c_str());
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

bool host_matches_everyone(const std::string &mask, userrec* user)
{
	char insanemasks[MAXBUF];
	char buffer[MAXBUF];
	char itrigger[MAXBUF];
	Config->ConfValue(Config->config_data, "insane","hostmasks", 0, insanemasks, MAXBUF);
	Config->ConfValue(Config->config_data, "insane","trigger", 0, itrigger, MAXBUF);
	if (*itrigger == 0)
		strlcpy(itrigger,"95.5",MAXBUF);
	if ((*insanemasks == 'y') || (*insanemasks == 't') || (*insanemasks == '1'))
		return false;
	long matches = 0;
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

bool ip_matches_everyone(const std::string &ip, userrec* user)
{
	char insanemasks[MAXBUF];
	char itrigger[MAXBUF];
	Config->ConfValue(Config->config_data, "insane","ipmasks",0,insanemasks,MAXBUF);
	Config->ConfValue(Config->config_data, "insane","trigger",0,itrigger,MAXBUF);
	if (*itrigger == 0)
		strlcpy(itrigger,"95.5",MAXBUF);
	if ((*insanemasks == 'y') || (*insanemasks == 't') || (*insanemasks == '1'))
		return false;
	long matches = 0;
	for (user_hash::iterator u = clientlist.begin(); u != clientlist.end(); u++)
	{
		if (match((char*)inet_ntoa(u->second->ip4),ip.c_str()))
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

bool nick_matches_everyone(const std::string &nick, userrec* user)
{
	char insanemasks[MAXBUF];
	char itrigger[MAXBUF];
	Config->ConfValue(Config->config_data, "insane","nickmasks",0,insanemasks,MAXBUF);
	Config->ConfValue(Config->config_data, "insane","trigger",0,itrigger,MAXBUF);
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
