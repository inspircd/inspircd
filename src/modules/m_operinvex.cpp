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
#include "u_listmode.h"

/* $ModDep: ../../include/u_listmode.h */

/* $ModDesc: Implements extban/invex +I O: - opertype bans */

class ModuleOperInvex : public Module
{
 private:
 public:
	ModuleOperInvex() 	{
		Implementation eventlist[] = { I_OnCheckBan, I_On005Numeric };
		ServerInstance->Modules->Attach(eventlist, this, 2);
	}

	~ModuleOperInvex()
	{
	}

	Version GetVersion()
	{
		return Version("ExtBan 'O' - oper type ban", VF_COMMON|VF_VENDOR);
	}

	ModResult OnCheckBan(User *user, Channel *c, const std::string& mask)
	{
		if (mask[0] == 'O' && mask[1] == ':')
		{
			if (IS_OPER(user) && InspIRCd::Match(user->oper, mask.substr(2)))
				return MOD_RES_DENY;
		}
		return MOD_RES_PASSTHRU;
	}

	virtual void On005Numeric(std::string &output)
	{
		ServerInstance->AddExtBanChar('O');
	}
};


MODULE_INIT(ModuleOperInvex)

