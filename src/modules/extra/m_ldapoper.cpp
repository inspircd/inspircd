/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2010 InspIRCd Development Team
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
#include "users.h"
#include "channels.h"
#include "modules.h"

#include <ldap.h>

/* $ModDesc: Allow/Deny connections based upon answer from LDAP server */
/* $LinkerFlags: -lldap */

class ModuleLDAPAuth : public Module
{
	std::string base;
	std::string ldapserver;
	std::string username;
	std::string password;
	int searchscope;
	LDAP *conn;

public:
	ModuleLDAPAuth()
		{
		conn = NULL;
		Implementation eventlist[] = { I_OnRehash, I_OnPassCompare };
		ServerInstance->Modules->Attach(eventlist, this, 2);
		OnRehash(NULL);
	}

	virtual ~ModuleLDAPAuth()
	{
		if (conn)
			ldap_unbind_ext(conn, NULL, NULL);
	}

	virtual void OnRehash(User* user)
	{
		ConfigReader Conf;

		base 			= Conf.ReadValue("ldapoper", "baserdn", 0);
		ldapserver		= Conf.ReadValue("ldapoper", "server", 0);
		std::string scope	= Conf.ReadValue("ldapoper", "searchscope", 0);
		username		= Conf.ReadValue("ldapoper", "binddn", 0);
		password		= Conf.ReadValue("ldapoper", "bindauth", 0);

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
			conn = NULL;
			return false;
		}

		res = ldap_set_option(conn, LDAP_OPT_PROTOCOL_VERSION, (void *)&v);
		if (res != LDAP_SUCCESS)
		{
			ldap_unbind_ext(conn, NULL, NULL);
			conn = NULL;
			return false;
		}
		return true;
	}

	virtual ModResult OnPassCompare(Extensible* ex, const std::string &data, const std::string &input, const std::string &hashtype)
	{
		User* user = dynamic_cast<User*>(ex);
		if (hashtype == "ldap")
		{
			if (LookupOper(user, data, input))
			{
				/* This is an ldap oper and has been found, claim the OPER command */
				return MOD_RES_DENY;
			}
		}
		/* We don't know this oper! */
		return MOD_RES_PASSTHRU;
	}

	bool LookupOper(User* user, const std::string &what, const std::string &opassword)
	{
		if (conn == NULL)
			if (!Connect())
				return false;

		int res;
		char* authpass = strdup(password.c_str());
		// bind anonymously if no bind DN and authentication are given in the config
		struct berval cred;
		cred.bv_val = authpass;
		cred.bv_len = password.length();

		if ((res = ldap_sasl_bind_s(conn, username.c_str(), LDAP_SASL_SIMPLE, &cred, NULL, NULL, NULL)) != LDAP_SUCCESS)
		{
			if (res == LDAP_SERVER_DOWN)
			{
				// Attempt to reconnect if the connection dropped
				ServerInstance->SNO->WriteToSnoMask('a', "LDAP server has gone away - reconnecting...");
				Connect();
				res = ldap_sasl_bind_s(conn, username.c_str(), LDAP_SASL_SIMPLE, &cred, NULL, NULL, NULL);
			}

			if (res != LDAP_SUCCESS)
			{
				free(authpass);
				ldap_unbind_ext(conn, NULL, NULL);
				conn = NULL;
				return false;
			}
		}
		free(authpass);

		LDAPMessage *msg, *entry;
		if ((res = ldap_search_ext_s(conn, base.c_str(), searchscope, what.c_str(), NULL, 0, NULL, NULL, NULL, 0, &msg)) != LDAP_SUCCESS)
		{
			return false;
		}
		if (ldap_count_entries(conn, msg) > 1)
		{
			ldap_msgfree(msg);
			return false;
		}
		if ((entry = ldap_first_entry(conn, msg)) == NULL)
		{
			ldap_msgfree(msg);
			return false;
		}
		authpass = strdup(opassword.c_str());
		cred.bv_val = authpass;
		cred.bv_len = opassword.length();
		if ((res = ldap_sasl_bind_s(conn, ldap_get_dn(conn, entry), LDAP_SASL_SIMPLE, &cred, NULL, NULL, NULL)) == LDAP_SUCCESS)
		{
			free(authpass);
			ldap_msgfree(msg);
			return true;
		}
		else
		{
			free(authpass);
			ldap_msgfree(msg);
			return false;
		}
	}

	virtual Version GetVersion()
	{
		return Version("Allow/Deny connections based upon answer from LDAP server", VF_VENDOR);
	}

};

MODULE_INIT(ModuleLDAPAuth)
