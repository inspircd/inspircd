/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2013, 2017-2018 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012-2015 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012, 2019 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2007-2008 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
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
#include "modules/whois.h"

/** Handle user mode +W
 */
class SeeWhois : public SimpleUserModeHandler {
  public:
    SeeWhois(Module* Creator)
        : SimpleUserModeHandler(Creator, "showwhois", 'W') {
    }

    void SetOperOnly(bool operonly) {
        oper = operonly;
    }
};

class WhoisNoticeCmd : public Command {
  public:
    WhoisNoticeCmd(Module* Creator) : Command(Creator,"WHOISNOTICE", 2) {
        flags_needed = FLAG_SERVERONLY;
    }

    void HandleFast(User* dest, User* src) {
        dest->WriteNotice("*** " + src->nick + " (" + src->ident + "@" +
                          src->GetHost(dest->HasPrivPermission("users/auspex")) +
                          ") did a /whois on you");
    }

    CmdResult Handle(User* user, const Params& parameters) CXX11_OVERRIDE {
        User* dest = ServerInstance->FindNick(parameters[0]);
        if (!dest) {
            return CMD_FAILURE;
        }

        User* source = ServerInstance->FindNick(parameters[1]);

        if (IS_LOCAL(dest) && source) {
            HandleFast(dest, source);
        }

        return CMD_SUCCESS;
    }
};

class ModuleShowwhois : public Module, public Whois::EventListener {
    bool ShowWhoisFromOpers;
    SeeWhois sw;
    WhoisNoticeCmd cmd;

  public:

    ModuleShowwhois()
        : Whois::EventListener(this)
        , sw(this)
        , cmd(this) {
    }

    void ReadConfig(ConfigStatus& status) CXX11_OVERRIDE {
        ConfigTag* tag = ServerInstance->Config->ConfValue("showwhois");

        sw.SetOperOnly(tag->getBool("opersonly", true));
        ShowWhoisFromOpers = tag->getBool("showfromopers", true);
    }

    Version GetVersion() CXX11_OVERRIDE {
        return Version("Adds user mode W (showwhois) which allows users to be informed when someone does a /WHOIS query on their nick.", VF_OPTCOMMON|VF_VENDOR);
    }

    void OnWhois(Whois::Context& whois) CXX11_OVERRIDE {
        User* const source = whois.GetSource();
        User* const dest = whois.GetTarget();
        if (!dest->IsModeSet(sw) || whois.IsSelfWhois()) {
            return;
        }

        if (!ShowWhoisFromOpers && source->IsOper()) {
            return;
        }

        if (IS_LOCAL(dest)) {
            cmd.HandleFast(dest, source);
        } else {
            CommandBase::Params params;
            params.push_back(dest->uuid);
            params.push_back(source->uuid);
            ServerInstance->PI->SendEncapsulatedData(dest->server->GetName(), cmd.name,
                    params);
        }
    }
};

MODULE_INIT(ModuleShowwhois)
