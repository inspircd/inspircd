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

/* $ModDesc: Provides support for unreal-style oper-override */

class OverrideMode : public ModeHandler
{
 public:
	int OverrideTimeout;
	LocalIntExt timeout;
	OverrideMode(Module* Creator) : ModeHandler(Creator, "override", 'O', PARAM_NONE, MODETYPE_USER),
		timeout(EXTENSIBLE_USER, "override_timeout", Creator)
	{
		oper = true;
	}

	ModeAction OnModeChange(User* source, User* dest, Channel*, std::string&, bool adding)
	{
		if (adding == dest->IsModeSet('O'))
			return MODEACTION_DENY;
		if (IS_LOCAL(dest))
		{
			if (adding)
				timeout.set(dest, ServerInstance->Time() + OverrideTimeout);
			else
				timeout.set(dest, 0);
		}
		dest->SetMode('O', adding);
		return MODEACTION_ALLOW;
	}
};

class ModuleOverride : public Module
{
	OverrideMode om;
	bool NoisyOverride;

 public:
	ModuleOverride() : om(this) {}

	void init()
	{
		ServerInstance->Modules->AddService(om);
		ServerInstance->Modules->AddService(om.timeout);
		ServerInstance->SNO->EnableSnomask('v', "OVERRIDE");
		Implementation eventlist[] = { I_OnBackgroundTimer, I_OnPermissionCheck };
		ServerInstance->Modules->Attach(eventlist, this, sizeof(eventlist)/sizeof(Implementation));
	}

	void ReadConfig(ConfigReadStatus&)
	{
		NoisyOverride = ServerInstance->Config->GetTag("override")->getBool("noisy");
		om.OverrideTimeout = ServerInstance->Config->GetTag("override")->getInt("timeout", 30);
	}

	void OnPermissionCheck(PermissionData& perm)
	{
		if (IS_LOCAL(perm.source) && perm.result != MOD_RES_ALLOW && perm.source->IsModeSet('O') && 
			perm.source->HasPrivPermission("override/" + perm.name))
		{
			perm.result = MOD_RES_ALLOW;
			// Override on exemptions is just noise
			if (perm.name.substr(0,7) == "exempt/")
				return;
			std::string msg = perm.source->nick+" used oper override for "+perm.name+" on "+
				(perm.chan ? perm.chan->name : "<none>");
			ServerInstance->SNO->WriteGlobalSno('v', msg);
			if (perm.chan && NoisyOverride)
			{
				CUList empty;
				perm.chan->WriteAllExcept(ServerInstance->FakeClient, true, '@', empty, "NOTICE @%s :%s", perm.chan->name.c_str(), msg.c_str());
				ServerInstance->PI->SendChannelNotice(perm.chan, '@', msg);
			}
		}
	}

	void OnBackgroundTimer(time_t Now)
	{
		for(std::list<User*>::iterator i = ServerInstance->Users->all_opers.begin(); i != ServerInstance->Users->all_opers.end(); i++)
		{
			User* u = *i;
			if (IS_LOCAL(u) && u->IsModeSet('O') && Now > om.timeout.get(u))
			{
				irc::modestacker ms;
				ms.push(irc::modechange(om.id, "", false));
				ServerInstance->SendMode(ServerInstance->FakeClient, u, ms, true);
			}
		}
	}

	void Prioritize()
	{
		ServerInstance->Modules->SetPriority(this, I_OnPermissionCheck, PRIORITY_LAST);
	}

	Version GetVersion()
	{
		return Version("Provides support for unreal-style oper-override",VF_VENDOR);
	}
};

MODULE_INIT(ModuleOverride)
