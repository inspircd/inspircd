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
# pragma comment(lib, "ldap.lib")
# pragma comment(lib, "lber.lib")
#endif

/* $ModDesc: Allow/Deny connections based upon answer from LDAP server */
/* $LinkerFlags: -lldap */

class ModuleLDAPAuth : public Module
{
	LocalIntExt ldapAuthed;
	std::string base;
	std::string attribute;
	std::string ldapserver;
	std::string allowpattern;
	std::string killreason;
	std::string username;
	std::string password;
	std::vector<std::string> whitelistedcidrs;
	int searchscope;
	bool verbose;
	bool useusername;
	LDAP *conn;

public:
	ModuleLDAPAuth() : ldapAuthed("ldapauth", this)
	{
		conn = NULL;
	}

	void init()
	{
		Implementation eventlist[] = { I_OnCheckReady, I_OnRehash, I_OnUserRegister };
		ServerInstance->Modules->Attach(eventlist, this, 3);
		OnRehash(NULL);
	}

	~ModuleLDAPAuth()
	{
		if (conn)
			ldap_unbind_ext(conn, NULL, NULL);
	}

	void OnRehash(User* user)
	{
		ConfigReader Conf;
		whitelistedcidrs.clear();

		base 			= Conf.ReadValue("ldapauth", "baserdn", 0);
		attribute		= Conf.ReadValue("ldapauth", "attribute", 0);
		ldapserver		= Conf.ReadValue("ldapauth", "server", 0);
		allowpattern		= Conf.ReadValue("ldapauth", "allowpattern", 0);
		killreason		= Conf.ReadValue("ldapauth", "killreason", 0);
		std::string scope	= Conf.ReadValue("ldapauth", "searchscope", 0);
		username		= Conf.ReadValue("ldapauth", "binddn", 0);
		password		= Conf.ReadValue("ldapauth", "bindauth", 0);
		verbose			= Conf.ReadFlag("ldapauth", "verbose", 0);		/* Set to true if failed connects should be reported to operators */
		useusername		= Conf.ReadFlag("ldapauth", "userfield", 0);

		ConfigTagList whitelisttags = ServerInstance->Config->ConfTags("ldapwhitelist");

		for (ConfigIter i = whitelisttags.first; i != whitelisttags.second; ++i)
		{
			std::string cidr = i->second->getString("cidr");
			if (!cidr.empty()) {
				whitelistedcidrs.push_back(cidr);
			}
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
				ServerInstance->SNO->WriteToSnoMask('c', "Forbidden connection from %s!%s@%s (No password provided)", user->nick.c_str(), user->ident.c_str(), user->host.c_str());
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
					ServerInstance->SNO->WriteToSnoMask('c', "Forbidden connection from %s!%s@%s (LDAP bind failed: %s)", user->nick.c_str(), user->ident.c_str(), user->host.c_str(), ldap_err2string(res));
				ldap_unbind_ext(conn, NULL, NULL);
				conn = NULL;
				return false;
			}
		}

		LDAPMessage *msg, *entry;
		std::string what = (attribute + "=" + (useusername ? user->ident : user->nick));
		if ((res = ldap_search_ext_s(conn, base.c_str(), searchscope, what.c_str(), NULL, 0, NULL, NULL, NULL, 0, &msg)) != LDAP_SUCCESS)
		{
			// Do a second search, based on password, if it contains a :
			// That is, PASS <user>:<password> will work.
			size_t pos = user->password.find(":");
			if (pos != std::string::npos)
			{
				std::string cutpassword = user->password.substr(0, pos);
				res = ldap_search_ext_s(conn, base.c_str(), searchscope, cutpassword.c_str(), NULL, 0, NULL, NULL, NULL, 0, &msg);

				if (res == LDAP_SUCCESS)
				{
					// Trim the user: prefix, leaving just 'pass' for later password check
					user->password = user->password.substr(pos + 1);
				}
			}

			// It may have found based on user:pass check above.
			if (res != LDAP_SUCCESS)
			{
				if (verbose)
					ServerInstance->SNO->WriteToSnoMask('c', "Forbidden connection from %s!%s@%s (LDAP search failed: %s)", user->nick.c_str(), user->ident.c_str(), user->host.c_str(), ldap_err2string(res));
				return false;
			}
		}
		if (ldap_count_entries(conn, msg) > 1)
		{
			if (verbose)
				ServerInstance->SNO->WriteToSnoMask('c', "Forbidden connection from %s!%s@%s (LDAP search returned more than one result: %s)", user->nick.c_str(), user->ident.c_str(), user->host.c_str(), ldap_err2string(res));
			ldap_msgfree(msg);
			return false;
		}
		if ((entry = ldap_first_entry(conn, msg)) == NULL)
		{
			if (verbose)
				ServerInstance->SNO->WriteToSnoMask('c', "Forbidden connection from %s!%s@%s (LDAP search returned no results: %s)", user->nick.c_str(), user->ident.c_str(), user->host.c_str(), ldap_err2string(res));
			ldap_msgfree(msg);
			return false;
		}
		cred.bv_val = (char*)user->password.data();
		cred.bv_len = user->password.length();
		if ((res = ldap_sasl_bind_s(conn, ldap_get_dn(conn, entry), LDAP_SASL_SIMPLE, &cred, NULL, NULL, NULL)) == LDAP_SUCCESS)
		{
			ldap_msgfree(msg);
			ldapAuthed.set(user,1);
			return true;
		}
		else
		{
			if (verbose)
				ServerInstance->SNO->WriteToSnoMask('c', "Forbidden connection from %s!%s@%s (%s)", user->nick.c_str(), user->ident.c_str(), user->host.c_str(), ldap_err2string(res));
			ldap_msgfree(msg);
			return false;
		}
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
