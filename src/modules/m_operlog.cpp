/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2007 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2006 Craig Edwards <craigedwards@brainbox.cc>
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

/* $ModDesc: A module which logs all oper commands to the ircd log at default loglevel. */

class ModuleOperLog : public Module
{
 private:

 public:
	ModuleOperLog(InspIRCd* Me) : Module(Me)
	{

		Implementation eventlist[] = { I_OnPreCommand, I_On005Numeric };
		ServerInstance->Modules->Attach(eventlist, this, 2);
	}

	virtual ~ModuleOperLog()
	{
	}

	virtual Version GetVersion()
	{
		return Version("$Id$", VF_VENDOR, API_VERSION);
	}


	virtual int OnPreCommand(std::string &command, std::vector<std::string> &parameters, User *user, bool validated, const std::string &original_line)
	{
		/* If the command doesnt appear to be valid, we dont want to mess with it. */
		if (!validated)
			return 0;

		if ((IS_OPER(user)) && (IS_LOCAL(user)) && (user->HasPermission(command)))
		{
			Command* thiscommand = ServerInstance->Parser->GetHandler(command);
			if ((thiscommand) && (thiscommand->flags_needed == 'o'))
				ServerInstance->Logs->Log("m_operlog",DEFAULT,"OPERLOG: [%s!%s@%s] %s %s",user->nick.c_str(), user->ident.c_str(), user->host.c_str(), command.c_str(), parameters.empty() ? "" : irc::stringjoiner(" ", parameters, 0, parameters.size() - 1).GetJoined().c_str());
		}

		return 0;
	}

	virtual void On005Numeric(std::string &output)
	{
		output.append(" OPERLOG");
	}

};


MODULE_INIT(ModuleOperLog)
