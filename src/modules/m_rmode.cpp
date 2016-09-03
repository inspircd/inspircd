/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2013 Daniel Vassdal <shutter@canternet.org>
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


#include "inspircd.h"
#include "listmode.h"

/** Handle /RMODE
 */
class CommandRMode : public Command
{
 public:
	CommandRMode(Module* Creator) : Command(Creator,"RMODE", 2, 3)
	{
		allow_empty_last_param = false;
		syntax = "<channel> <mode> [pattern]";
	}

	CmdResult Handle(const std::vector<std::string> &parameters, User *user)
	{
		ModeHandler* mh;
		Channel* chan = ServerInstance->FindChan(parameters[0]);
		char modeletter = parameters[1][0];

		if (chan == NULL)
		{
			user->WriteNotice("The channel " + parameters[0] + " does not exist.");
			return CMD_FAILURE;
		}

		mh = ServerInstance->Modes->FindMode(modeletter, MODETYPE_CHANNEL);
		if (mh == NULL || parameters[1].size() > 1)
		{
			user->WriteNotice(parameters[1] + " is not a valid channel mode.");
			return CMD_FAILURE;
		}

		if (chan->GetPrefixValue(user) < mh->GetLevelRequired())
		{
			user->WriteNotice("You do not have access to unset " + ConvToStr(modeletter) + " on " +  chan->name + ".");
			return CMD_FAILURE;
		}

		std::string pattern = parameters.size() > 2 ? parameters[2] : "*";
		PrefixMode* pm;
		ListModeBase* lm;
		ListModeBase::ModeList* ml;
		Modes::ChangeList changelist;

		if ((pm = mh->IsPrefixMode()))
		{
			// As user prefix modes don't have a GetList() method, let's iterate through the channel's users.
			const Channel::MemberMap& users = chan->GetUsers();
			for (Channel::MemberMap::const_iterator it = users.begin(); it != users.end(); ++it)
			{
				if (!InspIRCd::Match(it->first->nick, pattern))
					continue;
				if (it->second->HasMode(pm) && !((it->first == user) && (pm->GetPrefixRank() > VOICE_VALUE)))
					changelist.push_remove(mh, it->first->nick);
			}
		}
		else if ((lm = mh->IsListModeBase()) && ((ml = lm->GetList(chan)) != NULL))
		{
			for (ListModeBase::ModeList::iterator it = ml->begin(); it != ml->end(); ++it)
			{
				if (!InspIRCd::Match(it->mask, pattern))
					continue;
				changelist.push_remove(mh, it->mask);
			}
		}
		else
		{
			if (chan->IsModeSet(mh))
				changelist.push_remove(mh);
		}

		ServerInstance->Modes->Process(user, chan, NULL, changelist);
		return CMD_SUCCESS;
	}
};

class ModuleRMode : public Module
{
	CommandRMode cmd;

 public:
	ModuleRMode() : cmd(this) { }

	Version GetVersion() CXX11_OVERRIDE
	{
		return Version("Allows glob-based removal of list modes", VF_VENDOR);
	}
};

MODULE_INIT(ModuleRMode)
