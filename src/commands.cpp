/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2009 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

/* $Core */

#include "inspircd.h"
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
		if ((InspIRCd::Match(u->second->MakeHost(), mask, ascii_case_insensitive_map)) ||
		    (InspIRCd::Match(u->second->MakeHostIP(), mask, ascii_case_insensitive_map)))
		{
			matches++;
		}
	}

	if (!matches)
		return false;

	float percent = ((float)matches / (float)this->Users->clientlist->size()) * 100;
	if (percent > (float)atof(itrigger))
	{
		SNO->WriteToSnoMask('A', "\2WARNING\2: %s tried to set a G/K/E line mask of %s, which covers %.2f%% of the network!",user->nick.c_str(),mask.c_str(),percent);
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
		if (InspIRCd::Match(u->second->GetIPString(), ip, ascii_case_insensitive_map))
			matches++;
	}

	if (!matches)
		return false;

	float percent = ((float)matches / (float)this->Users->clientlist->size()) * 100;
	if (percent > (float)atof(itrigger))
	{
		SNO->WriteToSnoMask('A', "\2WARNING\2: %s tried to set a Z line mask of %s, which covers %.2f%% of the network!",user->nick.c_str(),ip.c_str(),percent);
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
		if (InspIRCd::Match(u->second->nick, nick))
			matches++;
	}

	if (!matches)
		return false;

	float percent = ((float)matches / (float)this->Users->clientlist->size()) * 100;
	if (percent > (float)atof(itrigger))
	{
		SNO->WriteToSnoMask('A', "\2WARNING\2: %s tried to set a Q line mask of %s, which covers %.2f%% of the network!",user->nick.c_str(),nick.c_str(),percent);
		return true;
	}
	return false;
}
