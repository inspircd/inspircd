/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
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

#ifndef CMD_ELINE_H
#define CMD_ELINE_H

// include the common header files

#include "users.h"
#include "channels.h"

/** Handle /ELINE. These command handlers can be reloaded by the core,
 * and handle basic RFC1459 commands. Commands within modules work
 * the same way, however, they can be fully unloaded, where these
 * may not.
 */
class CommandEline : public Command
{
 public:
	/** Constructor for eline.
	 */
	CommandEline ( Module* parent) : Command(parent,"ELINE",1,3) { flags_needed = 'o'; syntax = "<ident@host> [<duration> :<reason>]"; }
	/** Handle command.
	 * @param parameters The parameters to the comamnd
	 * @param pcnt The number of parameters passed to teh command
	 * @param user The user issuing the command
	 * @return A value from CmdResult to indicate command success or failure.
	 */
	CmdResult Handle(const std::vector<std::string>& parameters, User *user);
};

#endif


/** Handle /ELINE
 */
CmdResult CommandEline::Handle (const std::vector<std::string>& parameters, User *user)
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

		long duration = ServerInstance->Duration(parameters[1].c_str());

		ELine* el = new ELine(ServerInstance->Time(), duration, user->nick.c_str(), parameters[2].c_str(), ih.first.c_str(), ih.second.c_str());
		if (ServerInstance->XLines->AddLine(el, user))
		{
			if (!duration)
			{
				ServerInstance->SNO->WriteToSnoMask('x',"%s added permanent E-line for %s: %s", user->nick.c_str(), target.c_str(), parameters[2].c_str());
			}
			else
			{
				time_t c_requires_crap = duration + ServerInstance->Time();
				ServerInstance->SNO->WriteToSnoMask('x',"%s added timed E-line for %s, expires on %s: %s",user->nick.c_str(),target.c_str(),
						ServerInstance->TimeString(c_requires_crap).c_str(), parameters[2].c_str());
			}
		}
		else
		{
			delete el;
			user->WriteServ("NOTICE %s :*** E-Line for %s already exists",user->nick.c_str(),target.c_str());
		}
	}
	else
	{
		if (ServerInstance->XLines->DelLine(target.c_str(), "E", user))
		{
			ServerInstance->SNO->WriteToSnoMask('x',"%s removed E-line on %s",user->nick.c_str(),target.c_str());
		}
		else
		{
			user->WriteServ("NOTICE %s :*** E-Line %s not found in list, try /stats e.",user->nick.c_str(),target.c_str());
		}
	}

	return CMD_SUCCESS;
}

COMMAND_INIT(CommandEline)
