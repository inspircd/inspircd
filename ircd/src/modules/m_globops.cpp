/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2013, 2018-2020 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012, 2019 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2012 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2009 Uli Schlachter <psychon@inspircd.org>
 *   Copyright (C) 2007-2008 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2007 Robin Burchell <robin+git@viroteck.net>
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


// Globops and snomask +g module by C.J.Edwards

#include "inspircd.h"

/** Handle /GLOBOPS
 */
class CommandGlobops : public Command {
  public:
    CommandGlobops(Module* Creator) : Command(Creator,"GLOBOPS", 1,1) {
        flags_needed = 'o';
        syntax = ":<message>";
    }

    CmdResult Handle(User* user, const Params& parameters) CXX11_OVERRIDE {
        if (parameters[0].empty()) {
            user->WriteNumeric(ERR_NOTEXTTOSEND, "No text to send");
            return CMD_FAILURE;
        }

        ServerInstance->SNO->WriteGlobalSno('g', "From " + user->nick + ": " + parameters[0]);
        return CMD_SUCCESS;
    }
};

class ModuleGlobops : public Module {
    CommandGlobops cmd;
  public:
    ModuleGlobops() : cmd(this) {}

    void init() CXX11_OVERRIDE {
        ServerInstance->SNO->EnableSnomask('g',"GLOBOPS");
    }

    Version GetVersion() CXX11_OVERRIDE {
        return Version("Adds the /GLOBOPS command which allows server operators to send messages to all server operators with the g (globops) snomask.", VF_VENDOR);
    }
};

MODULE_INIT(ModuleGlobops)
