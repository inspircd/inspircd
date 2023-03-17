/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2020 Matt Schatz <genius3000@g3k.solutions>
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

/// $ModAuthor: genius3000
/// $ModAuthorMail: genius3000@g3k.solutions
/// $ModConfig: <iphost accounts="sally22 BillBob someoneElse"> <class privs="users/iphost">
/// $ModDepends: core 3
/// $ModDesc: Provides message tags for showing the IP address and real host to a privileged user.
// These tags will only be sent to a privileged user if they have enabled the 'message-tags' capability.
// Privileged users are set via an oper class privilege or a space separated list of services accounts.
// Note: m_ircv3_ctctags is required for this module to provide anything.
// Note: m_services_account is required for the services account method to be used.


#include "inspircd.h"
#include "modules/account.h"
#include "modules/ctctags.h"

class TagIPHost : public ClientProtocol::MessageTagProvider {
  private:
    CTCTags::CapReference ctctagcap;

  public:
    std::vector<std::string> accounts;

    TagIPHost(Module* mod)
        : ClientProtocol::MessageTagProvider(mod)
        , ctctagcap(mod) {
    }

    void OnPopulateTags(ClientProtocol::Message& msg) CXX11_OVERRIDE {
        User* const user = msg.GetSourceUser();
        if (user && !IS_SERVER(user) && !user->server->IsULine()) {
            msg.AddTag("inspircd.org/ip", this, user->GetIPString());
            msg.AddTag("inspircd.org/realhost", this, user->GetHost(true));
        }
    }

    ModResult OnProcessTag(User*, const std::string&, std::string&) CXX11_OVERRIDE {
        // Disallow anyone from sending this tag themselves.
        return MOD_RES_DENY;
    }

    bool ShouldSendTag(LocalUser* user,
                       const ClientProtocol::MessageTagData&) CXX11_OVERRIDE {
        if (!ctctagcap.get(user)) {
            return false;
        }

        if (user->HasPrivPermission("users/iphost")) {
            return true;
        }

        if (accounts.empty()) {
            return false;
        }

        const AccountExtItem* accountext = GetAccountExtItem();
        const std::string* account = accountext ? accountext->get(user) : NULL;
        if (!account) {
            return false;
        }

        for (std::vector<std::string>::const_iterator i = accounts.begin(); i != accounts.end(); ++i) {
            if (irc::equals(*account, *i)) {
                return true;
            }
        }

        return false;
    }
};

class ModuleTagIPHost : public Module {
  private:
    TagIPHost iphost;

  public:
    ModuleTagIPHost()
        : iphost(this) {
    }

    void ReadConfig(ConfigStatus&) CXX11_OVERRIDE {
        std::vector<std::string> newaccounts;

        ConfigTag* tag = ServerInstance->Config->ConfValue("iphost");
        const std::string accounts = tag->getString("accounts");

        irc::spacesepstream ss(accounts);
        for (std::string token; ss.GetToken(token); ) {
            newaccounts.push_back(token);
        }

        iphost.accounts.swap(newaccounts);
    }

    Version GetVersion() CXX11_OVERRIDE {
        return Version("Provides message tags for showing the IP address and real host to a privileged user.");
    }
};

MODULE_INIT(ModuleTagIPHost)
