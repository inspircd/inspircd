/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2006-2007, 2009 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2006-2009 Craig Edwards <craigedwards@brainbox.cc>
 *   Copyright (C) 2008 Robin Burchell <robin+git@viroteck.net>
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


/* Stop mysql wanting to use long long */
#define NO_CLIENT_LONG_LONG

#include "inspircd.h"
#include <mysql.h>
#include "sql.h"

#ifdef _WIN32
# pragma comment(lib, "libmysql.lib")
#endif

/* VERSION 3 API: With nonblocking (threaded) requests */

/* $ModDesc: SQL Service Provider module for all other m_sql* modules */
/* $CompileFlags: exec("mysql_config --include") */
/* $LinkerFlags: exec("mysql_config --libs_r") rpath("mysql_config --libs_r") */

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
 * to insert futher pending requests into the queue.
 *
 * Once the processing of a request is complete, it is removed from the incoming queue to
 * an outgoing queue, and initialized as a 'response'. The worker thread then signals the
 * ircd thread (via a loopback socket) of the fact a result is available, by sending the
 * connection ID through the connection.
 *
 * The ircd thread then mutexes the queue once more, reads the outbound response off the head
 * of the queue, and sends it on its way to the original calling module.
 *
 * XXX: You might be asking "why doesnt he just send the response from within the worker thread?"
 * The answer to this is simple. The majority of InspIRCd, and in fact most ircd's are not
 * threadsafe. This module is designed to be threadsafe and is careful with its use of threads,
 * however, if we were to call a module's OnRequest even from within a thread which was not the
 * one the module was originally instantiated upon, there is a chance of all hell breaking loose
 * if a module is ever put in a re-enterant state (stack corruption could occur, crashes, data
 * corruption, and worse, so DONT think about it until the day comes when InspIRCd is 100%
 * gauranteed threadsafe!)
 */

class SQLConnection;
class MySQLresult;
class DispatcherThread;

struct QQueueItem
{
	SQLQuery* q;
	std::string query;
	SQLConnection* c;
	QQueueItem(SQLQuery* Q, const std::string& S, SQLConnection* C) : q(Q), query(S), c(C) {}
};

struct RQueueItem
{
	SQLQuery* q;
	MySQLresult* r;
	RQueueItem(SQLQuery* Q, MySQLresult* R) : q(Q), r(R) {}
};

typedef std::map<std::string, SQLConnection*> ConnMap;
typedef std::deque<QQueueItem> QueryQueue;
typedef std::deque<RQueueItem> ResultQueue;

/** MySQL module
 *  */
class ModuleSQL : public Module
{
 public:
	DispatcherThread* Dispatcher;
	QueryQueue qq;       // MUST HOLD MUTEX
	ResultQueue rq;      // MUST HOLD MUTEX
	ConnMap connections; // main thread only

	ModuleSQL();
	void init();
	~ModuleSQL();
	void OnRehash(User* user);
	void OnUnloadModule(Module* mod);
	Version GetVersion();
};

class DispatcherThread : public SocketThread
{
 private:
	ModuleSQL* const Parent;
 public:
	DispatcherThread(ModuleSQL* CreatorModule) : Parent(CreatorModule) { }
	~DispatcherThread() { }
	virtual void Run();
	virtual void OnNotify();
};

#if !defined(MYSQL_VERSION_ID) || MYSQL_VERSION_ID<32224
#define mysql_field_count mysql_num_fields
#endif

/** Represents a mysql result set
 */
class MySQLresult : public SQLResult
{
 public:
	SQLerror err;
	int currentrow;
	int rows;
	std::vector<std::string> colnames;
	std::vector<SQLEntries> fieldlists;

	MySQLresult(MYSQL_RES* res, int affected_rows) : err(SQL_NO_ERROR), currentrow(0), rows(0)
	{
		if (affected_rows >= 1)
		{
			rows = affected_rows;
			fieldlists.resize(rows);
		}
		unsigned int field_count = 0;
		if (res)
		{
			MYSQL_ROW row;
			int n = 0;
			while ((row = mysql_fetch_row(res)))
			{
				if (fieldlists.size() < (unsigned int)rows+1)
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
							fieldlists[n].push_back(SQLEntry(row[field_count]));
						else
							fieldlists[n].push_back(SQLEntry());
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

	MySQLresult(SQLerror& e) : err(e)
	{

	}

	~MySQLresult()
	{
	}

	virtual int Rows()
	{
		return rows;
	}

	virtual void GetCols(std::vector<std::string>& result)
	{
		result.assign(colnames.begin(), colnames.end());
	}

	virtual SQLEntry GetValue(int row, int column)
	{
		if ((row >= 0) && (row < rows) && (column >= 0) && (column < (int)fieldlists[row].size()))
		{
			return fieldlists[row][column];
		}
		return SQLEntry();
	}

	virtual bool GetRow(SQLEntries& result)
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
class SQLConnection : public SQLProvider
{
 public:
	reference<ConfigTag> config;
	MYSQL *connection;
	Mutex lock;

	// This constructor creates an SQLConnection object with the given credentials, but does not connect yet.
	SQLConnection(Module* p, ConfigTag* tag) : SQLProvider(p, "SQL/" + tag->getString("id")),
		config(tag), connection(NULL)
	{
	}

	~SQLConnection()
	{
		Close();
	}

	// This method connects to the database using the credentials supplied to the constructor, and returns
	// true upon success.
	bool Connect()
	{
		unsigned int timeout = 1;
		connection = mysql_init(connection);
		mysql_options(connection,MYSQL_OPT_CONNECT_TIMEOUT,(char*)&timeout);
		std::string host = config->getString("host");
		std::string user = config->getString("user");
		std::string pass = config->getString("pass");
		std::string dbname = config->getString("name");
		int port = config->getInt("port");
		bool rv = mysql_real_connect(connection, host.c_str(), user.c_str(), pass.c_str(), dbname.c_str(), port, NULL, 0);
		if (!rv)
			return rv;
		std::string initquery;
		if (config->readString("initialquery", initquery))
		{
			mysql_query(connection,initquery.c_str());
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
			/* Successfull query */
			MYSQL_RES* res = mysql_use_result(connection);
			unsigned long rows = mysql_affected_rows(connection);
			return new MySQLresult(res, rows);
		}
		else
		{
			/* XXX: See /usr/include/mysql/mysqld_error.h for a list of
			 * possible error numbers and error messages */
			SQLerror e(SQL_QREPLY_FAIL, ConvToStr(mysql_errno(connection)) + ": " + mysql_error(connection));
			return new MySQLresult(e);
		}
	}

	bool CheckConnection()
	{
		if (!connection || mysql_ping(connection) != 0)
			return Connect();
		return true;
	}

	std::string GetError()
	{
		return mysql_error(connection);
	}

	void Close()
	{
		mysql_close(connection);
	}

	void submit(SQLQuery* q, const std::string& qs)
	{
		Parent()->Dispatcher->LockQueue();
		Parent()->qq.push_back(QQueueItem(q, qs, this));
		Parent()->Dispatcher->UnlockQueueWakeup();
	}

	void submit(SQLQuery* call, const std::string& q, const ParamL& p)
	{
		std::string res;
		unsigned int param = 0;
		for(std::string::size_type i = 0; i < q.length(); i++)
		{
			if (q[i] != '?')
				res.push_back(q[i]);
			else
			{
				if (param < p.size())
				{
					std::string parm = p[param++];
					// In the worst case, each character may need to be encoded as using two bytes,
					// and one byte is the terminating null
					std::vector<char> buffer(parm.length() * 2 + 1);

					// The return value of mysql_escape_string() is the length of the encoded string,
					// not including the terminating null
					unsigned long escapedsize = mysql_escape_string(&buffer[0], parm.c_str(), parm.length());
//					mysql_real_escape_string(connection, queryend, paramscopy[paramnum].c_str(), paramscopy[paramnum].length());
					res.append(&buffer[0], escapedsize);
				}
			}
		}
		submit(call, res);
	}

	void submit(SQLQuery* call, const std::string& q, const ParamM& p)
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

				ParamM::const_iterator it = p.find(field);
				if (it != p.end())
				{
					std::string parm = it->second;
					// NOTE: See above
					std::vector<char> buffer(parm.length() * 2 + 1);
					unsigned long escapedsize = mysql_escape_string(&buffer[0], parm.c_str(), parm.length());
					res.append(&buffer[0], escapedsize);
				}
			}
		}
		submit(call, res);
	}
};

ModuleSQL::ModuleSQL()
{
	Dispatcher = NULL;
}

void ModuleSQL::init()
{
	if (mysql_library_init(0, NULL, NULL))
		throw ModuleException("Unable to initialise the MySQL library!");

	Dispatcher = new DispatcherThread(this);
	ServerInstance->Threads->Start(Dispatcher);

	Implementation eventlist[] = { I_OnRehash, I_OnUnloadModule };
	ServerInstance->Modules->Attach(eventlist, this, sizeof(eventlist)/sizeof(Implementation));

	OnRehash(NULL);
}

ModuleSQL::~ModuleSQL()
{
	if (Dispatcher)
	{
		Dispatcher->join();
		Dispatcher->OnNotify();
		delete Dispatcher;
	}

	for(ConnMap::iterator i = connections.begin(); i != connections.end(); i++)
	{
		delete i->second;
	}

	mysql_library_end();
}

void ModuleSQL::OnRehash(User* user)
{
	ConnMap conns;
	ConfigTagList tags = ServerInstance->Config->ConfTags("database");
	for(ConfigIter i = tags.first; i != tags.second; i++)
	{
		if (i->second->getString("module", "mysql") != "mysql")
			continue;
		std::string id = i->second->getString("id");
		ConnMap::iterator curr = connections.find(id);
		if (curr == connections.end())
		{
			SQLConnection* conn = new SQLConnection(this, i->second);
			conns.insert(std::make_pair(id, conn));
			ServerInstance->Modules->AddService(*conn);
		}
		else
		{
			conns.insert(*curr);
			connections.erase(curr);
		}
	}

	// now clean up the deleted databases
	Dispatcher->LockQueue();
	SQLerror err(SQL_BAD_DBID);
	for(ConnMap::iterator i = connections.begin(); i != connections.end(); i++)
	{
		ServerInstance->Modules->DelService(*i->second);
		// it might be running a query on this database. Wait for that to complete
		i->second->lock.Lock();
		i->second->lock.Unlock();
		// now remove all active queries to this DB
		for (size_t j = qq.size(); j > 0; j--)
		{
			size_t k = j - 1;
			if (qq[k].c == i->second)
			{
				qq[k].q->OnError(err);
				delete qq[k].q;
				qq.erase(qq.begin() + k);
			}
		}
		// finally, nuke the connection
		delete i->second;
	}
	Dispatcher->UnlockQueue();
	connections.swap(conns);
}

void ModuleSQL::OnUnloadModule(Module* mod)
{
	SQLerror err(SQL_BAD_DBID);
	Dispatcher->LockQueue();
	unsigned int i = qq.size();
	while (i > 0)
	{
		i--;
		if (qq[i].q->creator == mod)
		{
			if (i == 0)
			{
				// need to wait until the query is done
				// (the result will be discarded)
				qq[i].c->lock.Lock();
				qq[i].c->lock.Unlock();
			}
			qq[i].q->OnError(err);
			delete qq[i].q;
			qq.erase(qq.begin() + i);
		}
	}
	Dispatcher->UnlockQueue();
	// clean up any result queue entries
	Dispatcher->OnNotify();
}

Version ModuleSQL::GetVersion()
{
	return Version("MySQL support", VF_VENDOR);
}

void DispatcherThread::Run()
{
	this->LockQueue();
	while (!this->GetExitFlag())
	{
		if (!Parent->qq.empty())
		{
			QQueueItem i = Parent->qq.front();
			i.c->lock.Lock();
			this->UnlockQueue();
			MySQLresult* res = i.c->DoBlockingQuery(i.query);
			i.c->lock.Unlock();

			/*
			 * At this point, the main thread could be working on:
			 *  Rehash - delete i.c out from under us. We don't care about that.
			 *  UnloadModule - delete i.q and the qq item. Need to avoid reporting results.
			 */

			this->LockQueue();
			if (!Parent->qq.empty() && Parent->qq.front().q == i.q)
			{
				Parent->qq.pop_front();
				Parent->rq.push_back(RQueueItem(i.q, res));
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
	for(ResultQueue::iterator i = Parent->rq.begin(); i != Parent->rq.end(); i++)
	{
		MySQLresult* res = i->r;
		if (res->err.id == SQL_NO_ERROR)
			i->q->OnResult(*res);
		else
			i->q->OnError(res->err);
		delete i->q;
		delete i->r;
	}
	Parent->rq.clear();
	this->UnlockQueue();
}

MODULE_INIT(ModuleSQL)
