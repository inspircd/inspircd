/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2018, 2022 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2018 linuxdaemon <linuxdaemon.irc@gmail.com>
 *   Copyright (C) 2017 Jordyn/The Linux Geek <onlinecloud1@gmail.com>
 *   Copyright (C) 2012, 2019 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2012, 2014-2016 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2009 Uli Schlachter <psychon@inspircd.org>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2006 Craig Edwards <brain@inspircd.org>
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

CommandRestart::CommandRestart(Module* parent, std::string& hashref)
    : Command(parent, "RESTART", 1, 1)
    , hash(hashref) {
    flags_needed = 'o';
    syntax = "<servername>";
}

CmdResult CommandRestart::Handle(User* user, const Params& parameters) {
    ServerInstance->Logs->Log(MODNAME, LOG_DEFAULT, "Restart: %s",
                              user->nick.c_str());
    if (ServerInstance->PassCompare(user, password, parameters[0], hash)) {
        ServerInstance->SNO->WriteGlobalSno('a',
                                            "RESTART command from %s, restarting server.", user->GetFullRealHost().c_str());

        DieRestart::SendError("Server restarting.");

#ifdef FD_CLOEXEC
        /* XXX: This hack sets FD_CLOEXEC on all possible file descriptors, so they're closed if the execvp() below succeeds.
         * Certainly, this is not a nice way to do things and it's slow when the fd limit is high.
         *
         * A better solution would be to set the close-on-exec flag for each fd we create (or create them with O_CLOEXEC),
         * however there is no guarantee that third party libs will do the same.
         */
        for (int i = SocketEngine::GetMaxFds(); --i > 2;) {
            int flags = fcntl(i, F_GETFD);
            if (flags != -1) {
                fcntl(i, F_SETFD, flags | FD_CLOEXEC);
            }
        }
#endif

        execvp(ServerInstance->Config->cmdline.argv[0],
               ServerInstance->Config->cmdline.argv);
        ServerInstance->SNO->WriteGlobalSno('a',
                                            "Failed RESTART - could not execute '%s' (%s)",
                                            ServerInstance->Config->cmdline.argv[0], strerror(errno));
    } else {
        ServerInstance->SNO->WriteGlobalSno('a', "Failed RESTART command from %s.",
                                            user->GetFullRealHost().c_str());
    }
    return CMD_FAILURE;
}
