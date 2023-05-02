/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2023 Sadie Powell <sadie@witchery.services>
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
#include "modules/account.h"
#include "modules/cloak.h"

class AccountMethod
	: public Cloak::Method
{
protected:
	// Dynamic reference to the account api.
	Account::API& accountapi;

	// The characters which are valid in a hostname.
	const CharState& hostmap;

	// The prefix for cloaks (e.g. users.).
	const std::string prefix;

	// Whether to strip non-host characters from the cloak.
	const bool sanitize;

	// The suffix for IP cloaks (e.g. .example.org).
	const std::string suffix;

	// Retrieves the middle segment of the cloak.
	virtual std::string GetMiddle(LocalUser* user)
	{
		const std::string* accountname = accountapi ? accountapi->GetAccountName(user) : nullptr;
		return accountname ? *accountname : "";
	}

public:
	AccountMethod(const Cloak::Engine* engine, const std::shared_ptr<ConfigTag>& tag, const CharState& hm, Account::API& api) ATTR_NOT_NULL(2)
		: Cloak::Method(engine, tag)
		, accountapi(api)
		, hostmap(hm)
		, prefix(tag->getString("prefix"))
		, sanitize(tag->getBool("sanitize", true))
		, suffix(tag->getString("suffix"))
	{
	}

	std::string Generate(LocalUser* user) override ATTR_NOT_NULL(2)
	{
		if (!MatchesUser(user))
			return {}; // We shouldn't cloak this user.

		const std::string middle = GetMiddle(user);
		if (middle.empty())
			return {}; // No middle cloak.

		std::string safemiddle;
		safemiddle.reserve(middle.length());
		for (const auto chr : middle)
		{
			if (!hostmap.test(static_cast<unsigned char>(chr)))
			{
				if (!sanitize)
					return {}; // Contains invalid characters.

				continue;
			}

			safemiddle.push_back(chr);
		}

		if (safemiddle.empty())
			return {}; // No cloak.

		return prefix + safemiddle + suffix;
	}

	std::string Generate(const std::string& hostip) override
	{
		// We can't generate account cloaks without a user.
		return {};
	}

	void GetLinkData(Module::LinkData& data, std::string& compatdata) override
	{
		data["prefix"]   = prefix;
		data["sanitize"] = sanitize ? "yes" : "no";
		data["suffix"]   = suffix;
	}
};

class AccountIdMethod final
	: public AccountMethod
{
protected:
	// Retrieves the middle segment of the cloak.
	std::string GetMiddle(LocalUser* user) override
	{
		const std::string* accountid = accountapi ? accountapi->GetAccountId(user) : nullptr;
		return accountid ? *accountid : "";
	}

public:
	AccountIdMethod(const Cloak::Engine* engine, const std::shared_ptr<ConfigTag>& tag, const CharState& hm, Account::API& api) ATTR_NOT_NULL(2)
		: AccountMethod(engine, tag, hm, api)
	{
	}
};

template <typename Method>
class AccountEngine final
	: public Cloak::Engine
{
private:
	// Dynamic reference to the account api.
	Account::API& accountapi;

	// The characters which are valid in a hostname.
	const CharState& hostmap;

public:
	AccountEngine(Module* Creator, const std::string& Name, const CharState& hm, Account::API& api)
		: Cloak::Engine(Creator, Name)
		, accountapi(api)
		, hostmap(hm)
	{
	}

	Cloak::MethodPtr Create(const std::shared_ptr<ConfigTag>& tag, bool primary) override
	{
		return std::make_shared<Method>(this, tag, hostmap, accountapi);
	}
};

class ModuleCloakAccount final
	: public Module
	, public Account::EventListener
{
private:
	Account::API accountapi;
	AccountEngine<AccountMethod> accountcloak;
	AccountEngine<AccountIdMethod> accountidcloak;
	Cloak::API cloakapi;
	CharState hostmap;

public:
	ModuleCloakAccount()
		: Module(VF_VENDOR, "Adds the account and account-id cloaking methods for use with the cloak module.")
		, Account::EventListener(this)
		, accountapi(this)
		, accountcloak(this, "account", hostmap, accountapi)
		, accountidcloak(this, "account-id", hostmap, accountapi)
		, cloakapi(this)
	{
	}

	void ReadConfig(ConfigStatus& status) override
	{
		CharState newhostmap;
		const auto& tag = ServerInstance->Config->ConfValue("hostname");
		for (const auto chr : tag->getString("charmap", "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz.-_/0123456789", 1))
		{
			// A hostname can not contain NUL, LF, CR, or SPACE.
			if (chr == 0x00 || chr == 0x0A || chr == 0x0D || chr == 0x20)
				throw ModuleException(this, INSP_FORMAT("<hostname:charmap> can not contain character 0x{:02X} ({})", chr, chr));
			newhostmap.set(static_cast<unsigned char>(chr));
		}
		std::swap(newhostmap, hostmap);
	}

	void OnAccountChange(User* user, const std::string& newaccount) override
	{
		LocalUser* luser = IS_LOCAL(user);
		if (luser && cloakapi)
			cloakapi->ResetCloaks(luser, true);
	}
};

MODULE_INIT(ModuleCloakAccount)
