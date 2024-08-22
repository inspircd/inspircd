/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2017-2023 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012-2014 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2011 jackmcbarn <jackmcbarn@inspircd.org>
 *   Copyright (C) 2010 Daniel De Graaf <danieldg@inspircd.org>
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

enum
{
	// InspIRCd-specific.
	RPL_ACCESSLIST = 910,
	RPL_ENDOFACCESSLIST = 911
};

class AutoOpList final
	: public ListModeBase
{
public:
	AutoOpList(Module* Creator)
		: ListModeBase(Creator, "autoop", 'w', RPL_ACCESSLIST, RPL_ENDOFACCESSLIST)
	{
		ranktoset = ranktounset = OP_VALUE;
		syntax = "<prefix>:<mask>";
	}

	static PrefixMode* FindMode(const std::string& mid)
	{
		if (mid.length() == 1)
		{
			PrefixMode* pm = ServerInstance->Modes.FindPrefixMode(mid[0]);
			if (!pm)
				pm = ServerInstance->Modes.FindPrefix(mid[0]);
			return pm;
		}

		ModeHandler* mh = ServerInstance->Modes.FindMode(mid, MODETYPE_CHANNEL);
		return mh ? mh->IsPrefixMode() : nullptr;
	}

	ModResult AccessCheck(User* source, Channel* channel, Modes::Change& change) override
	{
		std::string::size_type pos = change.param.find(':');
		if (pos == 0 || pos == std::string::npos)
			return change.adding ? MOD_RES_DENY : MOD_RES_PASSTHRU;

		ModeHandler::Rank mylevel = channel->GetPrefixValue(source);
		std::string mid(change.param, 0, pos);
		PrefixMode* mh = FindMode(mid);

		if (change.adding && !mh)
		{
			source->WriteNumeric(ERR_UNKNOWNMODE, mid, FMT::format("Cannot find prefix mode '{}' for autoop", mid));
			return MOD_RES_DENY;
		}
		else if (!mh)
			return MOD_RES_PASSTHRU;

		Modes::Change modechange(mh, true, "");
		if (mh->AccessCheck(source, channel, change) == MOD_RES_DENY)
			return MOD_RES_DENY;

		if (mh->GetLevelRequired(change.adding) > mylevel)
		{
			source->WriteNumeric(ERR_CHANOPRIVSNEEDED, channel->name, FMT::format("You must be able to {} mode {} ({}) to {} an autoop containing it",
				change.adding ? "set" : "unset", mh->GetModeChar(), mh->name, change.adding ? "add" : "remove"));
			return MOD_RES_DENY;
		}
		return MOD_RES_PASSTHRU;
	}
};

class ModuleAutoOp final
	: public Module
{
	AutoOpList mh;

public:
	ModuleAutoOp()
		: Module(VF_VENDOR, "Adds channel mode w (autoop) which allows channel operators to define an access list which gives status ranks to users on join.")
		, mh(this)
	{
	}

	void OnPostJoin(Membership* memb) override
	{
		if (!IS_LOCAL(memb->user))
			return;

		ListModeBase::ModeList* list = mh.GetList(memb->chan);
		if (list)
		{
			Modes::ChangeList changelist;
			for (const auto& entry : *list)
			{
				std::string::size_type colon = entry.mask.find(':');
				if (colon == std::string::npos)
					continue;

				if (memb->chan->CheckBan(memb->user, entry.mask.substr(colon + 1)))
				{
					PrefixMode* given = AutoOpList::FindMode(entry.mask.substr(0, colon));
					if (given)
						changelist.push_add(given, memb->user->nick);
				}
			}
			ServerInstance->Modes.Process(ServerInstance->FakeClient, memb->chan, nullptr, changelist);
		}
	}

	void ReadConfig(ConfigStatus& status) override
	{
		mh.DoRehash();
	}
};

MODULE_INIT(ModuleAutoOp)
