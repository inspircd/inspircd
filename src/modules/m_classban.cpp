/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2016 Johanna Abrahamsson <johanna-a@mjao.org>
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

class ModuleClassBan : public Module
{
 public:
	ModResult OnCheckBan(User* user, Channel* c, const std::string& mask) override
	{
		LocalUser* localUser = IS_LOCAL(user);
		if ((localUser) && (mask.length() > 2) && (mask[0] == 'n') && (mask[1] == ':'))
		{
			if (InspIRCd::Match(localUser->GetClass()->name, mask.substr(2)))
				return MOD_RES_DENY;
		}
		return MOD_RES_PASSTHRU;
	}

	void On005Numeric(std::map<std::string, std::string>& tokens) override
	{
		tokens["EXTBAN"].push_back('n');
	}

	Version GetVersion() override
	{
		return Version("Class 'n' - Connection class ban", VF_VENDOR | VF_OPTCOMMON);
	}
};

MODULE_INIT(ModuleClassBan)
