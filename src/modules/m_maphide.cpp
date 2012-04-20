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
	ModuleMapHide(InspIRCd* Me)
		: Module(Me)
	{
		// Create a new command
		ServerInstance->Modules->Attach(I_OnPreCommand, this);
		ServerInstance->Modules->Attach(I_OnRehash, this);
		OnRehash(NULL);
	}

	void OnRehash(User* user)
	{
		ConfigReader MyConf(ServerInstance);
		url = MyConf.ReadValue("security", "maphide", 0);
	}

	int OnPreCommand(std::string &command, std::vector<std::string> &parameters, User *user, bool validated, const std::string &original_line)
	{
		if (!IS_OPER(user) && !url.empty() && (command == "MAP" || command == "LINKS"))
		{
			user->WriteServ("NOTICE %s :/%s has been disabled; visit %s", user->nick.c_str(), command.c_str(), url.c_str());
			return 1;
		}
		else
			return 0;
	}

	virtual ~ModuleMapHide()
	{
	}

	virtual Version GetVersion()
	{
		return Version("$Id$", VF_VENDOR, API_VERSION);
	}
};

MODULE_INIT(ModuleMapHide)

