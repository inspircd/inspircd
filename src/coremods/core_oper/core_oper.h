/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2014 Attila Molnar <attilamolnar@hush.com>
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


#pragma once

#include "inspircd.h"

namespace DieRestart
{
	/** Checks a die or restart password
	 * @param user The user executing /DIE or /RESTART
	 * @param inputpass The password given by the user
	 * @param confkey The name of the key in the power tag containing the correct password
	 * @return True if the given password was correct, false if it was not
	 */
	bool CheckPass(User* user, const std::string& inputpass, const char* confkey);

	/** Send an ERROR to unregistered users and a NOTICE to all registered local users
	 * @param message Message to send
	 */
	void SendError(const std::string& message);
}

/** Handle /DIE.
 */
class CommandDie : public Command
{
 public:
	/** Constructor for die.
	 */
	CommandDie(Module* parent);

	/** Handle command.
	 * @param parameters The parameters to the command
	 * @param user The user issuing the command
	 * @return A value from CmdResult to indicate command success or failure.
	 */
	CmdResult Handle(const std::vector<std::string>& parameters, User* user);
};

/** Handle /KILL.
 */
class CommandKill : public Command
{
	std::string lastuuid;
	std::string killreason;

 public:
	/** Constructor for kill.
	 */
	CommandKill(Module* parent);

	/** Handle command.
	 * @param parameters The parameters to the command
	 * @param user The user issuing the command
	 * @return A value from CmdResult to indicate command success or failure.
	 */
	CmdResult Handle(const std::vector<std::string>& parameters, User* user);
	RouteDescriptor GetRouting(User* user, const std::vector<std::string>& parameters);

	void EncodeParameter(std::string& param, int index);
};

/** Handle /OPER.
 */
class CommandOper : public SplitCommand
{
 public:
	/** Constructor for oper.
	 */
	CommandOper(Module* parent);

	/** Handle command.
	 * @param parameters The parameters to the command
	 * @param user The user issuing the command
	 * @return A value from CmdResult to indicate command success or failure.
	 */
	CmdResult HandleLocal(const std::vector<std::string>& parameters, LocalUser* user);
};

/** Handle /REHASH.
 */
class CommandRehash : public Command
{
 public:
	/** Constructor for rehash.
	 */
	CommandRehash(Module* parent);

	/** Handle command.
	 * @param parameters The parameters to the command
	 * @param user The user issuing the command
	 * @return A value from CmdResult to indicate command success or failure.
	 */
	CmdResult Handle(const std::vector<std::string>& parameters, User *user);
};

/** Handle /RESTART
 */
class CommandRestart : public Command
{
 public:
	/** Constructor for restart.
	 */
	CommandRestart(Module* parent);

	/** Handle command.
	 * @param parameters The parameters to the command
	 * @param user The user issuing the command
	 * @return A value from CmdResult to indicate command success or failure.
	 */
	CmdResult Handle(const std::vector<std::string>& parameters, User* user);
};
