/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2021 Dominic Hamon
 *   Copyright (C) 2020 Joel Sing <joel@sing.id.au>
 *   Copyright (C) 2019 linuxdaemon <linuxdaemon.irc@gmail.com>
 *   Copyright (C) 2016-2017, 2019-2024 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2014 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2013-2016 Adam <Adam@anope.org>
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

/// $CompilerFlags: find_compiler_flags("lber" "") find_compiler_flags("ldap" "")
/// $LinkerFlags: find_linker_flags("lber" "-llber") find_linker_flags("ldap" "-lldap_r")

/// $PackageInfo: require_system("alpine") openldap-dev pkgconf
/// $PackageInfo: require_system("arch") libldap pkgconf
/// $PackageInfo: require_system("darwin") openldap pkg-config
/// $PackageInfo: require_system("debian~") libldap2-dev pkg-config
/// $PackageInfo: require_system("rhel~") openldap-devel pkg-config


#include "inspircd.h"
#include "modules/ldap.h"
#include "threadsocket.h"
#include "utility/string.h"

#if defined LDAP_API_FEATURE_X_OPENLDAP_REENTRANT && !LDAP_API_FEATURE_X_OPENLDAP_REENTRANT
# error InspIRCd requires OpenLDAP to be built as reentrant.
#endif

// Ignore OpenLDAP deprecation warnings on OS X Yosemite and newer.
#if defined __APPLE__
# pragma GCC diagnostic push
# pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

#ifdef _WIN32
# include <Winldap.h>
# include <WinBer.h>
# include <http_parser/http_parser.c>
# define LDAP_OPT_SUCCESS LDAP_SUCCESS
# define LDAP_OPT_NETWORK_TIMEOUT LDAP_OPT_SEND_TIMEOUT
# define LDAP_STR(X) const_cast<PSTR>((X).c_str())
# define LDAP_SASL_SIMPLE static_cast<PSTR>(0)
# define LDAP_TIME(X) reinterpret_cast<PLDAP_TIMEVAL>(&(X))
# define LDAP_VENDOR_VERSION_MAJOR (LDAP_VERSION / 100)
# define LDAP_VENDOR_VERSION_MINOR (LDAP_VERSION / 10 % 10)
# define LDAP_VENDOR_VERSION_PATCH (LDAP_VERSION / 10)
# define ldap_first_message ldap_first_entry
# define ldap_next_message ldap_next_entry
# define ldap_unbind_ext(LDAP, UNUSED1, UNUSED2) ldap_unbind(LDAP)
# pragma comment(lib, "Wldap32.lib")
#else
# include <ldap.h>
# define LDAP_STR(X) ((X).c_str())
# define LDAP_TIME(X) (&(X))
#endif

#ifdef __APPLE__
# pragma GCC diagnostic pop
#endif

class LDAPService;

class LDAPRequest
{
public:
	LDAPService* service;
	LDAPInterface* inter;
	LDAPMessage* message = nullptr; /* message returned by ldap_ */
	LDAPResult* result = nullptr; /* final result */
	struct timeval tv;
	QueryType type;
	int success;

	LDAPRequest(LDAPService* s, LDAPInterface* i, int c)
		: service(s)
		, inter(i)
		, success(c)
	{
		type = QUERY_UNKNOWN;
		tv.tv_sec = 0;
		tv.tv_usec = 100000;
	}

	virtual ~LDAPRequest()
	{
		delete result;
		if (message != nullptr)
			ldap_msgfree(message);
	}

	virtual int run() = 0;
	virtual std::string info() = 0;
};

class LDAPBind final
	: public LDAPRequest
{
	std::string who, pass;

public:
	LDAPBind(LDAPService* s, LDAPInterface* i, const std::string& w, const std::string& p)
		: LDAPRequest(s, i, LDAP_SUCCESS)
		, who(w)
		, pass(p)
	{
		type = QUERY_BIND;
	}

	int run() override;
	std::string info() override;
};

class LDAPSearchRequest final
	: public LDAPRequest
{
	std::string base;
	int searchscope;
	std::string filter;

public:
	LDAPSearchRequest(LDAPService* s, LDAPInterface* i, const std::string& b, int se, const std::string& f)
		: LDAPRequest(s, i, LDAP_SUCCESS)
		, base(b)
		, searchscope(se)
		, filter(f)
	{
		type = QUERY_SEARCH;
	}

	int run() override;
	std::string info() override;
};

class LDAPAdd final
	: public LDAPRequest
{
	std::string dn;
	LDAPMods attributes;

public:
	LDAPAdd(LDAPService* s, LDAPInterface* i, const std::string& d, const LDAPMods& attr)
		: LDAPRequest(s, i, LDAP_SUCCESS)
		, dn(d)
		, attributes(attr)
	{
		type = QUERY_ADD;
	}

	int run() override;
	std::string info() override;
};

class LDAPDel final
	: public LDAPRequest
{
	std::string dn;

public:
	LDAPDel(LDAPService* s, LDAPInterface* i, const std::string& d)
		: LDAPRequest(s, i, LDAP_SUCCESS)
		, dn(d)
	{
		type = QUERY_DELETE;
	}

	int run() override;
	std::string info() override;
};

class LDAPModify final
	: public LDAPRequest
{
	std::string base;
	LDAPMods attributes;

public:
	LDAPModify(LDAPService* s, LDAPInterface* i, const std::string& b, const LDAPMods& attr)
		: LDAPRequest(s, i, LDAP_SUCCESS)
		, base(b)
		, attributes(attr)
	{
		type = QUERY_MODIFY;
	}

	int run() override;
	std::string info() override;
};

class LDAPCompare final
	: public LDAPRequest
{
	std::string dn, attr, val;

public:
	LDAPCompare(LDAPService* s, LDAPInterface* i, const std::string& d, const std::string& a, const std::string& v)
		: LDAPRequest(s, i, LDAP_COMPARE_TRUE)
		, dn(d)
		, attr(a)
		, val(v)
	{
		type = QUERY_COMPARE;
	}

	int run() override;
	std::string info() override;
};

class LDAPService final
	: public LDAPProvider
	, public SocketThread
{
private:
	LDAP* con = nullptr;
	std::shared_ptr<ConfigTag> config;
	time_t last_connect = 0;
	int searchscope;
	time_t timeout;

#ifdef _WIN32
	// Windows LDAP does not implement this so we need to do it.
	int ldap_initialize(LDAP** ldap, const char* url)
	{
		http_parser_url up;
		http_parser_url_init(&up);
		if (http_parser_parse_url(url, strlen(url), 0, &up) != 0)
			return LDAP_CONNECT_ERROR; // Malformed url.

		if (!(up.field_set & (1 << UF_HOST)))
			return LDAP_CONNECT_ERROR; // Missing the host.

		unsigned long port = 389; // Default plaintext port.
		bool secure = false; // LDAP defaults to plaintext.
		if (up.field_set & (1 << UF_SCHEMA))
		{
			const std::string schema(url, up.field_data[UF_SCHEMA].off, up.field_data[UF_SCHEMA].len);
			if (insp::equalsci(schema, "ldaps"))
			{
				port = 636; // Default encrypted port.
				secure = true;
			}
			else if (!insp::equalsci(schema, "ldap"))
				return LDAP_CONNECT_ERROR; // Invalid protocol.
		}

		if (up.field_set & (1 << UF_PORT))
		{
			const std::string portstr(url, up.field_data[UF_PORT].off, up.field_data[UF_PORT].len);
			port = ConvToNum<unsigned long>(portstr);
		}

		const std::string host(url, up.field_data[UF_HOST].off, up.field_data[UF_HOST].len);
		*ldap = ldap_sslinit(LDAP_STR(host), port, secure);
		if (!*ldap)
			return LdapGetLastError(); // Something went wrong, find out what.

		// We're connected to the LDAP server!
		return LDAP_SUCCESS;
	}
#endif

public:
	static LDAPMod** BuildMods(const LDAPMods& attributes)
	{
		LDAPMod** mods = new LDAPMod*[attributes.size() + 1];
		memset(mods, 0, sizeof(LDAPMod*) * (attributes.size() + 1));
		for (unsigned int x = 0; x < attributes.size(); ++x)
		{
			const LDAPModification& l = attributes[x];
			auto* mod = new LDAPMod();
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
		for (unsigned int i = 0; mods[i] != nullptr; ++i)
		{
			LDAPMod* mod = mods[i];
			if (mod->mod_type != nullptr)
				free(mod->mod_type);
			if (mod->mod_values != nullptr)
			{
				for (unsigned int j = 0; mod->mod_values[j] != nullptr; ++j)
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

		ldap_unbind_ext(this->con, nullptr, nullptr);
		Connect();
	}

	int SetOption(int option, const void* value)
	{
		int ret = ldap_set_option(this->con, option, value);
		if (ret != LDAP_OPT_SUCCESS)
		{
			ldap_unbind_ext(this->con, nullptr, nullptr);
			this->con = nullptr;
		}
		return ret;
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
	std::mutex process_mutex; /* held when processing requests not in either queue */

	LDAPService(Module* c, const std::shared_ptr<ConfigTag>& tag)
		: LDAPProvider(c, "LDAP/" + tag->getString("id"))
		, config(tag)
	{
		std::string scope = config->getString("searchscope");
		if (insp::equalsci(scope, "base"))
			searchscope = LDAP_SCOPE_BASE;
		else if (insp::equalsci(scope, "onelevel"))
			searchscope = LDAP_SCOPE_ONELEVEL;
		else
			searchscope = LDAP_SCOPE_SUBTREE;
		timeout = config->getDuration("timeout", 5);

		Connect();
	}

	~LDAPService() override
	{
		this->LockQueue();

		for (auto* req : this->queries)
		{
			/* queries have no results yet */
			req->result = new LDAPResult();
			req->result->type = req->type;
			req->result->error = "LDAP Interface is going away";
			req->inter->OnError(*req->result);

			delete req;
		}
		this->queries.clear();

		for (auto* req : this->results)
		{
			/* even though this may have already finished successfully we return that it didn't */
			req->result->error = "LDAP Interface is going away";
			req->inter->OnError(*req->result);

			delete req;
		}
		this->results.clear();

		this->UnlockQueue();

		ldap_unbind_ext(this->con, nullptr, nullptr);
	}

	void Connect()
	{
		std::string server = config->getString("server");
		int i = ldap_initialize(&this->con, server.c_str());
		if (i != LDAP_SUCCESS)
			throw LDAPException("Unable to connect to LDAP service " + this->name + ": " + ldap_err2string(i));

		const int version = LDAP_VERSION3;
		i = SetOption(LDAP_OPT_PROTOCOL_VERSION, &version);
		if (i != LDAP_OPT_SUCCESS)
			throw LDAPException("Unable to set protocol version for " + this->name + ": " + ldap_err2string(i));

		const struct timeval tv = { 0, 0 };
		i = SetOption(LDAP_OPT_NETWORK_TIMEOUT, &tv);
		if (i != LDAP_OPT_SUCCESS)
			throw LDAPException("Unable to set timeout for " + this->name + ": " + ldap_err2string(i));
	}

	void BindAsManager(LDAPInterface* i) override
	{
		std::string binddn = config->getString("binddn");
		std::string bindauth = config->getString("bindauth");
		this->Bind(i, binddn, bindauth);
	}

	void Bind(LDAPInterface* i, const std::string& who, const std::string& pass) override
	{
		QueueRequest(new LDAPBind(this, i, who, pass));
	}

	void Search(LDAPInterface* i, const std::string& base, const std::string& filter) override
	{
		if (!i)
			throw LDAPException("No interface");

		QueueRequest(new LDAPSearchRequest(this, i, base, searchscope, filter));
	}

	void Add(LDAPInterface* i, const std::string& dn, LDAPMods& attributes) override
	{
		QueueRequest(new LDAPAdd(this, i, dn, attributes));
	}

	void Del(LDAPInterface* i, const std::string& dn) override
	{
		QueueRequest(new LDAPDel(this, i, dn));
	}

	void Modify(LDAPInterface* i, const std::string& base, LDAPMods& attributes) override
	{
		QueueRequest(new LDAPModify(this, i, base, attributes));
	}

	void Compare(LDAPInterface* i, const std::string& dn, const std::string& attr, const std::string& val) override
	{
		QueueRequest(new LDAPCompare(this, i, dn, attr, val));
	}

private:
	void BuildReply(int res, LDAPRequest* req)
	{
		LDAPResult* ldap_result = req->result = new LDAPResult();
		req->result->type = req->type;

		if (res != req->success)
		{
			ldap_result->error = fmt::format("{} ({})", ldap_err2string(res), req->info());
			return;
		}

		if (!req->message)
		{
			return;
		}

		/* a search result */

		for (LDAPMessage* cur = ldap_first_message(this->con, req->message); cur; cur = ldap_next_message(this->con, cur))
		{
			LDAPAttributes attributes;

			char* dn = ldap_get_dn(this->con, cur);
			if (dn != nullptr)
			{
				attributes["dn"].push_back(dn);
				ldap_memfree(dn);
				dn = nullptr;
			}

			BerElement* ber = nullptr;

			for (char* attr = ldap_first_attribute(this->con, cur, &ber); attr; attr = ldap_next_attribute(this->con, cur, ber))
			{
				berval** vals = ldap_get_values_len(this->con, cur, attr);
				int count = ldap_count_values_len(vals);

				std::vector<std::string> attrs;
				for (int j = 0; j < count; ++j)
					attrs.emplace_back(vals[j]->bv_val);
				attributes[attr] = attrs;

				ldap_value_free_len(vals);
				ldap_memfree(attr);
			}
			if (ber != nullptr)
				ber_free(ber, 0);

			ldap_result->messages.push_back(attributes);
		}
	}

	void SendRequests()
	{
		process_mutex.lock();

		query_queue q;
		this->LockQueue();
		queries.swap(q);
		this->UnlockQueue();

		if (q.empty())
		{
			process_mutex.unlock();
			return;
		}

		for (auto* req : q)
		{
			int ret = req->run();

			if (ret == LDAP_SERVER_DOWN || ret == LDAP_TIMEOUT)
			{
				/* try again */
				try
				{
					Reconnect();
				}
				catch (const LDAPException&)
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

		process_mutex.unlock();
	}

public:
	void OnStart() override
	{
		while (!this->IsStopping())
		{
			this->LockQueue();
			if (this->queries.empty())
				this->WaitForQueue();
			this->UnlockQueue();

			SendRequests();
		}
	}

	void OnNotify() override
	{
		query_queue r;

		this->LockQueue();
		this->results.swap(r);
		this->UnlockQueue();

		for (auto* req : r)
		{
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

class ModuleLDAP final
	: public Module
{
	typedef insp::flat_map<std::string, LDAPService*> ServiceMap;
	ServiceMap LDAPServices;

public:
	void init() override
	{
		ServerInstance->Logs.Normal(MODNAME, "Module was compiled against LDAP ({}) version {}.{}.{}",
			LDAP_VENDOR_NAME, LDAP_VENDOR_VERSION_MAJOR, LDAP_VENDOR_VERSION_MINOR,
			LDAP_VENDOR_VERSION_PATCH);
	}
	void ReadConfig(ConfigStatus& status) override
	{
		ServiceMap conns;

		for (const auto& [_, tag] : ServerInstance->Config->ConfTags("database"))
		{
			if (!insp::equalsci(tag->getString("module"), "ldap"))
				continue;

			std::string id = tag->getString("id");

			ServiceMap::iterator curr = LDAPServices.find(id);
			if (curr == LDAPServices.end())
			{
				auto* conn = new LDAPService(this, tag);
				conns[id] = conn;

				ServerInstance->Modules.AddService(*conn);
				conn->Start();
			}
			else
			{
				conns.insert(*curr);
				LDAPServices.erase(curr);
			}
		}

		for (auto& [_, conn] : LDAPServices)
		{
			ServerInstance->Modules.DelService(*conn);
			conn->Stop();
			conn->OnNotify();
			delete conn;
		}

		LDAPServices.swap(conns);
	}

	void OnUnloadModule(Module* m) override
	{
		for (const auto& [_, s] : LDAPServices)
		{
			s->process_mutex.lock();
			s->LockQueue();

			for (size_t i = s->queries.size(); i > 0; --i)
			{
				LDAPRequest* req = s->queries[i - 1];
				LDAPInterface* li = req->inter;

				if (li->creator == m)
				{
					s->queries.erase(s->queries.begin() + i - 1);
					delete req;
				}
			}

			for (size_t i = s->results.size(); i > 0; --i)
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
			s->process_mutex.unlock();
		}
	}

	ModuleLDAP()
		: Module(VF_VENDOR, "Provides the ability for LDAP modules to query a LDAP directory.")
	{
	}

	~ModuleLDAP() override
	{
		for (auto& [_, conn] : LDAPServices)
		{
			conn->Stop();
			conn->OnNotify();
			delete conn;
		}
	}
};

int LDAPBind::run()
{
	berval cred;
	cred.bv_val = strdup(pass.c_str());
	cred.bv_len = pass.length();

	int i = ldap_sasl_bind_s(service->GetConnection(), LDAP_STR(who), LDAP_SASL_SIMPLE, &cred, nullptr, nullptr, nullptr);

	free(cred.bv_val);

	return i;
}

std::string LDAPBind::info()
{
	return "bind dn=" + who;
}

int LDAPSearchRequest::run()
{
	return ldap_search_ext_s(service->GetConnection(), LDAP_STR(base), searchscope, LDAP_STR(filter), nullptr, 0, nullptr, nullptr, LDAP_TIME(tv), 0, &message);
}

std::string LDAPSearchRequest::info()
{
	return "search base=" + base + " filter=" + filter;
}

int LDAPAdd::run()
{
	LDAPMod** mods = LDAPService::BuildMods(attributes);
	int i = ldap_add_ext_s(service->GetConnection(), LDAP_STR(dn), mods, nullptr, nullptr);
	LDAPService::FreeMods(mods);
	return i;
}

std::string LDAPAdd::info()
{
	return "add dn=" + dn;
}

int LDAPDel::run()
{
	return ldap_delete_ext_s(service->GetConnection(), LDAP_STR(dn), nullptr, nullptr);
}

std::string LDAPDel::info()
{
	return "del dn=" + dn;
}

int LDAPModify::run()
{
	LDAPMod** mods = LDAPService::BuildMods(attributes);
	int i = ldap_modify_ext_s(service->GetConnection(), LDAP_STR(base), mods, nullptr, nullptr);
	LDAPService::FreeMods(mods);
	return i;
}

std::string LDAPModify::info()
{
	return "modify base=" + base;
}

int LDAPCompare::run()
{
	berval cred;
	cred.bv_val = strdup(val.c_str());
	cred.bv_len = val.length();

#ifdef _WIN32
	int ret = ldap_compare_ext_s(service->GetConnection(), LDAP_STR(dn), LDAP_STR(attr), nullptr, &cred, nullptr, nullptr);
#else
	int ret = ldap_compare_ext_s(service->GetConnection(), dn.c_str(), attr.c_str(), &cred, nullptr, nullptr);
#endif

	free(cred.bv_val);

	return ret;
}

std::string LDAPCompare::info()
{
	return "compare dn=" + dn + " attr=" + attr;
}

MODULE_INIT(ModuleLDAP)
