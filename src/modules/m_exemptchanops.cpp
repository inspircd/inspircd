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
#include "u_listmode.h"

/* $ModDesc: Provides the ability to allow channel operators to be exempt from certain modes. */

/** Handles channel mode +X
 */
class ExemptChanOps : public ListModeBase
{
 public:
	ExemptChanOps(Module* Creator) : ListModeBase(Creator, "exemptchanops", 'X', "End of channel exemptchanops list", 954, 953, false, "exemptchanops") { }

	bool ValidateParam(User* user, Channel* chan, std::string &word)
	{
		// TODO actually make sure there's a prop for this
		if ((word.length() > 35) || (word.empty()))
		{
			user->WriteNumeric(955, "%s %s %s :word is too %s for exemptchanops list",user->nick.c_str(), chan->name.c_str(), word.c_str(), (word.empty() ? "short" : "long"));
			return false;
		}

		return true;
	}

	bool TellListTooLong(User* user, Channel* chan, std::string &word)
	{
		user->WriteNumeric(959, "%s %s %s :Channel exemptchanops list is full", user->nick.c_str(), chan->name.c_str(), word.c_str());
		return true;
	}

	void TellAlreadyOnList(User* user, Channel* chan, std::string &word)
	{
		user->WriteNumeric(957, "%s %s :The word %s is already on the exemptchanops list",user->nick.c_str(), chan->name.c_str(), word.c_str());
	}

	void TellNotSet(User* user, Channel* chan, std::string &word)
	{
		user->WriteNumeric(958, "%s %s :No such exemptchanops word is set",user->nick.c_str(), chan->name.c_str());
	}
};

class ExemptHandler : public HandlerBase3<ModResult, User*, Channel*, const std::string&>
{
 public:
	ExemptChanOps ec;
	ExemptHandler(Module* me) : ec(me) {}
	
	ModeHandler* FindMode(const std::string& mid)
	{
		if (mid.length() == 1)
			return ServerInstance->Modes->FindMode(mid[0], MODETYPE_CHANNEL);
		for(char c='A'; c <= 'z'; c++)
		{
			ModeHandler* mh = ServerInstance->Modes->FindMode(c, MODETYPE_CHANNEL);
			if (mh && mh->name == mid)
				return mh;
		}
		return NULL;
	}

	ModResult Call(User* user, Channel* chan, const std::string& restriction)
	{
		unsigned int mypfx = chan->GetPrefixValue(user);
		std::string minmode;

		modelist* list = ec.extItem.get(chan);

		if (list)
		{
			for (modelist::iterator i = list->begin(); i != list->end(); ++i)
			{
				std::string::size_type pos = (*i).mask.find(':');
				if (pos == std::string::npos)
					continue;
				if ((*i).mask.substr(0,pos) == restriction)
					minmode = (*i).mask.substr(pos + 1);
			}
		}

		ModeHandler* mh = FindMode(minmode);
		if (mh && mypfx >= mh->GetPrefixRank())
			return MOD_RES_ALLOW;
		if (mh || minmode == "*")
			return MOD_RES_DENY;

		return ServerInstance->HandleOnCheckExemption.Call(user, chan, restriction);
	}
};

class ModuleExemptChanOps : public Module
{
	std::string defaults;
	ExemptHandler eh;

 public:

	ModuleExemptChanOps() : eh(this)
	{
	}

	void init()
	{
		ServerInstance->Modules->AddService(eh.ec);
		Implementation eventlist[] = { I_OnRehash, I_OnSyncChannel };
		ServerInstance->Modules->Attach(eventlist, this, sizeof(eventlist)/sizeof(Implementation));
		ServerInstance->OnCheckExemption = &eh;

		OnRehash(NULL);
	}

	~ModuleExemptChanOps()
	{
		ServerInstance->OnCheckExemption = &ServerInstance->HandleOnCheckExemption;
	}

	Version GetVersion()
	{
		return Version("Provides the ability to allow channel operators to be exempt from certain modes.",VF_VENDOR);
	}

	void OnRehash(User* user)
	{
		eh.ec.DoRehash();
	}

	void OnSyncChannel(Channel* chan, Module* proto, void* opaque)
	{
		eh.ec.DoSyncChannel(chan, proto, opaque);
	}
};

MODULE_INIT(ModuleExemptChanOps)
