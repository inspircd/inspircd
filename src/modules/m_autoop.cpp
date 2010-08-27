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
#include "account.h"
#include "opflags.h"
#include "u_listmode.h"

/* $ModDesc: Provides support for the +w channel mode, autoop list */

/** Handles +n user mode
 */
class NoAutoopMode : public SimpleUserModeHandler
{
 public:
	NoAutoopMode(Module* Creator) : SimpleUserModeHandler(Creator, "noautoop", 'n') { }
};

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
	NoAutoopMode mh2;
	dynamic_reference<OpFlagProvider> opflags;
	bool checkonlogin, checkonhostchange, allow_selfup;

public:
	ModuleAutoOp() : mh(this), mh2(this), opflags("opflags")
	{
	}

	void init()
	{
		mh.init();
		ServerInstance->Modules->AddService(mh);
		ServerInstance->Modules->AddService(mh2);

		Implementation list[] = { I_OnPostJoin, I_OnEvent, I_OnChangeHost, I_OnPermissionCheck };
		ServerInstance->Modules->Attach(list, this, 4);
	}

	void PrepareAutoop(Membership* memb, irc::modestacker& ms, std::set<std::string>& flags)
	{
		modelist* list = mh.extItem.get(memb->chan);
		if (!list)
			return;
		for (modelist::iterator it = list->begin(); it != list->end(); it++)
		{
			std::string::size_type colon = it->mask.find(':');
			if (colon == std::string::npos)
				continue;
			if (!memb->chan->CheckBan(memb->user, it->mask.substr(colon+1)))
				continue;
			irc::commasepstream modes(it->mask.substr(0, colon));
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
	}

	void DoAutoop(Membership* memb)
	{
		irc::modestacker ms;
		std::set<std::string> flags;
		PrepareAutoop(memb, ms, flags);
		if (!ms.empty())
		{
			User* src = ServerInstance->Config->CycleHostsFromUser ? memb->user : ServerInstance->FakeClient;
			ServerInstance->Modes->Process(src, memb->chan, ms, false, true);
			ServerInstance->Modes->Send(src, memb->chan, ms);
			ServerInstance->PI->SendMode(src, memb->chan, ms);
		}
		if (opflags && !flags.empty())
			opflags->SetFlags(memb, flags, true);
	}


	void OnPostJoin(Membership* memb)
	{
		if (!IS_LOCAL(memb->user) || memb->user->IsModeSet('n'))
			return;
		DoAutoop(memb);
	}

	void OnEvent(Event& event)
	{
		if(checkonlogin && event.id == "account_login"){
			AccountEvent& acct_event = static_cast<AccountEvent&>(event);
			if(!IS_LOCAL(acct_event.user) || acct_event.user->IsModeSet('n')) return;
			std::vector<std::string> params;
			for (UCListIter v = acct_event.user->chans.begin(); v != acct_event.user->chans.end(); ++v)
				DoAutoop(&*v);
		}
	}

	void OnChangeHost(User* u, const std::string&)
	{
		if(checkonhostchange && !u->IsModeSet('n'))
			for (UCListIter v = u->chans.begin(); v != u->chans.end(); ++v)
				DoAutoop(&*v);
	}

	void OnPermissionCheck(PermissionData& perm)
	{
		if (!allow_selfup || !perm.chan || perm.user != perm.source ||
				perm.result != MOD_RES_PASSTHRU)
			return;
		Membership* memb = perm.chan->GetUser(perm.user);
		if (!memb)
			return;
		irc::modestacker ms;
		std::set<std::string> flags;
		PrepareAutoop(memb, ms, flags);

		if (perm.name.substr(0,5) == "mode/")
		{
			ModePermissionData& mpd = static_cast<ModePermissionData&>(perm);
			if (!mpd.mc.adding)
				return;
			for(std::vector<irc::modechange>::iterator i = ms.sequence.begin(); i != ms.sequence.end(); i++)
			{
				if (i->mode == mpd.mc.mode)
				{
					mpd.result = MOD_RES_ALLOW;
					return;
				}
			}
		}
		else if (perm.name == "opflags")
		{
			OpFlagPermissionData& opd = static_cast<OpFlagPermissionData&>(perm);
			irc::commasepstream deltaflags(opd.delta);
			std::string flag;
			while (deltaflags.GetToken(flag))
			{
				if (flag[0] == '-' || flag[0] == '=')
					return;
				if (flag[0] == '+')
					flag = flag.substr(1);
				if (flags.find(flag) == flags.end())
					return;
			}
			// all flags mentioned in this change are in the autoop list
			opd.result = MOD_RES_ALLOW;
		}
	}

	void ReadConfig(ConfigReadStatus&)
	{
		mh.DoRehash();
		ConfigTag* tag = ServerInstance->Config->GetTag("autoop");
		allow_selfup = tag->getBool("selfop", true);
		checkonlogin = tag->getBool("checkonlogin", true);
		checkonhostchange = tag->getBool("checkonhostchange", true);
	}

	Version GetVersion()
	{
		return Version("Provides support for the +w channel mode", VF_VENDOR);
	}
};

MODULE_INIT(ModuleAutoOp)
