/*
 *
 * (C) 2011-2023 Anope Team
 * Contact us at team@anope.org
 *
 * Please read COPYING and README for further details.
 *
 * Based on the original code of Epona by Lara.
 * Based on the original code of Services by Andy Church.
 */

/* RequiredLibraries: ldap_r|ldap,lber */
/* RequiredWindowsLibraries: libldap_r|libldap,liblber */

#include "module.h"
#include "modules/ldap.h"
#include <ldap.h>

#if defined LDAP_API_FEATURE_X_OPENLDAP_REENTRANT && !LDAP_API_FEATURE_X_OPENLDAP_REENTRANT
# error Anope requires OpenLDAP to be built as reentrant.
#endif


class LDAPService;
static Pipe *me;

class LDAPRequest {
  public:
    LDAPService *service;
    LDAPInterface *inter;
    LDAPMessage *message; /* message returned by ldap_ */
    LDAPResult *result; /* final result */
    struct timeval tv;
    QueryType type;

    LDAPRequest(LDAPService *s, LDAPInterface *i)
        : service(s)
        , inter(i)
        , message(NULL)
        , result(NULL) {
        type = QUERY_UNKNOWN;
        tv.tv_sec = 0;
        tv.tv_usec = 100000;
    }

    virtual ~LDAPRequest() {
        delete result;
        if (inter != NULL) {
            inter->OnDelete();
        }
        if (message != NULL) {
            ldap_msgfree(message);
        }
    }

    virtual int run() = 0;
};

class LDAPBind : public LDAPRequest {
    Anope::string who, pass;

  public:
    LDAPBind(LDAPService *s, LDAPInterface *i, const Anope::string &w,
             const Anope::string &p)
        : LDAPRequest(s, i)
        , who(w)
        , pass(p) {
        type = QUERY_BIND;
    }

    int run() anope_override;
};

class LDAPSearch : public LDAPRequest {
    Anope::string base;
    Anope::string filter;

  public:
    LDAPSearch(LDAPService *s, LDAPInterface *i, const Anope::string &b,
               const Anope::string &f)
        : LDAPRequest(s, i)
        , base(b)
        , filter(f) {
        type = QUERY_SEARCH;
    }

    int run() anope_override;
};

class LDAPAdd : public LDAPRequest {
    Anope::string dn;
    LDAPMods attributes;

  public:
    LDAPAdd(LDAPService *s, LDAPInterface *i, const Anope::string &d,
            const LDAPMods &attr)
        : LDAPRequest(s, i)
        , dn(d)
        , attributes(attr) {
        type = QUERY_ADD;
    }

    int run() anope_override;
};

class LDAPDel : public LDAPRequest {
    Anope::string dn;

  public:
    LDAPDel(LDAPService *s, LDAPInterface *i, const Anope::string &d)
        : LDAPRequest(s, i)
        , dn(d) {
        type = QUERY_DELETE;
    }

    int run() anope_override;
};

class LDAPModify : public LDAPRequest {
    Anope::string base;
    LDAPMods attributes;

  public:
    LDAPModify(LDAPService *s, LDAPInterface *i, const Anope::string &b,
               const LDAPMods &attr)
        : LDAPRequest(s, i)
        , base(b)
        , attributes(attr) {
        type = QUERY_MODIFY;
    }

    int run() anope_override;
};

class LDAPService : public LDAPProvider, public Thread, public Condition {
    Anope::string server;
    Anope::string admin_binddn;
    Anope::string admin_pass;

    LDAP *con;

    time_t last_connect;

  public:
    static LDAPMod **BuildMods(const LDAPMods &attributes) {
        LDAPMod **mods = new LDAPMod*[attributes.size() + 1];
        memset(mods, 0, sizeof(LDAPMod*) * (attributes.size() + 1));
        for (unsigned x = 0; x < attributes.size(); ++x) {
            const LDAPModification &l = attributes[x];
            mods[x] = new LDAPMod();

            if (l.op == LDAPModification::LDAP_ADD) {
                mods[x]->mod_op = LDAP_MOD_ADD;
            } else if (l.op == LDAPModification::LDAP_DEL) {
                mods[x]->mod_op = LDAP_MOD_DELETE;
            } else if (l.op == LDAPModification::LDAP_REPLACE) {
                mods[x]->mod_op = LDAP_MOD_REPLACE;
            } else if (l.op != 0) {
                throw LDAPException("Unknown LDAP operation");
            }
            mods[x]->mod_type = strdup(l.name.c_str());
            mods[x]->mod_values = new char*[l.values.size() + 1];
            memset(mods[x]->mod_values, 0, sizeof(char *) * (l.values.size() + 1));
            for (unsigned j = 0, c = 0; j < l.values.size(); ++j)
                if (!l.values[j].empty()) {
                    mods[x]->mod_values[c++] = strdup(l.values[j].c_str());
                }
        }
        return mods;
    }

    static void FreeMods(LDAPMod **mods) {
        for (int i = 0; mods[i] != NULL; ++i) {
            free(mods[i]->mod_type);
            for (int j = 0; mods[i]->mod_values[j] != NULL; ++j) {
                free(mods[i]->mod_values[j]);
            }
            delete [] mods[i]->mod_values;
        }
        delete [] mods;
    }

  private:
    void Connect() {
        int i = ldap_initialize(&this->con, this->server.c_str());
        if (i != LDAP_SUCCESS) {
            throw LDAPException("Unable to connect to LDAP service " + this->name + ": " +
                                ldap_err2string(i));
        }

        const int version = LDAP_VERSION3;
        i = ldap_set_option(this->con, LDAP_OPT_PROTOCOL_VERSION, &version);
        if (i != LDAP_OPT_SUCCESS) {
            throw LDAPException("Unable to set protocol version for " + this->name + ": " +
                                ldap_err2string(i));
        }

        const struct timeval tv = { 0, 0 };
        i = ldap_set_option(this->con, LDAP_OPT_NETWORK_TIMEOUT, &tv);
        if (i != LDAP_OPT_SUCCESS) {
            throw LDAPException("Unable to set timeout for " + this->name + ": " +
                                ldap_err2string(i));
        }
    }

    void Reconnect() {
        /* Only try one connect a minute. It is an expensive blocking operation */
        if (last_connect > Anope::CurTime - 60) {
            throw LDAPException("Unable to connect to LDAP service " + this->name +
                                ": reconnecting too fast");
        }
        last_connect = Anope::CurTime;

        ldap_unbind_ext(this->con, NULL, NULL);

        Connect();
    }

    void QueueRequest(LDAPRequest *r) {
        this->Lock();
        this->queries.push_back(r);
        this->Wakeup();
        this->Unlock();
    }

  public:
    typedef std::vector<LDAPRequest *> query_queue;
    query_queue queries, results;
    Mutex process_mutex; /* held when processing requests not in either queue */

    LDAPService(Module *o, const Anope::string &n, const Anope::string &s,
                const Anope::string &b, const Anope::string &p) : LDAPProvider(o, n), server(s),
        admin_binddn(b), admin_pass(p), last_connect(0) {
        Connect();
    }

    ~LDAPService() {
        /* At this point the thread has stopped so we don't need to hold process_mutex */

        this->Lock();

        for (unsigned int i = 0; i < this->queries.size(); ++i) {
            LDAPRequest *req = this->queries[i];

            /* queries have no results yet */
            req->result = new LDAPResult();
            req->result->type = req->type;
            req->result->error = "LDAP Interface is going away";
            if (req->inter) {
                req->inter->OnError(*req->result);
            }

            delete req;
        }
        this->queries.clear();

        for (unsigned int i = 0; i < this->results.size(); ++i) {
            LDAPRequest *req = this->results[i];

            /* even though this may have already finished successfully we return that it didn't */
            req->result->error = "LDAP Interface is going away";
            if (req->inter) {
                req->inter->OnError(*req->result);
            }

            delete req;
        }

        this->Unlock();

        ldap_unbind_ext(this->con, NULL, NULL);
    }

    void BindAsAdmin(LDAPInterface *i) anope_override {
        this->Bind(i, this->admin_binddn, this->admin_pass);
    }

    void Bind(LDAPInterface *i, const Anope::string &who,
              const Anope::string &pass) anope_override {
        LDAPBind *b = new LDAPBind(this, i, who, pass);
        QueueRequest(b);
    }

    void Search(LDAPInterface *i, const Anope::string &base,
                const Anope::string &filter) anope_override {
        if (i == NULL) {
            throw LDAPException("No interface");
        }

        LDAPSearch *s = new LDAPSearch(this, i, base, filter);
        QueueRequest(s);
    }

    void Add(LDAPInterface *i, const Anope::string &dn,
             LDAPMods &attributes) anope_override {
        LDAPAdd *add = new LDAPAdd(this, i, dn, attributes);
        QueueRequest(add);
    }

    void Del(LDAPInterface *i, const Anope::string &dn) anope_override {
        LDAPDel *del = new LDAPDel(this, i, dn);
        QueueRequest(del);
    }

    void Modify(LDAPInterface *i, const Anope::string &base,
                LDAPMods &attributes) anope_override {
        LDAPModify *mod = new LDAPModify(this, i, base, attributes);
        QueueRequest(mod);
    }

  private:
    void BuildReply(int res, LDAPRequest *req) {
        LDAPResult *ldap_result = req->result = new LDAPResult();
        req->result->type = req->type;

        if (res != LDAP_SUCCESS) {
            ldap_result->error = ldap_err2string(res);
            return;
        }

        if (req->message == NULL) {
            return;
        }

        /* a search result */

        for (LDAPMessage *cur = ldap_first_message(this->con, req->message); cur;
                cur = ldap_next_message(this->con, cur)) {
            LDAPAttributes attributes;

            char *dn = ldap_get_dn(this->con, cur);
            if (dn != NULL) {
                attributes["dn"].push_back(dn);
                ldap_memfree(dn);
                dn = NULL;
            }

            BerElement *ber = NULL;

            for (char *attr = ldap_first_attribute(this->con, cur, &ber); attr;
                    attr = ldap_next_attribute(this->con, cur, ber)) {
                berval **vals = ldap_get_values_len(this->con, cur, attr);
                int count = ldap_count_values_len(vals);

                std::vector<Anope::string> attrs;
                for (int j = 0; j < count; ++j) {
                    attrs.push_back(vals[j]->bv_val);
                }
                attributes[attr] = attrs;

                ldap_value_free_len(vals);
                ldap_memfree(attr);
            }

            if (ber != NULL) {
                ber_free(ber, 0);
            }

            ldap_result->messages.push_back(attributes);
        }
    }

    void SendRequests() {
        process_mutex.Lock();

        query_queue q;
        this->Lock();
        queries.swap(q);
        this->Unlock();

        if (q.empty()) {
            process_mutex.Unlock();
            return;
        }

        for (unsigned int i = 0; i < q.size(); ++i) {
            LDAPRequest *req = q[i];
            int ret = req->run();

            if (ret == LDAP_SERVER_DOWN || ret == LDAP_TIMEOUT) {
                /* try again */
                try {
                    Reconnect();
                } catch (const LDAPException &) {
                }

                ret = req->run();
            }

            BuildReply(ret, req);

            this->Lock();
            results.push_back(req);
            this->Unlock();
        }

        me->Notify();

        process_mutex.Unlock();
    }

  public:
    void Run() anope_override {
        while (!this->GetExitState()) {
            this->Lock();
            /* Queries can be non empty if one is pushed during SendRequests() */
            if (queries.empty()) {
                this->Wait();
            }
            this->Unlock();

            SendRequests();
        }
    }

    LDAP* GetConnection() {
        return con;
    }
};

class ModuleLDAP : public Module, public Pipe {
    std::map<Anope::string, LDAPService *> LDAPServices;

  public:

    ModuleLDAP(const Anope::string &modname,
               const Anope::string &creator) : Module(modname, creator, EXTRA | VENDOR) {
        me = this;
    }

    ~ModuleLDAP() {
        for (std::map<Anope::string, LDAPService *>::iterator it =
                    this->LDAPServices.begin(); it != this->LDAPServices.end(); ++it) {
            it->second->SetExitState();
            it->second->Wakeup();
            it->second->Join();
            delete it->second;
        }
        LDAPServices.clear();
    }

    void OnReload(Configuration::Conf *config) anope_override {
        Configuration::Block *conf = config->GetModule(this);

        for (std::map<Anope::string, LDAPService *>::iterator it = this->LDAPServices.begin(); it != this->LDAPServices.end();) {
            const Anope::string &cname = it->first;
            LDAPService *s = it->second;
            int i;

            ++it;

            for (i = 0; i < conf->CountBlock("ldap"); ++i)
                if (conf->GetBlock("ldap", i)->Get<const Anope::string>("name",
                        "ldap/main") == cname) {
                    break;
                }

            if (i == conf->CountBlock("ldap")) {
                Log(LOG_NORMAL, "ldap") << "LDAP: Removing server connection " << cname;

                s->SetExitState();
                s->Wakeup();
                s->Join();
                delete s;
                this->LDAPServices.erase(cname);
            }
        }

        for (int i = 0; i < conf->CountBlock("ldap"); ++i) {
            Configuration::Block *ldap = conf->GetBlock("ldap", i);

            const Anope::string &connname = ldap->Get<const Anope::string>("name",
                                            "ldap/main");

            if (this->LDAPServices.find(connname) == this->LDAPServices.end()) {
                const Anope::string &server = ldap->Get<const Anope::string>("server",
                                              "127.0.0.1");
                const Anope::string &admin_binddn =
                    ldap->Get<const Anope::string>("admin_binddn");
                const Anope::string &admin_password =
                    ldap->Get<const Anope::string>("admin_password");

                try {
                    LDAPService *ss = new LDAPService(this, connname, server, admin_binddn,
                                                      admin_password);
                    ss->Start();
                    this->LDAPServices.insert(std::make_pair(connname, ss));

                    Log(LOG_NORMAL, "ldap") << "LDAP: Successfully initialized server " << connname
                                            << " (" << server << ")";
                } catch (const LDAPException &ex) {
                    Log(LOG_NORMAL, "ldap") << "LDAP: " << ex.GetReason();
                }
            }
        }
    }

    void OnModuleUnload(User *, Module *m) anope_override {
        for (std::map<Anope::string, LDAPService *>::iterator it = this->LDAPServices.begin(); it != this->LDAPServices.end(); ++it) {
            LDAPService *s = it->second;

            s->process_mutex.Lock();
            s->Lock();

            for (unsigned int i = s->queries.size(); i > 0; --i) {
                LDAPRequest *req = s->queries[i - 1];
                LDAPInterface *li = req->inter;

                if (li && li->owner == m) {
                    s->queries.erase(s->queries.begin() + i - 1);
                    delete req;
                }
            }
            for (unsigned int i = s->results.size(); i > 0; --i) {
                LDAPRequest *req = s->results[i - 1];
                LDAPInterface *li = req->inter;

                if (li && li->owner == m) {
                    s->results.erase(s->results.begin() + i - 1);
                    delete req;
                }
            }

            s->Unlock();
            s->process_mutex.Unlock();
        }
    }

    void OnNotify() anope_override {
        for (std::map<Anope::string, LDAPService *>::iterator it = this->LDAPServices.begin(); it != this->LDAPServices.end(); ++it) {
            LDAPService *s = it->second;

            LDAPService::query_queue results;
            s->Lock();
            results.swap(s->results);
            s->Unlock();

            for (unsigned int i = 0; i < results.size(); ++i) {
                LDAPRequest *req = results[i];
                LDAPInterface *li = req->inter;
                LDAPResult *r = req->result;

                if (li != NULL) {
                    if (!r->getError().empty()) {
                        Log(this) << "Error running LDAP query: " << r->getError();
                        li->OnError(*r);
                    } else {
                        li->OnResult(*r);
                    }
                }

                delete req;
            }
        }
    }
};

int LDAPBind::run() {
    berval cred;
    cred.bv_val = strdup(pass.c_str());
    cred.bv_len = pass.length();

    int i = ldap_sasl_bind_s(service->GetConnection(), who.c_str(),
                             LDAP_SASL_SIMPLE, &cred, NULL, NULL, NULL);

    free(cred.bv_val);

    return i;
}

int LDAPSearch::run() {
    return ldap_search_ext_s(service->GetConnection(), base.c_str(),
                             LDAP_SCOPE_SUBTREE, filter.c_str(), NULL, 0, NULL, NULL, &tv, 0, &message);
}

int LDAPAdd::run() {
    LDAPMod **mods = LDAPService::BuildMods(attributes);
    int i = ldap_add_ext_s(service->GetConnection(), dn.c_str(), mods, NULL, NULL);
    LDAPService::FreeMods(mods);
    return i;
}

int LDAPDel::run() {
    return ldap_delete_ext_s(service->GetConnection(), dn.c_str(), NULL, NULL);
}

int LDAPModify::run() {
    LDAPMod **mods = LDAPService::BuildMods(attributes);
    int i = ldap_modify_ext_s(service->GetConnection(), base.c_str(), mods, NULL,
                              NULL);
    LDAPService::FreeMods(mods);
    return i;
}

MODULE_INIT(ModuleLDAP)
