/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2014 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2013, 2017-2018 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012, 2019 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2008 Craig Edwards <brain@inspircd.org>
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

enum {
    // InspIRCd-specific.
    ERR_AMBIGUOUSCOMMAND = 420
};

class ModuleAbbreviation : public Module {
  public:
    void Prioritize() CXX11_OVERRIDE {
        ServerInstance->Modules->SetPriority(this, I_OnPreCommand, PRIORITY_FIRST);
    }

    Version GetVersion() CXX11_OVERRIDE {
        return Version("Allows commands to be abbreviated by appending a full stop.", VF_VENDOR);
    }

    ModResult OnPreCommand(std::string& command, CommandBase::Params& parameters,
                           LocalUser* user, bool validated) CXX11_OVERRIDE {
        /* Command is already validated, has a length of 0, or last character is not a . */
        if (validated || command.empty() || *command.rbegin() != '.') {
            return MOD_RES_PASSTHRU;
        }

        /* Look for any command that starts with the same characters, if it does, replace the command string with it */
        size_t clen = command.length() - 1;
        std::string foundcommand, matchlist;
        bool foundmatch = false;
        const CommandParser::CommandMap& commands = ServerInstance->Parser.GetCommands();
        for (CommandParser::CommandMap::const_iterator n = commands.begin(); n != commands.end(); ++n) {
            if (!command.compare(0, clen, n->first, 0, clen)) {
                if (matchlist.length() > 450) {
                    user->WriteNumeric(ERR_AMBIGUOUSCOMMAND,
                                       "Ambiguous abbreviation and too many possible matches.");
                    return MOD_RES_DENY;
                }

                if (!foundmatch) {
                    /* Found the command */
                    foundcommand = n->first;
                    foundmatch = true;
                } else {
                    matchlist.append(" ").append(n->first);
                }
            }
        }

        /* Ambiguous command, list the matches */
        if (!matchlist.empty()) {
            user->WriteNumeric(ERR_AMBIGUOUSCOMMAND,
                               InspIRCd::Format("Ambiguous abbreviation, possible matches: %s%s",
                                                foundcommand.c_str(), matchlist.c_str()));
            return MOD_RES_DENY;
        }

        if (!foundcommand.empty()) {
            command = foundcommand;
        }

        return MOD_RES_PASSTHRU;
    }
};

MODULE_INIT(ModuleAbbreviation)
