/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
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
	long matches = 0;

	ConfigTag* insane = Config->ConfValue("insane");

	if (insane->getBool("hostmasks"))
		return false;

	float itrigger = insane->getFloat("trigger", 95.5);

	for (user_hash::iterator u = this->Users->clientlist->begin(); u != this->Users->clientlist->end(); u++)
	{
		if ((InspIRCd::MatchCIDR(u->second->MakeHost(), mask, ascii_case_insensitive_map)) ||
		    (InspIRCd::MatchCIDR(u->second->MakeHostIP(), mask, ascii_case_insensitive_map)))
		{
			matches++;
		}
	}

	if (!matches)
		return false;

	float percent = ((float)matches / (float)this->Users->clientlist->size()) * 100;
	if (percent > itrigger)
	{
		SNO->WriteToSnoMask('a', "\2WARNING\2: %s tried to set a G/K/E line mask of %s, which covers %.2f%% of the network!",user->nick.c_str(),mask.c_str(),percent);
		return true;
	}
	return false;
}

bool InspIRCd::IPMatchesEveryone(const std::string &ip, User* user)
{
	long matches = 0;

	ConfigTag* insane = Config->ConfValue("insane");

	if (insane->getBool("ipmasks"))
		return false;

	float itrigger = insane->getFloat("trigger", 95.5);

	for (user_hash::iterator u = this->Users->clientlist->begin(); u != this->Users->clientlist->end(); u++)
	{
		if (InspIRCd::MatchCIDR(u->second->GetIPString(), ip, ascii_case_insensitive_map))
			matches++;
	}

	if (!matches)
		return false;

	float percent = ((float)matches / (float)this->Users->clientlist->size()) * 100;
	if (percent > itrigger)
	{
		SNO->WriteToSnoMask('a', "\2WARNING\2: %s tried to set a Z line mask of %s, which covers %.2f%% of the network!",user->nick.c_str(),ip.c_str(),percent);
		return true;
	}
	return false;
}

bool InspIRCd::NickMatchesEveryone(const std::string &nick, User* user)
{
	long matches = 0;

	ConfigTag* insane = Config->ConfValue("insane");

	if (insane->getBool("nickmasks"))
		return false;

	float itrigger = insane->getFloat("trigger", 95.5);

	for (user_hash::iterator u = this->Users->clientlist->begin(); u != this->Users->clientlist->end(); u++)
	{
		if (InspIRCd::Match(u->second->nick, nick))
			matches++;
	}

	if (!matches)
		return false;

	float percent = ((float)matches / (float)this->Users->clientlist->size()) * 100;
	if (percent > itrigger)
	{
		SNO->WriteToSnoMask('a', "\2WARNING\2: %s tried to set a Q line mask of %s, which covers %.2f%% of the network!",user->nick.c_str(),nick.c_str(),percent);
		return true;
	}
	return false;
}

CmdResult SplitCommand::Handle(const std::vector<std::string>& parms, User* u)
{
	if (IS_LOCAL(u))
		return HandleLocal(parms, IS_LOCAL(u));
	if (IS_REMOTE(u))
		return HandleRemote(parms, IS_REMOTE(u));
	if (IS_SERVER(u))
		return HandleServer(parms, IS_SERVER(u));
	ServerInstance->Logs->Log("COMMAND", DEFAULT, "Unknown user type in command (uuid=%s)!", u->uuid.c_str());
	return CMD_INVALID;
}

CmdResult SplitCommand::HandleLocal(const std::vector<std::string>&, LocalUser*)
{
	return CMD_INVALID;
}

CmdResult SplitCommand::HandleRemote(const std::vector<std::string>&, RemoteUser*)
{
	return CMD_INVALID;
}

CmdResult SplitCommand::HandleServer(const std::vector<std::string>&, FakeUser*)
{
	return CMD_INVALID;
}

