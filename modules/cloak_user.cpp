/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2023-2024 Sadie Powell <sadie@witchery.services>
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
#include "modules/ssl.h"

class UserMethodBase
	: public Cloak::Method
{
protected:
	// The action to take when an invalid character is encountered.
	enum class InvalidChar
		: uint8_t
	{
		// Reject the value as a valid cloak,
		REJECT,

		// Strip the invalid character from the value.
		STRIP,

		// Truncate the value at the missing character.
		TRUNCATE,
	};

	// The action to perform to the cloak value.
	enum class TransformCase
		: uint8_t
	{
		// Preserve the case of the cloak.
		PRESERVE,

		// Convert the cloak to upper case.
		UPPER,

		// Convert the cloak to lower case.
		LOWER,
	};

	// The characters which are valid in a hostname.
	const CharState& hostmap;

	// The action to take when an invalid character is encountered.
	InvalidChar invalidchar;

	// The prefix for cloaks (e.g. users.).
	const std::string prefix;

	// The suffix for IP cloaks (e.g. .example.org).
	const std::string suffix;

	// The case to transform cloaks to.
	TransformCase transformcase;

	// Retrieves the middle segment of the cloak.
	virtual std::string GetMiddle(LocalUser* user) = 0;

	UserMethodBase(const Cloak::Engine* engine, const std::shared_ptr<ConfigTag>& tag, const CharState& hm) ATTR_NOT_NULL(2)
		: Cloak::Method(engine, tag)
		, hostmap(hm)
		, prefix(tag->getString("prefix"))
		, suffix(tag->getString("suffix"))
	{
		invalidchar = tag->getEnum("invalidchar", InvalidChar::STRIP, {
			{ "reject",   InvalidChar::REJECT   },
			{ "strip",    InvalidChar::STRIP    },
			{ "truncate", InvalidChar::TRUNCATE },
		});
		transformcase = tag->getEnum("case", TransformCase::PRESERVE, {
			{ "lower",    TransformCase::LOWER    },
			{ "preserve", TransformCase::PRESERVE },
			{ "upper",    TransformCase::UPPER    },
		});
	}

public:
	std::optional<Cloak::Info> Cloak(LocalUser* user) override ATTR_NOT_NULL(2)
	{
		if (!MatchesUser(user))
			return std::nullopt; // We shouldn't cloak this user.

		const std::string middle = GetMiddle(user);
		if (middle.empty())
			return std::nullopt; // No middle cloak.

		std::string safemiddle;
		safemiddle.reserve(middle.length());
		for (auto chr : middle)
		{
			switch (transformcase)
			{
				case TransformCase::LOWER:
					chr = tolower(chr);
					break;

				case TransformCase::PRESERVE:
					break; // We don't need to do anything here.

				case TransformCase::UPPER:
					chr = toupper(chr);
					break;
			}

			if (hostmap.test(static_cast<unsigned char>(chr)))
			{
				safemiddle.push_back(chr);
				continue; // Character is valid.
			}

			// Character is invalid. What should we do?
			bool done = false;
			switch (invalidchar)
			{
				case InvalidChar::REJECT:
					safemiddle.clear();
					done = true;
					break;

				case InvalidChar::STRIP:
					continue;

				case InvalidChar::TRUNCATE:
					done = true;
					break;
			}

			if (done)
				break;
		}

		ServerInstance->Logs.Debug(MODNAME, "Cleaned {} for cloak: {} => {}",
			GetName(), middle, safemiddle);

		if (safemiddle.empty())
			return std::nullopt; // No cloak.

		return prefix + safemiddle + suffix;
	}

	std::optional<Cloak::Info> Cloak(const std::string& hostip) override
	{
		// We can't generate user cloaks without a user.
		return std::nullopt;
	}

	void GetLinkData(Module::LinkData& data) override
	{
		switch (invalidchar)
		{
			case InvalidChar::REJECT:
				data["invalidchar"] = "reject";
				break;
			case InvalidChar::STRIP:
				data["invalidchar"] = "strip";
				break;
			case InvalidChar::TRUNCATE:
				data["invalidchar"] = "truncate";
				break;
		}
		data["prefix"]   = prefix;
		data["suffix"]   = suffix;
	}
};

class AccountMethod final
	: public UserMethodBase
{
private:
	// Dynamic reference to the account api.
	Account::API accountapi;

	// Retrieves the middle segment of the cloak.
	std::string GetMiddle(LocalUser* user) override
	{
		const std::string* accountname = accountapi ? accountapi->GetAccountName(user) : nullptr;
		return accountname ? *accountname : "";
	}

public:
	AccountMethod(const Cloak::Engine* engine, const std::shared_ptr<ConfigTag>& tag, const CharState& hm) ATTR_NOT_NULL(2)
		: UserMethodBase(engine, tag, hm)
		, accountapi(engine->creator)
	{
	}
};

class AccountIdMethod final
	: public UserMethodBase
{
private:
	// Dynamic reference to the account api.
	Account::API accountapi;

	// Retrieves the middle segment of the cloak.
	std::string GetMiddle(LocalUser* user) override
	{
		const std::string* accountid = accountapi ? accountapi->GetAccountId(user) : nullptr;
		return accountid ? *accountid : "";
	}

public:
	AccountIdMethod(const Cloak::Engine* engine, const std::shared_ptr<ConfigTag>& tag, const CharState& hm) ATTR_NOT_NULL(2)
		: UserMethodBase(engine, tag, hm)
		, accountapi(engine->creator)
	{
	}
};

class FingerprintMethod final
	: public UserMethodBase
{
private:
	// Dynamic reference to the certificate api.
	UserCertificateAPI sslapi;

	// The number of octets of the fingerprint to use.
	size_t length;

	// Retrieves the middle segment of the cloak.
	std::string GetMiddle(LocalUser* user) override
	{
		const ssl_cert* cert = sslapi ? sslapi->GetCertificate(user) : nullptr;
		if (!cert || !cert->IsUsable())
			return {};

		return cert->GetFingerprint().substr(0, length);
	}

	// Calculates the longest valid fingerprint length.
	inline size_t GetMaxLength()
	{
		return ServerInstance->Config->Limits.MaxHost - prefix.length() - suffix.length();
	}

public:
	FingerprintMethod(const Cloak::Engine* engine, const std::shared_ptr<ConfigTag>& tag, const CharState& hm) ATTR_NOT_NULL(2)
		: UserMethodBase(engine, tag, hm)
		, sslapi(engine->creator)
		, length(tag->getNum<size_t>("length", GetMaxLength(), 1, GetMaxLength()))
	{
	}

	void GetLinkData(Module::LinkData& data) override
	{
		UserMethodBase::GetLinkData(data);
		data["length"] = ConvToStr(length);
	}
};

class NickMethod final
	: public UserMethodBase
{
private:
	// Retrieves the middle segment of the cloak.
	std::string GetMiddle(LocalUser* user) override
	{
		return user->nick;
	}

public:
	NickMethod(const Cloak::Engine* engine, const std::shared_ptr<ConfigTag>& tag, const CharState& hm) ATTR_NOT_NULL(2)
		: UserMethodBase(engine, tag, hm)
	{
	}
};

class UserMethod final
	: public UserMethodBase
{
private:
	// Retrieves the middle segment of the cloak.
	std::string GetMiddle(LocalUser* user) override
	{
		return user->GetRealUser();
	}

public:
	UserMethod(const Cloak::Engine* engine, const std::shared_ptr<ConfigTag>& tag, const CharState& hm) ATTR_NOT_NULL(2)
		: UserMethodBase(engine, tag, hm)
	{
	}
};

template <typename Method>
class UserEngine final
	: public Cloak::Engine
{
private:
	// The characters which are valid in a hostname.
	const CharState& hostmap;

public:
	UserEngine(Module* Creator, const std::string& Name, const CharState& hm)
		: Cloak::Engine(Creator, Name)
		, hostmap(hm)
	{
	}

	Cloak::MethodPtr Create(const std::shared_ptr<ConfigTag>& tag, bool primary) override
	{
		return std::make_shared<Method>(this, tag, hostmap);
	}
};

class ModuleCloakUser final
	: public Module
	, public Account::EventListener
{
private:
	UserEngine<AccountMethod> accountcloak;
	UserEngine<AccountIdMethod> accountidcloak;
	UserEngine<FingerprintMethod> fingerprintcloak;
	UserEngine<NickMethod> nicknamecloak;
	UserEngine<UserMethod> usernamecloak;
	Cloak::API cloakapi;
	CharState hostmap;

public:
	ModuleCloakUser()
		: Module(VF_VENDOR, "Adds the account, account-id, fingerprint, nickname, and username cloaking methods for use with the cloak module.")
		, Account::EventListener(this)
		, accountcloak(this, "account", hostmap)
		, accountidcloak(this, "account-id", hostmap)
		, fingerprintcloak(this, "fingerprint", hostmap)
		, nicknamecloak(this, "nickname", hostmap)
		, usernamecloak(this, "username", hostmap)
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
				throw ModuleException(this, FMT::format("<hostname:charmap> can not contain character 0x{:02X} ({})", chr, chr));
			newhostmap.set(static_cast<unsigned char>(chr));
		}
		std::swap(newhostmap, hostmap);
	}

	void OnAccountChange(User* user, const std::string& newaccount) override
	{
		LocalUser* luser = IS_LOCAL(user);
		if (!luser || !cloakapi)
			return;

		if (cloakapi->IsActiveCloak(accountcloak) || cloakapi->IsActiveCloak(accountidcloak))
			cloakapi->ResetCloaks(luser, true);
	}

	void OnChangeRealUser(User* user, const std::string& newuser) override
	{
		LocalUser* luser = IS_LOCAL(user);
		if (luser && cloakapi && cloakapi->IsActiveCloak(usernamecloak))
			cloakapi->ResetCloaks(luser, true);
	}

	void OnUserPostNick(User* user, const std::string& oldnick) override
	{
		LocalUser* luser = IS_LOCAL(user);
		if (luser && cloakapi && cloakapi->IsActiveCloak(nicknamecloak))
			cloakapi->ResetCloaks(luser, true);
	}
};

MODULE_INIT(ModuleCloakUser)
