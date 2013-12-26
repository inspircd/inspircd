/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2013 Adam <Adam@anope.org>
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

#include "modules/ldap.h"

static Module *me;

static void Fallback(User *user, const std::string &username, const std::string &password)
{
	Command* oper_command = ServerInstance->Parser->GetHandler("OPER");
	if (!oper_command)
		return;

	std::vector<std::string> params;
	params.push_back(username);
	params.push_back(password);
	oper_command->Handle(params, user);
}

class BindInterface : public LDAPInterface
{
	std::string uid,
		opername,
		password;

 public:
	BindInterface(Module *c, const std::string &u, const std::string &o, const std::string &p) : LDAPInterface(c), uid(u), opername(o), password(p)
	{
	}

	void OnResult(const LDAPResult &r) CXX11_OVERRIDE
	{
		User* user = ServerInstance->FindUUID(uid);
		OperIndex::iterator iter = ServerInstance->Config->oper_blocks.find(opername);

		if (!user || iter == ServerInstance->Config->oper_blocks.end())
		{
			if (user)
				Fallback(user, opername, password);
			delete this;
			return;
		}

		OperInfo* ifo = iter->second;
		user->Oper(ifo);
		delete this;
	}

	void OnError(const LDAPResult &err) CXX11_OVERRIDE
	{
		User* user = ServerInstance->FindUUID(uid);
		if (user)
			Fallback(user, opername, password);
		delete this;
	}
};

class SearchInterface : public LDAPInterface
{
	std::string provider,
		uid,
		opername,
		password;

 public:
	SearchInterface(Module *c, const std::string &p, const std::string &u, const std::string &o, const std::string &pa) : LDAPInterface(c), provider(p), uid(u), opername(o), password(pa)
	{
	}

	void OnResult(const LDAPResult &r) CXX11_OVERRIDE
	{
		dynamic_reference<LDAPProvider> LDAP(me, provider);
		if (!LDAP || r.empty())
		{
			User* user = ServerInstance->FindUUID(uid);
			if (user)
				Fallback(user, opername, password);
			delete this;
			return;
		}

		try
		{
			const LDAPAttributes &a = r.get(0);
			std::string bindDn = a.get("dn");
			if (bindDn.empty())
			{
				User* user = ServerInstance->FindUUID(uid);
				if (user)
					Fallback(user, opername, password);
				delete this;
				return;
			}

			LDAP->Bind(new BindInterface(this->creator, uid, opername, password), bindDn, password);
		}
		catch (LDAPException &ex)
		{
			ServerInstance->SNO->WriteToSnoMask('a', "Error searching LDAP server: %s", ex.GetReason());
		}
		delete this;
	}

	void OnError(const LDAPResult &err) CXX11_OVERRIDE
	{
		ServerInstance->SNO->WriteToSnoMask('a', "Error searching LDAP server: %s", err.getError().c_str());
		User* user = ServerInstance->FindUUID(uid);
		if (user)
			Fallback(user, opername, password);
		delete this;
	}
};

class ModuleLDAPAuth : public Module
{
	dynamic_reference<LDAPProvider> LDAP;
	std::string base;
	std::string attribute;

 public:
	ModuleLDAPAuth() : LDAP(this, "LDAP")
	{
		me = this;
	}

	~ModuleLDAPAuth()
	{
	}

	void ReadConfig(ConfigStatus& status) CXX11_OVERRIDE
	{
		ConfigTag* tag = ServerInstance->Config->ConfValue("ldapoper");

		LDAP.SetProvider("LDAP/" + tag->getString("dbid"));
		base = tag->getString("baserdn");
		attribute = tag->getString("attribute");
	}

	ModResult OnPreCommand(std::string& command, std::vector<std::string>& parameters, LocalUser* user, bool validated, const std::string& original_line) CXX11_OVERRIDE
	{
		if (validated && command == "OPER" && parameters.size() >= 2)
		{
			const std::string &opername = parameters[0], &password = parameters[1];

			OperIndex::iterator it = ServerInstance->Config->oper_blocks.find(opername);
			if (it == ServerInstance->Config->oper_blocks.end())
				return MOD_RES_PASSTHRU;

			ConfigTag* tag = it->second->oper_block;
			if (!tag)
				return MOD_RES_PASSTHRU;

			std::string acceptedhosts = tag->getString("host");
			std::string hostname = user->ident + "@" + user->host;
			if (!InspIRCd::MatchMask(acceptedhosts, hostname, user->GetIPString()))
				return MOD_RES_PASSTHRU;

			if (!LDAP)
				return MOD_RES_PASSTHRU;

			try
			{
				/* First, bind as the manager so the following search will go through */
				LDAP->BindAsManager(NULL);

				/* Fire off the search */
				std::string what = attribute + "=" + opername;
				LDAP->Search(new SearchInterface(this, LDAP.GetProvider(), user->uuid, opername, password), base, what);
				return MOD_RES_DENY;
			}
			catch (LDAPException &ex)
			{
				ServerInstance->SNO->WriteToSnoMask('a', "LDAP exception: %s", ex.GetReason());
			}
		}

		return MOD_RES_PASSTHRU;
	}

	Version GetVersion() CXX11_OVERRIDE
	{
		return Version("Adds the ability to authenticate opers via LDAP", VF_VENDOR);
	}
};

MODULE_INIT(ModuleLDAPAuth)
