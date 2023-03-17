/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2019 linuxdaemon <linuxdaemon.irc@gmail.com>
 *   Copyright (C) 2019 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2018-2020, 2022 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2017 B00mX0r <b00mx0r@aureus.pw>
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

class CommandMode : public Command {
  private:
    unsigned int sent[256];
    unsigned int seq;
    ChanModeReference secretmode;
    ChanModeReference privatemode;
    UserModeReference snomaskmode;

    /** Show the list of one or more list modes to a user.
     * @param user User to send to.
     * @param chan Channel whose lists to show.
     * @param mode_sequence Mode letters to show the lists of.
     */
    void DisplayListModes(User* user, Channel* chan,
                          const std::string& mode_sequence);

    /** Show the current modes of a channel or a user to a user.
     * @param user User to show the modes to.
     * @param targetuser User whose modes to show. NULL if showing the modes of a channel.
     * @param targetchannel Channel whose modes to show. NULL if showing the modes of a user.
     */
    void DisplayCurrentModes(User* user, User* targetuser, Channel* targetchannel);

    bool CanSeeChan(User* user, Channel* chan) {
        // A user can always see the channel modes if they are:
        // (1) In the channel.
        // (2) An oper with the channels/auspex privilege.
        if (chan->HasUser(user) ||  user->HasPrivPermission("channels/auspex")) {
            return true;
        }

        // Otherwise, they can only see the modes when the channel is not +p or +s.
        return !chan->IsModeSet(secretmode) && !chan->IsModeSet(privatemode);
    }

    std::string GetSnomasks(const User* user);

  public:
    /** Constructor for mode.
     */
    CommandMode(Module* parent);

    /** Handle command.
     * @param parameters The parameters to the command
     * @param user The user issuing the command
     * @return A value from CmdResult to indicate command success or failure.
     */
    CmdResult Handle(User* user, const Params& parameters) CXX11_OVERRIDE;

    RouteDescriptor GetRouting(User* user, const Params& parameters) CXX11_OVERRIDE;
};

CommandMode::CommandMode(Module* parent)
    : Command(parent, "MODE", 1)
    , seq(0)
    , secretmode(creator, "secret")
    , privatemode(creator, "private")
    , snomaskmode(creator, "snomask") {
    syntax = "<target> [[(+|-)]<modes> [<mode-parameters>]]";
    memset(&sent, 0, sizeof(sent));
}

CmdResult CommandMode::Handle(User* user, const Params& parameters) {
    const std::string& target = parameters[0];
    Channel* targetchannel = ServerInstance->FindChan(target);
    User* targetuser = NULL;
    if (!targetchannel) {
        if (IS_LOCAL(user)) {
            targetuser = ServerInstance->FindNickOnly(target);
        } else {
            targetuser = ServerInstance->FindNick(target);
        }
    }

    if ((!targetchannel || !CanSeeChan(user, targetchannel)) && (!targetuser)) {
        if (target[0] == '#') {
            user->WriteNumeric(Numerics::NoSuchChannel(target));
        } else {
            user->WriteNumeric(Numerics::NoSuchNick(target));
        }
        return CMD_FAILURE;
    }
    if (parameters.size() == 1) {
        this->DisplayCurrentModes(user, targetuser, targetchannel);
        return CMD_SUCCESS;
    }

    // Populate a temporary Modes::ChangeList with the parameters
    Modes::ChangeList changelist;
    ModeType type = targetchannel ? MODETYPE_CHANNEL : MODETYPE_USER;
    ServerInstance->Modes.ModeParamsToChangeList(user, type, parameters,
            changelist);

    ModResult MOD_RESULT;
    FIRST_MOD_RESULT(OnPreMode, MOD_RESULT, (user, targetuser, targetchannel,
                     changelist));

    ModeParser::ModeProcessFlag flags = ModeParser::MODE_NONE;
    if (IS_LOCAL(user)) {
        if (MOD_RESULT == MOD_RES_PASSTHRU) {
            if ((targetuser) && (user != targetuser)) {
                // Local users may only change the modes of other users if a module explicitly allows it
                user->WriteNumeric(ERR_USERSDONTMATCH, "Can't change mode for other users");
                return CMD_FAILURE;
            }

            // This is a mode change by a local user and modules didn't explicitly allow/deny.
            // Ensure access checks will happen for each mode being changed.
            flags |= ModeParser::MODE_CHECKACCESS;
        } else if (MOD_RESULT == MOD_RES_DENY) {
            return CMD_FAILURE;    // Entire mode change denied by a module
        }
    } else {
        flags |= ModeParser::MODE_LOCALONLY;
    }

    if (IS_LOCAL(user)) {
        ServerInstance->Modes->ProcessSingle(user, targetchannel, targetuser,
                                             changelist, flags);
    } else {
        ServerInstance->Modes->Process(user, targetchannel, targetuser, changelist,
                                       flags);
    }

    if ((ServerInstance->Modes.GetLastChangeList().empty()) && (targetchannel)
            && (parameters.size() == 2)) {
        /* Special case for displaying the list for listmodes,
         * e.g. MODE #chan b, or MODE #chan +b without a parameter
         */
        this->DisplayListModes(user, targetchannel, parameters[1]);
    }

    return CMD_SUCCESS;
}

RouteDescriptor CommandMode::GetRouting(User* user, const Params& parameters) {
    return (IS_LOCAL(user) ? ROUTE_LOCALONLY : ROUTE_BROADCAST);
}

void CommandMode::DisplayListModes(User* user, Channel* chan,
                                   const std::string& mode_sequence) {
    seq++;

    for (std::string::const_iterator i = mode_sequence.begin();
            i != mode_sequence.end(); ++i) {
        unsigned char mletter = *i;
        if (mletter == '+') {
            continue;
        }

        ModeHandler* mh = ServerInstance->Modes->FindMode(mletter, MODETYPE_CHANNEL);
        if (!mh || !mh->IsListMode()) {
            return;
        }

        /* Ensure the user doesnt request the same mode twice,
         * so they can't flood themselves off out of idiocy.
         */
        if (sent[mletter] == seq) {
            continue;
        }

        sent[mletter] = seq;
        ServerInstance->Modes.ShowListModeList(user, chan, mh);
    }
}

std::string CommandMode::GetSnomasks(const User* user) {
    std::string snomaskstr = snomaskmode->GetUserParameter(user);
    // snomaskstr is empty if the snomask mode isn't set, otherwise it begins with a '+'.
    // In the former case output a "+", not an empty string.
    if (snomaskstr.empty()) {
        snomaskstr.push_back('+');
    }
    return snomaskstr;
}

namespace {
void GetModeList(Numeric::Numeric& num, Channel* chan, User* user) {
    // We should only show the value of secret parameters (i.e. key) if
    // the user is a member of the channel.
    bool show_secret = chan->HasUser(user);

    size_t modepos = num.push("+").GetParams().size() - 1;
    std::string modes;
    std::string param;
    for (unsigned char chr = 65; chr < 123; ++chr) {
        // Check that the mode exists and is set.
        ModeHandler* mh = ServerInstance->Modes->FindMode(chr, MODETYPE_CHANNEL);
        if (!mh || !chan->IsModeSet(mh)) {
            continue;
        }

        // Add the mode to the set list.
        modes.push_back(mh->GetModeChar());

        // If the mode has a parameter we need to include that too.
        ParamModeBase* pm = mh->IsParameterMode();
        if (!pm) {
            continue;
        }

        // If a mode has a secret parameter and the user is not privy to
        // the value of it then we use <name> instead of the value.
        if (pm->IsParameterSecret() && !show_secret) {
            num.push("<" + pm->name + ">");
            continue;
        }

        // Retrieve the parameter and add it to the mode list.
        pm->GetParameter(chan, param);
        num.push(param);
        param.clear();
    }
    num.GetParams()[modepos].append(modes);
}
}

void CommandMode::DisplayCurrentModes(User* user, User* targetuser,
                                      Channel* targetchannel) {
    if (targetchannel) {
        // Display channel's current mode string
        Numeric::Numeric modenum(RPL_CHANNELMODEIS);
        modenum.push(targetchannel->name);
        GetModeList(modenum, targetchannel, user);
        user->WriteNumeric(modenum);
        user->WriteNumeric(RPL_CHANNELCREATED, targetchannel->name,
                           (unsigned long)targetchannel->age);
    } else {
        if (targetuser == user) {
            // Display user's current mode string
            user->WriteNumeric(RPL_UMODEIS, targetuser->GetModeLetters());
            if (targetuser->IsOper()) {
                user->WriteNumeric(RPL_SNOMASKIS, GetSnomasks(targetuser),
                                   "Server notice mask");
            }
        } else if (user->HasPrivPermission("users/auspex")) {
            // Querying the modes of another user.
            // We cannot use RPL_UMODEIS because that's only for showing the user's own modes.
            user->WriteNumeric(RPL_OTHERUMODEIS, targetuser->nick,
                               targetuser->GetModeLetters());
            if (targetuser->IsOper()) {
                user->WriteNumeric(RPL_OTHERSNOMASKIS, targetuser->nick,
                                   GetSnomasks(targetuser), "Server notice mask");
            }
        } else {
            user->WriteNumeric(ERR_USERSDONTMATCH, "Can't view modes for other users");
        }
    }
}

class CoreModMode : public Module {
  private:
    CommandMode cmdmode;

  public:
    CoreModMode()
        : cmdmode(this) {
    }

    void On005Numeric(std::map<std::string, std::string>& tokens) CXX11_OVERRIDE {
        tokens["CHANMODES"] = ServerInstance->Modes->GiveModeList(MODETYPE_CHANNEL);
        tokens["USERMODES"] = ServerInstance->Modes->GiveModeList(MODETYPE_USER);
    }

    Version GetVersion() CXX11_OVERRIDE {
        return Version("Provides the MODE command", VF_VENDOR|VF_CORE);
    }
};

MODULE_INIT(CoreModMode)
