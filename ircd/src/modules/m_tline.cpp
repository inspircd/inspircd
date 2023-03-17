/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2013, 2018 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012, 2019 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2012, 2014 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2007-2008 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2006, 2008 Craig Edwards <brain@inspircd.org>
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

/** Handle /TLINE
 */
class CommandTline : public Command {
  public:
    CommandTline(Module* Creator) : Command(Creator,"TLINE", 1) {
        flags_needed = 'o';
        this->syntax = "<mask>";
    }

    CmdResult Handle(User* user, const Params& parameters) CXX11_OVERRIDE {
        unsigned int n_matched = 0;
        unsigned int n_match_host = 0;
        unsigned int n_match_ip = 0;

        const user_hash& users = ServerInstance->Users->GetUsers();
        for (user_hash::const_iterator u = users.begin(); u != users.end(); ++u) {
            if (InspIRCd::Match(u->second->GetFullRealHost(),parameters[0])) {
                n_matched++;
                n_match_host++;
            } else {
                std::string host = u->second->ident + "@" + u->second->GetIPString();
                if (InspIRCd::MatchCIDR(host, parameters[0])) {
                    n_matched++;
                    n_match_ip++;
                }
            }
        }

        unsigned long n_counted = users.size();
        if (n_matched) {
            float p = (n_matched / (float)n_counted) * 100;
            user->WriteNotice(
                InspIRCd::Format("*** TLINE: Counted %lu user(s). Matched '%s' against %u user(s) (%0.2f%% of the userbase). %u by hostname and %u by IP address.",
                                 n_counted, parameters[0].c_str(), n_matched, p, n_match_host, n_match_ip));
        } else {
            user->WriteNotice(
                InspIRCd::Format("*** TLINE: Counted %lu user(s). Matched '%s' against no user(s).",
                                 n_counted, parameters[0].c_str()));
        }

        return CMD_SUCCESS;
    }
};

class ModuleTLine : public Module {
    CommandTline cmd;
  public:
    ModuleTLine()
        : cmd(this) {
    }

    Version GetVersion() CXX11_OVERRIDE {
        return Version("Adds the /TLINE command which allows server operators to determine how many users would be affected by an X-line on a specified pattern.", VF_VENDOR);
    }
};

MODULE_INIT(ModuleTLine)
