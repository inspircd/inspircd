/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
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
#include "users.h"
#include "channels.h"
#include "modules.h"

#include <ldap.h>

#ifdef _WIN32
# pragma comment(lib, "libldap.lib")
# pragma comment(lib, "liblber.lib")
#endif

/* $ModDesc: Allow/Deny connections based upon answer from LDAP server */
/* $LinkerFlags: -lldap */

struct RAIILDAPString
{
	char *str;

	RAIILDAPString(char *Str)
		: str(Str)
	{
	}

	~RAIILDAPString()
	{
		ldap_memfree(str);
	}

	operator char*()
	{
		return str;
	}

	operator std::string()
	{
		return str;
	}
};

struct RAIILDAPMessage
{
	RAIILDAPMessage()
	{
	}

	~RAIILDAPMessage()
	{
		dealloc();
	}

	void dealloc()
	{
		ldap_msgfree(msg);
	}

	operator LDAPMessage*()
	{
		return msg;
	}

	LDAPMessage **operator &()
	{
		return &msg;
	}

	LDAPMessage *msg;
};

class ModuleLDAPAuth : public Module
{
	LocalIntExt ldapAuthed;
	LocalStringExt ldapVhost;
	std::string base;
	std::string attribute;
	std::string ldapserver;
	std::string allowpattern;
	std::string killreason;
	std::string username;
	std::string password;
	std::string vhost;
	std::vector<std::string> whitelistedcidrs;
	std::vector<std::pair<std::string, std::string> > requiredattributes;
	int searchscope;
	bool verbose;
	bool useusername;
	LDAP *conn;

public:
	ModuleLDAPAuth()
		: ldapAuthed("ldapauth", this)
		, ldapVhost("ldapauth_vhost", this)
	{
		conn = NULL;
	}

	void init()
	{
		ServerInstance->Modules->AddService(ldapAuthed);
		ServerInstance->Modules->AddService(ldapVhost);
		Implementation eventlist[] = { I_OnCheckReady, I_OnRehash,I_OnUserRegister, I_OnUserConnect };
		ServerInstance->Modules->Attach(eventlist, this, sizeof(eventlist)/sizeof(Implementation));
		OnRehash(NULL);
	}

	~ModuleLDAPAuth()
	{
		if (conn)
			ldap_unbind_ext(conn, NULL, NULL);
	}

	void OnRehash(User* user)
	{
		ConfigTag* tag = ServerInstance->Config->ConfValue("ldapauth");
		whitelistedcidrs.clear();
		requiredattributes.clear();

		base 			= tag->getString("baserdn");
		attribute		= tag->getString("attribute");
		ldapserver		= tag->getString("server");
		allowpattern	= tag->getString("allowpattern");
		killreason		= tag->getString("killreason");
		std::string scope	= tag->getString("searchscope");
		username		= tag->getString("binddn");
		password		= tag->getString("bindauth");
		vhost			= tag->getString("host");
		verbose			= tag->getBool("verbose");		/* Set to true if failed connects should be reported to operators */
		useusername		= tag->getBool("userfield");

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

		if (scope == "base")
			searchscope = LDAP_SCOPE_BASE;
		else if (scope == "onelevel")
			searchscope = LDAP_SCOPE_ONELEVEL;
		else searchscope = LDAP_SCOPE_SUBTREE;

		Connect();
	}

	bool Connect()
	{
		if (conn != NULL)
			ldap_unbind_ext(conn, NULL, NULL);
		int res, v = LDAP_VERSION3;
		res = ldap_initialize(&conn, ldapserver.c_str());
		if (res != LDAP_SUCCESS)
		{
			if (verbose)
				ServerInstance->SNO->WriteToSnoMask('c', "LDAP connection failed: %s", ldap_err2string(res));
			conn = NULL;
			return false;
		}

		res = ldap_set_option(conn, LDAP_OPT_PROTOCOL_VERSION, (void *)&v);
		if (res != LDAP_SUCCESS)
		{
			if (verbose)
				ServerInstance->SNO->WriteToSnoMask('c', "LDAP set protocol to v3 failed: %s", ldap_err2string(res));
			ldap_unbind_ext(conn, NULL, NULL);
			conn = NULL;
			return false;
		}
		return true;
	}

	std::string SafeReplace(const std::string &text, std::map<std::string,
			std::string> &replacements)
	{
		std::string result;
		result.reserve(MAXBUF);

		for (unsigned int i = 0; i < text.length(); ++i) {
			char c = text[i];
			if (c == '$') {
				// find the first nonalpha
				i++;
				unsigned int start = i;

				while (i < text.length() - 1 && isalpha(text[i + 1]))
					++i;

				std::string key = text.substr(start, (i - start) + 1);
				result.append(replacements[key]);
			} else {
				result.push_back(c);
			}
		}

	   return result;
	}

	virtual void OnUserConnect(LocalUser *user)
	{
		std::string* cc = ldapVhost.get(user);
		if (cc)
		{
			user->ChangeDisplayedHost(cc->c_str());
			ldapVhost.unset(user);
		}
	}

	ModResult OnUserRegister(LocalUser* user)
	{
		if ((!allowpattern.empty()) && (InspIRCd::Match(user->nick,allowpattern)))
		{
			ldapAuthed.set(user,1);
			return MOD_RES_PASSTHRU;
		}

		for (std::vector<std::string>::iterator i = whitelistedcidrs.begin(); i != whitelistedcidrs.end(); i++)
		{
			if (InspIRCd::MatchCIDR(user->GetIPString(), *i, ascii_case_insensitive_map))
			{
				ldapAuthed.set(user,1);
				return MOD_RES_PASSTHRU;
			}
		}

		if (!CheckCredentials(user))
		{
			ServerInstance->Users->QuitUser(user, killreason);
			return MOD_RES_DENY;
		}
		return MOD_RES_PASSTHRU;
	}

	bool CheckCredentials(LocalUser* user)
	{
		if (conn == NULL)
			if (!Connect())
				return false;

		if (user->password.empty())
		{
			if (verbose)
				ServerInstance->SNO->WriteToSnoMask('c', "Forbidden connection from %s (No password provided)", user->GetFullRealHost().c_str());
			return false;
		}

		int res;
		// bind anonymously if no bind DN and authentication are given in the config
		struct berval cred;
		cred.bv_val = const_cast<char*>(password.c_str());
		cred.bv_len = password.length();

		if ((res = ldap_sasl_bind_s(conn, username.c_str(), LDAP_SASL_SIMPLE, &cred, NULL, NULL, NULL)) != LDAP_SUCCESS)
		{
			if (res == LDAP_SERVER_DOWN)
			{
				// Attempt to reconnect if the connection dropped
				if (verbose)
					ServerInstance->SNO->WriteToSnoMask('a', "LDAP server has gone away - reconnecting...");
				Connect();
				res = ldap_sasl_bind_s(conn, username.c_str(), LDAP_SASL_SIMPLE, &cred, NULL, NULL, NULL);
			}

			if (res != LDAP_SUCCESS)
			{
				if (verbose)
					ServerInstance->SNO->WriteToSnoMask('c', "Forbidden connection from %s (LDAP bind failed: %s)", user->GetFullRealHost().c_str(), ldap_err2string(res));
				ldap_unbind_ext(conn, NULL, NULL);
				conn = NULL;
				return false;
			}
		}

		RAIILDAPMessage msg;
		std::string what;
		std::string::size_type pos = user->password.find(':');
		// If a username is provided in PASS, use it, othewrise user their nick or ident
		if (pos != std::string::npos)
		{
			what = (attribute + "=" + user->password.substr(0, pos));

			// Trim the user: prefix, leaving just 'pass' for later password check
			user->password = user->password.substr(pos + 1);
		}
		else
		{
			what = (attribute + "=" + (useusername ? user->ident : user->nick));
		}
		if ((res = ldap_search_ext_s(conn, base.c_str(), searchscope, what.c_str(), NULL, 0, NULL, NULL, NULL, 0, &msg)) != LDAP_SUCCESS)
		{
			if (verbose)
				ServerInstance->SNO->WriteToSnoMask('c', "Forbidden connection from %s (LDAP search failed: %s)", user->GetFullRealHost().c_str(), ldap_err2string(res));
			return false;
		}
		if (ldap_count_entries(conn, msg) > 1)
		{
			if (verbose)
				ServerInstance->SNO->WriteToSnoMask('c', "Forbidden connection from %s (LDAP search returned more than one result: %s)", user->GetFullRealHost().c_str(), ldap_err2string(res));
			return false;
		}

		LDAPMessage *entry;
		if ((entry = ldap_first_entry(conn, msg)) == NULL)
		{
			if (verbose)
				ServerInstance->SNO->WriteToSnoMask('c', "Forbidden connection from %s (LDAP search returned no results: %s)", user->GetFullRealHost().c_str(), ldap_err2string(res));
			return false;
		}
		cred.bv_val = (char*)user->password.data();
		cred.bv_len = user->password.length();
		RAIILDAPString DN(ldap_get_dn(conn, entry));
		if ((res = ldap_sasl_bind_s(conn, DN, LDAP_SASL_SIMPLE, &cred, NULL, NULL, NULL)) != LDAP_SUCCESS)
		{
			if (verbose)
				ServerInstance->SNO->WriteToSnoMask('c', "Forbidden connection from %s (%s)", user->GetFullRealHost().c_str(), ldap_err2string(res));
			return false;
		}

		if (!requiredattributes.empty())
		{
			bool authed = false;

			for (std::vector<std::pair<std::string, std::string> >::const_iterator it = requiredattributes.begin(); it != requiredattributes.end(); ++it)
			{
				const std::string &attr = it->first;
				const std::string &val = it->second;

				struct berval attr_value;
				attr_value.bv_val = const_cast<char*>(val.c_str());
				attr_value.bv_len = val.length();

				ServerInstance->Logs->Log("m_ldapauth", DEBUG, "LDAP compare: %s=%s", attr.c_str(), val.c_str());

				authed = (ldap_compare_ext_s(conn, DN, attr.c_str(), &attr_value, NULL, NULL) == LDAP_COMPARE_TRUE);

				if (authed)
					break;
			}

			if (!authed)
			{
				if (verbose)
					ServerInstance->SNO->WriteToSnoMask('c', "Forbidden connection from %s (Lacks required LDAP attributes)", user->GetFullRealHost().c_str());
				return false;
			}
		}

		if (!vhost.empty())
		{
			irc::commasepstream stream(DN);

			// mashed map of key:value parts of the DN
			std::map<std::string, std::string> dnParts;

			std::string dnPart;
			while (stream.GetToken(dnPart))
			{
				pos = dnPart.find('=');
				if (pos == std::string::npos) // malformed
					continue;

				std::string key = dnPart.substr(0, pos);
				std::string value = dnPart.substr(pos + 1, dnPart.length() - pos + 1); // +1s to skip the = itself
				dnParts[key] = value;
			}

			// change host according to config key
			ldapVhost.set(user, SafeReplace(vhost, dnParts));
		}

		ldapAuthed.set(user,1);
		return true;
	}

	ModResult OnCheckReady(LocalUser* user)
	{
		return ldapAuthed.get(user) ? MOD_RES_PASSTHRU : MOD_RES_DENY;
	}

	Version GetVersion()
	{
		return Version("Allow/Deny connections based upon answer from LDAP server", VF_VENDOR);
	}

};

MODULE_INIT(ModuleLDAPAuth)
