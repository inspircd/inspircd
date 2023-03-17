/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2013-2014, 2017-2018, 2020-2021 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012-2014, 2016 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012, 2019 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009, 2012 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2007-2008 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2007, 2009 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2004-2006 Craig Edwards <brain@inspircd.org>
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

/** Handle /CHGHOST
 */
class CommandChghost : public Command {
  public:
    std::bitset<UCHAR_MAX + 1> hostmap;

    CommandChghost(Module* Creator)
        : Command(Creator,"CHGHOST", 2) {
        allow_empty_last_param = false;
        flags_needed = 'o';
        syntax = "<nick> <host>";
        TRANSLATE2(TR_NICK, TR_TEXT);
    }

    CmdResult Handle(User* user, const Params& parameters) CXX11_OVERRIDE {
        if (parameters[1].length() > ServerInstance->Config->Limits.MaxHost) {
            user->WriteNotice("*** CHGHOST: Host too long");
            return CMD_FAILURE;
        }

        for (std::string::const_iterator x = parameters[1].begin(); x != parameters[1].end(); x++) {
            if (!hostmap.test(static_cast<unsigned char>(*x))) {
                user->WriteNotice("*** CHGHOST: Invalid characters in hostname");
                return CMD_FAILURE;
            }
        }

        User* dest = ServerInstance->FindNick(parameters[0]);

        // Allow services to change the host of unregistered users
        if ((!dest) || ((dest->registered != REG_ALL) && (!user->server->IsULine()))) {
            user->WriteNumeric(Numerics::NoSuchNick(parameters[0]));
            return CMD_FAILURE;
        }

        if (IS_LOCAL(dest)) {
            if ((dest->ChangeDisplayedHost(parameters[1])) && (!user->server->IsULine())) {
                // fix by brain - ulines set hosts silently
                ServerInstance->SNO->WriteGlobalSno('a',
                                                    user->nick+" used CHGHOST to make the displayed host of "+dest->nick+" become "
                                                    +dest->GetDisplayedHost());
            }
        }

        return CMD_SUCCESS;
    }

    RouteDescriptor GetRouting(User* user,
                               const Params& parameters) CXX11_OVERRIDE {
        return ROUTE_OPT_UCAST(parameters[0]);
    }
};


class ModuleChgHost : public Module {
    CommandChghost cmd;

  public:
    ModuleChgHost()
        : cmd(this) {
    }

    void ReadConfig(ConfigStatus& status) CXX11_OVERRIDE {
        ConfigTag* tag = ServerInstance->Config->ConfValue("hostname");
        const std::string hmap = tag->getString("charmap", "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz.-_/0123456789", 1);

        cmd.hostmap.reset();
        for (std::string::const_iterator n = hmap.begin(); n != hmap.end(); n++) {
            cmd.hostmap.set(static_cast<unsigned char>(*n));
        }
    }

    Version GetVersion() CXX11_OVERRIDE {
        return Version("Adds the /CHGHOST command which allows server operators to change the displayed hostname of a user.", VF_OPTCOMMON | VF_VENDOR);
    }
};

MODULE_INIT(ModuleChgHost)
