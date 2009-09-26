/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2009 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"

/* $ModDesc: Prevents users who's nicks are not registered from creating new channels */

class ModuleRegOnlyCreate : public Module
{
 public:
	ModuleRegOnlyCreate()
			{
		Implementation eventlist[] = { I_OnUserPreJoin };
		ServerInstance->Modules->Attach(eventlist, this, 1);
	}


	virtual ModResult OnUserPreJoin(User* user, Channel* chan, const char* cname, std::string &privs, const std::string &keygiven)
	{
		if (chan)
			return MOD_RES_PASSTHRU;

		if (IS_OPER(user))
			return MOD_RES_PASSTHRU;

		if (user->GetExtList().find("accountname") == user->GetExtList().end() && !user->IsModeSet('r'))
		{
			// XXX. there may be a better numeric for this..
			user->WriteNumeric(ERR_CHANOPRIVSNEEDED, "%s %s :You must have a registered nickname to create a new channel", user->nick.c_str(), cname);
			return MOD_RES_DENY;
		}

		return MOD_RES_PASSTHRU;
	}

	virtual ~ModuleRegOnlyCreate()
	{
	}

	virtual Version GetVersion()
	{
		return Version("Prevents users who's nicks are not registered from creating new channels", VF_VENDOR, API_VERSION);
	}
};

MODULE_INIT(ModuleRegOnlyCreate)
