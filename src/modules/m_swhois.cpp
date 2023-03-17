/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2012-2016 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012-2013, 2017-2018, 2020-2021 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2007-2008 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2006 Oliver Lupton <om@inspircd.org>
 *   Copyright (C) 2005, 2007 Craig Edwards <brain@inspircd.org>
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

class CommandSwhois : public Command {
  public:
    LocalIntExt operblock;
    StringExtItem swhois;
    CommandSwhois(Module* Creator)
        : Command(Creator, "SWHOIS", 2, 2)
        , operblock("swhois_operblock", ExtensionItem::EXT_USER, Creator)
        , swhois("swhois", ExtensionItem::EXT_USER, Creator) {
        flags_needed = 'o';
        syntax = "<nick> :<swhois>";
        TRANSLATE2(TR_NICK, TR_TEXT);
    }

    CmdResult Handle(User* user, const Params& parameters) CXX11_OVERRIDE {
        User* dest = ServerInstance->FindNick(parameters[0]);

        if (!dest) { // allow setting swhois using SWHOIS before reg
            user->WriteNumeric(Numerics::NoSuchNick(parameters[0]));
            return CMD_FAILURE;
        }

        std::string* text = swhois.get(dest);
        if (text) {
            // We already had it set...
            if (!user->server->IsULine())
                // Ulines set SWHOISes silently
            {
                ServerInstance->SNO->WriteGlobalSno('a',
                                                    "%s used SWHOIS to set %s's extra whois from '%s' to '%s'", user->nick.c_str(),
                                                    dest->nick.c_str(), text->c_str(), parameters[1].c_str());
            }
        } else if (!user->server->IsULine()) {
            // Ulines set SWHOISes silently
            ServerInstance->SNO->WriteGlobalSno('a',
                                                "%s used SWHOIS to set %s's extra whois to '%s'", user->nick.c_str(),
                                                dest->nick.c_str(), parameters[1].c_str());
        }

        operblock.set(user, 0);
        if (parameters[1].empty()) {
            swhois.unset(dest);
        } else {
            swhois.set(dest, parameters[1]);
        }

        /* Bug #376 - feature request -
         * To cut down on the amount of commands services etc have to recognise, this only sends METADATA across the network now
         * not an actual SWHOIS command. Any SWHOIS command sent from services will be automatically translated to METADATA by this.
         * Sorry w00t i know this was your fix, but i got bored and wanted to clear down the tracker :)
         * -- Brain
         */
        ServerInstance->PI->SendMetaData(dest, "swhois", parameters[1]);

        return CMD_SUCCESS;
    }

};

class ModuleSWhois CXX11_FINAL
    : public Module
    , public Whois::LineEventListener {
  private:
    CommandSwhois cmd;
    UserModeReference hideopermode;

  public:
    ModuleSWhois()
        : Whois::LineEventListener(this)
        , cmd(this)
        , hideopermode(this, "hideoper") {
    }

    // :kenny.chatspike.net 320 Brain Azhrarn :is getting paid to play games.
    ModResult OnWhoisLine(Whois::Context& whois,
                          Numeric::Numeric& numeric) CXX11_OVERRIDE {
        // We use this and not OnWhois because this triggers for remote users too.
        if (numeric.GetNumeric() != RPL_WHOISSERVER) {
            return MOD_RES_PASSTHRU;
        }

        // Don't send soper swhois if hideoper is set.
        if (cmd.operblock.get(whois.GetTarget()) && whois.GetTarget()->IsModeSet(hideopermode)) {
            return MOD_RES_PASSTHRU;
        }

        // Insert our numeric before RPL_WHOISSERVER.
        const std::string* swhois = cmd.swhois.get(whois.GetTarget());
        if (swhois && !swhois->empty()) {
            whois.SendLine(RPL_WHOISSPECIAL, *swhois);
        }

        return MOD_RES_PASSTHRU;
    }

    void OnPostOper(User* user, const std::string &opertype,
                    const std::string &opername) CXX11_OVERRIDE {
        if (!IS_LOCAL(user)) {
            return;
        }

        std::string swhois = user->oper->getConfig("swhois");

        if (!swhois.length()) {
            return;
        }

        cmd.operblock.set(user, 1);
        cmd.swhois.set(user, swhois);
        ServerInstance->PI->SendMetaData(user, "swhois", swhois);
    }

    void OnPostDeoper(User* user) CXX11_OVERRIDE {
        std::string* swhois = cmd.swhois.get(user);
        if (!swhois) {
            return;
        }

        if (!cmd.operblock.get(user)) {
            return;
        }

        cmd.operblock.set(user, 0);
        cmd.swhois.unset(user);
        ServerInstance->PI->SendMetaData(user, "swhois", "");
    }

    void OnDecodeMetaData(Extensible* target, const std::string& extname,
                          const std::string&) CXX11_OVERRIDE {
        User* dest = static_cast<User*>(target);
        if (dest && (extname == "swhois")) {
            cmd.operblock.set(dest, 0);
        }
    }

    Version GetVersion() CXX11_OVERRIDE {
        return Version("Adds the /SWHOIS command which adds custom lines to a user's WHOIS response.", VF_OPTCOMMON | VF_VENDOR);
    }
};

MODULE_INIT(ModuleSWhois)
