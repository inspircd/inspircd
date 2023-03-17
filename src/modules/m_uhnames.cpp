/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2013, 2018-2020 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012, 2019 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2012, 2014-2015 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2009 Uli Schlachter <psychon@inspircd.org>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2007 Craig Edwards <brain@inspircd.org>
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
#include "modules/cap.h"
#include "modules/names.h"

class ModuleUHNames
    : public Module
    , public Names::EventListener {
  private:
    Cap::Capability cap;

  public:
    ModuleUHNames()
        : Names::EventListener(this)
        , cap(this, "userhost-in-names") {
    }

    Version GetVersion() CXX11_OVERRIDE {
        return Version("Provides the IRCv3 userhost-in-names client capability.", VF_VENDOR);
    }

    void On005Numeric(std::map<std::string, std::string>& tokens) CXX11_OVERRIDE {
        // The legacy PROTOCTL system is a wrapper around the cap.
        dynamic_reference_nocheck<Cap::Manager> capmanager(this, "capmanager");
        if (capmanager) {
            tokens["UHNAMES"];
        }
    }

    ModResult OnPreCommand(std::string& command, CommandBase::Params& parameters,
                           LocalUser* user, bool validated) CXX11_OVERRIDE {
        /* We don't actually create a proper command handler class for PROTOCTL,
         * because other modules might want to have PROTOCTL hooks too.
         * Therefore, we just hook its as an unvalidated command therefore we
         * can capture it even if it doesnt exist! :-)
         */
        if (command == "PROTOCTL") {
            if (!parameters.empty() && irc::equals(parameters[0], "UHNAMES")) {
                cap.set(user, true);
                return MOD_RES_DENY;
            }
        }
        return MOD_RES_PASSTHRU;
    }

    ModResult OnNamesListItem(LocalUser* issuer, Membership* memb,
                              std::string& prefixes, std::string& nick) CXX11_OVERRIDE {
        if (cap.get(issuer)) {
            nick = memb->user->GetFullHost();
        }

        return MOD_RES_PASSTHRU;
    }
};

MODULE_INIT(ModuleUHNames)
