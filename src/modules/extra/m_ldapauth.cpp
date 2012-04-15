/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2011 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 *
 * Taken from the UnrealIRCd 4.0 SVN version, based on
 * InspIRCd 1.1.x.
 *
 * UnrealIRCd 4.0 (C) 2007 Carsten Valdemar Munk
 * This program is free but copyrighted software; see
 *	    the file COPYING for details.
 *
 * ---------------------------------------------------
 * Heavily based on SQLauth
 */

#include "inspircd.h"
#include "account.h"

#include <ldap.h>

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
	bool setaccount;
	LDAP *conn;
	dynamic_reference<AccountProvider> acctprov;

public:
	ModuleLDAPAuth() : ldapAuthed(EXTENSIBLE_USER, "ldapauth", this), acctprov("account")
	{
		conn = NULL;
	}

	void init()
	{
		Implementation eventlist[] = { I_OnCheckReady, I_OnUserRegister };
		ServerInstance->Modules->Attach(eventlist, this, sizeof(eventlist)/sizeof(Implementation));
	}

	~ModuleLDAPAuth()
	{
		if (conn)
			ldap_unbind_ext(conn, NULL, NULL);
	}

	void ReadConfig(ConfigReadStatus&)
	{
		whitelistedcidrs.clear();

		base 			= ServerInstance->Config->GetTag("ldapauth")->getString("baserdn");
		attribute		= ServerInstance->Config->GetTag("ldapauth")->getString("attribute");
		ldapserver		= ServerInstance->Config->GetTag("ldapauth")->getString("server");
		allowpattern		= ServerInstance->Config->GetTag("ldapauth")->getString("allowpattern");
		killreason		= ServerInstance->Config->GetTag("ldapauth")->getString("killreason");
		std::string scope	= ServerInstance->Config->GetTag("ldapauth")->getString("searchscope");
		username		= ServerInstance->Config->GetTag("ldapauth")->getString("binddn");
		password		= ServerInstance->Config->GetTag("ldapauth")->getString("bindauth");
		verbose			= ServerInstance->Config->GetTag("ldapauth")->getBool("verbose");		/* Set to true if failed connects should be reported to operators */
		useusername		= ServerInstance->Config->GetTag("ldapauth")->getBool("userfield");
		setaccount		= ServerInstance->Config->GetTag("ldapauth")->getBool("setaccount");
		ConfigTagList whitelisttags	= ServerInstance->Config->GetTags("ldapwhitelist");

		for (ConfigIter i = whitelisttags.first; i != whitelisttags.second; ++i)
		{
			std::string cidr = i->second->getString("cidr");
			if (!cidr.empty())
				whitelistedcidrs.push_back(cidr);
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

	void OnUserRegister(LocalUser* user)
	{
		if ((!allowpattern.empty()) && (InspIRCd::Match(user->nick,allowpattern)))
		{
			ldapAuthed.set(user,1);
			return;
		}

		for (std::vector<std::string>::iterator i = whitelistedcidrs.begin(); i != whitelistedcidrs.end(); i++)
		{
			if (InspIRCd::MatchCIDR(user->GetIPString(), *i, ascii_case_insensitive_map))
			{
				ldapAuthed.set(user,1);
				return;
			}
		}

		if (!CheckCredentials(user))
		{
			ServerInstance->Users->QuitUser(user, killreason);
		}
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

		std::string acctname = useusername ? user->ident : user->nick;
		LDAPMessage *msg, *entry;
		std::string what = attribute + "=" + acctname;
		if ((res = ldap_search_ext_s(conn, base.c_str(), searchscope, what.c_str(), NULL, 0, NULL, NULL, NULL, 0, &msg)) != LDAP_SUCCESS)
		{
			// Do a second search, based on password, if it contains a :
			// That is, PASS <user>:<password> will work.
			size_t pos = user->password.find(":");
			if (pos != std::string::npos)
			{
				acctname = user->password.substr(0, pos);
				what = attribute + "=" + acctname;
				res = ldap_search_ext_s(conn, base.c_str(), searchscope, what.c_str(), NULL, 0, NULL, NULL, NULL, 0, &msg);

				if (res)
				{
					// Trim the user: prefix, leaving just 'pass' for later password check
					user->password = user->password.substr(pos + 1);
				}
			}

			// It may have found based on user:pass check above.
			if (!res)
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
			if (setaccount && acctprov)
				acctprov->DoLogin(user, acctname);
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
