/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2019 Matt Schatz <genius3000@g3k.solutions>
 *   Copyright (C) 2013, 2017, 2021 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012, 2019 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2012 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2009 Uli Schlachter <psychon@inspircd.org>
 *   Copyright (C) 2008 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
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


#include "inspircd.h"

class ModuleNoKicks : public Module {
    SimpleChannelModeHandler nk;

  public:
    ModuleNoKicks()
        : nk(this, "nokick", 'Q') {
    }

    void On005Numeric(std::map<std::string, std::string>& tokens) CXX11_OVERRIDE {
        tokens["EXTBAN"].push_back('Q');
    }

    ModResult OnUserPreKick(User* source, Membership* memb,
                            const std::string &reason) CXX11_OVERRIDE {
        bool modeset = memb->chan->IsModeSet(nk);
        if (!memb->chan->GetExtBanStatus(source, 'Q').check(!modeset)) {
            // Can't kick with Q in place, not even opers with override, and founders
            source->WriteNumeric(ERR_RESTRICTED, memb->chan->name,
                                 InspIRCd::Format("Can't kick user %s from channel (%s)",
                                                  memb->user->nick.c_str(), modeset ? "+Q is set" : "you're extbanned"));
            return MOD_RES_DENY;
        }
        return MOD_RES_PASSTHRU;
    }

    Version GetVersion() CXX11_OVERRIDE {
        return Version("Adds channel mode Q (nokick) which prevents privileged users from using the /KICK command.", VF_VENDOR);
    }
};

MODULE_INIT(ModuleNoKicks)
