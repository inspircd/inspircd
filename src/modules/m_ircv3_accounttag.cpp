/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2018, 2020 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2018 Attila Molnar <attilamolnar@hush.com>
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
#include "modules/ctctags.h"
#include "modules/ircv3.h"
#include "modules/account.h"

class AccountTag : public IRCv3::CapTag<AccountTag> {
  public:
    const std::string* GetValue(const ClientProtocol::Message& msg) const {
        User* const user = msg.GetSourceUser();
        if (!user) {
            return NULL;
        }

        AccountExtItem* const accextitem = GetAccountExtItem();
        if (!accextitem) {
            return NULL;
        }

        return accextitem->get(user);
    }

    AccountTag(Module* mod)
        : IRCv3::CapTag<AccountTag>(mod, "account-tag", "account") {
    }
};

class AccountIdTag : public ClientProtocol::MessageTagProvider {
  private:
    AccountTag& acctag;
    CTCTags::CapReference ctctagcap;

  public:
    AccountIdTag(Module* mod, AccountTag& tag)
        : ClientProtocol::MessageTagProvider(mod)
        , acctag(tag)
        , ctctagcap(mod) {
    }

    void OnPopulateTags(ClientProtocol::Message& msg) CXX11_OVERRIDE {
        const User* user = msg.GetSourceUser();
        const AccountExtItem* accextitem = user ? GetAccountIdExtItem() : NULL;
        const std::string* accountid = accextitem ? accextitem->get(user) : NULL;
        if (accountid) {
            msg.AddTag("inspircd.org/account-id", this, *accountid);
        }
    }

    bool ShouldSendTag(LocalUser* user,
                       const ClientProtocol::MessageTagData& tagdata) CXX11_OVERRIDE {
        return acctag.GetCap().get(user) && ctctagcap.get(user);
    }
};

class ModuleIRCv3AccountTag : public Module {
  private:
    AccountTag tag;
    AccountIdTag idtag;

  public:
    ModuleIRCv3AccountTag()
        : tag(this)
        , idtag(this, tag) {
    }

    Version GetVersion() CXX11_OVERRIDE {
        return Version("Provides the IRCv3 account-tag client capability.", VF_VENDOR);
    }
};

MODULE_INIT(ModuleIRCv3AccountTag)
