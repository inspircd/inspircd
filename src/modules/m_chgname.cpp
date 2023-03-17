/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2018 Matt Schatz <genius3000@g3k.solutions>
 *   Copyright (C) 2013, 2018, 2022 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012-2013, 2016 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012, 2019 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2007, 2009 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2007 Robin Burchell <robin+git@viroteck.net>
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

/** Handle /CHGNAME
 */
class CommandChgname : public Command {
  public:
    CommandChgname(Module* Creator) : Command(Creator,"CHGNAME", 2, 2) {
        allow_empty_last_param = false;
        flags_needed = 'o';
        syntax = "<nick> :<realname>";
        TRANSLATE2(TR_NICK, TR_TEXT);
    }

    CmdResult Handle(User* user, const Params& parameters) CXX11_OVERRIDE {
        User* dest = ServerInstance->FindNick(parameters[0]);

        if ((!dest) || (dest->registered != REG_ALL)) {
            user->WriteNumeric(Numerics::NoSuchNick(parameters[0]));
            return CMD_FAILURE;
        }

        if (parameters[1].empty()) {
            user->WriteNotice("*** CHGNAME: Real name must be specified");
            return CMD_FAILURE;
        }

        if (parameters[1].length() > ServerInstance->Config->Limits.MaxReal) {
            user->WriteNotice("*** CHGNAME: Real name is too long");
            return CMD_FAILURE;
        }

        if (IS_LOCAL(dest)) {
            dest->ChangeRealName(parameters[1]);
            ServerInstance->SNO->WriteGlobalSno('a',
                                                "%s used CHGNAME to change %s's real name to '%s\x0F'", user->nick.c_str(),
                                                dest->nick.c_str(), dest->GetRealName().c_str());
        }

        return CMD_SUCCESS;
    }

    RouteDescriptor GetRouting(User* user,
                               const Params& parameters) CXX11_OVERRIDE {
        return ROUTE_OPT_UCAST(parameters[0]);
    }
};

class ModuleChgName : public Module {
    CommandChgname cmd;

  public:
    ModuleChgName() : cmd(this) {
    }

    Version GetVersion() CXX11_OVERRIDE {
        return Version("Adds the /CHGNAME command which allows server operators to change the real name (gecos) of a user.", VF_OPTCOMMON | VF_VENDOR);
    }
};

MODULE_INIT(ModuleChgName)
