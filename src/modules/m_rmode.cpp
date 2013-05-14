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

/* $ModDesc: Provides support for the RMODE command - Makes mass removal of chan listmodes by glob pattern possible */

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

		unsigned int prefixrank;
		char prefixchar;
		std::string pattern = parameters.size() > 2 ? parameters[2] : "*";
		ListModeBase* lm;
		ListModeBase::ModeList* ml;
		irc::modestacker modestack(false);

		if (!mh->IsListMode())
		{
			if (chan->IsModeSet(modeletter))
				modestack.Push(modeletter);
		}
		else if (((prefixrank = mh->GetPrefixRank()) && (prefixchar = mh->GetPrefix())))
		{
			// As user prefix modes don't have a GetList() method, let's iterate through the channel's users.
			for (UserMembIter it = chan->userlist.begin(); it != chan->userlist.end(); ++it)
			{
				if (!InspIRCd::Match(it->first->nick, pattern))
					continue;
				if (((strchr(chan->GetAllPrefixChars(user), prefixchar)) != NULL) && !(it->first == user && prefixrank > VOICE_VALUE))
					modestack.Push(modeletter, it->first->nick);
			}
		}
		else if (((lm = dynamic_cast<ListModeBase*>(mh)) != NULL) && ((ml = lm->GetList(chan)) != NULL))
		{
			for (ListModeBase::ModeList::iterator it = ml->begin(); it != ml->end(); ++it)
			{
				if (!InspIRCd::Match(it->mask, pattern))
					continue;
				modestack.Push(modeletter, it->mask);
			}
		}
		else
		{
			user->WriteNotice("Could not remove channel mode " + ConvToStr(modeletter));
			return CMD_FAILURE;
		}

		parameterlist stackresult;
		stackresult.push_back(chan->name);
		while (modestack.GetStackedLine(stackresult))
		{
			ServerInstance->SendMode(stackresult, user);
			stackresult.erase(stackresult.begin() + 1, stackresult.end());
		}

		return CMD_SUCCESS;
	}
};

class ModuleRMode : public Module
{
	CommandRMode cmd;

 public:
	ModuleRMode() : cmd(this) { }

	void init()
	{
		ServerInstance->Modules->AddService(cmd);
	}

	Version GetVersion()
	{
		return Version("Allows glob-based removal of list modes", VF_VENDOR);
	}
};

MODULE_INIT(ModuleRMode)
