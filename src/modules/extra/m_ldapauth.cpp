/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2009 InspIRCd Development Team
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
	std::string attribute;
	std::string ldapserver;
	std::string allowpattern;
	std::string killreason;
	std::string username;
	std::string password;
	int searchscope;
	bool verbose;
	bool useusername;
	LDAP *conn;

public:
	ModuleLDAPAuth(InspIRCd* Me)
	: Module(Me)
	{
		conn = NULL;
		Implementation eventlist[] = { I_OnUserDisconnect, I_OnCheckReady, I_OnRehash, I_OnUserRegister };
		ServerInstance->Modules->Attach(eventlist, this, 4);
		OnRehash(NULL,"");
	}

	virtual ~ModuleLDAPAuth()
	{
		if (conn)
			ldap_unbind_ext(conn, NULL, NULL);
	}

	virtual void OnRehash(User* user, const std::string &parameter)
	{
		ConfigReader Conf(ServerInstance);

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
				ServerInstance->SNO->WriteToSnoMask('A', "LDAP connection failed: %s", ldap_err2string(res));
			conn = NULL;
			return false;
		}

		res = ldap_set_option(conn, LDAP_OPT_PROTOCOL_VERSION, (void *)&v);
		if (res != LDAP_SUCCESS)
		{
			if (verbose)
				ServerInstance->SNO->WriteToSnoMask('A', "LDAP set protocol to v3 failed: %s", ldap_err2string(res));
			ldap_unbind_ext(conn, NULL, NULL);
			conn = NULL;
			return false;
		}
		return true;
	}

	virtual int OnUserRegister(User* user)
	{
		if ((!allowpattern.empty()) && (InspIRCd::Match(user->nick,allowpattern)))
		{
			user->Extend("ldapauthed");
			return 0;
		}

		if (!CheckCredentials(user))
		{
			ServerInstance->Users->QuitUser(user, killreason);
			return 1;
		}
		return 0;
	}

	bool CheckCredentials(User* user)
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
			free(authpass);
			if (verbose)
				ServerInstance->SNO->WriteToSnoMask('A', "Forbidden connection from %s!%s@%s (LDAP bind failed: %s)", user->nick.c_str(), user->ident.c_str(), user->host.c_str(), ldap_err2string(res));
			ldap_unbind_ext(conn, NULL, NULL);
			conn = NULL;
			return false;
		}
		free(authpass);

		LDAPMessage *msg, *entry;
		std::string what = (attribute + "=" + (useusername ? user->ident : user->nick));
		if ((res = ldap_search_ext_s(conn, base.c_str(), searchscope, what.c_str(), NULL, 0, NULL, NULL, NULL, 0, &msg)) != LDAP_SUCCESS)
		{
			if (verbose)
				ServerInstance->SNO->WriteToSnoMask('A', "Forbidden connection from %s!%s@%s (LDAP search failed: %s)", user->nick.c_str(), user->ident.c_str(), user->host.c_str(), ldap_err2string(res));
			return false;
		}
		if (ldap_count_entries(conn, msg) > 1)
		{
			if (verbose)
				ServerInstance->SNO->WriteToSnoMask('A', "Forbidden connection from %s!%s@%s (LDAP search returned more than one result: %s)", user->nick.c_str(), user->ident.c_str(), user->host.c_str(), ldap_err2string(res));
			ldap_msgfree(msg);
			return false;
		}
		if ((entry = ldap_first_entry(conn, msg)) == NULL)
		{
			if (verbose)
				ServerInstance->SNO->WriteToSnoMask('A', "Forbidden connection from %s!%s@%s (LDAP search returned no results: %s)", user->nick.c_str(), user->ident.c_str(), user->host.c_str(), ldap_err2string(res));
			ldap_msgfree(msg);
			return false;
		}
		if (user->password.empty())
		{
			if (verbose)
				ServerInstance->SNO->WriteToSnoMask('A', "Forbidden connection from %s!%s@%s (No password provided)", user->nick.c_str(), user->ident.c_str(), user->host.c_str());
			user->Extend("ldapauth_failed");
			return false;
		}
		cred.bv_val = (char*)user->password.data();
		cred.bv_len = user->password.length();
		if ((res = ldap_sasl_bind_s(conn, ldap_get_dn(conn, entry), LDAP_SASL_SIMPLE, &cred, NULL, NULL, NULL)) == LDAP_SUCCESS)
		{
			ldap_msgfree(msg);
			user->Extend("ldapauthed");
			return true;
		}
		else
		{
			if (verbose)
				ServerInstance->SNO->WriteToSnoMask('A', "Forbidden connection from %s!%s@%s (%s)", user->nick.c_str(), user->ident.c_str(), user->host.c_str(), ldap_err2string(res));
			ldap_msgfree(msg);
			user->Extend("ldapauth_failed");
			return false;
		}
	}


	virtual void OnUserDisconnect(User* user)
	{
		user->Shrink("ldapauthed");
		user->Shrink("ldapauth_failed");
	}

	virtual bool OnCheckReady(User* user)
	{
		return user->GetExt("ldapauthed");
	}

	virtual Version GetVersion()
	{
		return Version("$Id$", VF_VENDOR, API_VERSION);
	}

};

MODULE_INIT(ModuleLDAPAuth)
