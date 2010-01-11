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

/* $ModDesc: Implements extban +b r: - realname (gecos) bans */

class ModuleGecosBan : public Module
{
 public:
	ModuleGecosBan()
	{
		Implementation eventlist[] = { I_OnCheckBan, I_On005Numeric };
		ServerInstance->Modules->Attach(eventlist, this, 2);
	}

	~ModuleGecosBan()
	{
	}

	Version GetVersion()
	{
		return Version("Extban 'r' - realname (gecos) ban", VF_COMMON|VF_VENDOR);
	}

	ModResult OnCheckBan(User *user, Channel *c, const std::string& mask)
	{
		if (mask[0] == 'r' && mask[1] == ':')
		{
			if (InspIRCd::Match(user->fullname, mask.substr(2)))
				return MOD_RES_DENY;
		}
		return MOD_RES_PASSTHRU;
	}

	void On005Numeric(std::string &output)
	{
		ServerInstance->AddExtBanChar('r');
	}
};

MODULE_INIT(ModuleGecosBan)
