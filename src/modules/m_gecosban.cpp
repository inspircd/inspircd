/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2008 Robin Burchell <robin+git@viroteck.net>
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


#include "inspircd.h"

class ModuleGecosBan : public Module
{
 public:
	Version GetVersion() CXX11_OVERRIDE
	{
		return Version("Extban 'r' - realname (gecos) ban", VF_OPTCOMMON|VF_VENDOR);
	}

	ModResult OnCheckBan(User *user, Channel *c, const std::string& mask) CXX11_OVERRIDE
	{
		if ((mask.length() > 2) && (mask[1] == ':'))
		{
			if (mask[0] == 'r')
			{
				if (InspIRCd::Match(user->fullname, mask.substr(2)))
					return MOD_RES_DENY;
			}
			else if (mask[0] == 'a')
			{
				std::string submask = mask.substr(2);
				std::string::size_type divider = submask.find('+');
				if (divider != std::string::npos &&
					c->CheckBan(user, submask.substr(0, divider)) &&
					InspIRCd::Match(user->fullname, submask.substr(divider + 1)))
					return MOD_RES_DENY;
			}
		}
		return MOD_RES_PASSTHRU;
	}

	void On005Numeric(std::map<std::string, std::string>& tokens) CXX11_OVERRIDE
	{
		tokens["EXTBAN"].push_back('a');
		tokens["EXTBAN"].push_back('r');
	}
};

MODULE_INIT(ModuleGecosBan)
