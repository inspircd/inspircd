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

/* $ModDesc: Allows for POSIX hashing of passwords */
/* $LinkerFlags: -lcrypt */

#include "inspircd.h"
#include "hash.h"

class ModulePosixCrypt : public Module
{
 public:
	ModulePosixCrypt()
	{
		Implementation eventlist[] = { I_OnPassCompare };
		ServerInstance->Modules->Attach(eventlist, this, 1);
	}

	ModResult OnPassCompare(Extensible* ex, const std::string &data, const std::string &input, const std::string &hashtype)
	{
		if (hashtype == "posix")
		{
			const char* res = crypt(input.c_str(), data.c_str());
			if (data == std::string(res))
				return MOD_RES_ALLOW;
			else
				return MOD_RES_DENY;
		}

		return MOD_RES_PASSTHRU;
	}

	virtual Version GetVersion()
	{
		return Version("Allows for hashed oper passwords",VF_VENDOR);
	}
};

MODULE_INIT(ModulePosixCrypt)
