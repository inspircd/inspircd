/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2013-2015 Adam <Adam@anope.org>
 *   Copyright (C) 2003-2015 Anope Team <team@anope.org>
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

/// $LinkerFlags: -llber -lldap_r

/// $PackageInfo: require_system("centos") openldap-devel
/// $PackageInfo: require_system("ubuntu") libldap2-dev

#include "inspircd.h"
#include "modules/ldap.h"

// Ignore OpenLDAP deprecation warnings on OS X Yosemite and newer.
#if defined __APPLE__
# pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

#include <ldap.h>

#ifdef _WIN32
# pragma comment(lib, "libldap_r.lib")
# pragma comment(lib, "liblber.lib")
#endif

class LDAPService;

class LDAPRequest
{
 public:
	LDAPService* service;
	LDAPInterface* inter;
	LDAPMessage* message; /* message returned by ldap_ */
	LDAPResult* result; /* final result */
	struct timeval tv;
	QueryType type;

	LDAPRequest(LDAPService* s, LDAPInterface* i)
		: service(s)
		, inter(i)
		, message(NULL)
		, result(NULL)
	{
		type = QUERY_UNKNOWN;
		tv.tv_sec = 0;
		tv.tv_usec = 100000;
	}

	virtual ~LDAPRequest()
	{
		delete result;
		if (message != NULL)
			ldap_msgfree(message);
	}

	virtual int run() = 0;
};

class LDAPBind : public LDAPRequest
{
	std::string who, pass;

 public:
	LDAPBind(LDAPService* s, LDAPInterface* i, const std::string& w, const std::string& p)
		: LDAPRequest(s, i)
		, who(w)
		, pass(p)
	{
		type = QUERY_BIND;
	}

	int run() CXX11_OVERRIDE;
};

class LDAPSearch : public LDAPRequest
{
	std::string base;
	int searchscope;
	std::string filter;

 public:
	LDAPSearch(LDAPService* s, LDAPInterface* i, const std::string& b, int se, const std::string& f)
		: LDAPRequest(s, i)
		, base(b)
		, searchscope(se)
		, filter(f)
	{
		type = QUERY_SEARCH;
	}

	int run() CXX11_OVERRIDE;
};

class LDAPAdd : public LDAPRequest
{
	std::string dn;
	LDAPMods attributes;

 public:
	LDAPAdd(LDAPService* s, LDAPInterface* i, const std::string& d, const LDAPMods& attr)
		: LDAPRequest(s, i)
		, dn(d)
		, attributes(attr)
	{
		type = QUERY_ADD;
	}

	int run() CXX11_OVERRIDE;
};

class LDAPDel : public LDAPRequest
{
	std::string dn;

 public:
	LDAPDel(LDAPService* s, LDAPInterface* i, const std::string& d)
		: LDAPRequest(s, i)
		, dn(d)
	{
		type = QUERY_DELETE;
	}

	int run() CXX11_OVERRIDE;
};

class LDAPModify : public LDAPRequest
{
	std::string base;
	LDAPMods attributes;

 public:
	LDAPModify(LDAPService* s, LDAPInterface* i, const std::string& b, const LDAPMods& attr)
		: LDAPRequest(s, i)
		, base(b)
		, attributes(attr)
	{
		type = QUERY_MODIFY;
	}

	int run() CXX11_OVERRIDE;
};

class LDAPCompare : public LDAPRequest
{
	std::string dn, attr, val;

 public:
	LDAPCompare(LDAPService* s, LDAPInterface* i, const std::string& d, const std::string& a, const std::string& v)
		: LDAPRequest(s, i)
		, dn(d)
		, attr(a)
		, val(v)
	{
		type = QUERY_COMPARE;
	}

	int run() CXX11_OVERRIDE;
};

class LDAPService : public LDAPProvider, public SocketThread
{
	LDAP* con;
	reference<ConfigTag> config;
	time_t last_connect;
	int searchscope;
	time_t timeout;

 public:
	static LDAPMod** BuildMods(const LDAPMods& attributes)
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

	static void FreeMods(LDAPMod** mods)
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

 private:
	void Reconnect()
	{
		// Only try one connect a minute. It is an expensive blocking operation
		if (last_connect > ServerInstance->Time() - 60)
			throw LDAPException("Unable to connect to LDAP service " + this->name + ": reconnecting too fast");
		last_connect = ServerInstance->Time();

		ldap_unbind_ext(this->con, NULL, NULL);
		Connect();
	}

	void QueueRequest(LDAPRequest* r)
	{
		this->LockQueue();
		this->queries.push_back(r);
		this->UnlockQueueWakeup();
	}

 public:
	typedef std::vector<LDAPRequest*> query_queue;
	query_queue queries, results;
	Mutex process_mutex; /* held when processing requests not in either queue */

	LDAPService(Module* c, ConfigTag* tag)
		: LDAPProvider(c, "LDAP/" + tag->getString("id"))
		, con(NULL), config(tag), last_connect(0)
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

		for (unsigned int i = 0; i < this->queries.size(); ++i)
		{
			LDAPRequest* req = this->queries[i];

			/* queries have no results yet */
			req->result = new LDAPResult();
			req->result->type = req->type;
			req->result->error = "LDAP Interface is going away";
			req->inter->OnError(*req->result);

			delete req;
		}
		this->queries.clear();

		for (unsigned int i = 0; i < this->results.size(); ++i)
		{
			LDAPRequest* req = this->results[i];

			/* even though this may have already finished successfully we return that it didn't */
			req->result->error = "LDAP Interface is going away";
			req->inter->OnError(*req->result);

			delete req;
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

	void BindAsManager(LDAPInterface* i) CXX11_OVERRIDE
	{
		std::string binddn = config->getString("binddn");
		std::string bindauth = config->getString("bindauth");
		this->Bind(i, binddn, bindauth);
	}

	void Bind(LDAPInterface* i, const std::string& who, const std::string& pass) CXX11_OVERRIDE
	{
		LDAPBind* b = new LDAPBind(this, i, who, pass);
		QueueRequest(b);
	}

	void Search(LDAPInterface* i, const std::string& base, const std::string& filter) CXX11_OVERRIDE
	{
		if (i == NULL)
			throw LDAPException("No interface");

		LDAPSearch* s = new LDAPSearch(this, i, base, searchscope, filter);
		QueueRequest(s);
	}

	void Add(LDAPInterface* i, const std::string& dn, LDAPMods& attributes) CXX11_OVERRIDE
	{
		LDAPAdd* add = new LDAPAdd(this, i, dn, attributes);
		QueueRequest(add);
	}

	void Del(LDAPInterface* i, const std::string& dn) CXX11_OVERRIDE
	{
		LDAPDel* del = new LDAPDel(this, i, dn);
		QueueRequest(del);
	}

	void Modify(LDAPInterface* i, const std::string& base, LDAPMods& attributes) CXX11_OVERRIDE
	{
		LDAPModify* mod = new LDAPModify(this, i, base, attributes);
		QueueRequest(mod);
	}

	void Compare(LDAPInterface* i, const std::string& dn, const std::string& attr, const std::string& val) CXX11_OVERRIDE
	{
		LDAPCompare* comp = new LDAPCompare(this, i, dn, attr, val);
		QueueRequest(comp);
	}

 private:
	void BuildReply(int res, LDAPRequest* req)
	{
		LDAPResult* ldap_result = req->result = new LDAPResult();
		req->result->type = req->type;

		if (res != LDAP_SUCCESS)
		{
			ldap_result->error = ldap_err2string(res);
			return;
		}

		if (req->message == NULL)
		{
			return;
		}

		/* a search result */

		for (LDAPMessage* cur = ldap_first_message(this->con, req->message); cur; cur = ldap_next_message(this->con, cur))
		{
			LDAPAttributes attributes;

			char* dn = ldap_get_dn(this->con, cur);
			if (dn != NULL)
			{
				attributes["dn"].push_back(dn);
				ldap_memfree(dn);
				dn = NULL;
			}

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

			ldap_result->messages.push_back(attributes);
		}
	}

	void SendRequests()
	{
		process_mutex.Lock();

		query_queue q;
		this->LockQueue();
		queries.swap(q);
		this->UnlockQueue();

		if (q.empty())
		{
			process_mutex.Unlock();
			return;
		}

		for (unsigned int i = 0; i < q.size(); ++i)
		{
			LDAPRequest* req = q[i];
			int ret = req->run();

			if (ret == LDAP_SERVER_DOWN || ret == LDAP_TIMEOUT)
			{
				/* try again */
				try
				{
					Reconnect();
				}
				catch (const LDAPException &)
				{
				}

				ret = req->run();
			}

			BuildReply(ret, req);

			this->LockQueue();
			this->results.push_back(req);
			this->UnlockQueue();
		}

		this->NotifyParent();

		process_mutex.Unlock();
	}

 public:
	void Run() CXX11_OVERRIDE
	{
		while (!this->GetExitFlag())
		{
			this->LockQueue();
			if (this->queries.empty())
				this->WaitForQueue();
			this->UnlockQueue();

			SendRequests();
		}
	}

	void OnNotify() CXX11_OVERRIDE
	{
		query_queue r;

		this->LockQueue();
		this->results.swap(r);
		this->UnlockQueue();

		for (unsigned int i = 0; i < r.size(); ++i)
		{
			LDAPRequest* req = r[i];
			LDAPInterface* li = req->inter;
			LDAPResult* res = req->result;

			if (!res->error.empty())
				li->OnError(*res);
			else
				li->OnResult(*res);

			delete req;
		}
	}

	LDAP* GetConnection()
	{
		return con;
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

			s->process_mutex.Lock();
			s->LockQueue();

			for (unsigned int i = s->queries.size(); i > 0; --i)
			{
				LDAPRequest* req = s->queries[i - 1];
				LDAPInterface* li = req->inter;

				if (li->creator == m)
				{
					s->queries.erase(s->queries.begin() + i - 1);
					delete req;
				}
			}

			for (unsigned int i = s->results.size(); i > 0; --i)
			{
				LDAPRequest* req = s->results[i - 1];
				LDAPInterface* li = req->inter;

				if (li->creator == m)
				{
					s->results.erase(s->results.begin() + i - 1);
					delete req;
				}
			}

			s->UnlockQueue();
			s->process_mutex.Unlock();
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

int LDAPBind::run()
{
	berval cred;
	cred.bv_val = strdup(pass.c_str());
	cred.bv_len = pass.length();

	int i = ldap_sasl_bind_s(service->GetConnection(), who.c_str(), LDAP_SASL_SIMPLE, &cred, NULL, NULL, NULL);

	free(cred.bv_val);

	return i;
}

int LDAPSearch::run()
{
	return ldap_search_ext_s(service->GetConnection(), base.c_str(), searchscope, filter.c_str(), NULL, 0, NULL, NULL, &tv, 0, &message);
}

int LDAPAdd::run()
{
	LDAPMod** mods = LDAPService::BuildMods(attributes);
	int i = ldap_add_ext_s(service->GetConnection(), dn.c_str(), mods, NULL, NULL);
	LDAPService::FreeMods(mods);
	return i;
}

int LDAPDel::run()
{
	return ldap_delete_ext_s(service->GetConnection(), dn.c_str(), NULL, NULL);
}

int LDAPModify::run()
{
	LDAPMod** mods = LDAPService::BuildMods(attributes);
	int i = ldap_modify_ext_s(service->GetConnection(), base.c_str(), mods, NULL, NULL);
	LDAPService::FreeMods(mods);
	return i;
}

int LDAPCompare::run()
{
	berval cred;
	cred.bv_val = strdup(val.c_str());
	cred.bv_len = val.length();

	int ret = ldap_compare_ext_s(service->GetConnection(), dn.c_str(), attr.c_str(), &cred, NULL, NULL);

	free(cred.bv_val);

	return ret;

}

MODULE_INIT(ModuleLDAP)
