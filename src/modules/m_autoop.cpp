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

/** Handle /UP
 */
class CommandUp : public Command
{
 public:
	CommandUp(Module* Creator) : Command(Creator,"UP", 0)
	{
		Penalty = 2; syntax = "[<channel>]";
		TRANSLATE2(TR_TEXT, TR_END);
	}

	CmdResult Handle (const std::vector<std::string> &parameters, User *user);
};

class ModuleAutoOp : public Module
{
	AutoOpList mh;
	dynamic_reference<OpFlagProvider> opflags;
	CommandUp cmd;

public:
	ModuleAutoOp() : mh(this), opflags("opflags"), cmd(this)
	{
	}

	void init()
	{
		mh.init();
		ServerInstance->Modules->AddService(mh);
		ServerInstance->AddCommand(&cmd);

		Implementation list[] = { I_OnPostJoin };
		ServerInstance->Modules->Attach(list, this, 1);
	}

	void DoAutoop(Membership* memb)
	{
		modelist* list = mh.extItem.get(memb->chan);
		if (!list)
			return;
		irc::modestacker ms;
		std::set<std::string> flags;
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
		if (!IS_LOCAL(memb->user))
			return;
		DoAutoop(memb);
	}

	void ReadConfig(ConfigReadStatus&)
	{
		mh.DoRehash();
	}

	Version GetVersion()
	{
		return Version("Provides support for the +w channel mode", VF_VENDOR);
	}
};

CmdResult CommandUp::Handle (const std::vector<std::string> &parameters, User *user)
{
	ModuleAutoOp* mod = (ModuleAutoOp*)(Module*) creator;
	if (parameters.size() && parameters[0].compare("*"))
	{
		Channel* c = ServerInstance->FindChan(parameters[0]);
		if (c && c->HasUser(user))
		{
			mod->DoAutoop(c->GetUser(user));
			return CMD_SUCCESS;
		}
		return CMD_FAILURE;
	}
	for (UCListIter v = user->chans.begin(); v != user->chans.end(); ++v)
		mod->DoAutoop(&*v);
	return CMD_SUCCESS;
}

MODULE_INIT(ModuleAutoOp)
