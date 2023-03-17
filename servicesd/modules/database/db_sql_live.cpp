/*
 *
 * (C) 2012-2023 Anope Team
 * Contact us at team@anope.org
 *
 * Please read COPYING and README for further details.
 */

#include "module.h"
#include "modules/sql.h"

using namespace SQL;

class DBMySQL : public Module, public Pipe {
  private:
    Anope::string prefix;
    ServiceReference<Provider> SQL;
    time_t lastwarn;
    bool ro;
    bool init;
    std::set<Serializable *> updated_items;

    bool CheckSQL() {
        if (SQL) {
            if (Anope::ReadOnly && this->ro) {
                Anope::ReadOnly = this->ro = false;
                Log() << "Found SQL again, going out of readonly mode...";
            }

            return true;
        } else {
            if (Anope::CurTime - Config->GetBlock("options")->Get<time_t>("updatetimeout",
                    "5m") > lastwarn) {
                Log() << "Unable to locate SQL reference, going to readonly...";
                Anope::ReadOnly = this->ro = true;
                this->lastwarn = Anope::CurTime;
            }

            return false;
        }
    }

    bool CheckInit() {
        return init && SQL;
    }

    void RunQuery(const Query &query) {
        /* Can this be threaded? */
        this->RunQueryResult(query);
    }

    Result RunQueryResult(const Query &query) {
        if (this->CheckSQL()) {
            Result res = SQL->RunQuery(query);
            if (!res.GetError().empty()) {
                Log(LOG_DEBUG) << "SQL-live got error " << res.GetError() << " for " +
                               res.finished_query;
            } else {
                Log(LOG_DEBUG) << "SQL-live got " << res.Rows() << " rows for " <<
                               res.finished_query;
            }
            return res;
        }
        throw SQL::Exception("No SQL!");
    }

  public:
    DBMySQL(const Anope::string &modname,
            const Anope::string &creator) : Module(modname, creator, DATABASE | VENDOR),
        SQL("", "") {
        this->lastwarn = 0;
        this->ro = false;
        this->init = false;


        if (ModuleManager::FindFirstOf(DATABASE) != this) {
            throw ModuleException("If db_sql_live is loaded it must be the first database module loaded.");
        }
    }

    void OnNotify() anope_override {
        if (!this->CheckInit()) {
            return;
        }

        for (std::set<Serializable *>::iterator it = this->updated_items.begin(), it_end = this->updated_items.end(); it != it_end; ++it) {
            Serializable *obj = *it;

            if (obj && this->SQL) {
                Data data;
                obj->Serialize(data);

                if (obj->IsCached(data)) {
                    continue;
                }

                obj->UpdateCache(data);

                Serialize::Type *s_type = obj->GetSerializableType();
                if (!s_type) {
                    continue;
                }

                std::vector<Query> create = this->SQL->CreateTable(this->prefix +
                                            s_type->GetName(), data);
                for (unsigned i = 0; i < create.size(); ++i) {
                    this->RunQueryResult(create[i]);
                }

                Result res = this->RunQueryResult(this->SQL->BuildInsert(
                                                      this->prefix + s_type->GetName(), obj->id, data));
                if (res.GetID() && obj->id != res.GetID()) {
                    /* In this case obj is new, so place it into the object map */
                    obj->id = res.GetID();
                    s_type->objects[obj->id] = obj;
                }
            }
        }

        this->updated_items.clear();
    }

    EventReturn OnLoadDatabase() anope_override {
        init = true;
        return EVENT_STOP;
    }

    void OnShutdown() anope_override {
        init = false;
    }

    void OnRestart() anope_override {
        init = false;
    }

    void OnReload(Configuration::Conf *conf) anope_override {
        Configuration::Block *block = conf->GetModule(this);
        this->SQL = ServiceReference<Provider>("SQL::Provider", block->Get<const Anope::string>("engine"));
        this->prefix = block->Get<const Anope::string>("prefix", "anope_db_");
    }

    void OnSerializableConstruct(Serializable *obj) anope_override {
        if (!this->CheckInit()) {
            return;
        }
        obj->UpdateTS();
        this->updated_items.insert(obj);
        this->Notify();
    }

    void OnSerializableDestruct(Serializable *obj) anope_override {
        if (!this->CheckInit()) {
            return;
        }
        Serialize::Type *s_type = obj->GetSerializableType();
        if (s_type) {
            if (obj->id > 0) {
                this->RunQuery("DELETE FROM `" + this->prefix + s_type->GetName() +
                               "` WHERE `id` = " + stringify(obj->id));
            }
            s_type->objects.erase(obj->id);
        }
        this->updated_items.erase(obj);
    }

    void OnSerializeCheck(Serialize::Type *obj) anope_override {
        if (!this->CheckInit() || obj->GetTimestamp() == Anope::CurTime) {
            return;
        }

        Query query("SELECT * FROM `" + this->prefix + obj->GetName() + "` WHERE (`timestamp` >= " + this->SQL->FromUnixtime(obj->GetTimestamp()) + " OR `timestamp` IS NULL)");

        obj->UpdateTimestamp();

        Result res = this->RunQueryResult(query);

        bool clear_null = false;
        for (int i = 0; i < res.Rows(); ++i) {
            const std::map<Anope::string, Anope::string> &row = res.Row(i);

            unsigned int id;
            try {
                id = convertTo<unsigned int>(res.Get(i, "id"));
            } catch (const ConvertException &) {
                Log(LOG_DEBUG) << "Unable to convert id from " << obj->GetName();
                continue;
            }

            if (res.Get(i, "timestamp").empty()) {
                clear_null = true;
                std::map<uint64_t, Serializable *>::iterator it = obj->objects.find(id);
                if (it != obj->objects.end()) {
                    delete it->second;    // This also removes this object from the map
                }
            } else {
                Data data;

                for (std::map<Anope::string, Anope::string>::const_iterator it = row.begin(),
                        it_end = row.end(); it != it_end; ++it) {
                    data[it->first] << it->second;
                }

                Serializable *s = NULL;
                std::map<uint64_t, Serializable *>::iterator it = obj->objects.find(id);
                if (it != obj->objects.end()) {
                    s = it->second;
                }

                Serializable *new_s = obj->Unserialize(s, data);
                if (new_s) {
                    // If s == new_s then s->id == new_s->id
                    if (s != new_s) {
                        new_s->id = id;
                        obj->objects[id] = new_s;

                        /* The Unserialize operation is destructive so rebuild the data for UpdateCache.
                         * Also the old data may contain columns that we don't use, so we reserialize the
                         * object to know for sure our cache is consistent
                         */

                        Data data2;
                        new_s->Serialize(data2);
                        new_s->UpdateCache(data2); /* We know this is the most up to date copy */
                    }
                } else {
                    if (!s) {
                        this->RunQuery("UPDATE `" + prefix + obj->GetName() + "` SET `timestamp` = " +
                                       this->SQL->FromUnixtime(obj->GetTimestamp()) + " WHERE `id` = " + stringify(
                                           id));
                    } else {
                        delete s;
                    }
                }
            }
        }

        if (clear_null) {
            query = "DELETE FROM `" + this->prefix + obj->GetName() +
                    "` WHERE `timestamp` IS NULL";
            this->RunQuery(query);
        }
    }

    void OnSerializableUpdate(Serializable *obj) anope_override {
        if (!this->CheckInit() || obj->IsTSCached()) {
            return;
        }
        obj->UpdateTS();
        this->updated_items.insert(obj);
        this->Notify();
    }
};

MODULE_INIT(DBMySQL)
