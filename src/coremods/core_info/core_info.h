/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2018, 2020 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2014, 2016 Attila Molnar <attilamolnar@hush.com>
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

/** These commands require no parameters, but if there is a parameter it is a server name where the command will be routed to.
 */
class ServerTargetCommand : public Command {
  public:
    ServerTargetCommand(Module* mod, const std::string& Name)
        : Command(mod, Name) {
    }

    RouteDescriptor GetRouting(User* user, const Params& parameters) CXX11_OVERRIDE;
};

/** Handle /ADMIN.
 */
class CommandAdmin : public ServerTargetCommand {
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
    CmdResult Handle(User* user, const Params& parameters) CXX11_OVERRIDE;
};

/** Handle /COMMANDS.
 */
class CommandCommands : public Command {
  public:
    /** Constructor for commands.
     */
    CommandCommands(Module* parent);

    /** Handle command.
     * @param parameters The parameters to the command
     * @param user The user issuing the command
     * @return A value from CmdResult to indicate command success or failure.
     */
    CmdResult Handle(User* user, const Params& parameters) CXX11_OVERRIDE;
};

/** Handle /INFO.
 */
class CommandInfo : public ServerTargetCommand {
  public:
    /** Constructor for info.
     */
    CommandInfo(Module* parent);

    /** Handle command.
     * @param parameters The parameters to the command
     * @param user The user issuing the command
     * @return A value from CmdResult to indicate command success or failure.
     */
    CmdResult Handle(User* user, const Params& parameters) CXX11_OVERRIDE;
};

/** Handle /MODULES.
 */
class CommandModules : public ServerTargetCommand {
  public:
    /** Constructor for modules.
     */
    CommandModules(Module* parent);

    /** Handle command.
     * @param parameters The parameters to the command
     * @param user The user issuing the command
     * @return A value from CmdResult to indicate command success or failure.
     */
    CmdResult Handle(User* user, const Params& parameters) CXX11_OVERRIDE;
};

/** Handle /MOTD.
 */
class CommandMotd : public ServerTargetCommand {
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
    CmdResult Handle(User* user, const Params& parameters) CXX11_OVERRIDE;
};

class CommandServList : public SplitCommand {
  private:
    UserModeReference invisiblemode;

  public:
    CommandServList(Module* parent);
    CmdResult HandleLocal(LocalUser* user, const Params& parameters) CXX11_OVERRIDE;
};

/** Handle /TIME.
 */
class CommandTime : public ServerTargetCommand {
  public:
    /** Constructor for time.
     */
    CommandTime(Module* parent);

    /** Handle command.
     * @param parameters The parameters to the command
     * @param user The user issuing the command
     * @return A value from CmdResult to indicate command success or failure.
     */
    CmdResult Handle(User* user, const Params& parameters) CXX11_OVERRIDE;
};

/** Handle /VERSION.
 */
class CommandVersion : public Command {
  public:
    /** Constructor for version.
     */
    CommandVersion(Module* parent);

    /** Handle command.
     * @param parameters The parameters to the command
     * @param user The user issuing the command
     * @return A value from CmdResult to indicate command success or failure.
     */
    CmdResult Handle(User* user, const Params& parameters) CXX11_OVERRIDE;
};
