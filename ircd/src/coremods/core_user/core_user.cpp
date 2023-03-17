/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2017-2021 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2014-2016, 2018 Attila Molnar <attilamolnar@hush.com>
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
#include "core_user.h"

/** Handle /PASS.
 */
class CommandPass : public SplitCommand {
  public:
    /** Constructor for pass.
     */
    CommandPass(Module* parent)
        : SplitCommand(parent, "PASS", 1, 1) {
        works_before_reg = true;
        Penalty = 0;
        syntax = "<password>";
    }

    /** Handle command.
     * @param parameters The parameters to the command
     * @param user The user issuing the command
     * @return A value from CmdResult to indicate command success or failure.
     */
    CmdResult HandleLocal(LocalUser* user,
                          const Params& parameters) CXX11_OVERRIDE {
        // Check to make sure they haven't registered -- Fix by FCS
        if (user->registered == REG_ALL) {
            user->CommandFloodPenalty += 1000;
            user->WriteNumeric(ERR_ALREADYREGISTERED, "You may not reregister");
            return CMD_FAILURE;
        }
        user->password = parameters[0];

        return CMD_SUCCESS;
    }
};

/** Handle /PING.
 */
class CommandPing : public SplitCommand {
  public:
    /** Constructor for ping.
     */
    CommandPing(Module* parent)
        : SplitCommand(parent, "PING", 1) {
        syntax = "<cookie> [<servername>]";
    }

    /** Handle command.
     * @param parameters The parameters to the command
     * @param user The user issuing the command
     * @return A value from CmdResult to indicate command success or failure.
     */
    CmdResult HandleLocal(LocalUser* user,
                          const Params& parameters) CXX11_OVERRIDE {
        size_t origin = parameters.size() > 1 ? 1 : 0;
        if (parameters[origin].empty()) {
            user->WriteNumeric(ERR_NOORIGIN, "No origin specified");
            return CMD_FAILURE;
        }

        ClientProtocol::Messages::Pong pong(parameters[0], origin ? parameters[1] : ServerInstance->Config->GetServerName());
        user->Send(ServerInstance->GetRFCEvents().pong, pong);
        return CMD_SUCCESS;
    }
};

/** Handle /PONG.
 */
class CommandPong : public Command {
  public:
    /** Constructor for pong.
     */
    CommandPong(Module* parent)
        : Command(parent, "PONG", 1) {
        Penalty = 0;
        syntax = "<cookie> [<servername>]";
    }

    /** Handle command.
     * @param parameters The parameters to the command
     * @param user The user issuing the command
     * @return A value from CmdResult to indicate command success or failure.
     */
    CmdResult Handle(User* user, const Params& parameters) CXX11_OVERRIDE {
        size_t origin = parameters.size() > 1 ? 1 : 0;
        if (parameters[origin].empty()) {
            user->WriteNumeric(ERR_NOORIGIN, "No origin specified");
            return CMD_FAILURE;
        }

        // set the user as alive so they survive to next ping
        LocalUser* localuser = IS_LOCAL(user);
        if (localuser) {
            // Increase penalty unless we've sent a PING and this is the reply
            if (localuser->lastping) {
                localuser->CommandFloodPenalty += 1000;
            } else {
                localuser->lastping = 1;
            }
        }
        return CMD_SUCCESS;
    }
};

void MessageWrapper::Wrap(const std::string& message, std::string& out) {
    // If there is a fixed message, it is stored in prefix. Otherwise prefix contains
    // only the prefix, so append the message and the suffix
    out.assign(prefix);
    if (!fixed) {
        out.append(message).append(suffix);
    }
}

void MessageWrapper::ReadConfig(const char* prefixname, const char* suffixname,
                                const char* fixedname) {
    ConfigTag* tag = ServerInstance->Config->ConfValue("options");
    prefix = tag->getString(fixedname);
    fixed = (!prefix.empty());
    if (!fixed) {
        prefix = tag->getString(prefixname);
        suffix = tag->getString(suffixname);
    }
}

class CoreModUser : public Module {
    CommandAway cmdaway;
    CommandNick cmdnick;
    CommandPart cmdpart;
    CommandPass cmdpass;
    CommandPing cmdping;
    CommandPong cmdpong;
    CommandQuit cmdquit;
    CommandUser cmduser;
    CommandIson cmdison;
    CommandUserhost cmduserhost;
    SimpleUserModeHandler invisiblemode;
    ModeUserOperator operatormode;
    ModeUserServerNoticeMask snomaskmode;

  public:
    CoreModUser()
        : cmdaway(this)
        , cmdnick(this)
        , cmdpart(this)
        , cmdpass(this)
        , cmdping(this)
        , cmdpong(this)
        , cmdquit(this)
        , cmduser(this)
        , cmdison(this)
        , cmduserhost(this)
        , invisiblemode(this, "invisible", 'i')
        , operatormode(this)
        , snomaskmode(this) {
    }

    void ReadConfig(ConfigStatus& status) CXX11_OVERRIDE {
        cmdpart.msgwrap.ReadConfig("prefixpart", "suffixpart", "fixedpart");
        cmdquit.msgwrap.ReadConfig("prefixquit", "suffixquit", "fixedquit");
    }

    Version GetVersion() CXX11_OVERRIDE {
        return Version("Provides the AWAY, ISON, NICK, PART, PASS, PING, PONG, QUIT, USERHOST, and USER commands", VF_VENDOR|VF_CORE);
    }
};

MODULE_INIT(CoreModUser)
