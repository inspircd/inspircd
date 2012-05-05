/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2008 Craig Edwards <craigedwards@brainbox.cc>
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

/* $ModDesc: Hide /MAP and /LINKS in the same form as ircu (mostly useless) */

class ModuleMapHide : public Module
{
	std::string url;
 public:
	ModuleMapHide()
	{
		Implementation eventlist[] = { I_OnPreCommand, I_OnRehash };
		ServerInstance->Modules->Attach(eventlist, this, 2);
		OnRehash(NULL);
	}

	void OnRehash(User* user)
	{
		ConfigReader MyConf;
		url = MyConf.ReadValue("security", "maphide", 0);
	}

	ModResult OnPreCommand(std::string &command, std::vector<std::string> &parameters, LocalUser *user, bool validated, const std::string &original_line)
	{
		if (!IS_OPER(user) && !url.empty() && (command == "MAP" || command == "LINKS"))
		{
			user->WriteServ("NOTICE %s :/%s has been disabled; visit %s", user->nick.c_str(), command.c_str(), url.c_str());
			return MOD_RES_DENY;
		}
		else
			return MOD_RES_PASSTHRU;
	}

	virtual ~ModuleMapHide()
	{
	}

	virtual Version GetVersion()
	{
		return Version("Hide /MAP and /LINKS in the same form as ircu (mostly useless)", VF_VENDOR);
	}
};

MODULE_INIT(ModuleMapHide)

