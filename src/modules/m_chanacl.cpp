/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2010 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
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
	ChannelACLMode(Module* Creator) : ListModeBase(Creator, "chanacl", 'W', "End of channel access group list", 954, 953, false, "chanacl") { fixed_letter = false; }

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
		ServerInstance->Modules->Attach(eventlist, this, 1);
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
				std::string::size_type pos = (**i).mask.find(':');
				if (pos == std::string::npos)
					continue;
				irc::commasepstream contents((**i).mask.substr(pos + 1));
				std::string flag;
				while (contents.GetToken(flag))
				{
					if (flag == perm.name)
						given.insert((**i).mask.substr(0,pos));
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
