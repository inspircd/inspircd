/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2008 InspIRCd Development Team
 * See: http://www.inspircd.org/wiki/index.php/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

/* $Core: libIRCDcommands */

#include "inspircd.h"
#include "wildcard.h"
#include "xline.h"
#include "command_parse.h"

/* All other ircds when doing this check usually just look for a string of *@* or *. We're smarter than that, though. */

bool InspIRCd::HostMatchesEveryone(const std::string &mask, User* user)
{
	char itrigger[MAXBUF];
	long matches = 0;
	
	if (!Config->ConfValue(Config->config_data, "insane","trigger", 0, itrigger, MAXBUF))
		strlcpy(itrigger,"95.5",MAXBUF);
	
	if (Config->ConfValueBool(Config->config_data, "insane","hostmasks", 0))
		return false;
	
	for (user_hash::iterator u = this->Users->clientlist->begin(); u != this->Users->clientlist->end(); u++)
	{
		if ((match(u->second->MakeHost(), mask, true)) || (match(u->second->MakeHostIP(), mask, true)))
		{
			matches++;
		}
	}

	if (!matches)
		return false;

	float percent = ((float)matches / (float)this->Users->clientlist->size()) * 100;
	if (percent > (float)atof(itrigger))
	{
		SNO->WriteToSnoMask('A', "\2WARNING\2: %s tried to set a G/K/E line mask of %s, which covers %.2f%% of the network!",user->nick,mask.c_str(),percent);
		return true;
	}
	return false;
}

bool InspIRCd::IPMatchesEveryone(const std::string &ip, User* user)
{
	char itrigger[MAXBUF];
	long matches = 0;
	
	if (!Config->ConfValue(Config->config_data, "insane","trigger",0,itrigger,MAXBUF))
		strlcpy(itrigger,"95.5",MAXBUF);
	
	if (Config->ConfValueBool(Config->config_data, "insane","ipmasks",0))
		return false;
	
	for (user_hash::iterator u = this->Users->clientlist->begin(); u != this->Users->clientlist->end(); u++)
	{
		if (match(u->second->GetIPString(),ip,true))
			matches++;
	}

	if (!matches)
		return false;

	float percent = ((float)matches / (float)this->Users->clientlist->size()) * 100;
	if (percent > (float)atof(itrigger))
	{
		SNO->WriteToSnoMask('A', "\2WARNING\2: %s tried to set a Z line mask of %s, which covers %.2f%% of the network!",user->nick,ip.c_str(),percent);
		return true;
	}
	return false;
}

bool InspIRCd::NickMatchesEveryone(const std::string &nick, User* user)
{
	char itrigger[MAXBUF];
	long matches = 0;
	
	if (!Config->ConfValue(Config->config_data, "insane","trigger",0,itrigger,MAXBUF))
		strlcpy(itrigger,"95.5",MAXBUF);
	
	if (Config->ConfValueBool(Config->config_data, "insane","nickmasks",0))
		return false;

	for (user_hash::iterator u = this->Users->clientlist->begin(); u != this->Users->clientlist->end(); u++)
	{
		if (match(u->second->nick,nick))
			matches++;
	}

	if (!matches)
		return false;

	float percent = ((float)matches / (float)this->Users->clientlist->size()) * 100;
	if (percent > (float)atof(itrigger))
	{
		SNO->WriteToSnoMask('A', "\2WARNING\2: %s tried to set a Q line mask of %s, which covers %.2f%% of the network!",user->nick,nick.c_str(),percent);
		return true;
	}
	return false;
}
