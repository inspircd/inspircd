/*
 *
 * (C) 2003-2023 Anope Team
 * Contact us at team@anope.org
 *
 * Please read COPYING and README for further details.
 *
 * Based on the original code of Epona by Lara.
 * Based on the original code of Services by Andy Church.
 */

#include "module.h"
#include "modules/sql.h"

using namespace SQL;

class SQLSQLInterface : public Interface {
  public:
    SQLSQLInterface(Module *o) : Interface(o) { }

    void OnResult(const Result &r) anope_override {
        Log(LOG_DEBUG) << "SQL successfully executed query: " << r.finished_query;
    }

    void OnError(const Result &r) anope_override {
        if (!r.GetQuery().query.empty()) {
            Log(LOG_DEBUG) << "Error executing query " << r.finished_query << ": " <<
                           r.GetError();
        } else {
            Log(LOG_DEBUG) << "Error executing query: " << r.GetError();
        }
    }
};

class ResultSQLSQLInterface : public SQLSQLInterface {
    Reference<Serializable> obj;

  public:
    ResultSQLSQLInterface(Module *o, Serializable *ob) : SQLSQLInterface(o),
        obj(ob) { }

    void OnResult(const Result &r) anope_override {
        SQLSQLInterface::OnResult(r);
        if (r.GetID() > 0 && this->obj) {
            this->obj->id = r.GetID();
        }
        delete this;
    }

    void OnError(const Result &r) anope_override {
        SQLSQLInterface::OnError(r);
        delete this;
    }
};

class DBSQL : public Module, public Pipe {
    ServiceReference<Provider> sql;
    SQLSQLInterface sqlinterface;
    Anope::string prefix;
    bool import;

    std::set<Serializable *> updated_items;
    bool shutting_down;
    bool loading_databases;
    bool loaded;
    bool imported;

    void RunBackground(const Query &q, Interface *iface = NULL) {
        if (!this->sql) {
            static time_t last_warn = 0;
            if (last_warn + 300 < Anope::CurTime) {
                last_warn = Anope::CurTime;
                Log(this) << "db_sql: Unable to execute query, is SQL configured correctly?";
            }
        } else if (!Anope::Quitting) {
            if (iface == NULL) {
                iface = &this->sqlinterface;
            }
            this->sql->Run(iface, q);
        } else {
            this->sql->RunQuery(q);
        }
    }

  public:
    DBSQL(const Anope::string &modname,
          const Anope::string &creator) : Module(modname, creator, DATABASE | VENDOR),
        sql("", ""), sqlinterface(this), shutting_down(false), loading_databases(false),
        loaded(false), imported(false) {


        if (ModuleManager::FindModule("db_sql_live") != NULL) {
            throw ModuleException("db_sql can not be loaded after db_sql_live");
        }
    }

    void OnNotify() anope_override {
        for (std::set<Serializable *>::iterator it = this->updated_items.begin(), it_end = this->updated_items.end(); it != it_end; ++it) {
            Serializable *obj = *it;

            if (this->sql) {
                Data data;
                obj->Serialize(data);

                if (obj->IsCached(data)) {
                    continue;
                }

                obj->UpdateCache(data);

                /* If we didn't load these objects and we don't want to import just update the cache and continue */
                if (!this->loaded && !this->imported && !this->import) {
                    continue;
                }

                Serialize::Type *s_type = obj->GetSerializableType();
                if (!s_type) {
                    continue;
                }

                std::vector<Query> create = this->sql->CreateTable(this->prefix +
                                            s_type->GetName(), data);
                Query insert = this->sql->BuildInsert(this->prefix + s_type->GetName(), obj->id,
                                                      data);

                if (this->imported) {
                    for (unsigned i = 0; i < create.size(); ++i) {
                        this->RunBackground(create[i]);
                    }

                    this->RunBackground(insert, new ResultSQLSQLInterface(this, obj));
                } else {
                    for (unsigned i = 0; i < create.size(); ++i) {
                        this->sql->RunQuery(create[i]);
                    }

                    /* We are importing objects from another database module, so don't do asynchronous
                     * queries in case the core has to shut down, it will cut short the import
                     */
                    Result r = this->sql->RunQuery(insert);
                    if (r.GetID() > 0) {
                        obj->id = r.GetID();
                    }
                }
            }
        }

        this->updated_items.clear();
        this->imported = true;
    }

    void OnReload(Configuration::Conf *conf) anope_override {
        Configuration::Block *block = conf->GetModule(this);
        this->sql = ServiceReference<Provider>("SQL::Provider", block->Get<const Anope::string>("engine"));
        this->prefix = block->Get<const Anope::string>("prefix", "anope_db_");
        this->import = block->Get<bool>("import");
    }

    void OnShutdown() anope_override {
        this->shutting_down = true;
        this->OnNotify();
    }

    void OnRestart() anope_override {
        this->OnShutdown();
    }

    EventReturn OnLoadDatabase() anope_override {
        if (!this->sql) {
            Log(this) << "Unable to load databases, is SQL configured correctly?";
            return EVENT_CONTINUE;
        }

        this->loading_databases = true;

        const std::vector<Anope::string> type_order = Serialize::Type::GetTypeOrder();
        for (unsigned i = 0; i < type_order.size(); ++i) {
            Serialize::Type *sb = Serialize::Type::Find(type_order[i]);
            this->OnSerializeTypeCreate(sb);
        }

        this->loading_databases = false;
        this->loaded = true;

        return EVENT_STOP;
    }

    void OnSerializableConstruct(Serializable *obj) anope_override {
        if (this->shutting_down || this->loading_databases) {
            return;
        }
        obj->UpdateTS();
        this->updated_items.insert(obj);
        this->Notify();
    }

    void OnSerializableDestruct(Serializable *obj) anope_override {
        if (this->shutting_down) {
            return;
        }
        Serialize::Type *s_type = obj->GetSerializableType();
        if (s_type && obj->id > 0) {
            this->RunBackground("DELETE FROM `" + this->prefix + s_type->GetName() +
                                "` WHERE `id` = " + stringify(obj->id));
        }
        this->updated_items.erase(obj);
    }

    void OnSerializableUpdate(Serializable *obj) anope_override {
        if (this->shutting_down || obj->IsTSCached()) {
            return;
        }
        if (obj->id == 0) {
            return;    /* object is pending creation */
        }
        obj->UpdateTS();
        this->updated_items.insert(obj);
        this->Notify();
    }

    void OnSerializeTypeCreate(Serialize::Type *sb) anope_override {
        if (!this->loading_databases && !this->loaded) {
            return;
        }

        Query query("SELECT * FROM `" + this->prefix + sb->GetName() + "`");
        Result res = this->sql->RunQuery(query);

        for (int j = 0; j < res.Rows(); ++j) {
            Data data;

            const std::map<Anope::string, Anope::string> &row = res.Row(j);
            for (std::map<Anope::string, Anope::string>::const_iterator rit = row.begin(),
                    rit_end = row.end(); rit != rit_end; ++rit) {
                data[rit->first] << rit->second;
            }

            Serializable *obj = sb->Unserialize(NULL, data);
            try {
                if (obj) {
                    obj->id = convertTo<unsigned int>(res.Get(j, "id"));
                }
            } catch (const ConvertException &) {
                Log(this) << "Unable to convert id for object #" << j << " of type " <<
                          sb->GetName();
            }

            if (obj) {
                /* The Unserialize operation is destructive so rebuild the data for UpdateCache.
                 * Also the old data may contain columns that we don't use, so we reserialize the
                 * object to know for sure our cache is consistent
                 */

                Data data2;
                obj->Serialize(data2);
                obj->UpdateCache(data2); /* We know this is the most up to date copy */
            }
        }
    }
};

MODULE_INIT(DBSQL)
