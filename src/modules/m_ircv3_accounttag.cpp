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

class AccountTag final
	: public IRCv3::CapTag<AccountTag>
{
private:
	Account::API accountapi;

public:
	const std::string* GetValue(const ClientProtocol::Message& msg) const
	{
		User* const user = msg.GetSourceUser();
		if (!user || !accountapi)
			return nullptr;

		return accountapi->GetAccountName(user);
	}

	AccountTag(Module* mod)
		: IRCv3::CapTag<AccountTag>(mod, "account-tag", "account")
		, accountapi(mod)
	{
	}
};

class AccountIdTag final
	: public ClientProtocol::MessageTagProvider
{
private:
	Account::API accountapi;
	AccountTag& acctag;
	CTCTags::CapReference ctctagcap;

public:
	AccountIdTag(Module* mod, AccountTag& tag)
		: ClientProtocol::MessageTagProvider(mod)
		, accountapi(mod)
		, acctag(tag)
		, ctctagcap(mod)
	{
	}

	void OnPopulateTags(ClientProtocol::Message& msg) override
	{
		const User* user = msg.GetSourceUser();
		const std::string* accountid = accountapi ? accountapi->GetAccountId(user) : nullptr;
		if (accountid)
			msg.AddTag("inspircd.org/account-id", this, *accountid);
	}

	bool ShouldSendTag(LocalUser* user, const ClientProtocol::MessageTagData& tagdata) override
	{
		return acctag.GetCap().IsEnabled(user) && ctctagcap.IsEnabled(user);
	}
};

class ModuleIRCv3AccountTag final
	: public Module
{
private:
	AccountTag tag;
	AccountIdTag idtag;

public:
	ModuleIRCv3AccountTag()
		: Module(VF_VENDOR, "Provides the IRCv3 account-tag client capability.")
		, tag(this)
		, idtag(this, tag)
	{
	}
};

MODULE_INIT(ModuleIRCv3AccountTag)
