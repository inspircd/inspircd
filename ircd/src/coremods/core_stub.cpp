/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2019 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2017-2020 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2014-2016 Attila Molnar <attilamolnar@hush.com>
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

enum {
    // From RFC 1459.
    ERR_SUMMONDISABLED = 445,
    ERR_USERSDISABLED = 446
};

class CommandCapab : public Command {
  public:
    CommandCapab(Module* parent)
        : Command(parent, "CAPAB") {
        works_before_reg = true;
    }

    CmdResult Handle(User* user, const Params& parameters) CXX11_OVERRIDE {
        if (user->registered == REG_NONE) {
            // The CAPAB command is used in the server protocol for negotiating
            // the protocol version when initiating a server connection. There
            // is no legitimate reason for a user to send this so we disconnect
            // users who sent it in order to help out server admins who have
            // misconfigured their server.
            ServerInstance->Users->QuitUser(user,
                                            "You can not connect a server to a client port. Read " INSPIRCD_DOCS
                                            "modules/spanningtree for docs on how to link a server.");
        }
        return CMD_FAILURE;
    }
};

/** Handle /CONNECT.
 */
class CommandConnect : public Command {
  public:
    /** Constructor for connect.
     */
    CommandConnect(Module* parent)
        : Command(parent, "CONNECT", 1) {
        flags_needed = 'o';
        syntax = "<servermask>";
    }

    /** Handle command.
     * @param parameters The parameters to the command
     * @param user The user issuing the command
     * @return A value from CmdResult to indicate command success or failure.
     */
    CmdResult Handle(User* user, const Params& parameters) CXX11_OVERRIDE {
        /*
         * This is handled by the server linking module, if necessary. Do not remove this stub.
         */
        user->WriteNotice("Look into loading a linking module (like m_spanningtree) if you want this to do anything useful.");
        return CMD_SUCCESS;
    }
};

/** Handle /LINKS.
 */
class CommandLinks : public Command {
  public:
    /** Constructor for links.
     */
    CommandLinks(Module* parent)
        : Command(parent, "LINKS", 0, 0) {
    }

    /** Handle command.
     * @param parameters The parameters to the command
     * @param user The user issuing the command
     * @return A value from CmdResult to indicate command success or failure.
     */
    CmdResult Handle(User* user, const Params& parameters) CXX11_OVERRIDE {
        user->WriteNumeric(RPL_LINKS, ServerInstance->Config->GetServerName(), ServerInstance->Config->GetServerName(), InspIRCd::Format("0 %s", ServerInstance->Config->GetServerDesc().c_str()));
        user->WriteNumeric(RPL_ENDOFLINKS, '*', "End of /LINKS list.");
        return CMD_SUCCESS;
    }
};

/** Handle /SQUIT.
 */
class CommandSquit : public Command {
  public:
    /** Constructor for squit.
     */
    CommandSquit(Module* parent)
        : Command(parent, "SQUIT", 1, 2) {
        flags_needed = 'o';
        syntax = "<servermask>";
    }

    /** Handle command.
     * @param parameters The parameters to the command
     * @param user The user issuing the command
     * @return A value from CmdResult to indicate command success or failure.
     */
    CmdResult Handle(User* user, const Params& parameters) CXX11_OVERRIDE {
        user->WriteNotice("Look into loading a linking module (like m_spanningtree) if you want this to do anything useful.");
        return CMD_FAILURE;
    }
};

class CommandSummon
    : public SplitCommand {
  public:
    CommandSummon(Module* Creator)
        : SplitCommand(Creator, "SUMMON", 1) {
    }

    CmdResult HandleLocal(LocalUser* user,
                          const Params& parameters) CXX11_OVERRIDE {
        user->WriteNumeric(ERR_SUMMONDISABLED, "SUMMON has been disabled");
        return CMD_SUCCESS;
    }
};

class CommandUsers
    : public SplitCommand {
  public:
    CommandUsers(Module* Creator)
        : SplitCommand(Creator, "USERS") {
    }

    CmdResult HandleLocal(LocalUser* user,
                          const Params& parameters) CXX11_OVERRIDE {
        user->WriteNumeric(ERR_USERSDISABLED, "USERS has been disabled");
        return CMD_SUCCESS;
    }
};

class CoreModStub : public Module {
  private:
    CommandCapab cmdcapab;
    CommandConnect cmdconnect;
    CommandLinks cmdlinks;
    CommandSquit cmdsquit;
    CommandSummon cmdsummon;
    CommandUsers cmdusers;

  public:
    CoreModStub()
        : cmdcapab(this)
        , cmdconnect(this)
        , cmdlinks(this)
        , cmdsquit(this)
        , cmdsummon(this)
        , cmdusers(this) {
    }

    Version GetVersion() CXX11_OVERRIDE {
        return Version("Provides stubs for unimplemented commands", VF_VENDOR|VF_CORE);
    }
};

MODULE_INIT(CoreModStub)
