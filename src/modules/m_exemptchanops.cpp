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
	ExemptChanOps(Module* Creator) : ListModeBase(Creator, "exemptchanops", 'X', "End of channel exemptchanops list", 954, 953, false, "exemptchanops") { fixed_letter = false; }

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

class ModuleExemptChanOps : public Module
{
	ExemptChanOps ec;

 public:

	ModuleExemptChanOps() : ec(this) {}

	void init()
	{
		ec.init();
		ServerInstance->Modules->AddService(ec);
		Implementation eventlist[] = { I_OnPermissionCheck };
		ServerInstance->Modules->Attach(eventlist, this, sizeof(eventlist)/sizeof(Implementation));

	}

	void Prioritize()
	{
		ServerInstance->Modules->SetPriority(this, I_OnPermissionCheck, PRIORITY_AFTER, ServerInstance->Modules->Find("m_opflags.so"));
	}

	void OnPermissionCheck(PermissionData& perm)
	{
		if (perm.name.substr(0,7) != "exempt/" || perm.result != MOD_RES_PASSTHRU)
			return;
		Membership* memb = perm.chan->GetUser(perm.source);
		std::string minmode;

		modelist* list = ec.extItem.get(perm.chan);

		if (list)
		{
			for (modelist::iterator i = list->begin(); i != list->end(); ++i)
			{
				std::string::size_type pos = i->mask.find(':');
				if (pos == std::string::npos)
					continue;
				if (i->mask.substr(0,pos) == perm.name.substr(7))
					minmode = i->mask.substr(pos + 1);
			}
		}

		if (memb && !minmode.empty())
		{
			ModeHandler* mh = minmode.length() == 1 ?
				ServerInstance->Modes->FindMode(minmode[0], MODETYPE_CHANNEL) :
				ServerInstance->Modes->FindMode(minmode);
			if (mh && memb->GetAccessRank() >= mh->GetPrefixRank())
				perm.result = MOD_RES_ALLOW;
			else if (mh || minmode == "*")
				perm.result = MOD_RES_DENY;
		}
	}

	Version GetVersion()
	{
		return Version("Provides the ability to allow channel operators to be exempt from certain modes.",VF_VENDOR);
	}

	void ReadConfig(ConfigReadStatus&)
	{
		ec.DoRehash();
	}

};

MODULE_INIT(ModuleExemptChanOps)
