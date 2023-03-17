/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2013, 2018-2019 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012-2013, 2015, 2018 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2008 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2006 Oliver Lupton <om@inspircd.org>
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

class ModuleWaitPong : public Module {
    bool sendsnotice;
    bool killonbadreply;
    LocalStringExt ext;

  public:
    ModuleWaitPong()
        : ext("waitpong_pingstr", ExtensionItem::EXT_USER, this) {
    }

    void ReadConfig(ConfigStatus& status) CXX11_OVERRIDE {
        ConfigTag* tag = ServerInstance->Config->ConfValue("waitpong");
        sendsnotice = tag->getBool("sendsnotice", false);
        killonbadreply = tag->getBool("killonbadreply", true);
    }

    ModResult OnUserRegister(LocalUser* user) CXX11_OVERRIDE {
        std::string pingrpl = ServerInstance->GenRandomStr(10);
        {
            ClientProtocol::Messages::Ping pingmsg(pingrpl);
            user->Send(ServerInstance->GetRFCEvents().ping, pingmsg);
        }

        if(sendsnotice) {
            user->WriteNotice("*** If you are having problems connecting due to registration timeouts type /quote PONG "
                              + pingrpl + " or /raw PONG " + pingrpl + " now.");
        }

        ext.set(user, pingrpl);
        return MOD_RES_PASSTHRU;
    }

    ModResult OnPreCommand(std::string& command, CommandBase::Params& parameters,
                           LocalUser* user, bool validated) CXX11_OVERRIDE {
        if (command == "PONG") {
            std::string* pingrpl = ext.get(user);

            if (pingrpl) {
                if (!parameters.empty() && *pingrpl == parameters[0]) {
                    ext.unset(user);
                    return MOD_RES_DENY;
                } else {
                    if(killonbadreply) {
                        ServerInstance->Users->QuitUser(user, "Incorrect ping reply for registration");
                    }
                    return MOD_RES_DENY;
                }
            }
        }
        return MOD_RES_PASSTHRU;
    }

    ModResult OnCheckReady(LocalUser* user) CXX11_OVERRIDE {
        return ext.get(user) ? MOD_RES_DENY : MOD_RES_PASSTHRU;
    }

    Version GetVersion() CXX11_OVERRIDE {
        return Version("Requires all clients to respond to a PING request before they can fully connect.", VF_VENDOR);
    }
};

MODULE_INIT(ModuleWaitPong)
