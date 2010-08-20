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

/* $ModDesc: Provides support for the +w channel mode, autoop list */

/** Handles +w channel mode
 */
class AutoOpList : public ListModeBase
{
 public:
	AutoOpList(Module* Creator) : ListModeBase(Creator, "autoop", 'w', "End of Channel Access List", 910, 911, true)
	{
		levelrequired = OP_VALUE;
		fixed_letter = false;
		tidy = false;
	}

	void AccessCheck(ModePermissionData& perm)
	{
		std::string::size_type pos = perm.mc.value.find(':');
		if (pos == 0 || pos == std::string::npos)
		{
			if (perm.mc.adding)
				perm.result = MOD_RES_DENY;
			return;
		}
		unsigned int mylevel = perm.chan->GetAccessRank(perm.source);
		irc::commasepstream modes(perm.mc.value.substr(0, pos));
		std::string mid;
		while (modes.GetToken(mid))
		{
			ModeHandler* mh = mid.length() == 1 ?
				ServerInstance->Modes->FindMode(mid[0], MODETYPE_CHANNEL) :
				ServerInstance->Modes->FindMode(mid);

			if (perm.mc.adding && mh && mh->GetLevelRequired() > mylevel)
			{
				perm.ErrorNumeric(482, "%s :You must be able to set mode '%s' to include it in an autoop",
					perm.chan->name.c_str(), mid.c_str());
				perm.result = MOD_RES_DENY;
			}
		}
	}
};

class ModuleAutoOp : public Module
{
	AutoOpList mh;
	dynamic_reference<OpFlagProvider> opflags;

public:
	ModuleAutoOp() : mh(this), opflags("opflags")
	{
	}

	void init()
	{
		mh.init();
		ServerInstance->Modules->AddService(mh);

		Implementation list[] = { I_OnUserJoin, I_OnRehash };
		ServerInstance->Modules->Attach(list, this, 2);
	}

	void Prioritize()
	{
		Module* st = ServerInstance->Modules->Find("m_spanningtree.so");
		ServerInstance->Modules->SetPriority(this, I_OnUserJoin, PRIORITY_AFTER, &st);
	}

	void OnUserJoin(Membership* memb, bool, bool, CUList&)
	{
		if (!IS_LOCAL(memb->user))
			return;
		modelist* list = mh.extItem.get(memb->chan);
		if (!list)
			return;
		irc::modestacker ms;
		std::set<std::string> flags;
		for (modelist::iterator it = list->begin(); it != list->end(); it++)
		{
			std::string::size_type colon = (**it).mask.find(':');
			if (colon == std::string::npos)
				continue;
			if (!memb->chan->CheckBan(memb->user, (**it).mask.substr(colon+1)))
				continue;
			irc::commasepstream modes((**it).mask.substr(0, colon));
			std::string mflag;
			while (modes.GetToken(mflag))
			{
				ModeHandler* given = mflag.length() == 1 ?
					ServerInstance->Modes->FindMode(mflag[0], MODETYPE_CHANNEL) :
					ServerInstance->Modes->FindMode(mflag);
				if (given && given->GetPrefixRank())
					ms.push(irc::modechange(given->id, memb->user->nick, true));
				else if (!given)
				{
					// it's a flag
					flags.insert(mflag);
				}
			}
		}
		if (!ms.empty())
		{
			User* src = ServerInstance->Config->CycleHostsFromUser ? memb->user : ServerInstance->FakeClient;
			ServerInstance->Modes->Process(src, memb->chan, ms, false, true);
			// need SendMode because we are AFTER spanningtree due to opflags
			ServerInstance->PI->SendMode(src, memb->chan, ms);
		}
		if (opflags && !flags.empty())
			opflags->SetFlags(memb, flags, true);
	}

	void OnRehash(User* user)
	{
		mh.DoRehash();
	}

	Version GetVersion()
	{
		return Version("Provides support for the +w channel mode", VF_VENDOR);
	}
};

MODULE_INIT(ModuleAutoOp)
