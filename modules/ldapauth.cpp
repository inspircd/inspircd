/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2020 Joel Sing <joel@sing.id.au>
 *   Copyright (C) 2019-2023 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2014-2015 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2014 Thiago Crepaldi <thiago@thiagocrepaldi.com>
 *   Copyright (C) 2013-2014, 2017 Adam <Adam@anope.org>
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
#include "modules/ldap.h"

namespace
{
	Module* me;
	std::string killreason;
	BoolExtItem* authed;
	bool verbose;
	std::string vhost;
	StringExtItem* vhosts;
	std::vector<std::pair<std::string, std::string>> requiredattributes;
}

class BindInterface final
	: public LDAPInterface
{
	const std::string provider;
	const std::string uid;
	std::string DN;
	bool checkingAttributes = false;
	bool passed = false;
	int attrCount = 0;

	static std::string SafeReplace(const std::string& text, std::map<std::string, std::string>& replacements)
	{
		std::string result;
		result.reserve(text.length());

		for (unsigned int i = 0; i < text.length(); ++i)
		{
			char c = text[i];
			if (c == '$')
			{
				// find the first nonalpha
				i++;
				unsigned int start = i;

				while (i < text.length() - 1 && isalpha(text[i + 1]))
					++i;

				std::string key(text, start, (i - start) + 1);
				result.append(replacements[key]);
			}
			else
				result.push_back(c);
		}

		return result;
	}

	static void SetVHost(User* user, const std::string& DN)
	{
		if (!vhost.empty())
		{
			irc::commasepstream stream(DN);

			// mashed map of key:value parts of the DN
			std::map<std::string, std::string> dnParts;

			std::string dnPart;
			while (stream.GetToken(dnPart))
			{
				std::string::size_type pos = dnPart.find('=');
				if (pos == std::string::npos) // malformed
					continue;

				std::string key(dnPart, 0, pos);
				std::string value(dnPart, pos + 1, dnPart.length() - pos + 1); // +1s to skip the = itself
				dnParts[key] = value;
			}

			// change host according to config key
			vhosts->Set(user, SafeReplace(vhost, dnParts));
		}
	}

public:
	BindInterface(Module* c, const std::string& p, const std::string& u, const std::string& dn)
		: LDAPInterface(c)
		, provider(p)
		, uid(u)
		, DN(dn)
	{
	}

	void OnResult(const LDAPResult& r) override
	{
		auto* user = ServerInstance->Users.FindUUID(uid);
		dynamic_reference<LDAPProvider> LDAP(me, provider);

		if (!user || !LDAP)
		{
			if (!checkingAttributes || !--attrCount)
				delete this;
			return;
		}

		if (!checkingAttributes && requiredattributes.empty())
		{
			if (verbose)
				ServerInstance->SNO.WriteToSnoMask('c', "Successful connection from {} (dn={})", user->GetRealMask(), DN);

			// We're done, there are no attributes to check
			SetVHost(user, DN);
			authed->Set(user);

			delete this;
			return;
		}

		// Already checked attributes?
		if (checkingAttributes)
		{
			if (!passed)
			{
				// Only one has to pass
				passed = true;

				if (verbose)
					ServerInstance->SNO.WriteToSnoMask('c', "Successful connection from {} (dn={})", user->GetRealMask(), DN);

				SetVHost(user, DN);
				authed->Set(user);
			}

			// Delete this if this is the last ref
			if (!--attrCount)
				delete this;

			return;
		}

		// check required attributes
		checkingAttributes = true;

		for (const auto& [attr, val] : requiredattributes)
		{
			// Note that only one of these has to match for it to be success
			ServerInstance->Logs.Debug(MODNAME, "LDAP compare: {}={}", attr, val);
			try
			{
				LDAP->Compare(this, DN, attr, val);
				++attrCount;
			}
			catch (const LDAPException& ex)
			{
				if (verbose)
					ServerInstance->SNO.WriteToSnoMask('c', "Unable to compare attributes {}={}: {}", attr, val, ex.GetReason());
			}
		}

		// Nothing done
		if (!attrCount)
		{
			if (verbose)
			{
				ServerInstance->SNO.WriteToSnoMask('c', "Forbidden connection from {} (dn={}) (unable to validate attributes)",
					user->GetRealMask(), DN);
			}
			ServerInstance->Users.QuitUser(user, killreason);
			delete this;
		}
	}

	void OnError(const LDAPResult& err) override
	{
		if (checkingAttributes && --attrCount)
			return;

		if (passed)
		{
			delete this;
			return;
		}

		auto* user = ServerInstance->Users.FindUUID(uid);
		if (user)
		{
			if (verbose)
			{
				ServerInstance->SNO.WriteToSnoMask('c', "Forbidden connection from {} ({})",
					user->GetRealMask(), err.getError());
			}
			ServerInstance->Users.QuitUser(user, killreason);
		}

		delete this;
	}
};

class SearchInterface final
	: public LDAPInterface
{
	const std::string provider;
	const std::string uid;

public:
	SearchInterface(Module* c, const std::string& p, const std::string& u)
		: LDAPInterface(c)
		, provider(p)
		, uid(u)
	{
	}

	void OnResult(const LDAPResult& r) override
	{
		LocalUser* user = ServerInstance->Users.FindUUID<LocalUser>(uid);
		dynamic_reference<LDAPProvider> LDAP(me, provider);
		if (!LDAP || r.empty() || !user)
		{
			if (user)
				ServerInstance->Users.QuitUser(user, killreason);
			delete this;
			return;
		}

		try
		{
			const LDAPAttributes& a = r.get(0);
			std::string bindDn = a.get("dn");
			if (bindDn.empty())
			{
				ServerInstance->Users.QuitUser(user, killreason);
				delete this;
				return;
			}

			LDAP->Bind(new BindInterface(this->creator, provider, uid, bindDn), bindDn, user->password);
		}
		catch (const LDAPException& ex)
		{
			ServerInstance->SNO.WriteToSnoMask('a', "Error searching LDAP server: " + ex.GetReason());
		}
		delete this;
	}

	void OnError(const LDAPResult& err) override
	{
		ServerInstance->SNO.WriteToSnoMask('a', "Error searching LDAP server: {}", err.getError());
		auto* user = ServerInstance->Users.FindUUID(uid);
		if (user)
			ServerInstance->Users.QuitUser(user, killreason);
		delete this;
	}
};

class AdminBindInterface final
	: public LDAPInterface
{
	const std::string provider;
	const std::string uuid;
	const std::string base;
	const std::string what;

public:
	AdminBindInterface(Module* c, const std::string& p, const std::string& u, const std::string& b, const std::string& w)
		: LDAPInterface(c)
		, provider(p)
		, uuid(u)
		, base(b)
		, what(w)
	{
	}

	void OnResult(const LDAPResult& r) override
	{
		dynamic_reference<LDAPProvider> LDAP(me, provider);
		if (LDAP)
		{
			try
			{
				LDAP->Search(new SearchInterface(this->creator, provider, uuid), base, what);
			}
			catch (const LDAPException& ex)
			{
				ServerInstance->SNO.WriteToSnoMask('a', "Error searching LDAP server: " + ex.GetReason());
			}
		}
		delete this;
	}

	void OnError(const LDAPResult& err) override
	{
		ServerInstance->SNO.WriteToSnoMask('a', "Error binding as manager to LDAP server: " + err.getError());
		delete this;
	}
};

enum class AuthField
	: uint8_t
{
	NICKNAME,
	USERNAME,
	PASSWORD,
};

class ModuleLDAPAuth final
	: public Module
{
	dynamic_reference<LDAPProvider> LDAP;
	BoolExtItem ldapAuthed;
	StringExtItem ldapVhost;
	std::string base;
	std::string attribute;
	std::vector<std::string> exemptions;
	AuthField field;

public:
	ModuleLDAPAuth()
		: Module(VF_VENDOR, "Allows connecting users to be authenticated against an LDAP database.")
		, LDAP(this, "LDAP")
		, ldapAuthed(this, "ldapauth", ExtensionType::USER)
		, ldapVhost(this, "ldapauth-vhost", ExtensionType::USER)
	{
		me = this;
		authed = &ldapAuthed;
		vhosts = &ldapVhost;
	}

	void ReadConfig(ConfigStatus& status) override
	{
		const auto& tag = ServerInstance->Config->ConfValue("ldapauth");
		base			= tag->getString("baserdn");
		attribute		= tag->getString("attribute");
		killreason		= tag->getString("killreason");
		vhost			= tag->getString("host");
		// Set to true if failed connects should be reported to operators
		verbose			= tag->getBool("verbose");
		field = tag->getEnum("field", AuthField::NICKNAME, {
			{ "nickname", AuthField::USERNAME },
			{ "password", AuthField::PASSWORD },
			{ "username", AuthField::NICKNAME },
		});

		LDAP.SetProvider("LDAP/" + tag->getString("dbid"));

		requiredattributes.clear();
		for (const auto& [_, rtag] : ServerInstance->Config->ConfTags("ldaprequire"))
		{
			const std::string attr = rtag->getString("attribute");
			const std::string val = rtag->getString("value");

			if (!attr.empty() && !val.empty())
				requiredattributes.emplace_back(attr, val);
		}

		exemptions.clear();
		for (const auto& [_, etag] : ServerInstance->Config->ConfTags("ldapexemption"))
		{
			const std::string mask = etag->getString("mask");
			if (!mask.empty())
				exemptions.push_back(mask);
		}
	}

	void OnUserConnect(LocalUser* user) override
	{
		std::string* cc = ldapVhost.Get(user);
		if (cc)
		{
			user->ChangeDisplayedHost(*cc);
			ldapVhost.Unset(user);
		}
	}

	ModResult OnUserRegister(LocalUser* user) override
	{
		for (const auto& exemption : exemptions)
		{
			if (InspIRCd::MatchCIDR(user->GetRealMask(), exemption) || InspIRCd::MatchCIDR(user->GetMask(), exemption))
			{
				ldapAuthed.Set(user, true);
				return MOD_RES_PASSTHRU;
			}
		}

		if (user->password.empty())
		{
			if (verbose)
				ServerInstance->SNO.WriteToSnoMask('c', "Forbidden connection from {} (no password provided)", user->GetRealMask());
			ServerInstance->Users.QuitUser(user, killreason);
			return MOD_RES_DENY;
		}

		if (!LDAP)
		{
			if (verbose)
				ServerInstance->SNO.WriteToSnoMask('c', "Forbidden connection from {} (unable to find LDAP provider)", user->GetRealMask());
			ServerInstance->Users.QuitUser(user, killreason);
			return MOD_RES_DENY;
		}

		std::string what = attribute + "=";
		switch (field)
		{
			case AuthField::NICKNAME:
				what += user->nick;
				break;

			case AuthField::USERNAME:
				what += user->GetRealUser();
				break;

			case AuthField::PASSWORD:
			{
				auto pos = user->password.find(':');
				if (pos == std::string::npos)
				{
					if (verbose)
						ServerInstance->SNO.WriteToSnoMask('c', "Forbidden connection from {} (no username provided)", user->GetRealMask());
					ServerInstance->Users.QuitUser(user, killreason);
					return MOD_RES_DENY;
				}

				what += user->password.substr(0, pos);
				user->password.erase(0, pos + 1);
				user->password.shrink_to_fit();
				break;
			}
		}

		try
		{
			LDAP->BindAsManager(new AdminBindInterface(this, LDAP.GetProvider(), user->uuid, base, what));
		}
		catch (const LDAPException& ex)
		{
			ServerInstance->SNO.WriteToSnoMask('a', "LDAP exception: " + ex.GetReason());
			ServerInstance->Users.QuitUser(user, killreason);
		}

		return MOD_RES_DENY;
	}

	ModResult OnCheckReady(LocalUser* user) override
	{
		return ldapAuthed.Get(user) ? MOD_RES_PASSTHRU : MOD_RES_DENY;
	}
};

MODULE_INIT(ModuleLDAPAuth)
