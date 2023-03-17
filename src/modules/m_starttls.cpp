/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2020 Matt Schatz <genius3000@g3k.solutions>
 *   Copyright (C) 2019 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2018, 2021 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2014 Adam <Adam@anope.org>
 *   Copyright (C) 2013, 2016 Attila Molnar <attilamolnar@hush.com>
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
#include "modules/ssl.h"
#include "modules/cap.h"

// From IRCv3 tls-3.1
enum {
    RPL_STARTTLS = 670,
    ERR_STARTTLS = 691
};

class CommandStartTLS : public SplitCommand {
    dynamic_reference_nocheck<IOHookProvider>& ssl;

  public:
    CommandStartTLS(Module* mod, dynamic_reference_nocheck<IOHookProvider>& s)
        : SplitCommand(mod, "STARTTLS")
        , ssl(s) {
        works_before_reg = true;
    }

    CmdResult HandleLocal(LocalUser* user,
                          const Params& parameters) CXX11_OVERRIDE {
        if (!ssl) {
            user->WriteNumeric(ERR_STARTTLS, "STARTTLS is not enabled");
            return CMD_FAILURE;
        }

        if (user->registered == REG_ALL) {
            user->WriteNumeric(ERR_STARTTLS,
                               "STARTTLS is not permitted after client registration is complete");
            return CMD_FAILURE;
        }

        if (user->eh.GetIOHook()) {
            user->WriteNumeric(ERR_STARTTLS, "STARTTLS failure");
            return CMD_FAILURE;
        }

        user->WriteNumeric(RPL_STARTTLS, "STARTTLS successful, go ahead with TLS handshake");
        /* We need to flush the write buffer prior to adding the IOHook,
         * otherwise we'll be sending this line inside the TLS (SSL) session - which
         * won't start its handshake until the client gets this line. Currently,
         * we assume the write will not block here; this is usually safe, as
         * STARTTLS is sent very early on in the registration phase, where the
         * user hasn't built up much sendq. Handling a blocked write here would
         * be very annoying.
         */
        user->eh.DoWrite();

        ssl->OnAccept(&user->eh, NULL, NULL);

        return CMD_SUCCESS;
    }
};

class TLSCap : public Cap::Capability {
  private:
    dynamic_reference_nocheck<IOHookProvider>& sslref;

    bool OnList(LocalUser* user) CXX11_OVERRIDE {
        return sslref;
    }

    bool OnRequest(LocalUser* user, bool adding) CXX11_OVERRIDE {
        return sslref;
    }

  public:
    TLSCap(Module* mod, dynamic_reference_nocheck<IOHookProvider>& ssl)
        : Cap::Capability(mod, "tls")
        , sslref(ssl) {
    }
};

class ModuleStartTLS : public Module {
    CommandStartTLS starttls;
    TLSCap tls;
    dynamic_reference_nocheck<IOHookProvider> ssl;

  public:
    ModuleStartTLS()
        : starttls(this, ssl)
        , tls(this, ssl)
        , ssl(this, "ssl") {
    }

    void ReadConfig(ConfigStatus& status) CXX11_OVERRIDE {
        ConfigTag* conf = ServerInstance->Config->ConfValue("starttls");

        std::string newprovider = conf->getString("provider");
        if (newprovider.empty()) {
            ssl.SetProvider("ssl");
        } else {
            ssl.SetProvider("ssl/" + newprovider);
        }
    }

    Version GetVersion() CXX11_OVERRIDE {
        return Version("Provides the IRCv3 tls client capability.", VF_VENDOR);
    }
};

MODULE_INIT(ModuleStartTLS)
