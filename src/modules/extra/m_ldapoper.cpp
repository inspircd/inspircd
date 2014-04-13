/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2009 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2008 Pippijn van Steenhoven <pip88nl@gmail.com>
 *   Copyright (C) 2008 Craig Edwards <craigedwards@brainbox.cc>
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

/* $ModDesc: Adds the ability to authenticate opers via LDAP */
/* $LinkerFlags: -lldap */

// Duplicated code, also found in cmd_oper and m_sqloper
static bool OneOfMatches(const char* host, const char* ip, const std::string& hostlist)
{
	std::stringstream hl(hostlist);
	std::string xhost;
	while (hl >> xhost)
	{
		if (InspIRCd::Match(host, xhost, ascii_case_insensitive_map) || InspIRCd::MatchCIDR(ip, xhost, ascii_case_insensitive_map))
		{
			return true;
		}
	}
	return false;
}

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

class ModuleLDAPAuth : public Module
{
	std::string base;
	std::string ldapserver;
	std::string username;
	std::string password;
	std::string attribute;
	int searchscope;
	LDAP *conn;

	bool HandleOper(LocalUser* user, const std::string& opername, const std::string& inputpass)
	{
		OperIndex::iterator it = ServerInstance->Config->oper_blocks.find(opername);
		if (it == ServerInstance->Config->oper_blocks.end())
			return false;

		ConfigTag* tag = it->second->oper_block;
		if (!tag)
			return false;

		std::string acceptedhosts = tag->getString("host");
		std::string hostname = user->ident + "@" + user->host;
		if (!OneOfMatches(hostname.c_str(), user->GetIPString(), acceptedhosts))
			return false;

		if (!LookupOper(opername, inputpass))
			return false;

		user->Oper(it->second);
		return true;
	}

public:
	ModuleLDAPAuth()
		: conn(NULL)
	{
	}

	void init()
	{
		Implementation eventlist[] = { I_OnRehash, I_OnPreCommand };
		ServerInstance->Modules->Attach(eventlist, this, sizeof(eventlist)/sizeof(Implementation));
		OnRehash(NULL);
	}

	virtual ~ModuleLDAPAuth()
	{
		if (conn)
			ldap_unbind_ext(conn, NULL, NULL);
	}

	virtual void OnRehash(User* user)
	{
		ConfigTag* tag = ServerInstance->Config->ConfValue("ldapoper");

		base 			= tag->getString("baserdn");
		ldapserver		= tag->getString("server");
		std::string scope	= tag->getString("searchscope");
		username		= tag->getString("binddn");
		password		= tag->getString("bindauth");
		attribute		= tag->getString("attribute");

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

	ModResult OnPreCommand(std::string& command, std::vector<std::string>& parameters, LocalUser* user, bool validated, const std::string& original_line)
	{
		if (validated && command == "OPER" && parameters.size() >= 2)
		{
			if (HandleOper(user, parameters[0], parameters[1]))
				return MOD_RES_DENY;
		}
		return MOD_RES_PASSTHRU;
	}

	bool LookupOper(const std::string& opername, const std::string& opassword)
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
		std::string what = attribute + "=" + opername;
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
		RAIILDAPString DN(ldap_get_dn(conn, entry));
		if ((res = ldap_sasl_bind_s(conn, DN, LDAP_SASL_SIMPLE, &cred, NULL, NULL, NULL)) == LDAP_SUCCESS)
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
		return Version("Adds the ability to authenticate opers via LDAP", VF_VENDOR);
	}

};

MODULE_INIT(ModuleLDAPAuth)
