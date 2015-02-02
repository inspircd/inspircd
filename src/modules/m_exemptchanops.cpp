/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
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

/** Handles channel mode +X
 */
class ExemptChanOps : public ListModeBase
{
 public:
	ExemptChanOps(Module* Creator) : ListModeBase(Creator, "exemptchanops", 'X', "End of channel exemptchanops list", 954, 953, false, "exemptchanops") { }

	bool ValidateParam(User* user, Channel* chan, std::string &word)
	{
		std::string::size_type p = word.find(':');
		if (p == std::string::npos)
		{
			user->WriteNumeric(955, "%s %s :Invalid exemptchanops entry, format is <restriction>:<prefix>", chan->name.c_str(), word.c_str());
			return false;
		}

		std::string restriction(word, 0, p);
		// If there is a '-' in the restriction string ignore it and everything after it
		// to support "auditorium-vis" and "auditorium-see" in m_auditorium
		p = restriction.find('-');
		if (p != std::string::npos)
			restriction.erase(p);

		if (!ServerInstance->Modes->FindMode(restriction, MODETYPE_CHANNEL))
		{
			user->WriteNumeric(955, "%s %s :Unknown restriction", chan->name.c_str(), restriction.c_str());
			return false;
		}

		return true;
	}

	void TellListTooLong(User* user, Channel* chan, std::string &word)
	{
		user->WriteNumeric(959, "%s %s :Channel exemptchanops list is full", chan->name.c_str(), word.c_str());
	}

	void TellAlreadyOnList(User* user, Channel* chan, std::string &word)
	{
		user->WriteNumeric(957, "%s :The word %s is already on the exemptchanops list", chan->name.c_str(), word.c_str());
	}

	void TellNotSet(User* user, Channel* chan, std::string &word)
	{
		user->WriteNumeric(958, "%s :No such exemptchanops word is set", chan->name.c_str());
	}
};

class ExemptHandler : public HandlerBase3<ModResult, User*, Channel*, const std::string&>
{
 public:
	ExemptChanOps ec;
	ExemptHandler(Module* me) : ec(me) {}

	PrefixMode* FindMode(const std::string& mid)
	{
		if (mid.length() == 1)
			return ServerInstance->Modes->FindPrefixMode(mid[0]);

		ModeHandler* mh = ServerInstance->Modes->FindMode(mid, MODETYPE_CHANNEL);
		return mh ? mh->IsPrefixMode() : NULL;
	}

	ModResult Call(User* user, Channel* chan, const std::string& restriction)
	{
		unsigned int mypfx = chan->GetPrefixValue(user);
		std::string minmode;

		ListModeBase::ModeList* list = ec.GetList(chan);

		if (list)
		{
			for (ListModeBase::ModeList::iterator i = list->begin(); i != list->end(); ++i)
			{
				std::string::size_type pos = (*i).mask.find(':');
				if (pos == std::string::npos)
					continue;
				if (!i->mask.compare(0, pos, restriction))
					minmode.assign(i->mask, pos + 1, std::string::npos);
			}
		}

		PrefixMode* mh = FindMode(minmode);
		if (mh && mypfx >= mh->GetPrefixRank())
			return MOD_RES_ALLOW;
		if (mh || minmode == "*")
			return MOD_RES_DENY;

		return ServerInstance->HandleOnCheckExemption.Call(user, chan, restriction);
	}
};

class ModuleExemptChanOps : public Module
{
	ExemptHandler eh;

 public:
	ModuleExemptChanOps() : eh(this)
	{
	}

	void init() CXX11_OVERRIDE
	{
		ServerInstance->OnCheckExemption = &eh;
	}

	~ModuleExemptChanOps()
	{
		ServerInstance->OnCheckExemption = &ServerInstance->HandleOnCheckExemption;
	}

	Version GetVersion() CXX11_OVERRIDE
	{
		return Version("Provides the ability to allow channel operators to be exempt from certain modes.",VF_VENDOR);
	}

	void ReadConfig(ConfigStatus& status) CXX11_OVERRIDE
	{
		eh.ec.DoRehash();
	}
};

MODULE_INIT(ModuleExemptChanOps)
