/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2010 Daniel De Graaf <danieldg@inspircd.org>
 *
 * This file is part of InspIRCd.  InspIRCd is free software: you can
 * redistribute it and/or modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation, version 2.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */


/* $ModDesc: Allows for POSIX hashing of passwords */
/* $LinkerFlags: if(!"IS_DARWIN") -lcrypt */

#include "inspircd.h"
#include "hash.h"

class ModulePosixCrypt : public Module
{
 public:
	void init()
	{
		Implementation eventlist[] = { I_OnPassCompare };
		ServerInstance->Modules->Attach(eventlist, this, sizeof(eventlist)/sizeof(Implementation));
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
