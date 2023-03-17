/*
 *
 * (C) 2011-2023 Anope Team
 * Contact us at team@anope.org
 *
 * Please read COPYING and README for further details.
 */

/* RequiredLibraries: sqlite3 */
/* RequiredWindowsLibraries: sqlite3 */

#include "module.h"
#include "modules/sql.h"
#include <sqlite3.h>

using namespace SQL;

/* SQLite3 API, based from InspIRCd */

/** A SQLite result
 */
class SQLiteResult : public Result {
  public:
    SQLiteResult(unsigned int i, const Query &q,
                 const Anope::string &fq) : Result(i, q, fq) {
    }

    SQLiteResult(const Query &q, const Anope::string &fq,
                 const Anope::string &err) : Result(0, q, fq, err) {
    }

    void AddRow(const std::map<Anope::string, Anope::string> &data) {
        this->entries.push_back(data);
    }
};

/** A SQLite database, there can be multiple
 */
class SQLiteService : public Provider {
    std::map<Anope::string, std::set<Anope::string> > active_schema;

    Anope::string database;

    sqlite3 *sql;

    Anope::string Escape(const Anope::string &query);

  public:
    SQLiteService(Module *o, const Anope::string &n, const Anope::string &d);

    ~SQLiteService();

    void Run(Interface *i, const Query &query) anope_override;

    Result RunQuery(const Query &query);

    std::vector<Query> CreateTable(const Anope::string &table,
                                   const Data &data) anope_override;

    Query BuildInsert(const Anope::string &table, unsigned int id, Data &data);

    Query GetTables(const Anope::string &prefix);

    Anope::string BuildQuery(const Query &q);

    Anope::string FromUnixtime(time_t);
};

class ModuleSQLite : public Module {
    /* SQL connections */
    std::map<Anope::string, SQLiteService *> SQLiteServices;
  public:
    ModuleSQLite(const Anope::string &modname,
                 const Anope::string &creator) : Module(modname, creator, EXTRA | VENDOR) {
    }

    ~ModuleSQLite() {
        for (std::map<Anope::string, SQLiteService *>::iterator it =
                    this->SQLiteServices.begin(); it != this->SQLiteServices.end(); ++it) {
            delete it->second;
        }
        SQLiteServices.clear();
    }

    void OnReload(Configuration::Conf *conf) anope_override {
        Configuration::Block *config = conf->GetModule(this);

        for (std::map<Anope::string, SQLiteService *>::iterator it = this->SQLiteServices.begin(); it != this->SQLiteServices.end();) {
            const Anope::string &cname = it->first;
            SQLiteService *s = it->second;
            int i, num;
            ++it;

            for (i = 0, num = config->CountBlock("sqlite"); i < num; ++i)
                if (config->GetBlock("sqlite", i)->Get<const Anope::string>("name",
                        "sqlite/main") == cname) {
                    break;
                }

            if (i == num) {
                Log(LOG_NORMAL, "sqlite") << "SQLite: Removing server connection " << cname;

                delete s;
                this->SQLiteServices.erase(cname);
            }
        }

        for (int i = 0; i < config->CountBlock("sqlite"); ++i) {
            Configuration::Block *block = config->GetBlock("sqlite", i);
            Anope::string connname = block->Get<const Anope::string>("name", "sqlite/main");

            if (this->SQLiteServices.find(connname) == this->SQLiteServices.end()) {
                Anope::string database = Anope::DataDir + "/" +
                                         block->Get<const Anope::string>("database", "anope");

                try {
                    SQLiteService *ss = new SQLiteService(this, connname, database);
                    this->SQLiteServices[connname] = ss;

                    Log(LOG_NORMAL, "sqlite") << "SQLite: Successfully added database " << database;
                } catch (const SQL::Exception &ex) {
                    Log(LOG_NORMAL, "sqlite") << "SQLite: " << ex.GetReason();
                }
            }
        }
    }
};

SQLiteService::SQLiteService(Module *o, const Anope::string &n,
                             const Anope::string &d)
    : Provider(o, n), database(d), sql(NULL) {
    int db = sqlite3_open_v2(database.c_str(), &this->sql,
                             SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, 0);
    if (db != SQLITE_OK) {
        Anope::string exstr = "Unable to open SQLite database " + database;
        if (this->sql) {
            exstr += ": ";
            exstr += sqlite3_errmsg(this->sql);
            sqlite3_close(this->sql);
        }
        throw SQL::Exception(exstr);
    }
}

SQLiteService::~SQLiteService() {
    sqlite3_interrupt(this->sql);
    sqlite3_close(this->sql);
}

void SQLiteService::Run(Interface *i, const Query &query) {
    Result res = this->RunQuery(query);
    if (!res.GetError().empty()) {
        i->OnError(res);
    } else {
        i->OnResult(res);
    }
}

Result SQLiteService::RunQuery(const Query &query) {
    Anope::string real_query = this->BuildQuery(query);
    sqlite3_stmt *stmt;
    int err = sqlite3_prepare_v2(this->sql, real_query.c_str(), real_query.length(),
                                 &stmt, NULL);
    if (err != SQLITE_OK) {
        return SQLiteResult(query, real_query, sqlite3_errmsg(this->sql));
    }

    std::vector<Anope::string> columns;
    int cols = sqlite3_column_count(stmt);
    columns.resize(cols);
    for (int i = 0; i < cols; ++i) {
        columns[i] = sqlite3_column_name(stmt, i);
    }

    SQLiteResult result(0, query, real_query);

    while ((err = sqlite3_step(stmt)) == SQLITE_ROW) {
        std::map<Anope::string, Anope::string> items;
        for (int i = 0; i < cols; ++i) {
            const char *data = reinterpret_cast<const char *>(sqlite3_column_text(stmt, i));
            if (data && *data) {
                items[columns[i]] = data;
            }
        }
        result.AddRow(items);
    }

    result.id = sqlite3_last_insert_rowid(this->sql);

    sqlite3_finalize(stmt);

    if (err != SQLITE_DONE) {
        return SQLiteResult(query, real_query, sqlite3_errmsg(this->sql));
    }

    return result;
}

std::vector<Query> SQLiteService::CreateTable(const Anope::string &table,
        const Data &data) {
    std::vector<Query> queries;
    std::set<Anope::string> &known_cols = this->active_schema[table];

    if (known_cols.empty()) {
        Log(LOG_DEBUG) << "m_sqlite: Fetching columns for " << table;

        Result columns = this->RunQuery("PRAGMA table_info(" + table + ")");
        for (int i = 0; i < columns.Rows(); ++i) {
            const Anope::string &column = columns.Get(i, "name");

            Log(LOG_DEBUG) << "m_sqlite: Column #" << i << " for " << table << ": " <<
                           column;
            known_cols.insert(column);
        }
    }

    if (known_cols.empty()) {
        Anope::string query_text = "CREATE TABLE `" + table +
                                   "` (`id` INTEGER PRIMARY KEY, `timestamp` timestamp DEFAULT CURRENT_TIMESTAMP";

        for (Data::Map::const_iterator it = data.data.begin(), it_end = data.data.end();
                it != it_end; ++it) {
            known_cols.insert(it->first);

            query_text += ", `" + it->first + "` ";
            if (data.GetType(it->first) == Serialize::Data::DT_INT) {
                query_text += "int(11)";
            } else {
                query_text += "text";
            }
        }

        query_text += ")";

        queries.push_back(query_text);

        query_text = "CREATE UNIQUE INDEX `" + table + "_id_idx` ON `" + table +
                     "` (`id`)";
        queries.push_back(query_text);

        query_text = "CREATE INDEX `" + table + "_timestamp_idx` ON `" + table +
                     "` (`timestamp`)";
        queries.push_back(query_text);

        query_text = "CREATE TRIGGER `" + table + "_trigger` AFTER UPDATE ON `" + table
                     + "` FOR EACH ROW BEGIN UPDATE `" + table +
                     "` SET `timestamp` = CURRENT_TIMESTAMP WHERE `id` = `old.id`; end;";
        queries.push_back(query_text);
    } else
        for (Data::Map::const_iterator it = data.data.begin(), it_end = data.data.end();
                it != it_end; ++it) {
            if (known_cols.count(it->first) > 0) {
                continue;
            }

            known_cols.insert(it->first);

            Anope::string query_text = "ALTER TABLE `" + table + "` ADD `" + it->first +
                                       "` ";
            if (data.GetType(it->first) == Serialize::Data::DT_INT) {
                query_text += "int(11)";
            } else {
                query_text += "text";
            }

            queries.push_back(query_text);
        }

    return queries;
}

Query SQLiteService::BuildInsert(const Anope::string &table, unsigned int id,
                                 Data &data) {
    /* Empty columns not present in the data set */
    const std::set<Anope::string> &known_cols = this->active_schema[table];
    for (std::set<Anope::string>::iterator it = known_cols.begin(),
            it_end = known_cols.end(); it != it_end; ++it)
        if (*it != "id" && *it != "timestamp" && data.data.count(*it) == 0) {
            data[*it] << "";
        }

    Anope::string query_text = "REPLACE INTO `" + table + "` (";
    if (id > 0) {
        query_text += "`id`,";
    }
    for (Data::Map::const_iterator it = data.data.begin(), it_end = data.data.end();
            it != it_end; ++it) {
        query_text += "`" + it->first + "`,";
    }
    query_text.erase(query_text.length() - 1);
    query_text += ") VALUES (";
    if (id > 0) {
        query_text += stringify(id) + ",";
    }
    for (Data::Map::const_iterator it = data.data.begin(), it_end = data.data.end();
            it != it_end; ++it) {
        query_text += "@" + it->first + "@,";
    }
    query_text.erase(query_text.length() - 1);
    query_text += ")";

    Query query(query_text);
    for (Data::Map::const_iterator it = data.data.begin(), it_end = data.data.end();
            it != it_end; ++it) {
        Anope::string buf;
        *it->second >> buf;
        query.SetValue(it->first, buf);
    }

    return query;
}

Query SQLiteService::GetTables(const Anope::string &prefix) {
    return Query("SELECT name FROM sqlite_master WHERE type='table' AND name LIKE '"
                 + prefix + "%';");
}

Anope::string SQLiteService::Escape(const Anope::string &query) {
    char *e = sqlite3_mprintf("%q", query.c_str());
    Anope::string buffer = e;
    sqlite3_free(e);
    return buffer;
}

Anope::string SQLiteService::BuildQuery(const Query &q) {
    Anope::string real_query = q.query;

    for (std::map<Anope::string, QueryData>::const_iterator it =
                q.parameters.begin(), it_end = q.parameters.end(); it != it_end; ++it) {
        real_query = real_query.replace_all_cs("@" + it->first + "@",
                                               (it->second.escape ? ("'" + this->Escape(it->second.data) + "'") :
                                                it->second.data));
    }

    return real_query;
}

Anope::string SQLiteService::FromUnixtime(time_t t) {
    return "datetime('" + stringify(t) + "', 'unixepoch')";
}

MODULE_INIT(ModuleSQLite)
