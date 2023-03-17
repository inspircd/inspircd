/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2013, 2017-2018 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012, 2014 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2008 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2005 Craig Edwards <brain@inspircd.org>
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

class ModuleModesOnOper : public Module {
  public:
    Version GetVersion() CXX11_OVERRIDE {
        return Version("Allows the server administrator to set user modes on server operators when they log into their server operator account.", VF_VENDOR);
    }

    void OnPostOper(User* user, const std::string &opertype,
                    const std::string &opername) CXX11_OVERRIDE {
        if (!IS_LOCAL(user)) {
            return;
        }

        // whenever a user opers, go through the oper types, find their <type:modes>,
        // and if they have one apply their modes. The mode string can contain +modes
        // to add modes to the user or -modes to take modes from the user.
        std::string ThisOpersModes = user->oper->getConfig("modes");
        if (!ThisOpersModes.empty()) {
            ApplyModes(user, ThisOpersModes);
        }
    }

    void ApplyModes(User *u, std::string &smodes) {
        char first = *(smodes.c_str());
        if ((first != '+') && (first != '-')) {
            smodes = "+" + smodes;
        }

        std::string buf;
        irc::spacesepstream ss(smodes);
        CommandBase::Params modes;

        modes.push_back(u->nick);
        // split into modes and mode params
        while (ss.GetToken(buf)) {
            modes.push_back(buf);
        }

        ServerInstance->Parser.CallHandler("MODE", modes, u);
    }
};

MODULE_INIT(ModuleModesOnOper)
