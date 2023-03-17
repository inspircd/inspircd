/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2013, 2017-2021 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012-2013 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2008 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2007-2008 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2006-2007 Craig Edwards <brain@inspircd.org>
 *   Copyright (C) 2006 Oliver Lupton <om@inspircd.org>
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
    RPL_EXCEPTLIST = 348,
    RPL_ENDOFEXCEPTLIST = 349
};

class BanException : public ListModeBase {
  public:
    BanException(Module* Creator)
        : ListModeBase(Creator, "banexception", 'e', "End of Channel Exception List",
                       RPL_EXCEPTLIST, RPL_ENDOFEXCEPTLIST, true) {
        syntax = "<mask>";
    }
};

class ModuleBanException : public Module {
    BanException be;

  public:
    ModuleBanException() : be(this) {
    }

    void On005Numeric(std::map<std::string, std::string>& tokens) CXX11_OVERRIDE {
        tokens["EXCEPTS"] = ConvToStr(be.GetModeChar());
    }

    ModResult OnExtBanCheck(User *user, Channel *chan, char type) CXX11_OVERRIDE {
        ListModeBase::ModeList* list = be.GetList(chan);
        if (!list) {
            return MOD_RES_PASSTHRU;
        }

        for (ListModeBase::ModeList::iterator it = list->begin(); it != list->end(); it++) {
            if (it->mask.length() <= 2 || it->mask[0] != type || it->mask[1] != ':') {
                continue;
            }

            if (chan->CheckBan(user, it->mask.substr(2))) {
                // They match an entry on the list, so let them pass this.
                return MOD_RES_ALLOW;
            }
        }

        return MOD_RES_PASSTHRU;
    }

    ModResult OnCheckChannelBan(User* user, Channel* chan) CXX11_OVERRIDE {
        ListModeBase::ModeList* list = be.GetList(chan);
        if (!list) {
            // No list, proceed normally
            return MOD_RES_PASSTHRU;
        }

        for (ListModeBase::ModeList::iterator it = list->begin(); it != list->end(); it++) {
            if (chan->CheckBan(user, it->mask)) {
                // They match an entry on the list, so let them in.
                return MOD_RES_ALLOW;
            }
        }
        return MOD_RES_PASSTHRU;
    }

    void ReadConfig(ConfigStatus& status) CXX11_OVERRIDE {
        be.DoRehash();
    }

    Version GetVersion() CXX11_OVERRIDE {
        return Version("Adds channel mode e (banexception) which allows channel operators to exempt user masks from channel mode b (ban).", VF_VENDOR);
    }
};

MODULE_INIT(ModuleBanException)
