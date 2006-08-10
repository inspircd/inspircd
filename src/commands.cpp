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

#include "inspircd_config.h"
#include "inspircd.h"
#include "configreader.h"
#include <unistd.h>
#include <sys/errno.h>
#include <sys/ioctl.h>
#include <sys/utsname.h>
#include <cstdio>
#include <time.h>
#include <string>
#include <sstream>
#include <vector>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>
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
#include "helperfuncs.h"
#include "hashcomp.h"
#include "socketengine.h"
#include "typedefs.h"
#include "command_parse.h"

extern InspIRCd* ServerInstance;

extern int MODCOUNT;
extern ModuleList modules;
extern FactoryList factory;
extern time_t TIME;

const long duration_m = 60;
const long duration_h = duration_m * 60;
const long duration_d = duration_h * 24;
const long duration_w = duration_d * 7;
const long duration_y = duration_w * 52;

extern std::vector<userrec*> all_opers;

void split_chlist(userrec* user, userrec* dest, const std::string &cl)
{
	std::string line;
	std::ostringstream prefix;
	std::string::size_type start, pos, length;
	
	prefix << ":" << ServerInstance->Config->ServerName << " 319 " << user->nick << " " << dest->nick << " :";
	line = prefix.str();
	
	for (start = 0; (pos = cl.find(' ', start)) != std::string::npos; start = pos+1)
	{
		length = (pos == std::string::npos) ? cl.length() : pos;
		
		if (line.length() + length - start > 510)
		{
			user->Write(line);
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
		user->Write(line);
	}
}

/* XXX - these really belong in helperfuncs perhaps -- w00t */
bool is_uline(const char* server)
{
	if (!server)
		return false;
	if (!*server)
		return true;

	return (find(ServerInstance->Config->ulines.begin(),ServerInstance->Config->ulines.end(),server) != ServerInstance->Config->ulines.end());
}

int operstrcmp(const char* data,const char* input)
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
	char buffer[MAXBUF];
	char itrigger[MAXBUF];
	long matches = 0;
	
	if (!ServerInstance->Config->ConfValue(ServerInstance->Config->config_data, "insane","trigger", 0, itrigger, MAXBUF))
		strlcpy(itrigger,"95.5",MAXBUF);
	
	if (ServerInstance->Config->ConfValueBool(ServerInstance->Config->config_data, "insane","hostmasks", 0))
		return false;
	
	for (user_hash::iterator u = ServerInstance->clientlist.begin(); u != ServerInstance->clientlist.end(); u++)
	{
		strlcpy(buffer,u->second->ident,MAXBUF);
		charlcat(buffer,'@',MAXBUF);
		strlcat(buffer,u->second->host,MAXBUF);
		if (match(buffer,mask.c_str()))
			matches++;
	}
	float percent = ((float)matches / (float)ServerInstance->clientlist.size()) * 100;
	if (percent > (float)atof(itrigger))
	{
		ServerInstance->WriteOpers("*** \2WARNING\2: %s tried to set a G/K/E line mask of %s, which covers %.2f%% of the network!",user->nick,mask.c_str(),percent);
		return true;
	}
	return false;
}

bool ip_matches_everyone(const std::string &ip, userrec* user)
{
	char itrigger[MAXBUF];
	long matches = 0;
	
	if (!ServerInstance->Config->ConfValue(ServerInstance->Config->config_data, "insane","trigger",0,itrigger,MAXBUF))
		strlcpy(itrigger,"95.5",MAXBUF);
	
	if (ServerInstance->Config->ConfValueBool(ServerInstance->Config->config_data, "insane","ipmasks",0))
		return false;
	
	for (user_hash::iterator u = ServerInstance->clientlist.begin(); u != ServerInstance->clientlist.end(); u++)
	{
		if (match(u->second->GetIPString(),ip.c_str(),true))
			matches++;
	}
	
	float percent = ((float)matches / (float)ServerInstance->clientlist.size()) * 100;
	if (percent > (float)atof(itrigger))
	{
		ServerInstance->WriteOpers("*** \2WARNING\2: %s tried to set a Z line mask of %s, which covers %.2f%% of the network!",user->nick,ip.c_str(),percent);
		return true;
	}
	return false;
}

bool nick_matches_everyone(const std::string &nick, userrec* user)
{
	char itrigger[MAXBUF];
	long matches = 0;
	
	if (!ServerInstance->Config->ConfValue(ServerInstance->Config->config_data, "insane","trigger",0,itrigger,MAXBUF))
		strlcpy(itrigger,"95.5",MAXBUF);
	
	if (ServerInstance->Config->ConfValueBool(ServerInstance->Config->config_data, "insane","nickmasks",0))
		return false;

	for (user_hash::iterator u = ServerInstance->clientlist.begin(); u != ServerInstance->clientlist.end(); u++)
	{
		if (match(u->second->nick,nick.c_str()))
			matches++;
	}
	
	float percent = ((float)matches / (float)ServerInstance->clientlist.size()) * 100;
	if (percent > (float)atof(itrigger))
	{
		ServerInstance->WriteOpers("*** \2WARNING\2: %s tried to set a Q line mask of %s, which covers %.2f%% of the network!",user->nick,nick.c_str(),percent);
		return true;
	}
	return false;
}
