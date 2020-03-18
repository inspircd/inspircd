/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2014 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2013-2020 Sadie Powell <sadie@witchery.services>
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
#include "modules/isupport.h"

/** This class manages the generation and transmission of ISUPPORT. */
class CoreExport ISupportManager
{
 private:
	/** The generated lines which are sent to clients. */
	std::vector<Numeric::Numeric> cachedlines;

	/** Provider for the ISupport::EventListener event. */
	ISupport::EventProvider isupportevprov;

	/** Escapes an ISUPPORT token value and appends it to the buffer.
	 * @param buffer The buffer to append to.
	 * @param value An ISUPPORT token value.
	 */
	void AppendValue(std::string& buffer, const std::string& value);

 public:
	ISupportManager(Module* mod);

	/** (Re)build the ISUPPORT vector.
	 * Called by the core on boot after all modules have been loaded, and every time when a module is loaded
	 * or unloaded. Calls the OnBuildISupport hook, letting modules manipulate the ISUPPORT tokens.
	 */
	void Build();

	/** Returns the cached std::vector of ISUPPORT lines.
	 * @return A list of Numeric::Numeric objects prepared for sending to users
	 */
	const std::vector<Numeric::Numeric>& GetLines() const { return cachedlines; }

	/** Send the 005 numerics (ISUPPORT) to a user.
	 * @param user The user to send the ISUPPORT numerics to
	 */
	void SendTo(LocalUser* user);
};

/** These commands require no parameters, but if there is a parameter it is a server name where the command will be routed to.
 */
class ServerTargetCommand : public Command
{
 public:
	ServerTargetCommand(Module* mod, const std::string& Name)
		: Command(mod, Name)
	{
	}

	RouteDescriptor GetRouting(User* user, const Params& parameters) override;
};

/** Handle /ADMIN.
 */
class CommandAdmin : public ServerTargetCommand
{
 public:
	/** Holds the admin's name, for output in
	 * the /ADMIN command.
	 */
	std::string AdminName;

	/** Holds the email address of the admin,
	 * for output in the /ADMIN command.
	 */
	std::string AdminEmail;

	/** Holds the admin's nickname, for output
	 * in the /ADMIN command
	 */
	std::string AdminNick;

	/** Constructor for admin.
	 */
	CommandAdmin(Module* parent);

	/** Handle command.
	 * @param parameters The parameters to the command
	 * @param user The user issuing the command
	 * @return A value from CmdResult to indicate command success or failure.
	 */
	CmdResult Handle(User* user, const Params& parameters) override;
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
	CmdResult Handle(User* user, const Params& parameters) override;
};

/** Handle /INFO.
 */
class CommandInfo : public ServerTargetCommand
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
	CmdResult Handle(User* user, const Params& parameters) override;
};

/** Handle /MODULES.
 */
class CommandModules : public ServerTargetCommand
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
	CmdResult Handle(User* user, const Params& parameters) override;
};

/** Handle /MOTD.
 */
class CommandMotd : public ServerTargetCommand
{
 public:
	ConfigFileCache motds;

	/** Constructor for motd.
	 */
	CommandMotd(Module* parent);

	/** Handle command.
	 * @param parameters The parameters to the command
	 * @param user The user issuing the command
	 * @return A value from CmdResult to indicate command success or failure.
	 */
	CmdResult Handle(User* user, const Params& parameters) override;
};

class CommandServList : public SplitCommand
{
 private:
	UserModeReference invisiblemode;

 public:
	CommandServList(Module* parent);
	CmdResult HandleLocal(LocalUser* user, const Params& parameters) override;
};

/** Handle /TIME.
 */
class CommandTime : public ServerTargetCommand
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
	CmdResult Handle(User* user, const Params& parameters) override;
};

/** Handle /VERSION.
 */
class CommandVersion : public Command
{
 private:
	ISupportManager& isupport;

 public:
	/** Constructor for version.
	 */
	CommandVersion(Module* parent, ISupportManager& isupportmgr);

	/** Handle command.
	 * @param parameters The parameters to the command
	 * @param user The user issuing the command
	 * @return A value from CmdResult to indicate command success or failure.
	 */
	CmdResult Handle(User* user, const Params& parameters) override;
};
