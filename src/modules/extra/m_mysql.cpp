/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2019 linuxdaemon <linuxdaemon.irc@gmail.com>
 *   Copyright (C) 2015 Daniel Vassdal <shutter@canternet.org>
 *   Copyright (C) 2014, 2016 Adam <Adam@anope.org>
 *   Copyright (C) 2013-2014 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2013, 2016-2020 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2012 ChrisTX <xpipe@hotmail.de>
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2009 Uli Schlachter <psychon@inspircd.org>
 *   Copyright (C) 2007, 2009 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2005, 2008-2010 Craig Edwards <brain@inspircd.org>
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

/// $CompilerFlags: execute("mysql_config --include" "MYSQL_CXXFLAGS")
/// $LinkerFlags: execute("mysql_config --libs_r" "MYSQL_LDFLAGS" "-lmysqlclient")

/// $PackageInfo: require_system("arch") mariadb-libs
/// $PackageInfo: require_system("centos" "6.0" "6.99") mysql-devel
/// $PackageInfo: require_system("centos" "7.0") mariadb-devel
/// $PackageInfo: require_system("darwin") mysql-connector-c
/// $PackageInfo: require_system("debian") libmysqlclient-dev
/// $PackageInfo: require_system("ubuntu") libmysqlclient-dev

#ifdef __GNUC__
# pragma GCC diagnostic push
#endif

#include "inspircd.h"
#include <mysql.h>
#include "modules/sql.h"

#ifdef __GNUC__
# pragma GCC diagnostic pop
#endif

#ifdef _WIN32
# pragma comment(lib, "libmysql.lib")
#endif

/* VERSION 3 API: With nonblocking (threaded) requests */

/* THE NONBLOCKING MYSQL API!
 *
 * MySQL provides no nonblocking (asyncronous) API of its own, and its developers recommend
 * that instead, you should thread your program. This is what i've done here to allow for
 * asyncronous SQL requests via mysql. The way this works is as follows:
 *
 * The module spawns a thread via class Thread, and performs its mysql queries in this thread,
 * using a queue with priorities. There is a mutex on either end which prevents two threads
 * adjusting the queue at the same time, and crashing the ircd. Every 50 milliseconds, the
 * worker thread wakes up, and checks if there is a request at the head of its queue.
 * If there is, it processes this request, blocking the worker thread but leaving the ircd
 * thread to go about its business as usual. During this period, the ircd thread is able
 * to insert further pending requests into the queue.
 *
 * Once the processing of a request is complete, it is removed from the incoming queue to
 * an outgoing queue, and initialized as a 'response'. The worker thread then signals the
 * ircd thread (via a loopback socket) of the fact a result is available, by sending the
 * connection ID through the connection.
 *
 * The ircd thread then mutexes the queue once more, reads the outbound response off the head
 * of the queue, and sends it on its way to the original calling module.
 *
 * XXX: You might be asking "why doesnt it just send the response from within the worker thread?"
 * The answer to this is simple. The majority of InspIRCd, and in fact most ircd's are not
 * threadsafe. This module is designed to be threadsafe and is careful with its use of threads,
 * however, if we were to call a module's OnRequest even from within a thread which was not the
 * one the module was originally instantiated upon, there is a chance of all hell breaking loose
 * if a module is ever put in a re-enterant state (stack corruption could occur, crashes, data
 * corruption, and worse, so DONT think about it until the day comes when InspIRCd is 100%
 * guaranteed threadsafe!)
 */

class SQLConnection;
class MySQLresult;
class DispatcherThread;

struct QueryQueueItem
{
	// An SQL database which this query is executed on.
	SQLConnection* connection;

	// An object which handles the result of the query.
	SQL::Query* query;

	// The SQL query which is to be executed.
	std::string querystr;

	QueryQueueItem(SQL::Query* q, const std::string& s, SQLConnection* c)
		: connection(c)
		, query(q)
		, querystr(s)
	{
	}
};

struct ResultQueueItem
{
	// An object which handles the result of the query.
	SQL::Query* query;

	// The result returned from executing the MySQL query.
	MySQLresult* result;

	ResultQueueItem(SQL::Query* q, MySQLresult* r)
		: query(q)
		, result(r)
	{
	}
};

typedef insp::flat_map<std::string, SQLConnection*> ConnMap;
typedef std::deque<QueryQueueItem> QueryQueue;
typedef std::deque<ResultQueueItem> ResultQueue;

/** MySQL module
 *  */
class ModuleSQL : public Module
{
 public:
	DispatcherThread* Dispatcher = nullptr;
	QueryQueue qq;       // MUST HOLD MUTEX
	ResultQueue rq;      // MUST HOLD MUTEX
	ConnMap connections; // main thread only

	void init() override;
	ModuleSQL();
	~ModuleSQL() override;
	void ReadConfig(ConfigStatus& status) override;
	void OnUnloadModule(Module* mod) override;
};

class DispatcherThread : public SocketThread
{
 private:
	ModuleSQL* const Parent;
 public:
	DispatcherThread(ModuleSQL* CreatorModule) : Parent(CreatorModule) { }
	void OnStart() override;
	void OnNotify() override;
};

/** Represents a mysql result set
 */
class MySQLresult : public SQL::Result
{
 public:
	SQL::Error err;
	int currentrow = 0;
	int rows = 0;
	std::vector<std::string> colnames;
	std::vector<SQL::Row> fieldlists;

	MySQLresult(MYSQL_RES* res, unsigned long affected_rows)
		: err(SQL::SUCCESS)
	{
		if (affected_rows >= 1)
		{
			rows = int(affected_rows);
			fieldlists.resize(rows);
		}
		unsigned int field_count = 0;
		if (res)
		{
			MYSQL_ROW row;
			int n = 0;
			while ((row = mysql_fetch_row(res)))
			{
				if (fieldlists.size() < (size_t)rows+1)
				{
					fieldlists.resize(fieldlists.size()+1);
				}
				field_count = 0;
				MYSQL_FIELD *fields = mysql_fetch_fields(res);
				if(mysql_num_fields(res) == 0)
					break;
				if (fields && mysql_num_fields(res))
				{
					colnames.clear();
					while (field_count < mysql_num_fields(res))
					{
						std::string a = (fields[field_count].name ? fields[field_count].name : "");
						if (row[field_count])
							fieldlists[n].emplace_back(row[field_count]);
						else
							fieldlists[n].emplace_back();
						colnames.push_back(a);
						field_count++;
					}
					n++;
				}
				rows++;
			}
			mysql_free_result(res);
		}
	}

	MySQLresult(SQL::Error& e)
		: err(e)
	{

	}

	int Rows() override
	{
		return rows;
	}

	void GetCols(std::vector<std::string>& result) override
	{
		result.assign(colnames.begin(), colnames.end());
	}

	bool HasColumn(const std::string& column, size_t& index) override
	{
		for (size_t i = 0; i < colnames.size(); ++i)
		{
			if (colnames[i] == column)
			{
				index = i;
				return true;
			}
		}
		return false;
	}

	SQL::Field GetValue(int row, int column)
	{
		if ((row >= 0) && (row < rows) && (column >= 0) && (column < (int)fieldlists[row].size()))
		{
			return fieldlists[row][column];
		}
		return std::nullopt;
	}

	bool GetRow(SQL::Row& result) override
	{
		if (currentrow < rows)
		{
			result.assign(fieldlists[currentrow].begin(), fieldlists[currentrow].end());
			currentrow++;
			return true;
		}
		else
		{
			result.clear();
			return false;
		}
	}
};

/** Represents a connection to a mysql database
 */
class SQLConnection : public SQL::Provider
{
 private:
	bool EscapeString(SQL::Query* query, const std::string& in, std::string& out)
	{
		// In the worst case each character may need to be encoded as using two bytes and one
		// byte is the NUL terminator.
		std::vector<char> buffer(in.length() * 2 + 1);

		// The return value of mysql_escape_string() is either an error or the length of the
		// encoded string not including the NUL terminator.
		//
		// Unfortunately, someone genius decided that mysql_escape_string should return an
		// unsigned type even though -1 is returned on error so checking whether an error
		// happened is a bit cursed.
		unsigned long escapedsize = mysql_escape_string(&buffer[0], in.c_str(), in.length());
		if (escapedsize == static_cast<unsigned long>(-1))
		{
			SQL::Error err(SQL::QSEND_FAIL, InspIRCd::Format("%u: %s", mysql_errno(connection), mysql_error(connection)));
			query->OnError(err);
			return false;
		}

		out.append(&buffer[0], escapedsize);
		return true;
	}

 public:
	std::shared_ptr<ConfigTag> config;
	MYSQL* connection = nullptr;
	std::mutex lock;

	// This constructor creates an SQLConnection object with the given credentials, but does not connect yet.
	SQLConnection(Module* p, std::shared_ptr<ConfigTag> tag)
		: SQL::Provider(p, tag->getString("id"))
		, config(tag)
	{
	}

	~SQLConnection() override
	{
		mysql_close(connection);
	}

	// This method connects to the database using the credentials supplied to the constructor, and returns
	// true upon success.
	bool Connect()
	{
		connection = mysql_init(connection);

		// Set the connection timeout.
		unsigned int timeout = static_cast<unsigned int>(config->getDuration("timeout", 5, 1, 30));
		mysql_options(connection, MYSQL_OPT_CONNECT_TIMEOUT, &timeout);

		// Attempt to connect to the database.
		const std::string host = config->getString("host");
		const std::string user = config->getString("user");
		const std::string pass = config->getString("pass");
		const std::string dbname = config->getString("name");
		unsigned int port = static_cast<unsigned int>(config->getUInt("port", 3306, 1, 65535));
		if (!mysql_real_connect(connection, host.c_str(), user.c_str(), pass.c_str(), dbname.c_str(), port, NULL, CLIENT_IGNORE_SIGPIPE))
		{
			ServerInstance->Logs.Log(MODNAME, LOG_DEFAULT, "Unable to connect to the %s MySQL server: %s",
				GetId().c_str(), mysql_error(connection));
			return false;
		}

		// Set the default character set.
		const std::string charset = config->getString("charset");
		if (!charset.empty() && mysql_set_character_set(connection, charset.c_str()))
		{
			ServerInstance->Logs.Log(MODNAME, LOG_DEFAULT, "Could not set character set for %s to \"%s\": %s",
				GetId().c_str(), charset.c_str(), mysql_error(connection));
			return false;
		}

		// Execute the initial SQL query.
		const std::string initialquery = config->getString("initialquery");
		if (!initialquery.empty() && mysql_real_query(connection, initialquery.data(), initialquery.length()))
		{
			ServerInstance->Logs.Log(MODNAME, LOG_DEFAULT, "Could not execute initial query \"%s\" for %s: %s",
				initialquery.c_str(), name.c_str(), mysql_error(connection));
			return false;
		}

		return true;
	}

	ModuleSQL* Parent()
	{
		return (ModuleSQL*)(Module*)creator;
	}

	MySQLresult* DoBlockingQuery(const std::string& query)
	{

		/* Parse the command string and dispatch it to mysql */
		if (CheckConnection() && !mysql_real_query(connection, query.data(), query.length()))
		{
			/* Successful query */
			MYSQL_RES* res = mysql_use_result(connection);
			unsigned long rows = mysql_affected_rows(connection);
			return new MySQLresult(res, rows);
		}
		else
		{
			/* XXX: See /usr/include/mysql/mysqld_error.h for a list of
			 * possible error numbers and error messages */
			SQL::Error e(SQL::QREPLY_FAIL, InspIRCd::Format("%u: %s", mysql_errno(connection), mysql_error(connection)));
			return new MySQLresult(e);
		}
	}

	bool CheckConnection()
	{
		if (!connection || mysql_ping(connection) != 0)
			return Connect();
		return true;
	}

	void Submit(SQL::Query* q, const std::string& qs) override
	{
		ServerInstance->Logs.Log(MODNAME, LOG_DEBUG, "Executing MySQL query: " + qs);
		Parent()->Dispatcher->LockQueue();
		Parent()->qq.emplace_back(q, qs, this);
		Parent()->Dispatcher->UnlockQueueWakeup();
	}

	void Submit(SQL::Query* call, const std::string& q, const SQL::ParamList& p) override
	{
		std::string res;
		unsigned int param = 0;
		for(std::string::size_type i = 0; i < q.length(); i++)
		{
			if (q[i] != '?')
				res.push_back(q[i]);
			else if (param < p.size() && !EscapeString(call, p[param++], res))
				return;
		}
		Submit(call, res);
	}

	void Submit(SQL::Query* call, const std::string& q, const SQL::ParamMap& p) override
	{
		std::string res;
		for(std::string::size_type i = 0; i < q.length(); i++)
		{
			if (q[i] != '$')
				res.push_back(q[i]);
			else
			{
				std::string field;
				i++;
				while (i < q.length() && isalnum(q[i]))
					field.push_back(q[i++]);
				i--;

				SQL::ParamMap::const_iterator it = p.find(field);
				if (it != p.end() && !EscapeString(call, it->second, res))
					return;
			}
		}
		Submit(call, res);
	}
};

void ModuleSQL::init()
{
	if (mysql_library_init(0, NULL, NULL))
		throw ModuleException("Unable to initialise the MySQL library!");

	Dispatcher = new DispatcherThread(this);
	Dispatcher->Start();
}

ModuleSQL::ModuleSQL()
	: Module(VF_VENDOR, "Provides the ability for SQL modules to query a MySQL database.")
{
}

ModuleSQL::~ModuleSQL()
{
	if (Dispatcher)
	{
		Dispatcher->Stop();
		Dispatcher->OnNotify();
		delete Dispatcher;
	}

	for (const auto& [_, connection] : connections)
		delete connection;

	mysql_library_end();
}

void ModuleSQL::ReadConfig(ConfigStatus& status)
{
	ConnMap conns;

	for (const auto& [_, tag] : ServerInstance->Config->ConfTags("database"))
	{
		if (!stdalgo::string::equalsci(tag->getString("module"), "mysql"))
			continue;

		std::string id = tag->getString("id");
		ConnMap::iterator curr = connections.find(id);
		if (curr == connections.end())
		{
			SQLConnection* conn = new SQLConnection(this, tag);
			conns.emplace(id, conn);
			ServerInstance->Modules.AddService(*conn);
		}
		else
		{
			conns.insert(*curr);
			connections.erase(curr);
		}
	}

	// now clean up the deleted databases
	Dispatcher->LockQueue();
	SQL::Error err(SQL::BAD_DBID);

	for (const auto& [_, connection] : connections)
	{
		ServerInstance->Modules.DelService(*connection);
		// it might be running a query on this database. Wait for that to complete
		connection->lock.lock();
		connection->lock.unlock();
		// now remove all active queries to this DB
		for (size_t j = qq.size(); j > 0; j--)
		{
			size_t k = j - 1;
			if (qq[k].connection == connection)
			{
				qq[k].query->OnError(err);
				delete qq[k].query;
				qq.erase(qq.begin() + k);
			}
		}
		// finally, nuke the connection
		delete connection;
	}
	Dispatcher->UnlockQueue();
	connections.swap(conns);
}

void ModuleSQL::OnUnloadModule(Module* mod)
{
	SQL::Error err(SQL::BAD_DBID);
	Dispatcher->LockQueue();
	size_t i = qq.size();
	while (i > 0)
	{
		i--;
		if (qq[i].query->creator == mod)
		{
			if (i == 0)
			{
				// need to wait until the query is done
				// (the result will be discarded)
				qq[i].connection->lock.lock();
			}
			qq[i].query->OnError(err);
			delete qq[i].query;
			qq.erase(qq.begin() + i);
		}
	}
	Dispatcher->UnlockQueue();
	// clean up any result queue entries
	Dispatcher->OnNotify();
}

void DispatcherThread::OnStart()
{
	this->LockQueue();
	while (!this->IsStopping())
	{
		if (!Parent->qq.empty())
		{
			QueryQueueItem i = Parent->qq.front();
			i.connection->lock.lock();
			this->UnlockQueue();
			MySQLresult* res = i.connection->DoBlockingQuery(i.querystr);
			i.connection->lock.unlock();

			/*
			 * At this point, the main thread could be working on:
			 *  Rehash - delete i.connection out from under us. We don't care about that.
			 *  UnloadModule - delete i.query and the qq item. Need to avoid reporting results.
			 */

			this->LockQueue();
			if (!Parent->qq.empty() && Parent->qq.front().query == i.query)
			{
				Parent->qq.pop_front();
				Parent->rq.emplace_back(i.query, res);
				NotifyParent();
			}
			else
			{
				// UnloadModule ate the query
				delete res;
			}
		}
		else
		{
			/* We know the queue is empty, we can safely hang this thread until
			 * something happens
			 */
			this->WaitForQueue();
		}
	}
	this->UnlockQueue();
}

void DispatcherThread::OnNotify()
{
	// this could unlock during the dispatch, but OnResult isn't expected to take that long
	this->LockQueue();
	for (const auto& item : Parent->rq)
	{
		MySQLresult* res = item.result;
		if (res->err.code == SQL::SUCCESS)
			item.query->OnResult(*res);
		else
			item.query->OnError(res->err);
		delete item.query;
		delete item.result;
	}
	Parent->rq.clear();
	this->UnlockQueue();
}

MODULE_INIT(ModuleSQL)
