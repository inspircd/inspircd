/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2012 InspIRCd Development Team
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
	ModuleRegOnlyCreate()
	{
		Implementation eventlist[] = { I_OnUserPreJoin };
		ServerInstance->Modules->Attach(eventlist, this, 1);
	}

	ModResult OnUserPreJoin(User* user, Channel* chan, const char* cname, std::string &privs, const std::string &keygiven)
	{
		if (chan)
			return MOD_RES_PASSTHRU;

		if (IS_OPER(user))
			return MOD_RES_PASSTHRU;

		if (user->IsModeSet('r'))
			return MOD_RES_PASSTHRU;

		const AccountExtItem* ext = GetAccountExtItem();
		if (ext && ext->get(user))
			return MOD_RES_PASSTHRU;

		// XXX. there may be a better numeric for this..
		user->WriteNumeric(ERR_CHANOPRIVSNEEDED, "%s %s :You must have a registered nickname to create a new channel", user->nick.c_str(), cname);
		return MOD_RES_DENY;
	}

	~ModuleRegOnlyCreate()
	{
	}

	Version GetVersion()
	{
		return Version("Prevents users whose nicks are not registered from creating new channels", VF_VENDOR);
	}
};

MODULE_INIT(ModuleRegOnlyCreate)
