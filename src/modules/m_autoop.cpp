/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2011 Jackmcbarn <jackmcbarn@jackmcbarn.no-ip.org>
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

/** Handles +w channel mode
 */
class AutoOpList : public ListModeBase
{
 public:
	AutoOpList(Module* Creator) : ListModeBase(Creator, "autoop", 'w', "End of Channel Access List", 910, 911, true)
	{
		levelrequired = OP_VALUE;
		tidy = false;
	}

	PrefixMode* FindMode(const std::string& mid)
	{
		if (mid.length() == 1)
			return ServerInstance->Modes->FindPrefixMode(mid[0]);

		const ModeParser::PrefixModeList& pmlist = ServerInstance->Modes->GetPrefixModes();
		for (ModeParser::PrefixModeList::const_iterator i = pmlist.begin(); i != pmlist.end(); ++i)
		{
			PrefixMode* mh = *i;
			if (mh->name == mid)
				return mh;
		}
		return NULL;
	}

	ModResult AccessCheck(User* source, Channel* channel, std::string &parameter, bool adding)
	{
		std::string::size_type pos = parameter.find(':');
		if (pos == 0 || pos == std::string::npos)
			return adding ? MOD_RES_DENY : MOD_RES_PASSTHRU;
		unsigned int mylevel = channel->GetPrefixValue(source);
		std::string mid = parameter.substr(0, pos);
		PrefixMode* mh = FindMode(mid);

		if (adding && !mh)
		{
			source->WriteNumeric(415, "%s %s :Cannot find prefix mode '%s' for autoop",
				source->nick.c_str(), mid.c_str(), mid.c_str());
			return MOD_RES_DENY;
		}
		else if (!mh)
			return MOD_RES_PASSTHRU;

		std::string dummy;
		if (mh->AccessCheck(source, channel, dummy, true) == MOD_RES_DENY)
			return MOD_RES_DENY;
		if (mh->GetLevelRequired() > mylevel)
		{
			source->WriteNumeric(482, "%s %s :You must be able to set mode '%s' to include it in an autoop",
				source->nick.c_str(), channel->name.c_str(), mid.c_str());
			return MOD_RES_DENY;
		}
		return MOD_RES_PASSTHRU;
	}
};

class ModuleAutoOp : public Module
{
	AutoOpList mh;

 public:
	ModuleAutoOp() : mh(this)
	{
	}

	void OnPostJoin(Membership *memb) CXX11_OVERRIDE
	{
		if (!IS_LOCAL(memb->user))
			return;

		ListModeBase::ModeList* list = mh.GetList(memb->chan);
		if (list)
		{
			std::string modeline("+");
			std::vector<std::string> modechange;
			modechange.push_back(memb->chan->name);
			for (ListModeBase::ModeList::iterator it = list->begin(); it != list->end(); it++)
			{
				std::string::size_type colon = it->mask.find(':');
				if (colon == std::string::npos)
					continue;
				if (memb->chan->CheckBan(memb->user, it->mask.substr(colon+1)))
				{
					PrefixMode* given = mh.FindMode(it->mask.substr(0, colon));
					if (given)
						modeline.push_back(given->GetModeChar());
				}
			}
			modechange.push_back(modeline);
			for(std::string::size_type i = modeline.length(); i > 1; --i) // we use "i > 1" instead of "i" so we skip the +
				modechange.push_back(memb->user->nick);
			if(modechange.size() >= 3)
				ServerInstance->Modes->Process(modechange, ServerInstance->FakeClient);
		}
	}

	void ReadConfig(ConfigStatus& status) CXX11_OVERRIDE
	{
		mh.DoRehash();
	}

	Version GetVersion() CXX11_OVERRIDE
	{
		return Version("Provides support for the +w channel mode", VF_VENDOR);
	}
};

MODULE_INIT(ModuleAutoOp)
