/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2017-2020 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2013-2016 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2013 Daniel Vassdal <shutter@canternet.org>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009 Uli Schlachter <psychon@inspircd.org>
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2005, 2007-2008 Craig Edwards <brain@inspircd.org>
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
#include "core_info.h"

enum {
    // From ircd-ratbox with an InspIRCd-specific format.
    RPL_MODLIST = 702,
    RPL_ENDOFMODLIST = 703
};

CommandModules::CommandModules(Module* parent)
    : ServerTargetCommand(parent, "MODULES") {
    Penalty = 4;
    syntax = "[<servername>]";
}

/** Handle /MODULES
 */
CmdResult CommandModules::Handle(User* user, const Params& parameters) {
    // Don't ask remote servers about their modules unless the local user asking is an oper
    // 2.0 asks anyway, so let's handle that the same way
    bool for_us = (parameters.empty()
                   || irc::equals(parameters[0], ServerInstance->Config->ServerName));
    if ((!for_us) || (!IS_LOCAL(user))) {
        if (!user->IsOper()) {
            user->WriteNotice("*** You cannot check what modules other servers have loaded.");
            return CMD_FAILURE;
        }

        // From an oper and not for us, forward
        if (!for_us) {
            return CMD_SUCCESS;
        }
    }

    const ModuleManager::ModuleMap& mods = ServerInstance->Modules->GetModules();

    for (ModuleManager::ModuleMap::const_iterator i = mods.begin(); i != mods.end();
            ++i) {
        Module* m = i->second;
        Version V = m->GetVersion();

        if (IS_LOCAL(user) && user->HasPrivPermission("servers/auspex")) {
            std::string flags("VCO");
            size_t pos = 0;
            for (int mult = 2; mult <= VF_OPTCOMMON; mult *= 2, ++pos)
                if (!(V.Flags & mult)) {
                    flags[pos] = '-';
                }

            const char* srcrev = m->ModuleDLLManager->GetVersion();
            user->WriteRemoteNumeric(RPL_MODLIST, m->ModuleSourceFile,
                                     srcrev ? srcrev : "*", flags, V.description);
        } else {
            user->WriteRemoteNumeric(RPL_MODLIST, m->ModuleSourceFile, '*', '*',
                                     V.description);
        }
    }
    user->WriteRemoteNumeric(RPL_ENDOFMODLIST, "End of MODULES list");

    return CMD_SUCCESS;
}
