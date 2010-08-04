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

class ModuleOverride : public Module
{
	bool RequireKey;
	bool NoisyOverride;

 public:
	void init()
	{
		// read our config options (main config file)
		OnRehash(NULL);
		ServerInstance->SNO->EnableSnomask('v', "OVERRIDE");
		Implementation eventlist[] = { I_OnRehash, I_On005Numeric, I_OnPermissionCheck };
		ServerInstance->Modules->Attach(eventlist, this, 3);
	}

	void OnRehash(User* user)
	{
		ConfigReader Conf;

		// re-read our config options on a rehash
		NoisyOverride = Conf.ReadFlag("override", "noisy", 0);
		RequireKey = Conf.ReadFlag("override", "requirekey", 0);
	}

	void On005Numeric(std::string &output)
	{
		output.append(" OVERRIDE");
	}

	void OnPermissionCheck(PermissionData& perm)
	{
		if (IS_LOCAL(perm.source) && perm.source->HasPrivPermission("override/" + perm.name))
		{
			ServerInstance->SNO->WriteGlobalSno('v',perm.source->nick+" used oper override for "+perm.name+" on "+
				(perm.chan ? perm.chan->name : "<none>"));

			perm.result = MOD_RES_ALLOW;
		}
	}

	void OnCheckJoin(ChannelPermissionData& join)
	{
		if (!RequireKey || join.key == "override")
			OnPermissionCheck(join);
	}

	Version GetVersion()
	{
		return Version("Provides support for unreal-style oper-override",VF_VENDOR);
	}
};

MODULE_INIT(ModuleOverride)
