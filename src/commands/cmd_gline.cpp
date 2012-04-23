/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2007-2008 Robin Burchell <robin+git@viroteck.net>
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
#include "commands/cmd_gline.h"

extern "C" DllExport Command* init_command(InspIRCd* Instance)
{
	return new CommandGline(Instance);
}

/** Handle /GLINE
 */
CmdResult CommandGline::Handle (const std::vector<std::string>& parameters, User *user)
{
	std::string target = parameters[0];

	if (parameters.size() >= 3)
	{
		IdentHostPair ih;
		User* find = ServerInstance->FindNick(target.c_str());
		if (find)
		{
			ih.first = "*";
			ih.second = find->GetIPString();
			target = std::string("*@") + find->GetIPString();
		}
		else
			ih = ServerInstance->XLines->IdentSplit(target.c_str());

		if (ih.first.empty())
		{
			user->WriteServ("NOTICE %s :*** Target not found", user->nick.c_str());
			return CMD_FAILURE;
		}

		if (ServerInstance->HostMatchesEveryone(ih.first+"@"+ih.second,user))
			return CMD_FAILURE;

		else if (target.find('!') != std::string::npos)
		{
			user->WriteServ("NOTICE %s :*** G-Line cannot operate on nick!user@host masks",user->nick.c_str());
			return CMD_FAILURE;
		}

		long duration = ServerInstance->Duration(parameters[1].c_str());
		GLine* gl = new GLine(ServerInstance, ServerInstance->Time(), duration, user->nick.c_str(), parameters[2].c_str(), ih.first.c_str(), ih.second.c_str());
		if (ServerInstance->XLines->AddLine(gl, user))
		{
			if (!duration)
			{
				ServerInstance->SNO->WriteToSnoMask('x',"%s added permanent G-line for %s: %s",user->nick.c_str(),target.c_str(), parameters[2].c_str());
			}
			else
			{
				time_t c_requires_crap = duration + ServerInstance->Time();
				ServerInstance->SNO->WriteToSnoMask('x',"%s added timed G-line for %s, expires on %s: %s",user->nick.c_str(),target.c_str(),
						ServerInstance->TimeString(c_requires_crap).c_str(), parameters[2].c_str());
			}

			ServerInstance->XLines->ApplyLines();
		}
		else
		{
			delete gl;
			user->WriteServ("NOTICE %s :*** G-Line for %s already exists",user->nick.c_str(),target.c_str());
		}

	}
	else
	{
		if (ServerInstance->XLines->DelLine(target.c_str(),"G",user))
		{
			ServerInstance->SNO->WriteToSnoMask('x',"%s Removed G-line on %s.",user->nick.c_str(),target.c_str());
		}
		else
		{
			user->WriteServ("NOTICE %s :*** G-line %s not found in list, try /stats g.",user->nick.c_str(),target.c_str());
		}
	}

	return CMD_SUCCESS;
}
