/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2018 linuxdaemon <linuxdaemon.irc@gmail.com>
 *   Copyright (C) 2017-2018 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2014-2015, 2018 Attila Molnar <attilamolnar@hush.com>
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

namespace DieRestart {
/** Send an ERROR to unregistered users and a NOTICE to all registered local users
 * @param message Message to send
 */
void SendError(const std::string& message);
}

/** Handle /DIE.
 */
class CommandDie : public Command {
  public:
    std::string& hash;
    std::string password;

    /** Constructor for die.
     */
    CommandDie(Module* parent, std::string& hashref);

    /** Handle command.
     * @param parameters The parameters to the command
     * @param user The user issuing the command
     * @return A value from CmdResult to indicate command success or failure.
     */
    CmdResult Handle(User* user, const Params& parameters) CXX11_OVERRIDE;
};

/** Handle /KILL.
 */
class CommandKill : public Command {
    std::string lastuuid;
    std::string killreason;
    ClientProtocol::EventProvider protoev;

  public:
    /** Set to a non empty string to obfuscate nicknames prepended to a KILL. */
    std::string hidenick;

    /** Set to hide kills from clients of ulined servers in snotices. */
    bool hideuline;

    /** Constructor for kill.
     */
    CommandKill(Module* parent);

    /** Handle command.
     * @param user User issuing the command
     * @param parameters Parameters to the command
     * @return A value from CmdResult to indicate command success or failure.
     */
    CmdResult Handle(User* user, const Params& parameters) CXX11_OVERRIDE;

    RouteDescriptor GetRouting(User* user, const Params& parameters) CXX11_OVERRIDE;

    void EncodeParameter(std::string& param, unsigned int index) CXX11_OVERRIDE;
};

/** Handle /OPER.
 */
class CommandOper : public SplitCommand {
  public:
    /** Constructor for oper.
     */
    CommandOper(Module* parent);

    /** Handle command.
     * @param user User issuing the command
     * @param parameters Parameters to the command
     * @return A value from CmdResult to indicate command success or failure.
     */
    CmdResult HandleLocal(LocalUser* user, const Params& parameters) CXX11_OVERRIDE;
};

/** Handle /REHASH.
 */
class CommandRehash : public Command {
  public:
    /** Constructor for rehash.
     */
    CommandRehash(Module* parent);

    /** Handle command.
     * @param user User issuing the command
     * @param parameters Parameters to the command
     * @return A value from CmdResult to indicate command success or failure.
     */
    CmdResult Handle(User* user, const Params& parameters) CXX11_OVERRIDE;
};

/** Handle /RESTART
 */
class CommandRestart : public Command {
  public:
    std::string& hash;
    std::string password;

    /** Constructor for restart.
     */
    CommandRestart(Module* parent, std::string& hashref);

    /** Handle command.
     * @param user User issuing the command
     * @param parameters Parameters to the command
     * @return A value from CmdResult to indicate command success or failure.
     */
    CmdResult Handle(User* user, const Params& parameters) CXX11_OVERRIDE;
};
