/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2013, 2017-2020 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2013 Daniel Vassdal <shutter@canternet.org>
 *   Copyright (C) 2012-2016, 2018 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012, 2019 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2007 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2004, 2006 Craig Edwards <brain@inspircd.org>
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

/** Handle /SAMODE
 */
class CommandSamode : public Command {
    bool logged;

  public:
    bool active;
    CommandSamode(Module* Creator) : Command(Creator,"SAMODE", 2) {
        allow_empty_last_param = false;
        flags_needed = 'o';
        syntax = "<target> (+|-)<modes> [<mode-parameters>]";
        active = false;
    }

    CmdResult Handle(User* user, const Params& parameters) CXX11_OVERRIDE {
        if (parameters[0].c_str()[0] != '#') {
            User* target = ServerInstance->FindNickOnly(parameters[0]);
            if ((!target) || (target->registered != REG_ALL)) {
                user->WriteNumeric(Numerics::NoSuchNick(parameters[0]));
                return CMD_FAILURE;
            }

            // Changing the modes of another user requires a special permission
            if ((target != user) && (!user->HasPrivPermission("users/samode-usermodes"))) {
                user->WriteNotice("*** You are not allowed to /SAMODE other users (the privilege users/samode-usermodes is needed to /SAMODE others).");
                return CMD_FAILURE;
            }
        }

        // XXX: Make ModeParser clear LastParse
        Modes::ChangeList emptychangelist;
        ServerInstance->Modes->ProcessSingle(ServerInstance->FakeClient, NULL, ServerInstance->FakeClient, emptychangelist);

        logged = false;
        this->active = true;
        ServerInstance->Parser.CallHandler("MODE", parameters, user);
        this->active = false;

        if (!logged) {
            // If we haven't logged anything yet then the client queried the list of a listmode
            // (e.g. /SAMODE #chan b), which was handled internally by the MODE command handler.
            //
            // Viewing the modes of a user or a channel could also result in this, but
            // that is not possible with /SAMODE because we require at least 2 parameters.
            LogUsage(user, stdalgo::string::join(parameters));
        }

        return CMD_SUCCESS;
    }

    void LogUsage(const User* user, const std::string& text) {
        logged = true;
        ServerInstance->SNO->WriteGlobalSno('a', user->nick + " used SAMODE: " + text);
    }
};

class ModuleSaMode : public Module {
    CommandSamode cmd;
  public:
    ModuleSaMode()
        : cmd(this) {
    }

    Version GetVersion() CXX11_OVERRIDE {
        return Version("Adds the /SAMODE command which allows server operators to change the modes of a target (channel, user) that they would not otherwise have the privileges to change.", VF_VENDOR);
    }

    ModResult OnPreMode(User* source, User* dest, Channel* channel,
                        Modes::ChangeList& modes) CXX11_OVERRIDE {
        if (cmd.active) {
            return MOD_RES_ALLOW;
        }
        return MOD_RES_PASSTHRU;
    }

    void OnMode(User* user, User* destuser, Channel* destchan,
                const Modes::ChangeList& modes,
                ModeParser::ModeProcessFlag processflags) CXX11_OVERRIDE {
        if (!cmd.active) {
            return;
        }

        std::string logtext = (destuser ? destuser->nick : destchan->name);
        logtext.push_back(' ');
        logtext += ClientProtocol::Messages::Mode::ToModeLetters(modes);

        for (Modes::ChangeList::List::const_iterator i = modes.getlist().begin(); i != modes.getlist().end(); ++i) {
            const Modes::Change& item = *i;
            if (!item.param.empty()) {
                logtext.append(1, ' ').append(item.param);
            }
        }

        cmd.LogUsage(user, logtext);
    }

    void Prioritize() CXX11_OVERRIDE {
        Module* disable = ServerInstance->Modules->Find("m_disable.so");
        ServerInstance->Modules->SetPriority(this, I_OnRawMode, PRIORITY_BEFORE, disable);

        Module *override = ServerInstance->Modules->Find("m_override.so");
        ServerInstance->Modules->SetPriority(this, I_OnPreMode, PRIORITY_BEFORE, override);
    }
};

MODULE_INIT(ModuleSaMode)
