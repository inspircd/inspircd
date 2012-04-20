/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2007 John Brooks <john.brooks@dereferenced.net>
 *   Copyright (C) 2007 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2006-2007 Craig Edwards <craigedwards@brainbox.cc>
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
#include "xline.h"

/* $ModDesc: Sends a numeric on connect which cripples a common type of trojan/spambot */

class ModuleAntiBear : public Module
{
 private:

 public:
	ModuleAntiBear(InspIRCd* Me) : Module(Me)
	{

		Implementation eventlist[] = { I_OnUserRegister, I_OnPreCommand };
		ServerInstance->Modules->Attach(eventlist, this, 2);
	}

	virtual ~ModuleAntiBear()
	{
	}

	virtual Version GetVersion()
	{
		return Version("$Id$",VF_VENDOR,API_VERSION);
	}

	virtual int OnPreCommand(std::string &command, std::vector<std::string> &parameters, User *user, bool validated, const std::string &original_line)
	{
		if (command == "NOTICE" && !validated && parameters.size() > 1 && user->GetExt("antibear_timewait"))
		{
			if (!strncmp(parameters[1].c_str(), "\1TIME Mon May 01 18:54:20 2006", 30))
			{
				ZLine* zl = new ZLine(ServerInstance, ServerInstance->Time(), 86400, ServerInstance->Config->ServerName,
						"Unless you're stuck in a time warp, you appear to be a bear bot!", user->GetIPString());
				if (ServerInstance->XLines->AddLine(zl,NULL))
				{
					ServerInstance->XLines->ApplyLines();
				}
				else
					delete zl;

				return 1;
			}

			user->Shrink("antibear_timewait");
			// Block the command, so the user doesn't receive a no such nick notice
			return 1;
		}

		return 0;
	}

	virtual int OnUserRegister(User* user)
	{
		user->WriteNumeric(439, "%s :This server has anti-spambot mechanisms enabled.", user->nick.c_str());
		user->WriteNumeric(931, "%s :Malicious bots, spammers, and other automated systems of dubious origin are NOT welcome here.", user->nick.c_str());
		user->WriteServ("PRIVMSG %s :\1TIME\1", user->nick.c_str());
		user->Extend("antibear_timewait");
		return 0;
	}
};

MODULE_INIT(ModuleAntiBear)
