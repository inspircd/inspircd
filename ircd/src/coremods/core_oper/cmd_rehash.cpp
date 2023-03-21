/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2018-2019 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2013 Adam <Adam@anope.org>
 *   Copyright (C) 2012-2015 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009 Matt Smith <dz@inspircd.org>
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2008 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2007-2008 Craig Edwards <brain@inspircd.org>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
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
#include "core_oper.h"

CommandRehash::CommandRehash(Module* parent)
    : Command(parent, "REHASH", 0) {
    flags_needed = 'o';
    Penalty = 2;
    syntax = "[<servermask>]";
}

CmdResult CommandRehash::Handle(User* user, const Params& parameters) {
    std::string param = parameters.size() ? parameters[0] : "";

    FOREACH_MOD(OnPreRehash, (user, param));

    if (param.empty()) {
        // standard rehash of local server
    } else if (param.find_first_of("*.") != std::string::npos) {
        // rehash of servers by server name (with wildcard)
        if (!InspIRCd::Match(ServerInstance->Config->ServerName, parameters[0])) {
            // Doesn't match us. PreRehash is already done, nothing left to do
            return CMD_SUCCESS;
        }
    } else {
        // parameterized rehash

        // the leading "-" is optional; remove it if present.
        if (param[0] == '-') {
            param.erase(param.begin());
        }

        FOREACH_MOD(OnModuleRehash, (user, param));
        return CMD_SUCCESS;
    }

    // Rehash for me. Try to start the rehash thread
    if (!ServerInstance->ConfigThread) {
        const std::string configfile = FileSystem::GetFileName(
                                           ServerInstance->ConfigFileName);
        user->WriteRemoteNumeric(RPL_REHASHING, configfile,
                                 "Rehashing " + ServerInstance->Config->ServerName);
        ServerInstance->SNO->WriteGlobalSno('a', "%s is rehashing %s on %s",
                                            user->nick.c_str(),
                                            configfile.c_str(), ServerInstance->Config->ServerName.c_str());

        /* Don't do anything with the logs here -- logs are restarted
         * after the config thread has completed.
         */
        ServerInstance->Rehash(user->uuid);
    } else {
        /*
         * A rehash is already in progress! ahh shit.
         * XXX, todo: we should find some way to kill runaway rehashes that are blocking, this is a major problem for unrealircd users
         */
        user->WriteRemoteNotice("*** Could not rehash: A rehash is already in progress.");
    }

    // Always return success so spanningtree forwards an incoming REHASH even if we failed
    return CMD_SUCCESS;
}
