/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2013-2014 Adam <Adam@anope.org>
 *   Copyright (C) 2003-2014 Anope Team <team@anope.org>
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

#include <ldap.h>

#ifdef _WIN32
# pragma comment(lib, "libldap_r.lib")
# pragma comment(lib, "liblber.lib")
#endif

/* $LinkerFlags: -lldap_r */

class LDAPService : public LDAPProvider, public SocketThread
{
	LDAP* con;
	reference<ConfigTag> config;
	time_t last_connect;
	int searchscope;
	time_t timeout;
	time_t last_timeout_check;

	LDAPMod** BuildMods(const LDAPMods& attributes)
	{
		LDAPMod** mods = new LDAPMod*[attributes.size() + 1];
		memset(mods, 0, sizeof(LDAPMod*) * (attributes.size() + 1));
		for (unsigned int x = 0; x < attributes.size(); ++x)
		{
			const LDAPModification& l = attributes[x];
			LDAPMod* mod = new LDAPMod;
			mods[x] = mod;

			if (l.op == LDAPModification::LDAP_ADD)
				mod->mod_op = LDAP_MOD_ADD;
			else if (l.op == LDAPModification::LDAP_DEL)
				mod->mod_op = LDAP_MOD_DELETE;
			else if (l.op == LDAPModification::LDAP_REPLACE)
				mod->mod_op = LDAP_MOD_REPLACE;
			else if (l.op != 0)
			{
				FreeMods(mods);
				throw LDAPException("Unknown LDAP operation");
			}
			mod->mod_type = strdup(l.name.c_str());
			mod->mod_values = new char*[l.values.size() + 1];
			memset(mod->mod_values, 0, sizeof(char*) * (l.values.size() + 1));
			for (unsigned int j = 0, c = 0; j < l.values.size(); ++j)
				if (!l.values[j].empty())
					mod->mod_values[c++] = strdup(l.values[j].c_str());
		}
		return mods;
	}

	void FreeMods(LDAPMod** mods)
	{
		for (unsigned int i = 0; mods[i] != NULL; ++i)
		{
			LDAPMod* mod = mods[i];
			if (mod->mod_type != NULL)
				free(mod->mod_type);
			if (mod->mod_values != NULL)
			{
				for (unsigned int j = 0; mod->mod_values[j] != NULL; ++j)
					free(mod->mod_values[j]);
				delete[] mod->mod_values;
			}
		}
		delete[] mods;
	}

	void Reconnect()
	{
		// Only try one connect a minute. It is an expensive blocking operation
		if (last_connect > ServerInstance->Time() - 60)
			throw LDAPException("Unable to connect to LDAP service " + this->name + ": reconnecting too fast");
		last_connect = ServerInstance->Time();

		ldap_unbind_ext(this->con, NULL, NULL);
		Connect();
	}

	void SaveInterface(LDAPInterface* i, LDAPQuery msgid)
	{
		if (i != NULL)
		{
			this->LockQueue();
			this->queries[msgid] = std::make_pair(ServerInstance->Time(), i);
			this->UnlockQueueWakeup();
		}
	}

	void Timeout()
	{
		if (last_timeout_check == ServerInstance->Time())
			return;
		last_timeout_check = ServerInstance->Time();

		for (query_queue::iterator it = this->queries.begin(); it != this->queries.end(); )
		{
			LDAPQuery msgid = it->first;
			time_t created = it->second.first;
			LDAPInterface* i = it->second.second;
			++it;

			if (ServerInstance->Time() > created + timeout)
			{
				LDAPResult* ldap_result = new LDAPResult();
				ldap_result->id = msgid;
				ldap_result->error = "Query timed out";

				this->queries.erase(msgid);
				this->results.push_back(std::make_pair(i, ldap_result));

				this->NotifyParent();
			}
		}
	}

 public:
	typedef std::map<LDAPQuery, std::pair<time_t, LDAPInterface*> > query_queue;
	typedef std::vector<std::pair<LDAPInterface*, LDAPResult*> > result_queue;
	query_queue queries;
	result_queue results;

	LDAPService(Module* c, ConfigTag* tag)
		: LDAPProvider(c, "LDAP/" + tag->getString("id"))
		, con(NULL), config(tag), last_connect(0), last_timeout_check(0)
	{
		std::string scope = config->getString("searchscope");
		if (scope == "base")
			searchscope = LDAP_SCOPE_BASE;
		else if (scope == "onelevel")
			searchscope = LDAP_SCOPE_ONELEVEL;
		else
			searchscope = LDAP_SCOPE_SUBTREE;
		timeout = config->getInt("timeout", 5);

		Connect();
	}

	~LDAPService()
	{
		this->LockQueue();

		for (query_queue::iterator i = this->queries.begin(); i != this->queries.end(); ++i)
		{
			LDAPQuery msgid = i->first;
			LDAPInterface* inter = i->second.second;

			ldap_abandon_ext(this->con, msgid, NULL, NULL);

			if (inter)
			{
				LDAPResult r;
				r.error = "LDAP Interface is going away";
				inter->OnError(r);
			}
		}
		this->queries.clear();

		for (result_queue::iterator i = this->results.begin(); i != this->results.end(); ++i)
		{
			LDAPInterface* inter = i->first;
			LDAPResult* r = i->second;

			r->error = "LDAP Interface is going away";
			if (inter)
				inter->OnError(*r);
		}
		this->results.clear();

		this->UnlockQueue();

		ldap_unbind_ext(this->con, NULL, NULL);
	}

	void Connect()
	{
		std::string server = config->getString("server");
		int i = ldap_initialize(&this->con, server.c_str());
		if (i != LDAP_SUCCESS)
			throw LDAPException("Unable to connect to LDAP service " + this->name + ": " + ldap_err2string(i));

		const int version = LDAP_VERSION3;
		i = ldap_set_option(this->con, LDAP_OPT_PROTOCOL_VERSION, &version);
		if (i != LDAP_OPT_SUCCESS)
		{
			ldap_unbind_ext(this->con, NULL, NULL);
			this->con = NULL;
			throw LDAPException("Unable to set protocol version for " + this->name + ": " + ldap_err2string(i));
		}

		const struct timeval tv = { 0, 0 };
		i = ldap_set_option(this->con, LDAP_OPT_NETWORK_TIMEOUT, &tv);
		if (i != LDAP_OPT_SUCCESS)
		{
			ldap_unbind_ext(this->con, NULL, NULL);
			this->con = NULL;
			throw LDAPException("Unable to set timeout for " + this->name + ": " + ldap_err2string(i));
		}
	}

	LDAPQuery BindAsManager(LDAPInterface* i) CXX11_OVERRIDE
	{
		std::string binddn = config->getString("binddn");
		std::string bindauth = config->getString("bindauth");
		return this->Bind(i, binddn, bindauth);
	}

	LDAPQuery Bind(LDAPInterface* i, const std::string& who, const std::string& pass) CXX11_OVERRIDE
	{
		berval cred;
		cred.bv_val = strdup(pass.c_str());
		cred.bv_len = pass.length();

		LDAPQuery msgid;
		int ret = ldap_sasl_bind(con, who.c_str(), LDAP_SASL_SIMPLE, &cred, NULL, NULL, &msgid);
		free(cred.bv_val);
		if (ret != LDAP_SUCCESS)
		{
			if (ret == LDAP_SERVER_DOWN || ret == LDAP_TIMEOUT)
			{
				this->Reconnect();
				return this->Bind(i, who, pass);
			}
			else
				throw LDAPException(ldap_err2string(ret));
		}

		SaveInterface(i, msgid);
		return msgid;
	}

	LDAPQuery Search(LDAPInterface* i, const std::string& base, const std::string& filter) CXX11_OVERRIDE
	{
		if (i == NULL)
			throw LDAPException("No interface");

		LDAPQuery msgid;
		int ret = ldap_search_ext(this->con, base.c_str(), searchscope, filter.c_str(), NULL, 0, NULL, NULL, NULL, 0, &msgid);
		if (ret != LDAP_SUCCESS)
		{
			if (ret == LDAP_SERVER_DOWN || ret == LDAP_TIMEOUT)
			{
				this->Reconnect();
				return this->Search(i, base, filter);
			}
			else
				throw LDAPException(ldap_err2string(ret));
		}

		SaveInterface(i, msgid);
		return msgid;
	}

	LDAPQuery Add(LDAPInterface* i, const std::string& dn, LDAPMods& attributes) CXX11_OVERRIDE
	{
		LDAPMod** mods = this->BuildMods(attributes);
		LDAPQuery msgid;
		int ret = ldap_add_ext(this->con, dn.c_str(), mods, NULL, NULL, &msgid);
		this->FreeMods(mods);

		if (ret != LDAP_SUCCESS)
		{
			if (ret == LDAP_SERVER_DOWN || ret == LDAP_TIMEOUT)
			{
				this->Reconnect();
				return this->Add(i, dn, attributes);
			}
			else
				throw LDAPException(ldap_err2string(ret));
		}

		SaveInterface(i, msgid);
		return msgid;
	}

	LDAPQuery Del(LDAPInterface* i, const std::string& dn) CXX11_OVERRIDE
	{
		LDAPQuery msgid;
		int ret = ldap_delete_ext(this->con, dn.c_str(), NULL, NULL, &msgid);

		if (ret != LDAP_SUCCESS)
		{
			if (ret == LDAP_SERVER_DOWN || ret == LDAP_TIMEOUT)
			{
				this->Reconnect();
				return this->Del(i, dn);
			}
			else
				throw LDAPException(ldap_err2string(ret));
		}

		SaveInterface(i, msgid);
		return msgid;
	}

	LDAPQuery Modify(LDAPInterface* i, const std::string& base, LDAPMods& attributes) CXX11_OVERRIDE
	{
		LDAPMod** mods = this->BuildMods(attributes);
		LDAPQuery msgid;
		int ret = ldap_modify_ext(this->con, base.c_str(), mods, NULL, NULL, &msgid);
		this->FreeMods(mods);

		if (ret != LDAP_SUCCESS)
		{
			if (ret == LDAP_SERVER_DOWN || ret == LDAP_TIMEOUT)
			{
				this->Reconnect();
				return this->Modify(i, base, attributes);
			}
			else
				throw LDAPException(ldap_err2string(ret));
		}

		SaveInterface(i, msgid);
		return msgid;
	}

	LDAPQuery Compare(LDAPInterface* i, const std::string& dn, const std::string& attr, const std::string& val) CXX11_OVERRIDE
	{
		berval cred;
		cred.bv_val = strdup(val.c_str());
		cred.bv_len = val.length();

		LDAPQuery msgid;
		int ret = ldap_compare_ext(con, dn.c_str(), attr.c_str(), &cred, NULL, NULL, &msgid);
		free(cred.bv_val);

		if (ret != LDAP_SUCCESS)
		{
			if (ret == LDAP_SERVER_DOWN || ret == LDAP_TIMEOUT)
			{
				this->Reconnect();
				return this->Compare(i, dn, attr, val);
			}
			else
				throw LDAPException(ldap_err2string(ret));
		}

		SaveInterface(i, msgid);
		return msgid;
	}

	void Run() CXX11_OVERRIDE
	{
		while (!this->GetExitFlag())
		{
			this->LockQueue();
			if (this->queries.empty())
			{
				this->WaitForQueue();
				this->UnlockQueue();
				continue;
			}
			this->Timeout();
			this->UnlockQueue();

			struct timeval tv = { 1, 0 };
			LDAPMessage* result;
			int rtype = ldap_result(this->con, LDAP_RES_ANY, 1, &tv, &result);
			if (rtype <= 0 || this->GetExitFlag())
				continue;

			int cur_id = ldap_msgid(result);

			this->LockQueue();

			query_queue::iterator it = this->queries.find(cur_id);
			if (it == this->queries.end())
			{
				this->UnlockQueue();
				ldap_msgfree(result);
				continue;
			}
			LDAPInterface* i = it->second.second;
			this->queries.erase(it);

			this->UnlockQueue();

			LDAPResult* ldap_result = new LDAPResult();
			ldap_result->id = cur_id;

			for (LDAPMessage* cur = ldap_first_message(this->con, result); cur; cur = ldap_next_message(this->con, cur))
			{
				int cur_type = ldap_msgtype(cur);

				LDAPAttributes attributes;

				{
					char* dn = ldap_get_dn(this->con, cur);
					if (dn != NULL)
					{
						attributes["dn"].push_back(dn);
						ldap_memfree(dn);
					}
				}

				switch (cur_type)
				{
					case LDAP_RES_BIND:
						ldap_result->type = LDAPResult::QUERY_BIND;
						break;
					case LDAP_RES_SEARCH_ENTRY:
						ldap_result->type = LDAPResult::QUERY_SEARCH;
						break;
					case LDAP_RES_ADD:
						ldap_result->type = LDAPResult::QUERY_ADD;
						break;
					case LDAP_RES_DELETE:
						ldap_result->type = LDAPResult::QUERY_DELETE;
						break;
					case LDAP_RES_MODIFY:
						ldap_result->type = LDAPResult::QUERY_MODIFY;
						break;
					case LDAP_RES_SEARCH_RESULT:
						// If we get here and ldap_result->type is LDAPResult::QUERY_UNKNOWN
						// then the result set is empty
						ldap_result->type = LDAPResult::QUERY_SEARCH;
						break;
					case LDAP_RES_COMPARE:
						ldap_result->type = LDAPResult::QUERY_COMPARE;
						break;
					default:
						continue;
				}

				switch (cur_type)
				{
					case LDAP_RES_SEARCH_ENTRY:
					{
						BerElement* ber = NULL;
						for (char* attr = ldap_first_attribute(this->con, cur, &ber); attr; attr = ldap_next_attribute(this->con, cur, ber))
						{
							berval** vals = ldap_get_values_len(this->con, cur, attr);
							int count = ldap_count_values_len(vals);

							std::vector<std::string> attrs;
							for (int j = 0; j < count; ++j)
								attrs.push_back(vals[j]->bv_val);
							attributes[attr] = attrs;

							ldap_value_free_len(vals);
							ldap_memfree(attr);
						}
						if (ber != NULL)
							ber_free(ber, 0);

						break;
					}
					case LDAP_RES_BIND:
					case LDAP_RES_ADD:
					case LDAP_RES_DELETE:
					case LDAP_RES_MODIFY:
					case LDAP_RES_COMPARE:
					{
						int errcode = -1;
						int parse_result = ldap_parse_result(this->con, cur, &errcode, NULL, NULL, NULL, NULL, 0);
						if (parse_result != LDAP_SUCCESS)
						{
							ldap_result->error = ldap_err2string(parse_result);
						}
						else
						{
							if (cur_type == LDAP_RES_COMPARE)
							{
								if (errcode != LDAP_COMPARE_TRUE)
									ldap_result->error = ldap_err2string(errcode);
							}
							else if (errcode != LDAP_SUCCESS)
								ldap_result->error = ldap_err2string(errcode);
						}
						break;
					}
					default:
						continue;
				}

				ldap_result->messages.push_back(attributes);
			}

			ldap_msgfree(result);

			this->LockQueue();
			this->results.push_back(std::make_pair(i, ldap_result));
			this->UnlockQueueWakeup();

			this->NotifyParent();
		}
	}

	void OnNotify() CXX11_OVERRIDE
	{
		LDAPService::result_queue r;

		this->LockQueue();
		this->results.swap(r);
		this->UnlockQueue();

		for (LDAPService::result_queue::iterator i = r.begin(); i != r.end(); ++i)
		{
			LDAPInterface* li = i->first;
			LDAPResult* res = i->second;

			if (!res->error.empty())
				li->OnError(*res);
			else
				li->OnResult(*res);

			delete res;
		}
	}
};

class ModuleLDAP : public Module
{
	typedef insp::flat_map<std::string, LDAPService*> ServiceMap;
	ServiceMap LDAPServices;

 public:
	void ReadConfig(ConfigStatus& status) CXX11_OVERRIDE
	{
		ServiceMap conns;

		ConfigTagList tags = ServerInstance->Config->ConfTags("database");
		for (ConfigIter i = tags.first; i != tags.second; i++)
		{
			const reference<ConfigTag>& tag = i->second;

			if (tag->getString("module") != "ldap")
				continue;

			std::string id = tag->getString("id");

			ServiceMap::iterator curr = LDAPServices.find(id);
			if (curr == LDAPServices.end())
			{
				LDAPService* conn = new LDAPService(this, tag);
				conns[id] = conn;

				ServerInstance->Modules->AddService(*conn);
				ServerInstance->Threads.Start(conn);
			}
			else
			{
				conns.insert(*curr);
				LDAPServices.erase(curr);
			}
		}

		for (ServiceMap::iterator i = LDAPServices.begin(); i != LDAPServices.end(); ++i)
		{
			LDAPService* conn = i->second;
			ServerInstance->Modules->DelService(*conn);
			conn->join();
			conn->OnNotify();
			delete conn;
		}

		LDAPServices.swap(conns);
	}

	void OnUnloadModule(Module* m) CXX11_OVERRIDE
	{
		for (ServiceMap::iterator it = this->LDAPServices.begin(); it != this->LDAPServices.end(); ++it)
		{
			LDAPService* s = it->second;
			s->LockQueue();
			for (LDAPService::query_queue::iterator it2 = s->queries.begin(); it2 != s->queries.end();)
			{
				int msgid = it2->first;
				LDAPInterface* i = it2->second.second;
				++it2;

				if (i->creator == m)
					s->queries.erase(msgid);
			}
			for (unsigned int i = s->results.size(); i > 0; --i)
			{
				LDAPInterface* li = s->results[i - 1].first;
				LDAPResult* r = s->results[i - 1].second;

				if (li->creator == m)
				{
					s->results.erase(s->results.begin() + i - 1);
					delete r;
				}
			}
			s->UnlockQueue();
		}
	}

	~ModuleLDAP()
	{
		for (ServiceMap::iterator i = LDAPServices.begin(); i != LDAPServices.end(); ++i)
		{
			LDAPService* conn = i->second;
			conn->join();
			conn->OnNotify();
			delete conn;
		}
	}

	Version GetVersion() CXX11_OVERRIDE
	{
		return Version("LDAP support", VF_VENDOR);
	}
};

MODULE_INIT(ModuleLDAP)
