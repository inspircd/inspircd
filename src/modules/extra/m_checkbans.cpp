/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2017-2020 Matt Schatz <genius3000@g3k.solutions>
 *
 * This file is a module for InspIRCd.  InspIRCd is free software: you can
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

/// $ModAuthor: genius3000
/// $ModAuthorMail: genius3000@g3k.solutions
/// $ModDepends: core 3
/// $ModDesc: Adds commands /checkbans, /testban, and /whyban

/* Helpop Lines for the CUSER section
 * Find: '<helpop key="cuser" title="User Commands" value="'
 * Place 'CHECKBANS', 'TESTBAN', and 'WHYBAN' in the
 * command list accordingly. Re-space as needed.
 * Find: '<helpop key="sslinfo" ...'
 * Place just above that line:
<helpop key="testban" title="/TESTBAN <channel> <mask>" value="
Test a channel ban mask against the users currently in the channel.
">

<helpop key="whyban" title="/WHYBAN <channel> [user]" value="
Get a list of bans and exceptions that match you (or the given user)
on the specified channel.
">

<helpop key="checkbans" title="/CHECKBANS <channel>" value="
Get a list of bans and exceptions that match current users on the channel.
">

 */


#include "inspircd.h"
#include "listmode.h"

namespace
{
enum
{
	RPL_BANMATCH = 540,
	RPL_EXCEPTIONMATCH = 541,
	RPL_ENDLIST = 542
};

bool CanCheck(Channel* chan, User* user, ChanModeReference& ban)
{
	if (user->HasPrivPermission("channels/auspex"))
		return true;

	if (ban->GetLevelRequired(true) > chan->GetPrefixValue(user))
	{
		user->WriteNumeric(ERR_CHANOPRIVSNEEDED, chan->name, "You do not have access to modify the ban list.");
		return false;
	}

	return true;
}

void CheckLists(User* source, Channel* chan, User* user, ChanModeReference& ban, ChanModeReference& exc)
{
	ListModeBase::ModeList* list;
	ListModeBase::ModeList::const_iterator iter;

	ListModeBase* banlm = ban->IsListModeBase();
	list = banlm ? banlm->GetList(chan) : NULL;
	if (list)
	{
		for (iter = list->begin(); iter != list->end(); ++iter)
		{
			if (!chan->CheckBan(user, iter->mask))
				continue;

			source->WriteNumeric(RPL_BANMATCH, chan->name, InspIRCd::Format("Ban %s matches %s (set by %s on %s)",
				iter->mask.c_str(), user->nick.c_str(), iter->setter.c_str(),
				ServerInstance->TimeString(iter->time, "%Y-%m-%d %H:%M:%S UTC", true).c_str()));
		}
	}

	ListModeBase* exclm = exc ? exc->IsListModeBase() : NULL;
	list = exclm ? exclm->GetList(chan) : NULL;
	if (list)
	{
		for (iter = list->begin(); iter != list->end(); ++iter)
		{
			if (!chan->CheckBan(user, iter->mask))
				continue;

			source->WriteNumeric(RPL_EXCEPTIONMATCH, chan->name, InspIRCd::Format("Exception %s matches %s (set by %s on %s)",
				iter->mask.c_str(), user->nick.c_str(), iter->setter.c_str(),
				ServerInstance->TimeString(iter->time, "%Y-%m-%d %H:%M:%S UTC", true).c_str()));
		}
	}
}
} // namespace

class CommandCheckBans : public Command
{
	ChanModeReference& ban;
	ChanModeReference& exc;

 public:
	CommandCheckBans(Module* Creator, ChanModeReference& _ban, ChanModeReference& _exc)
		: Command(Creator, "CHECKBANS", 1, 1)
		, ban(_ban)
		, exc(_exc)
	{
		this->syntax = "<channel>";
		this->Penalty = 6;
	}

	CmdResult Handle(User* user, const Params& parameters) CXX11_OVERRIDE
	{
		Channel* chan = ServerInstance->FindChan(parameters[0]);
		if (!chan)
		{
			user->WriteNumeric(Numerics::NoSuchChannel(parameters[0]));
			return CMD_FAILURE;
		}

		// Only allow checking for matching users if you have access to the ban list
		if (!CanCheck(chan, user, ban))
			return CMD_FAILURE;

		// Loop through all users of the channel, checking for matches to bans and exceptions (if available)
		const Channel::MemberMap& users = chan->GetUsers();
		for (Channel::MemberMap::const_iterator u = users.begin(); u != users.end(); ++u)
			CheckLists(user, chan, u->first, ban, exc);

		user->WriteNumeric(RPL_ENDLIST, chan->name, "End of check bans list");
		return CMD_SUCCESS;
	}
};

class CommandTestBan : public Command
{
	ChanModeReference& ban;

 public:
	CommandTestBan(Module* Creator, ChanModeReference& _ban)
		: Command(Creator, "TESTBAN", 2, 2)
		, ban(_ban)
	{
		this->syntax = "<channel> <mask>";
		this->Penalty = 6;
	}

	CmdResult Handle(User* user, const Params& parameters) CXX11_OVERRIDE
	{
		Channel* chan = ServerInstance->FindChan(parameters[0]);
		if (!chan)
		{
			user->WriteNumeric(Numerics::NoSuchChannel(parameters[0]));
			return CMD_FAILURE;
		}

		// Only allow testing bans if the user has access to set a ban on the channel
		if (!CanCheck(chan, user, ban))
			return CMD_FAILURE;

		unsigned int matched = 0;
		const Channel::MemberMap& users = chan->GetUsers();
		for (Channel::MemberMap::const_iterator u = users.begin(); u != users.end(); ++u)
		{
			if (chan->CheckBan(u->first, parameters[1]))
			{
				user->WriteNumeric(RPL_BANMATCH, chan->name, InspIRCd::Format("Mask %s matches %s",
					parameters[1].c_str(), u->first->nick.c_str()));
				matched++;
			}
		}

		if (matched > 0)
		{
			float percent = ((float)matched / (float)users.size()) * 100;
			user->WriteNumeric(RPL_BANMATCH, chan->name, InspIRCd::Format("Mask %s matched %d of %lu users (%.2f%%).",
						parameters[1].c_str(), matched, users.size(), percent));
		}

		user->WriteNumeric(RPL_ENDLIST, chan->name, parameters[1], "End of test ban list");
		return CMD_SUCCESS;
	}
};

class CommandWhyBan : public Command
{
	ChanModeReference& ban;
	ChanModeReference& exc;

 public:
	CommandWhyBan(Module* Creator, ChanModeReference& _ban, ChanModeReference& _exc)
		: Command(Creator, "WHYBAN", 1, 2)
		, ban(_ban)
		, exc(_exc)
	{
		this->syntax = "<channel> [user]";
	}

	CmdResult Handle(User* user, const Params& parameters) CXX11_OVERRIDE
	{
		Channel* chan = ServerInstance->FindChan(parameters[0]);
		if (!chan)
		{
			user->WriteNumeric(Numerics::NoSuchChannel(parameters[0]));
			return CMD_FAILURE;
		}

		/* Allow checking yourself against channel bans with no access, but only
		 * allow checking others if you have access to the channel ban list.
		 */
		User* u = parameters.size() == 1 ? user : NULL;
		if (!u)
		{
			// Use a penalty of 10 when checking other users
			LocalUser* lu = IS_LOCAL(user);
			if (lu)
				lu->CommandFloodPenalty += 10000;

			if (!CanCheck(chan, user, ban))
				return CMD_FAILURE;

			u = ServerInstance->FindNick(parameters[1]);
			if (!u)
			{
				user->WriteNumeric(Numerics::NoSuchNick(parameters[1]));
				return CMD_FAILURE;
			}
		}

		// Check for matching bans and exceptions (if available)
		CheckLists(user, chan, u, ban, exc);

		user->WriteNumeric(RPL_ENDLIST, chan->name, u->nick, "End of why ban list");
		return CMD_SUCCESS;
	}
};

class ModuleCheckBans : public Module
{
	ChanModeReference ban;
	ChanModeReference exc;
	CommandCheckBans ccb;
	CommandTestBan ctb;
	CommandWhyBan cwb;

 public:
	ModuleCheckBans()
		: ban(this, "ban")
		, exc(this, "banexception")
		, ccb(this, ban, exc)
		, ctb(this, ban)
		, cwb(this, ban, exc)
	{
	}

	Version GetVersion() CXX11_OVERRIDE
	{
		return Version("Gives /checkbans, /testban, and /whyban - channel ban helper commands.");
	}
};

MODULE_INIT(ModuleCheckBans)
