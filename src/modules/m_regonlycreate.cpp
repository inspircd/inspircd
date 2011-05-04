/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2011 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"
#include "account.h"

/* $ModDesc: Prevents users whose nicks are not registered from creating new channels */

class ModuleRegOnlyCreate : public Module
{
 public:
	dynamic_reference<AccountProvider> account;
	ModuleRegOnlyCreate() : account("account") {}

	void init()
	{
		Implementation eventlist[] = { I_OnCheckJoin };
		ServerInstance->Modules->Attach(eventlist, this, sizeof(eventlist)/sizeof(Implementation));
	}

	void OnCheckJoin(ChannelPermissionData& join)
	{
		if (join.chan || join.result != MOD_RES_PASSTHRU)
			return;

		if (IS_OPER(join.user))
			return;

		if (account && account->IsRegistered(join.user))
			return;

		join.ErrorNumeric(ERR_CHANOPRIVSNEEDED, "%s :You must have a registered nickname to create a new channel", join.channel.c_str());
		join.result = MOD_RES_DENY;
	}

	Version GetVersion()
	{
		return Version("Prevents users whose nicks are not registered from creating new channels", VF_VENDOR);
	}
};

MODULE_INIT(ModuleRegOnlyCreate)
