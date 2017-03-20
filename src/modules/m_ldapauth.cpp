/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2013 Adam <Adam@anope.org>
 *   Copyright (C) 2011 Pierre Carrier <pierre@spotify.com>
 *   Copyright (C) 2009-2010 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2008 Pippijn van Steenhoven <pip88nl@gmail.com>
 *   Copyright (C) 2008 Craig Edwards <craigedwards@brainbox.cc>
 *   Copyright (C) 2008 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2007 Carsten Valdemar Munk <carsten.munk+inspircd@gmail.com>
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
#include "modules/ldap.h"

namespace
{
	Module* me;
	std::string killreason;
	LocalIntExt* authed;
	bool verbose;
	std::string vhost;
	LocalStringExt* vhosts;
	std::vector<std::pair<std::string, std::string> > requiredattributes;
}

class BindInterface : public LDAPInterface
{
	const std::string provider;
	const std::string uid;
	std::string DN;
	bool checkingAttributes;
	bool passed;
	int attrCount;

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
			vhosts->set(user, SafeReplace(vhost, dnParts));
		}
	}

 public:
	BindInterface(Module* c, const std::string& p, const std::string& u, const std::string& dn)
		: LDAPInterface(c)
		, provider(p), uid(u), DN(dn), checkingAttributes(false), passed(false), attrCount(0)
	{
	}

	void OnResult(const LDAPResult& r) CXX11_OVERRIDE
	{
		User* user = ServerInstance->FindUUID(uid);
		dynamic_reference<LDAPProvider> LDAP(me, provider);

		if (!user || !LDAP)
		{
			if (!checkingAttributes || !--attrCount)
				delete this;
			return;
		}

		if (!checkingAttributes && requiredattributes.empty())
		{
			// We're done, there are no attributes to check
			SetVHost(user, DN);
			authed->set(user, 1);

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

				SetVHost(user, DN);
				authed->set(user, 1);
			}

			// Delete this if this is the last ref
			if (!--attrCount)
				delete this;

			return;
		}

		// check required attributes
		checkingAttributes = true;

		for (std::vector<std::pair<std::string, std::string> >::const_iterator it = requiredattributes.begin(); it != requiredattributes.end(); ++it)
		{
			// Note that only one of these has to match for it to be success
			const std::string& attr = it->first;
			const std::string& val = it->second;

			ServerInstance->Logs->Log(MODNAME, LOG_DEBUG, "LDAP compare: %s=%s", attr.c_str(), val.c_str());
			try
			{
				LDAP->Compare(this, DN, attr, val);
				++attrCount;
			}
			catch (LDAPException &ex)
			{
				if (verbose)
					ServerInstance->SNO->WriteToSnoMask('c', "Unable to compare attributes %s=%s: %s", attr.c_str(), val.c_str(), ex.GetReason().c_str());
			}
		}

		// Nothing done
		if (!attrCount)
		{
			if (verbose)
				ServerInstance->SNO->WriteToSnoMask('c', "Forbidden connection from %s (unable to validate attributes)", user->GetFullRealHost().c_str());
			ServerInstance->Users->QuitUser(user, killreason);
			delete this;
		}
	}

	void OnError(const LDAPResult& err) CXX11_OVERRIDE
	{
		if (checkingAttributes && --attrCount)
			return;

		if (passed)
		{
			delete this;
			return;
		}

		User* user = ServerInstance->FindUUID(uid);
		if (user)
		{
			if (verbose)
				ServerInstance->SNO->WriteToSnoMask('c', "Forbidden connection from %s (%s)", user->GetFullRealHost().c_str(), err.getError().c_str());
			ServerInstance->Users->QuitUser(user, killreason);
		}

		delete this;
	}
};

class SearchInterface : public LDAPInterface
{
	const std::string provider;
	const std::string uid;

 public:
	SearchInterface(Module* c, const std::string& p, const std::string& u)
		: LDAPInterface(c), provider(p), uid(u)
	{
	}

	void OnResult(const LDAPResult& r) CXX11_OVERRIDE
	{
		LocalUser* user = static_cast<LocalUser*>(ServerInstance->FindUUID(uid));
		dynamic_reference<LDAPProvider> LDAP(me, provider);
		if (!LDAP || r.empty() || !user)
		{
			if (user)
				ServerInstance->Users->QuitUser(user, killreason);
			delete this;
			return;
		}

		try
		{
			const LDAPAttributes& a = r.get(0);
			std::string bindDn = a.get("dn");
			if (bindDn.empty())
			{
				ServerInstance->Users->QuitUser(user, killreason);
				delete this;
				return;
			}

			LDAP->Bind(new BindInterface(this->creator, provider, uid, bindDn), bindDn, user->password);
		}
		catch (LDAPException& ex)
		{
			ServerInstance->SNO->WriteToSnoMask('a', "Error searching LDAP server: " + ex.GetReason());
		}
		delete this;
	}

	void OnError(const LDAPResult& err) CXX11_OVERRIDE
	{
		ServerInstance->SNO->WriteToSnoMask('a', "Error searching LDAP server: %s", err.getError().c_str());
		User* user = ServerInstance->FindUUID(uid);
		if (user)
			ServerInstance->Users->QuitUser(user, killreason);
		delete this;
	}
};

class AdminBindInterface : public LDAPInterface
{
	const std::string provider;
	const std::string uuid;
	const std::string base;
	const std::string what;

 public:
	AdminBindInterface(Module* c, const std::string& p, const std::string& u, const std::string& b, const std::string& w)
		: LDAPInterface(c), provider(p), uuid(u), base(b), what(w)
	{
	}

	void OnResult(const LDAPResult& r) CXX11_OVERRIDE
	{
		dynamic_reference<LDAPProvider> LDAP(me, provider);
		if (LDAP)
		{
			try
			{
				LDAP->Search(new SearchInterface(this->creator, provider, uuid), base, what);
			}
			catch (LDAPException& ex)
			{
				ServerInstance->SNO->WriteToSnoMask('a', "Error searching LDAP server: " + ex.GetReason());
			}
		}
		delete this;
	}

	void OnError(const LDAPResult& err) CXX11_OVERRIDE
	{
		ServerInstance->SNO->WriteToSnoMask('a', "Error binding as manager to LDAP server: " + err.getError());
		delete this;
	}
};

class ModuleLDAPAuth : public Module
{
	dynamic_reference<LDAPProvider> LDAP;
	LocalIntExt ldapAuthed;
	LocalStringExt ldapVhost;
	std::string base;
	std::string attribute;
	std::vector<std::string> allowpatterns;
	std::vector<std::string> whitelistedcidrs;
	bool useusername;

public:
	ModuleLDAPAuth()
		: LDAP(this, "LDAP")
		, ldapAuthed("ldapauth", ExtensionItem::EXT_USER, this)
		, ldapVhost("ldapauth_vhost", ExtensionItem::EXT_USER, this)
	{
		me = this;
		authed = &ldapAuthed;
		vhosts = &ldapVhost;
	}

	void ReadConfig(ConfigStatus& status) CXX11_OVERRIDE
	{
		ConfigTag* tag = ServerInstance->Config->ConfValue("ldapauth");
		whitelistedcidrs.clear();
		requiredattributes.clear();

		base 			= tag->getString("baserdn");
		attribute		= tag->getString("attribute");
		killreason		= tag->getString("killreason");
		vhost			= tag->getString("host");
		// Set to true if failed connects should be reported to operators
		verbose			= tag->getBool("verbose");
		useusername		= tag->getBool("userfield");

		LDAP.SetProvider("LDAP/" + tag->getString("dbid"));

		ConfigTagList whitelisttags = ServerInstance->Config->ConfTags("ldapwhitelist");

		for (ConfigIter i = whitelisttags.first; i != whitelisttags.second; ++i)
		{
			std::string cidr = i->second->getString("cidr");
			if (!cidr.empty()) {
				whitelistedcidrs.push_back(cidr);
			}
		}

		ConfigTagList attributetags = ServerInstance->Config->ConfTags("ldaprequire");

		for (ConfigIter i = attributetags.first; i != attributetags.second; ++i)
		{
			const std::string attr = i->second->getString("attribute");
			const std::string val = i->second->getString("value");

			if (!attr.empty() && !val.empty())
				requiredattributes.push_back(make_pair(attr, val));
		}

		std::string allowpattern = tag->getString("allowpattern");
		irc::spacesepstream ss(allowpattern);
		for (std::string more; ss.GetToken(more); )
		{
			allowpatterns.push_back(more);
		}
	}

	void OnUserConnect(LocalUser *user) CXX11_OVERRIDE
	{
		std::string* cc = ldapVhost.get(user);
		if (cc)
		{
			user->ChangeDisplayedHost(*cc);
			ldapVhost.unset(user);
		}
	}

	ModResult OnUserRegister(LocalUser* user) CXX11_OVERRIDE
	{
		for (std::vector<std::string>::const_iterator i = allowpatterns.begin(); i != allowpatterns.end(); ++i)
		{
			if (InspIRCd::Match(user->nick, *i))
			{
				ldapAuthed.set(user,1);
				return MOD_RES_PASSTHRU;
			}
		}

		for (std::vector<std::string>::iterator i = whitelistedcidrs.begin(); i != whitelistedcidrs.end(); i++)
		{
			if (InspIRCd::MatchCIDR(user->GetIPString(), *i, ascii_case_insensitive_map))
			{
				ldapAuthed.set(user,1);
				return MOD_RES_PASSTHRU;
			}
		}

		if (user->password.empty())
		{
			if (verbose)
				ServerInstance->SNO->WriteToSnoMask('c', "Forbidden connection from %s (No password provided)", user->GetFullRealHost().c_str());
			ServerInstance->Users->QuitUser(user, killreason);
			return MOD_RES_DENY;
		}

		if (!LDAP)
		{
			if (verbose)
				ServerInstance->SNO->WriteToSnoMask('c', "Forbidden connection from %s (Unable to find LDAP provider)", user->GetFullRealHost().c_str());
			ServerInstance->Users->QuitUser(user, killreason);
			return MOD_RES_DENY;
		}

		std::string what;
		std::string::size_type pos = user->password.find(':');
		if (pos != std::string::npos)
		{
			what = attribute + "=" + user->password.substr(0, pos);

			// Trim the user: prefix, leaving just 'pass' for later password check
			user->password = user->password.substr(pos + 1);
		}
		else
		{
			what = attribute + "=" + (useusername ? user->ident : user->nick);
		}

		try
		{
			LDAP->BindAsManager(new AdminBindInterface(this, LDAP.GetProvider(), user->uuid, base, what));
		}
		catch (LDAPException &ex)
		{
			ServerInstance->SNO->WriteToSnoMask('a', "LDAP exception: " + ex.GetReason());
			ServerInstance->Users->QuitUser(user, killreason);
		}

		return MOD_RES_DENY;
	}

	ModResult OnCheckReady(LocalUser* user) CXX11_OVERRIDE
	{
		return ldapAuthed.get(user) ? MOD_RES_PASSTHRU : MOD_RES_DENY;
	}

	Version GetVersion() CXX11_OVERRIDE
	{
		return Version("Allow/Deny connections based upon answer from LDAP server", VF_VENDOR);
	}
};

MODULE_INIT(ModuleLDAPAuth)
