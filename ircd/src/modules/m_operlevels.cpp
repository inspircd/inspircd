/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2013, 2018 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012-2013, 2016 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012, 2019 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009 Thomas Stagner <aquanight@inspircd.org>
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2007-2008 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2005-2006 Craig Edwards <brain@inspircd.org>
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

class ModuleOperLevels : public Module {
  public:
    Version GetVersion() CXX11_OVERRIDE {
        return Version("Allows the server administrator to define ranks for server operators which prevent lower ranked server operators from using /KILL on higher ranked server operators.", VF_VENDOR);
    }

    ModResult OnKill(User* source, User* dest,
                     const std::string &reason) CXX11_OVERRIDE {
        // oper killing an oper?
        if (dest->IsOper() && source->IsOper()) {
            unsigned long dest_level = ConvToNum<unsigned long>
            (dest->oper->getConfig("level"));
            unsigned long source_level = ConvToNum<unsigned long>
            (source->oper->getConfig("level"));

            if (dest_level > source_level) {
                if (IS_LOCAL(source)) {
                    ServerInstance->SNO->WriteGlobalSno('a',
                                                        "Oper %s (level %lu) attempted to /KILL a higher level oper: %s (level %lu), reason: %s",
                                                        source->nick.c_str(), source_level, dest->nick.c_str(), dest_level,
                                                        reason.c_str());
                }
                dest->WriteNotice("*** Oper " + source->nick + " attempted to /KILL you!");
                source->WriteNumeric(ERR_NOPRIVILEGES,
                                     InspIRCd::Format("Permission Denied - Oper %s is a higher level than you",
                                                      dest->nick.c_str()));
                return MOD_RES_DENY;
            }
        }
        return MOD_RES_PASSTHRU;
    }
};

MODULE_INIT(ModuleOperLevels)
