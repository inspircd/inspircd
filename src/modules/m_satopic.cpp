/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2008 Oliver Lupton <oliverlupton@gmail.com>
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


/* $ModDesc: Provides a SATOPIC command */

#include "inspircd.h"

/** Handle /SATOPIC
 */
class CommandSATopic : public Command
{
 public:
	CommandSATopic(Module* Creator) : Command(Creator,"SATOPIC", 2, 2)
	{
		flags_needed = 'o'; Penalty = 0; syntax = "<target> <topic>";
	}

	CmdResult Handle (const std::vector<std::string>& parameters, User *user)
	{
		/*
		 * Handles a SATOPIC request. Notifies all +s users.
	 	 */
		Channel* target = ServerInstance->FindChan(parameters[0]);

		if(target)
		{
			std::string newTopic = parameters[1];

			// 3rd parameter overrides access checks
			target->SetTopic(user, newTopic, true);
			ServerInstance->SNO->WriteGlobalSno('a', user->nick + " used SATOPIC on " + target->name + ", new topic: " + newTopic);

			return CMD_SUCCESS;
		}
		else
		{
			user->WriteNumeric(401, "%s %s :No such nick/channel", user->nick.c_str(), parameters[0].c_str());
			return CMD_FAILURE;
		}
	}
};

class ModuleSATopic : public Module
{
	CommandSATopic cmd;
 public:
	ModuleSATopic()
	: cmd(this)
	{
	}

	void init()
	{
		ServerInstance->Modules->AddService(cmd);
	}

	virtual ~ModuleSATopic()
	{
	}

	virtual Version GetVersion()
	{
		return Version("Provides a SATOPIC command", VF_VENDOR);
	}
};

MODULE_INIT(ModuleSATopic)
