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

class ModuleMapHide : public Module
{
	std::string url;
 public:
	void init() CXX11_OVERRIDE
	{
		Implementation eventlist[] = { I_OnPreCommand, I_OnRehash };
		ServerInstance->Modules->Attach(eventlist, this, sizeof(eventlist)/sizeof(Implementation));
		OnRehash(NULL);
	}

	void OnRehash(User* user) CXX11_OVERRIDE
	{
		url = ServerInstance->Config->ConfValue("security")->getString("maphide");
	}

	ModResult OnPreCommand(std::string &command, std::vector<std::string> &parameters, LocalUser *user, bool validated, const std::string &original_line) CXX11_OVERRIDE
	{
		if (validated && !user->IsOper() && !url.empty() && (command == "MAP" || command == "LINKS"))
		{
			user->WriteNotice("/" + command + " has been disabled; visit " + url);
			return MOD_RES_DENY;
		}
		else
			return MOD_RES_PASSTHRU;
	}

	Version GetVersion() CXX11_OVERRIDE
	{
		return Version("Hide /MAP and /LINKS in the same form as ircu (mostly useless)", VF_VENDOR);
	}
};

MODULE_INIT(ModuleMapHide)
