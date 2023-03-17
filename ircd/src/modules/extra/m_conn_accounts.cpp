/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2018-2020 Matt Schatz <genius3000@g3k.solutions>
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
/// $ModConfig: <connect accounts="sally22 BillBob someoneElse">
/// $ModDepends: core 3
/// $ModDesc: Limit SASL connect classes by account(s).
// "accounts" is a space separated list of EXACT matching account names.
// This module requires that 'm_services_account' be loaded and will
// respect the "requireaccount" setting.


#include "inspircd.h"
#include "modules/account.h"

class ModuleConnAccounts : public Module {
  public:
    void Prioritize() CXX11_OVERRIDE {
        // Go after services_account, it will deny non-authed clients
        Module* servicesaccount = ServerInstance->Modules->Find("m_services_account.so");
        ServerInstance->Modules->SetPriority(this, I_OnSetConnectClass, PRIORITY_AFTER, servicesaccount);
    }

    ModResult OnSetConnectClass(LocalUser* user,
                                ConnectClass* connclass) CXX11_OVERRIDE {
        const std::string accounts = connclass->config->getString("accounts");
        if (accounts.empty() || !connclass->config->getBool("requireaccount")) {
            return MOD_RES_PASSTHRU;
        }

        const AccountExtItem* accountext = GetAccountExtItem();
        const std::string* account = accountext ? accountext->get(user) : NULL;
        if (!account) {
            return MOD_RES_DENY;
        }

        irc::spacesepstream ss(accounts);
        for (std::string token; ss.GetToken(token); ) {
            if (irc::equals(*account, token)) {
                return MOD_RES_PASSTHRU;
            }
        }

        return MOD_RES_DENY;
    }

    Version GetVersion() CXX11_OVERRIDE {
        return Version("Limit SASL connect classes by account(s).");
    }
};

MODULE_INIT(ModuleConnAccounts)
