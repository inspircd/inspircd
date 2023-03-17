/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2013, 2018 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012-2014, 2016 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012, 2019 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2007-2008 Craig Edwards <brain@inspircd.org>
 *   Copyright (C) 2007, 2009 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2007 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2007 John Brooks <special@inspircd.org>
 *   Copyright (C) 2006 Oliver Lupton <om@inspircd.org>
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

/** Handle /CHGIDENT
 */
class CommandChgident : public Command {
  public:
    CommandChgident(Module* Creator) : Command(Creator,"CHGIDENT", 2) {
        allow_empty_last_param = false;
        flags_needed = 'o';
        syntax = "<nick> <ident>";
        TRANSLATE2(TR_NICK, TR_TEXT);
    }

    CmdResult Handle(User* user, const Params& parameters) CXX11_OVERRIDE {
        User* dest = ServerInstance->FindNick(parameters[0]);

        if ((!dest) || (dest->registered != REG_ALL)) {
            user->WriteNumeric(Numerics::NoSuchNick(parameters[0]));
            return CMD_FAILURE;
        }

        if (parameters[1].length() > ServerInstance->Config->Limits.IdentMax) {
            user->WriteNotice("*** CHGIDENT: Ident is too long");
            return CMD_FAILURE;
        }

        if (!ServerInstance->IsIdent(parameters[1])) {
            user->WriteNotice("*** CHGIDENT: Invalid characters in ident");
            return CMD_FAILURE;
        }

        if (IS_LOCAL(dest)) {
            dest->ChangeIdent(parameters[1]);

            if (!user->server->IsULine()) {
                ServerInstance->SNO->WriteGlobalSno('a',
                                                    "%s used CHGIDENT to change %s's ident to '%s'", user->nick.c_str(),
                                                    dest->nick.c_str(), dest->ident.c_str());
            }
        }

        return CMD_SUCCESS;
    }

    RouteDescriptor GetRouting(User* user,
                               const Params& parameters) CXX11_OVERRIDE {
        return ROUTE_OPT_UCAST(parameters[0]);
    }
};

class ModuleChgIdent : public Module {
    CommandChgident cmd;

  public:
    ModuleChgIdent() : cmd(this) {
    }

    Version GetVersion() CXX11_OVERRIDE {
        return Version("Adds the /CHGIDENT command which allows server operators to change the username (ident) of a user.", VF_OPTCOMMON | VF_VENDOR);
    }
};

MODULE_INIT(ModuleChgIdent)
