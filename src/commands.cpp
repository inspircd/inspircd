/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2004-2007 Craig Edwards <craigedwards@brainbox.cc>
 *   Copyright (C) 2006 Oliver Lupton <oliverlupton@gmail.com>
 *   Copyright (C) 2005 Craig McLure <craig@chatspike.net>
 *
 * This file is part of InspIRCd.  InspIRCd is free software: you can
 * redistribute it and/or modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation, version 2.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
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

	if (!Config->ConfValue("insane","trigger", 0, itrigger, MAXBUF))
		strlcpy(itrigger,"95.5",MAXBUF);

	if (Config->ConfValueBool("insane","hostmasks", 0))
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
		SNO->WriteToSnoMask('a', "\2WARNING\2: %s tried to set a G/K/E line mask of %s, which covers %.2f%% of the network!",user->nick.c_str(),mask.c_str(),percent);
		return true;
	}
	return false;
}

bool InspIRCd::IPMatchesEveryone(const std::string &ip, User* user)
{
	char itrigger[MAXBUF];
	long matches = 0;

	if (!Config->ConfValue("insane","trigger",0,itrigger,MAXBUF))
		strlcpy(itrigger,"95.5",MAXBUF);

	if (Config->ConfValueBool("insane","ipmasks",0))
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
		SNO->WriteToSnoMask('a', "\2WARNING\2: %s tried to set a Z line mask of %s, which covers %.2f%% of the network!",user->nick.c_str(),ip.c_str(),percent);
		return true;
	}
	return false;
}

bool InspIRCd::NickMatchesEveryone(const std::string &nick, User* user)
{
	char itrigger[MAXBUF];
	long matches = 0;

	if (!Config->ConfValue("insane","trigger",0,itrigger,MAXBUF))
		strlcpy(itrigger,"95.5",MAXBUF);

	if (Config->ConfValueBool("insane","nickmasks",0))
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
		SNO->WriteToSnoMask('a', "\2WARNING\2: %s tried to set a Q line mask of %s, which covers %.2f%% of the network!",user->nick.c_str(),nick.c_str(),percent);
		return true;
	}
	return false;
}
