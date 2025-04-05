/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2025 Sadie Powell <sadie@witchery.services>
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
#include "extension.h"
#include "modules/cloak.h"
#include "modules/hash.h"
#include "modules/ircv3_replies.h"

class CustomCloakExtItem final
	: public SimpleExtItem<Cloak::Info>
{
public:
	Cloak::API cloakapi;
	Cloak::Engine& customcloak;

	CustomCloakExtItem(Module* mod, Cloak::Engine& cloak)
		: SimpleExtItem<Cloak::Info>(mod, "custom-cloak", ExtensionType::USER, true)
		, cloakapi(mod)
		, customcloak(cloak)
	{
	}

	void FromInternal(Extensible* container, const std::string& value) noexcept override
	{
		if (container->extype != this->extype)
			return;

		if (value.empty())
			Unset(container, false);
		else
		{
			auto cloak = Cloak::Info::FromString(Percent::Decode(value));
			if (cloak.hostname.empty())
				return; // Malformed.

			Set(container, cloak, false);

			auto *luser = IS_LOCAL(static_cast<User*>(container));
			if (!luser || !cloakapi)
				return;

			auto update = false;
			if (!cloak.username.empty() && luser->GetDisplayedUser() != cloak.username)
				update = true;
			else if (cloak.hostname != luser->GetDisplayedUser())
				update = true;

			if (update && cloakapi->IsActiveCloak(customcloak))
				cloakapi->ResetCloaks(luser, true);
		}
	}

	std::string ToInternal(const Extensible* container, void* item) const noexcept override
	{
		return item ? Percent::Encode(static_cast<Cloak::Info*>(item)->ToString()) : std::string();
	}
};

class CloakAccount final
{
private:
	const std::string host;
	const std::string password;
	const std::string passwordhash;

public:
	const Cloak::Info cloak;

	CloakAccount(const std::shared_ptr<ConfigTag>& tag, const Cloak::Info &c, const std::string& p)
		: host(tag->getString("host", "*@*", 1))
		, password(p)
		, passwordhash(tag->getString("hash", "plaintext", 1))
		, cloak(c)
	{
		if (insp::equalsci(passwordhash, "plaintext"))
		{
			ServerInstance->Logs.Warning(MODNAME, "<customcloak> tag at {} contains an plain text password, this is insecure!",
				tag->source.str());
		}
	}

	bool CheckHost(LocalUser* user) const
	{
		return InspIRCd::MatchMask(host, user->GetRealUserHost(), user->GetUserAddress());
	}

	bool CheckPassword(const std::string& pass) const
	{
		return Hash::CheckPassword(password, passwordhash, pass);
	}
};

using CloakAccounts = std::unordered_map<std::string, CloakAccount>;

class CommandCustomCloak final
	: public SplitCommand
{
private:
	CustomCloakExtItem& cloakext;
	UserModeReference cloakmode;
	IRCv3::Replies::Fail failrpl;
	IRCv3::Replies::Note noterpl;
	IRCv3::Replies::CapReference stdrplcap;

	CmdResult FailedLogin(LocalUser* user, const std::string& account)
	{
		failrpl.SendIfCap(user, stdrplcap, this, "LOGIN_FAIL", account, FMT::format("Failed to log into the \x02{}\x02 custom cloak account.", account));
		user->CommandFloodPenalty += 2500;
		return CmdResult::FAILURE;
	}

public:
	CloakAccounts accounts;

	CommandCustomCloak(Module* mod, CustomCloakExtItem& ext)
		: SplitCommand(mod, "CUSTOMCLOAK", 2)
		, cloakext(ext)
		, cloakmode(mod, "cloak")
		, failrpl(mod)
		, noterpl(mod)
		, stdrplcap(mod)
	{
		syntax = { "<username> <password>" };
	}

	CmdResult HandleLocal(LocalUser* user, const Params& parameters) override
	{
		// Check whether the account exists.
		auto it = accounts.find(parameters[0]);
		if (it == accounts.end())
		{
			ServerInstance->Logs.Debug(MODNAME, "{} ({}) [{}] failed to log into the \x02{}\x02 custom cloak account because no account with that name exists.",
				user->nick, user->GetRealUserHost(), user->GetAddress(), parameters[0]);
			return FailedLogin(user, parameters[0]);
		}

		// Check whether the host is correct.
		auto &account = it->second;
		if (!account.CheckHost(user))
		{
			ServerInstance->Logs.Normal(MODNAME, "{} ({}) [{}] failed to log into the \x02{}\x02 custom cloak account because they are connecting from the wrong user@host.",
				user->nick, user->GetRealUserHost(), user->GetAddress(), parameters[0]);
			return FailedLogin(user, it->first);
		}

		// Check whether the password is correct.
		if (!account.CheckPassword(parameters[1]))
		{
			ServerInstance->Logs.Normal(MODNAME, "{} ({}) [{}] failed to log into the \x02{}\x02 custom cloak account because they specified the wrong password.",
				user->nick, user->GetRealUserHost(), user->GetAddress(), parameters[0]);
			return FailedLogin(user, it->first);
		}

		// If they have reached this point then the login succeeded,
		noterpl.SendIfCap(user, stdrplcap, this, "LOGIN_SUCCESS", it->first, account.cloak.ToString(), FMT::format("You are now logged in as \x02{}\x02; updating your cloak to \x02{}\x02.",
			it->first, account.cloak.ToString()));

		cloakext.Set(user, account.cloak);
		if (cloakext.cloakapi && cloakext.cloakapi->IsActiveCloak(cloakext.customcloak))
			cloakext.cloakapi->ResetCloaks(user, true);

		if (cloakmode && !user->IsModeSet(cloakmode))
		{
			Modes::ChangeList changelist;
			changelist.push_add(*cloakmode);
			ServerInstance->Modes.Process(ServerInstance->FakeClient, nullptr, user, changelist);
		}

		return CmdResult::SUCCESS;
	}
};

class CustomMethod final
	: public Cloak::Method
{
private:
	// The cloak to set on users.
	CustomCloakExtItem &customcloakext;

public:
	static bool created;

	CustomMethod(const Cloak::Engine* engine, const std::shared_ptr<ConfigTag>& tag, CustomCloakExtItem &ext) ATTR_NOT_NULL(2)
		: Cloak::Method(engine, tag)
		, customcloakext(ext)
	{
		created = true;
	}

	~CustomMethod()
	{
		created = false;
	}

	std::optional<Cloak::Info> Cloak(LocalUser* user) override ATTR_NOT_NULL(2)
	{
		if (!MatchesUser(user))
			return {}; // We shouldn't cloak this user.

		auto *cloak = customcloakext.Get(user);
		if (!cloak)
			return {}; // No custom cloak;

		return *cloak;
	}

	std::optional<Cloak::Info> Cloak(const std::string& hostip) override
	{
		// We can't generate custom cloaks without a user.
		return std::nullopt;
	}

	void GetLinkData(Module::LinkData& data) override
	{
		// We don't have any link data.
	}
};

bool CustomMethod::created = false;

class CustomEngine final
	: public Cloak::Engine
{
public:
	CustomCloakExtItem customcloakext;

	CustomEngine(Module* mod)
		: Cloak::Engine(mod, "custom")
		, customcloakext(mod, *this)
	{
	}

	Cloak::MethodPtr Create(const std::shared_ptr<ConfigTag>& tag, bool primary) override
	{
		if (CustomMethod::created)
			throw ModuleException(creator, "You can only have one custom cloak method, at " + tag->source.str());

		return std::make_shared<CustomMethod>(this, tag, customcloakext);
	}
};

class ModuleCloakCustom final
	: public Module
{
private:
	CommandCustomCloak cmdcustomcloak;
	CustomEngine customcloak;

public:
	ModuleCloakCustom()
		: Module(VF_VENDOR, "Adds the custom cloaking method for use with the cloak module.")
		, cmdcustomcloak(this, customcloak.customcloakext)
		, customcloak(this)
	{
	}

	void ReadConfig(ConfigStatus& status) override
	{
		CloakAccounts newaccounts;
		for (const auto& [_, tag] : ServerInstance->Config->ConfTags("customcloak"))
		{
			const auto name = tag->getString("name");
			if (name.empty())
				throw ModuleException(this, "<customcloak:name> must not be empty, at " + tag->source.str());

			const auto password = tag->getString("password");
			if (password.empty())
				throw ModuleException(this, "<customcloak:password> must not be empty, at " + tag->source.str());

			Cloak::Info cloak(tag->getString("username"), tag->getString("hostname"));
			if (cloak.hostname.empty())
				throw ModuleException(this, "<customcloak:hostname> must not be empty, at " + tag->source.str());

			CloakAccount account(tag, cloak, password);
			if (!newaccounts.emplace(name, account).second)
				throw ModuleException(this, "<customcloak:name> (" + name + ") used in multiple tags, at " + tag->source.str());
		}
		std::swap(newaccounts, cmdcustomcloak.accounts);
	}
};

MODULE_INIT(ModuleCloakCustom)
