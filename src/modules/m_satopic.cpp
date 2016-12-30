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


#include "inspircd.h"

/** Handle /SATOPIC
 */
class CommandSATopic : public Command
{
 public:
	CommandSATopic(Module* Creator) : Command(Creator,"SATOPIC", 2, 2)
	{
		flags_needed = 'o'; syntax = "<target> <topic>";
	}

	CmdResult Handle (const std::vector<std::string>& parameters, User *user)
	{
		/*
		 * Handles a SATOPIC request. Notifies all +s users.
	 	 */
		Channel* target = ServerInstance->FindChan(parameters[0]);

		if(target)
		{
			const std::string newTopic(parameters[1], 0, ServerInstance->Config->Limits.MaxTopic);
			if (target->topic == newTopic)
			{
				user->WriteNotice(InspIRCd::Format("The topic on %s is already what you are trying to change it to.", target->name.c_str()));
				return CMD_SUCCESS;
			}

			target->SetTopic(user, newTopic, ServerInstance->Time(), NULL);
			ServerInstance->SNO->WriteGlobalSno('a', user->nick + " used SATOPIC on " + target->name + ", new topic: " + newTopic);

			return CMD_SUCCESS;
		}
		else
		{
			user->WriteNumeric(Numerics::NoSuchNick(parameters[0]));
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

	Version GetVersion() CXX11_OVERRIDE
	{
		return Version("Provides a SATOPIC command", VF_VENDOR);
	}
};

MODULE_INIT(ModuleSATopic)
