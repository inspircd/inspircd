/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2013, 2017-2021 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012-2013 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
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
#include "listmode.h"

enum {
    // From RFC 2812.
    RPL_INVEXLIST = 346,
    RPL_ENDOFINVEXLIST = 347
};

class InviteException : public ListModeBase {
  public:
    InviteException(Module* Creator)
        : ListModeBase(Creator, "invex", 'I', "End of Channel Invite Exception List",
                       RPL_INVEXLIST, RPL_ENDOFINVEXLIST, true) {
        syntax = "<mask>";
    }
};

class ModuleInviteException : public Module {
    bool invite_bypass_key;
    InviteException ie;
  public:
    ModuleInviteException() : ie(this) {
    }

    void On005Numeric(std::map<std::string, std::string>& tokens) CXX11_OVERRIDE {
        tokens["INVEX"] = ConvToStr(ie.GetModeChar());
    }

    ModResult OnCheckInvite(User* user, Channel* chan) CXX11_OVERRIDE {
        ListModeBase::ModeList* list = ie.GetList(chan);
        if (list) {
            for (ListModeBase::ModeList::iterator it = list->begin(); it != list->end();
                    it++) {
                if (chan->CheckBan(user, it->mask)) {
                    return MOD_RES_ALLOW;
                }
            }
        }

        return MOD_RES_PASSTHRU;
    }

    ModResult OnCheckKey(User* user, Channel* chan,
                         const std::string& key) CXX11_OVERRIDE {
        if (invite_bypass_key) {
            return OnCheckInvite(user, chan);
        }
        return MOD_RES_PASSTHRU;
    }

    void ReadConfig(ConfigStatus& status) CXX11_OVERRIDE {
        ie.DoRehash();
        invite_bypass_key = ServerInstance->Config->ConfValue("inviteexception")->getBool("bypasskey", true);
    }

    Version GetVersion() CXX11_OVERRIDE {
        return Version("Adds channel mode I (invex) which allows channel operators to exempt user masks from channel mode i (inviteonly).", VF_VENDOR);
    }
};

MODULE_INIT(ModuleInviteException)
