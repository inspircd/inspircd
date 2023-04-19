/*
 *
 * (C) 2010-2023 Anope Team
 * Contact us at team@anope.org
 *
 * Please read COPYING and README for further details.
 */

/* RequiredLibraries: mysqlclient */
/* RequiredWindowsLibraries: libmysql */

#include "module.h"
#include "modules/sql.h"
#define NO_CLIENT_LONG_LONG
#include <mysql.h>

using namespace SQL;

/** Non blocking threaded MySQL API, based loosely from InspIRCd's m_mysql.cpp
 *
 * This module spawns a single thread that is used to execute blocking MySQL queries.
 * When a module requests a query to be executed it is added to a list for the thread
 * (which never stops looping and sleeping) to pick up and execute, the result of which
 * is inserted in to another queue to be picked up by the main thread. The main thread
 * uses Pipe to become notified through the socket engine when there are results waiting
 * to be sent back to the modules requesting the query
 */

class MySQLService;

/** A query request
 */
struct QueryRequest {
    /* The connection to the database */
    MySQLService *service;
    /* The interface to use once we have the result to send the data back */
    Interface *sqlinterface;
    /* The actual query */
    Query query;

    QueryRequest(MySQLService *s, Interface *i, const Query &q) : service(s),
        sqlinterface(i), query(q) { }
};

/** A query result */
struct QueryResult {
    /* The interface to send the data back on */
    Interface *sqlinterface;
    /* The result */
    Result result;

    QueryResult(Interface *i, Result &r) : sqlinterface(i), result(r) { }
};

/** A MySQL result
 */
class MySQLResult : public Result {
    MYSQL_RES *res;

  public:
    MySQLResult(unsigned int i, const Query &q, const Anope::string &fq,
                MYSQL_RES *r) : Result(i, q, fq), res(r) {
        unsigned num_fields = res ? mysql_num_fields(res) : 0;

        /* It is not thread safe to log anything here using Log(this->owner) now :( */

        if (!num_fields) {
            return;
        }

        for (MYSQL_ROW row; (row = mysql_fetch_row(res));) {
            MYSQL_FIELD *fields = mysql_fetch_fields(res);

            if (fields) {
                std::map<Anope::string, Anope::string> items;

                for (unsigned field_count = 0; field_count < num_fields; ++field_count) {
                    Anope::string column = (fields[field_count].name ? fields[field_count].name :
                                            "");
                    Anope::string data = (row[field_count] ? row[field_count] : "");

                    items[column] = data;
                }

                this->entries.push_back(items);
            }
        }
    }

    MySQLResult(const Query &q, const Anope::string &fq,
                const Anope::string &err) : Result(0, q, fq, err), res(NULL) {
    }

    ~MySQLResult() {
        if (this->res) {
            mysql_free_result(this->res);
        }
    }
};

/** A MySQL connection, there can be multiple
 */
class MySQLService : public Provider {
    std::map<Anope::string, std::set<Anope::string> > active_schema;

    Anope::string database;
    Anope::string server;
    Anope::string user;
    Anope::string password;
    int port;

    MYSQL *sql;

    /** Escape a query.
     * Note the mutex must be held!
     */
    Anope::string Escape(const Anope::string &query);

  public:
    /* Locked by the SQL thread when a query is pending on this database,
     * prevents us from deleting a connection while a query is executing
     * in the thread
     */
    Mutex Lock;

    MySQLService(Module *o, const Anope::string &n, const Anope::string &d,
                 const Anope::string &s, const Anope::string &u, const Anope::string &p, int po);

    ~MySQLService();

    void Run(Interface *i, const Query &query) anope_override;

    Result RunQuery(const Query &query) anope_override;

    std::vector<Query> CreateTable(const Anope::string &table,
                                   const Data &data) anope_override;

    Query BuildInsert(const Anope::string &table, unsigned int id,
                      Data &data) anope_override;

    Query GetTables(const Anope::string &prefix) anope_override;

    void Connect();

    bool CheckConnection();

    Anope::string BuildQuery(const Query &q);

    Anope::string FromUnixtime(time_t);
};

/** The SQL thread used to execute queries
 */
class DispatcherThread : public Thread, public Condition {
  public:
    DispatcherThread() : Thread() { }

    void Run() anope_override;
};

class ModuleSQL;
static ModuleSQL *me;
class ModuleSQL : public Module, public Pipe {
    /* SQL connections */
    std::map<Anope::string, MySQLService *> MySQLServices;
  public:
    /* Pending query requests */
    std::deque<QueryRequest> QueryRequests;
    /* Pending finished requests with results */
    std::deque<QueryResult> FinishedRequests;
    /* The thread used to execute queries */
    DispatcherThread *DThread;

    ModuleSQL(const Anope::string &modname,
              const Anope::string &creator) : Module(modname, creator, EXTRA | VENDOR) {
        me = this;


        DThread = new DispatcherThread();
        DThread->Start();
    }

    ~ModuleSQL() {
        for (std::map<Anope::string, MySQLService *>::iterator it =
                    this->MySQLServices.begin(); it != this->MySQLServices.end(); ++it) {
            delete it->second;
        }
        MySQLServices.clear();

        DThread->SetExitState();
        DThread->Wakeup();
        DThread->Join();
        delete DThread;
    }

    void OnReload(Configuration::Conf *conf) anope_override {
        Configuration::Block *config = conf->GetModule(this);

        for (std::map<Anope::string, MySQLService *>::iterator it = this->MySQLServices.begin(); it != this->MySQLServices.end();) {
            const Anope::string &cname = it->first;
            MySQLService *s = it->second;
            int i;

            ++it;

            for (i = 0; i < config->CountBlock("mysql"); ++i)
                if (config->GetBlock("mysql", i)->Get<const Anope::string>("name",
                        "mysql/main") == cname) {
                    break;
                }

            if (i == config->CountBlock("mysql")) {
                Log(LOG_NORMAL, "mysql") << "MySQL: Removing server connection " << cname;

                delete s;
                this->MySQLServices.erase(cname);
            }
        }

        for (int i = 0; i < config->CountBlock("mysql"); ++i) {
            Configuration::Block *block = config->GetBlock("mysql", i);
            const Anope::string &connname = block->Get<const Anope::string>("name",
                                            "mysql/main");

            if (this->MySQLServices.find(connname) == this->MySQLServices.end()) {
                const Anope::string &database = block->Get<const Anope::string>("database",
                                                "anope");
                const Anope::string &server = block->Get<const Anope::string>("server",
                                              "127.0.0.1");
                const Anope::string &user = block->Get<const Anope::string>("username",
                                            "anope");
                const Anope::string &password = block->Get<const Anope::string>("password");
                int port = block->Get<int>("port", "3306");

                try {
                    MySQLService *ss = new MySQLService(this, connname, database, server, user,
                                                        password, port);
                    this->MySQLServices.insert(std::make_pair(connname, ss));

                    Log(LOG_NORMAL, "mysql") << "MySQL: Successfully connected to server " <<
                                             connname << " (" << server << ")";
                } catch (const SQL::Exception &ex) {
                    Log(LOG_NORMAL, "mysql") << "MySQL: " << ex.GetReason();
                }
            }
        }
    }

    void OnModuleUnload(User *, Module *m) anope_override {
        this->DThread->Lock();

        for (unsigned i = this->QueryRequests.size(); i > 0; --i) {
            QueryRequest &r = this->QueryRequests[i - 1];

            if (r.sqlinterface && r.sqlinterface->owner == m) {
                if (i == 1) {
                    r.service->Lock.Lock();
                    r.service->Lock.Unlock();
                }

                this->QueryRequests.erase(this->QueryRequests.begin() + i - 1);
            }
        }

        this->DThread->Unlock();

        this->OnNotify();
    }

    void OnNotify() anope_override {
        this->DThread->Lock();
        std::deque<QueryResult> finishedRequests = this->FinishedRequests;
        this->FinishedRequests.clear();
        this->DThread->Unlock();

        for (std::deque<QueryResult>::const_iterator it = finishedRequests.begin(), it_end = finishedRequests.end(); it != it_end; ++it) {
            const QueryResult &qr = *it;

            if (!qr.sqlinterface) {
                throw SQL::Exception("NULL qr.sqlinterface in MySQLPipe::OnNotify() ?");
            }

            if (qr.result.GetError().empty()) {
                qr.sqlinterface->OnResult(qr.result);
            } else {
                qr.sqlinterface->OnError(qr.result);
            }
        }
    }
};

MySQLService::MySQLService(Module *o, const Anope::string &n,
                           const Anope::string &d, const Anope::string &s, const Anope::string &u,
                           const Anope::string &p, int po)
    : Provider(o, n), database(d), server(s), user(u), password(p), port(po),
      sql(NULL) {
    Connect();
}

MySQLService::~MySQLService() {
    me->DThread->Lock();
    this->Lock.Lock();
    mysql_close(this->sql);
    this->sql = NULL;

    for (unsigned i = me->QueryRequests.size(); i > 0; --i) {
        QueryRequest &r = me->QueryRequests[i - 1];

        if (r.service == this) {
            if (r.sqlinterface) {
                r.sqlinterface->OnError(Result(0, r.query, "SQL Interface is going away"));
            }
            me->QueryRequests.erase(me->QueryRequests.begin() + i - 1);
        }
    }
    this->Lock.Unlock();
    me->DThread->Unlock();
}

void MySQLService::Run(Interface *i, const Query &query) {
    me->DThread->Lock();
    me->QueryRequests.push_back(QueryRequest(this, i, query));
    me->DThread->Unlock();
    me->DThread->Wakeup();
}

Result MySQLService::RunQuery(const Query &query) {
    this->Lock.Lock();

    Anope::string real_query = this->BuildQuery(query);

    if (this->CheckConnection()
            && !mysql_real_query(this->sql, real_query.c_str(), real_query.length())) {
        MYSQL_RES *res = mysql_store_result(this->sql);
        unsigned int id = mysql_insert_id(this->sql);

        /* because we enabled CLIENT_MULTI_RESULTS in our options
         * a multiple statement or a procedure call can return
         * multiple result sets.
         * we must process them all before the next query.
         */

        while (!mysql_next_result(this->sql)) {
            mysql_free_result(mysql_store_result(this->sql));
        }

        this->Lock.Unlock();
        return MySQLResult(id, query, real_query, res);
    } else {
        Anope::string error = mysql_error(this->sql);
        this->Lock.Unlock();
        return MySQLResult(query, real_query, error);
    }
}

std::vector<Query> MySQLService::CreateTable(const Anope::string &table,
        const Data &data) {
    std::vector<Query> queries;
    std::set<Anope::string> &known_cols = this->active_schema[table];

    if (known_cols.empty()) {
        Log(LOG_DEBUG) << "m_mysql: Fetching columns for " << table;

        Result columns = this->RunQuery("SHOW COLUMNS FROM `" + table + "`");
        for (int i = 0; i < columns.Rows(); ++i) {
            const Anope::string &column = columns.Get(i, "Field");

            Log(LOG_DEBUG) << "m_mysql: Column #" << i << " for " << table << ": " <<
                           column;
            known_cols.insert(column);
        }
    }

    if (known_cols.empty()) {
        Anope::string query_text = "CREATE TABLE `" + table +
                                   "` (`id` int(10) unsigned NOT NULL AUTO_INCREMENT,"
                                   " `timestamp` timestamp NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP";
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
        query_text += ", PRIMARY KEY (`id`), KEY `timestamp_idx` (`timestamp`))";
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

Query MySQLService::BuildInsert(const Anope::string &table, unsigned int id,
                                Data &data) {
    /* Empty columns not present in the data set */
    const std::set<Anope::string> &known_cols = this->active_schema[table];
    for (std::set<Anope::string>::iterator it = known_cols.begin(),
            it_end = known_cols.end(); it != it_end; ++it)
        if (*it != "id" && *it != "timestamp" && data.data.count(*it) == 0) {
            data[*it] << "";
        }

    Anope::string query_text = "INSERT INTO `" + table + "` (`id`";
    for (Data::Map::const_iterator it = data.data.begin(), it_end = data.data.end();
            it != it_end; ++it) {
        query_text += ",`" + it->first + "`";
    }
    query_text += ") VALUES (" + stringify(id);
    for (Data::Map::const_iterator it = data.data.begin(), it_end = data.data.end();
            it != it_end; ++it) {
        query_text += ",@" + it->first + "@";
    }
    query_text += ") ON DUPLICATE KEY UPDATE ";
    for (Data::Map::const_iterator it = data.data.begin(), it_end = data.data.end();
            it != it_end; ++it) {
        query_text += "`" + it->first + "`=VALUES(`" + it->first + "`),";
    }
    query_text.erase(query_text.end() - 1);

    Query query(query_text);
    for (Data::Map::const_iterator it = data.data.begin(), it_end = data.data.end();
            it != it_end; ++it) {
        Anope::string buf;
        *it->second >> buf;

        bool escape = true;
        if (buf.empty()) {
            buf = "NULL";
            escape = false;
        }

        query.SetValue(it->first, buf, escape);
    }

    return query;
}

Query MySQLService::GetTables(const Anope::string &prefix) {
    return Query("SHOW TABLES LIKE '" + prefix + "%';");
}

void MySQLService::Connect() {
    this->sql = mysql_init(this->sql);

    const unsigned int timeout = 1;
    mysql_options(this->sql, MYSQL_OPT_CONNECT_TIMEOUT,
                  reinterpret_cast<const char *>(&timeout));

    bool connect = mysql_real_connect(this->sql, this->server.c_str(),
                                      this->user.c_str(), this->password.c_str(), this->database.c_str(), this->port,
                                      NULL, CLIENT_MULTI_RESULTS);

    if (!connect) {
        throw SQL::Exception("Unable to connect to MySQL service " + this->name + ": " +
                             mysql_error(this->sql));
    }

    Log(LOG_DEBUG) << "Successfully connected to MySQL service " << this->name <<
                   " at " << this->server << ":" << this->port;
}


bool MySQLService::CheckConnection() {
    if (!this->sql || mysql_ping(this->sql)) {
        try {
            this->Connect();
        } catch (const SQL::Exception &) {
            return false;
        }
    }

    return true;
}

Anope::string MySQLService::Escape(const Anope::string &query) {
    std::vector<char> buffer(query.length() * 2 + 1);
    mysql_real_escape_string(this->sql, &buffer[0], query.c_str(), query.length());
    return &buffer[0];
}

Anope::string MySQLService::BuildQuery(const Query &q) {
    Anope::string real_query = q.query;

    for (std::map<Anope::string, QueryData>::const_iterator it =
                q.parameters.begin(), it_end = q.parameters.end(); it != it_end; ++it) {
        real_query = real_query.replace_all_cs("@" + it->first + "@",
                                               (it->second.escape ? ("'" + this->Escape(it->second.data) + "'") :
                                                it->second.data));
    }

    return real_query;
}

Anope::string MySQLService::FromUnixtime(time_t t) {
    return "FROM_UNIXTIME(" + stringify(t) + ")";
}

void DispatcherThread::Run() {
    this->Lock();

    while (!this->GetExitState()) {
        if (!me->QueryRequests.empty()) {
            QueryRequest &r = me->QueryRequests.front();
            this->Unlock();

            Result sresult = r.service->RunQuery(r.query);

            this->Lock();
            if (!me->QueryRequests.empty() && me->QueryRequests.front().query == r.query) {
                if (r.sqlinterface) {
                    me->FinishedRequests.push_back(QueryResult(r.sqlinterface, sresult));
                }
                me->QueryRequests.pop_front();
            }
        } else {
            if (!me->FinishedRequests.empty()) {
                me->Notify();
            }
            this->Wait();
        }
    }

    this->Unlock();
}

MODULE_INIT(ModuleSQL)
