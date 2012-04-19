/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2010 Jackmcbarn <jackmcbarn@jackmcbarn.no-ip.org>
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
#include "opflags.h"
#include "u_listmode.h"

/* $ModDesc: Handles channel ACLs */

/** Handles channel mode +W
 */
class ChannelACLMode : public ListModeBase
{
 public:
	ChannelACLMode(Module* Creator) : ListModeBase(Creator, "chanacl", 'W', "End of channel access group list", 954, 953, false, "chanacl")
	{
		fixed_letter = false;
	}

	virtual void DoRehash()
	{
		this->ListModeBase::DoRehash();
		std::string prefixmode = ServerInstance->Config->GetTag("chanacl")->getString("moderequired", "op");
		ModeHandler *prefixmodehandler = ServerInstance->Modes->FindMode(prefixmode);
		if (prefixmodehandler)
			levelrequired = prefixmodehandler->GetPrefixRank();
		else
		{
			levelrequired = OP_VALUE;
			ServerInstance->Logs->Log("CONFIG", DEFAULT, "Prefix " + prefixmode + " specified in <chanacl:moderequired> not found, defaulting to op");
		}
	}

	bool TellListTooLong(User* user, Channel* chan, std::string &word)
	{
		user->WriteNumeric(959, "%s %s %s :Channel access group list is full", user->nick.c_str(), chan->name.c_str(), word.c_str());
		return true;
	}

	void TellAlreadyOnList(User* user, Channel* chan, std::string &word)
	{
		user->WriteNumeric(957, "%s %s :The word %s is already on the access group list",user->nick.c_str(), chan->name.c_str(), word.c_str());
	}

	void TellNotSet(User* user, Channel* chan, std::string &word)
	{
		user->WriteNumeric(958, "%s %s :No such access group is set",user->nick.c_str(), chan->name.c_str());
	}
};

class ModuleChanACL : public Module
{
	ChannelACLMode ec;
	dynamic_reference<OpFlagProvider> permcheck;

 public:
	ModuleChanACL() : ec(this), permcheck("opflags") {}

	void init()
	{
		ec.init();
		ServerInstance->Modules->AddService(ec);
		Implementation eventlist[] = { I_OnPermissionCheck };
		ServerInstance->Modules->Attach(eventlist, this, sizeof(eventlist)/sizeof(Implementation));
	}

	void Prioritize()
	{
		// this module makes a final yes/no determination; it should be last.
		ServerInstance->Modules->SetPriority(this, I_OnPermissionCheck, PRIORITY_LAST);
	}

	void OnPermissionCheck(PermissionData& perm)
	{
		if (!perm.chan || !perm.source || perm.result != MOD_RES_PASSTHRU)
			return;

		Membership* memb = perm.chan->GetUser(perm.source);
		modelist* list = ec.extItem.get(perm.chan);
		std::set<std::string> given;

		if (list)
		{
			for (modelist::iterator i = list->begin(); i != list->end(); ++i)
			{
				std::string::size_type pos = i->mask.find(':');
				if (pos == std::string::npos)
					continue;
				irc::commasepstream contents(i->mask.substr(pos + 1));
				std::string flag;
				while (contents.GetToken(flag))
				{
					if (InspIRCd::Match(perm.name, flag))
						given.insert(i->mask.substr(0,pos));
				}
			}
		}

		// if the permission was never mentioned, fall back to default
		if (given.empty())
			return;
		// otherwise, it was mentioned at least once: you must have an access on this list

		// problem: the list will have both modes and flags. Pull out the modes first.
		std::string flaglist;
		unsigned int minrank = INT_MAX;
		for(std::set<std::string>::iterator i = given.begin(); i != given.end(); i++)
		{
			ModeHandler* privmh = i->length() == 1 ?
				ServerInstance->Modes->FindMode(i->at(0), MODETYPE_CHANNEL) :
				ServerInstance->Modes->FindMode(*i);
			if (privmh && privmh->GetPrefixRank())
			{
				if (privmh->GetPrefixRank() < minrank)
					minrank = privmh->GetPrefixRank();
			}
			else
			{
				if (!flaglist.empty())
					flaglist.push_back(',');
				flaglist.append(*i);
			}
		}

		// do they have an opflag, or are they above minimum rank?
		if (permcheck && permcheck->PermissionCheck(memb, flaglist))
			perm.result = MOD_RES_ALLOW;
		else if (memb && memb->GetAccessRank() >= minrank)
			perm.result = MOD_RES_ALLOW;
		else
		{
			perm.ErrorNumeric(ERR_CHANOPRIVSNEEDED, "%s :You do not have access to %s on this channel (ACLs active)",
				perm.chan->name.c_str(), perm.name.c_str());
			perm.result = MOD_RES_DENY;
		}
	}

	Version GetVersion()
	{
		return Version("Provides the ability to define channel access control lists.",VF_VENDOR);
	}

	void ReadConfig(ConfigReadStatus&)
	{
		ec.DoRehash();
	}

};

MODULE_INIT(ModuleChanACL)
