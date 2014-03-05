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

/** Handle /ADMIN.
 */
class CommandAdmin : public Command
{
 public:
	/** Constructor for admin.
	 */
	CommandAdmin(Module* parent);

	/** Handle command.
	 * @param parameters The parameters to the command
	 * @param user The user issuing the command
	 * @return A value from CmdResult to indicate command success or failure.
	 */
	CmdResult Handle(const std::vector<std::string>& parameters, User* user);
	RouteDescriptor GetRouting(User* user, const std::vector<std::string>& parameters);
};

/** Handle /COMMANDS.
 */
class CommandCommands : public Command
{
 public:
	/** Constructor for commands.
	 */
	CommandCommands(Module* parent);

	/** Handle command.
	 * @param parameters The parameters to the command
	 * @param user The user issuing the command
	 * @return A value from CmdResult to indicate command success or failure.
	 */
	CmdResult Handle(const std::vector<std::string>& parameters, User* user);
};

/** Handle /INFO.
 */
class CommandInfo : public Command
{
 public:
	/** Constructor for info.
	 */
	CommandInfo(Module* parent);

	/** Handle command.
	 * @param parameters The parameters to the command
	 * @param user The user issuing the command
	 * @return A value from CmdResult to indicate command success or failure.
	 */
	CmdResult Handle(const std::vector<std::string>& parameters, User* user);
	RouteDescriptor GetRouting(User* user, const std::vector<std::string>& parameters);
};

/** Handle /MODULES.
 */
class CommandModules : public Command
{
 public:
	/** Constructor for modules.
	 */
	CommandModules(Module* parent);

	/** Handle command.
	 * @param parameters The parameters to the command
	 * @param user The user issuing the command
	 * @return A value from CmdResult to indicate command success or failure.
	 */
	CmdResult Handle(const std::vector<std::string>& parameters, User* user);
	RouteDescriptor GetRouting(User* user, const std::vector<std::string>& parameters);
};

/** Handle /MOTD.
 */
class CommandMotd : public Command
{
 public:
	/** Constructor for motd.
	 */
	CommandMotd(Module* parent);

	/** Handle command.
	 * @param parameters The parameters to the command
	 * @param user The user issuing the command
	 * @return A value from CmdResult to indicate command success or failure.
	 */
	CmdResult Handle(const std::vector<std::string>& parameters, User* user);
	RouteDescriptor GetRouting(User* user, const std::vector<std::string>& parameters);
};

/** Handle /TIME.
 */
class CommandTime : public Command
{
 public:
	/** Constructor for time.
	 */
	CommandTime(Module* parent);

	/** Handle command.
	 * @param parameters The parameters to the command
	 * @param user The user issuing the command
	 * @return A value from CmdResult to indicate command success or failure.
	 */
	CmdResult Handle(const std::vector<std::string>& parameters, User* user);
	RouteDescriptor GetRouting(User* user, const std::vector<std::string>& parameters);
};

/** Handle /VERSION.
 */
class CommandVersion : public Command
{
 public:
	/** Constructor for version.
	 */
	CommandVersion(Module* parent);

	/** Handle command.
	 * @param parameters The parameters to the command
	 * @param user The user issuing the command
	 * @return A value from CmdResult to indicate command success or failure.
	 */
	CmdResult Handle(const std::vector<std::string>& parameters, User* user);
};
