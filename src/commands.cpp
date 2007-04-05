/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2007 InspIRCd Development Team
 * See: http://www.inspircd.org/wiki/index.php/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"
#include "configreader.h"
#include "users.h"
#include "modules.h"
#include "wildcard.h"
#include "xline.h"
#include "command_parse.h"

/* All other ircds when doing this check usually just look for a string of *@* or *. We're smarter than that, though. */

bool InspIRCd::HostMatchesEveryone(const std::string &mask, userrec* user)
{
	char itrigger[MAXBUF];
	long matches = 0;
	
	if (!Config->ConfValue(Config->config_data, "insane","trigger", 0, itrigger, MAXBUF))
		strlcpy(itrigger,"95.5",MAXBUF);
	
	if (Config->ConfValueBool(Config->config_data, "insane","hostmasks", 0))
		return false;
	
	for (user_hash::iterator u = clientlist->begin(); u != clientlist->end(); u++)
	{
		if ((match(u->second->MakeHost(),mask.c_str(),true)) || (match(u->second->MakeHostIP(),mask.c_str(),true)))
		{
			matches++;
		}
	}

	if (!matches)
		return false;

	float percent = ((float)matches / (float)clientlist->size()) * 100;
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
	
	for (user_hash::iterator u = clientlist->begin(); u != clientlist->end(); u++)
	{
		if (match(u->second->GetIPString(),ip.c_str(),true))
			matches++;
	}

	if (!matches)
		return false;

	float percent = ((float)matches / (float)clientlist->size()) * 100;
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

	for (user_hash::iterator u = clientlist->begin(); u != clientlist->end(); u++)
	{
		if (match(u->second->nick,nick.c_str()))
			matches++;
	}

	if (!matches)
		return false;

	float percent = ((float)matches / (float)clientlist->size()) * 100;
	if (percent > (float)atof(itrigger))
	{
		WriteOpers("*** \2WARNING\2: %s tried to set a Q line mask of %s, which covers %.2f%% of the network!",user->nick,nick.c_str(),percent);
		return true;
	}
	return false;
}
