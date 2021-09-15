/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2021 David Schultz <me@zpld.me>
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

class ModuleBadChannelExtban : public Module
{
 public:
	Version GetVersion() CXX11_OVERRIDE
	{
		return Version("Adds extended ban J: which checks whether users are banned from a certain channel.", VF_OPTCOMMON|VF_VENDOR);
	}

	ModResult OnCheckBan(User *user, Channel *c, const std::string& mask) CXX11_OVERRIDE
	{
		if ((mask.length() > 2) && (mask[0] == 'J') && (mask[1] == ':'))
		{
			std::string rm(mask, 2);
			Channel* chan = ServerInstance->FindChan(rm);

			if (chan && chan->IsBanned(user))
			{
				return MOD_RES_DENY;
			}
		}
		return MOD_RES_PASSTHRU;
	}

	void On005Numeric(std::map<std::string, std::string>& tokens) CXX11_OVERRIDE
	{
		tokens["EXTBAN"].push_back('J');
	}
};

MODULE_INIT(ModuleBadChannelExtban)
