/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2006-2007, 2009 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2006-2007, 2009 Craig Edwards <craigedwards@brainbox.cc>
 *   Copyright (C) 2008 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2008 Thomas Stagner <aquanight@inspircd.org>
 *   Copyright (C) 2006 Oliver Lupton <oliverlupton@gmail.com>
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

/// $CompilerFlags: -Iexecute("pg_config --includedir" "POSTGRESQL_INCLUDE_DIR")
/// $LinkerFlags: -Lexecute("pg_config --libdir" "POSTGRESQL_LIBRARY_DIR") -lpq

/// $PackageInfo: require_system("centos") postgresql-devel
/// $PackageInfo: require_system("darwin") postgresql
/// $PackageInfo: require_system("ubuntu") libpq-dev


#include "inspircd.h"
#include <cstdlib>
#include <libpq-fe.h>
#include "modules/sql.h"

/* SQLConn rewritten by peavey to
 * use EventHandler instead of
 * BufferedSocket. This is much neater
 * and gives total control of destroy
 * and delete of resources.
 */

/* Forward declare, so we can have the typedef neatly at the top */
class SQLConn;
class ModulePgSQL;

typedef insp::flat_map<std::string, SQLConn*> ConnMap;

/* CREAD,	Connecting and wants read event
 * CWRITE,	Connecting and wants write event
 * WREAD,	Connected/Working and wants read event
 * WWRITE,	Connected/Working and wants write event
 * RREAD,	Resetting and wants read event
 * RWRITE,	Resetting and wants write event
 */
enum SQLstatus { CREAD, CWRITE, WREAD, WWRITE, RREAD, RWRITE };

class ReconnectTimer : public Timer
{
 private:
	ModulePgSQL* mod;
 public:
	ReconnectTimer(ModulePgSQL* m) : Timer(5, false), mod(m)
	{
	}
	bool Tick(time_t TIME);
};

struct QueueItem
{
	SQLQuery* c;
	std::string q;
	QueueItem(SQLQuery* C, const std::string& Q) : c(C), q(Q) {}
};

/** PgSQLresult is a subclass of the mostly-pure-virtual class SQLresult.
 * All SQL providers must create their own subclass and define it's methods using that
 * database library's data retriveal functions. The aim is to avoid a slow and inefficient process
 * of converting all data to a common format before it reaches the result structure. This way
 * data is passes to the module nearly as directly as if it was using the API directly itself.
 */

class PgSQLresult : public SQLResult
{
	PGresult* res;
	int currentrow;
	int rows;
 public:
	PgSQLresult(PGresult* result) : res(result), currentrow(0)
	{
		rows = PQntuples(res);
		if (!rows)
			rows = atoi(PQcmdTuples(res));
	}

	~PgSQLresult()
	{
		PQclear(res);
	}

	int Rows()
	{
		return rows;
	}

	void GetCols(std::vector<std::string>& result)
	{
		result.resize(PQnfields(res));
		for(unsigned int i=0; i < result.size(); i++)
		{
			result[i] = PQfname(res, i);
		}
	}

	SQLEntry GetValue(int row, int column)
	{
		char* v = PQgetvalue(res, row, column);
		if (!v || PQgetisnull(res, row, column))
			return SQLEntry();

		return SQLEntry(std::string(v, PQgetlength(res, row, column)));
	}

	bool GetRow(SQLEntries& result)
	{
		if (currentrow >= PQntuples(res))
			return false;
		int ncols = PQnfields(res);

		for(int i = 0; i < ncols; i++)
		{
			result.push_back(GetValue(currentrow, i));
		}
		currentrow++;

		return true;
	}
};

/** SQLConn represents one SQL session.
 */
class SQLConn : public SQLProvider, public EventHandler
{
 public:
	reference<ConfigTag> conf;	/* The <database> entry */
	std::deque<QueueItem> queue;
	PGconn* 		sql;		/* PgSQL database connection handle */
	SQLstatus		status;		/* PgSQL database connection status */
	QueueItem		qinprog;	/* If there is currently a query in progress */

	SQLConn(Module* Creator, ConfigTag* tag)
	: SQLProvider(Creator, "SQL/" + tag->getString("id")), conf(tag), sql(NULL), status(CWRITE), qinprog(NULL, "")
	{
		if (!DoConnect())
		{
			ServerInstance->Logs->Log(MODNAME, LOG_DEFAULT, "WARNING: Could not connect to database " + tag->getString("id"));
			DelayReconnect();
		}
	}

	CullResult cull() CXX11_OVERRIDE
	{
		this->SQLProvider::cull();
		ServerInstance->Modules->DelService(*this);
		return this->EventHandler::cull();
	}

	~SQLConn()
	{
		SQLerror err(SQL_BAD_DBID);
		if (qinprog.c)
		{
			qinprog.c->OnError(err);
			delete qinprog.c;
		}
		for(std::deque<QueueItem>::iterator i = queue.begin(); i != queue.end(); i++)
		{
			SQLQuery* q = i->c;
			q->OnError(err);
			delete q;
		}
	}

	void OnEventHandlerRead() CXX11_OVERRIDE
	{
		DoEvent();
	}

	void OnEventHandlerWrite() CXX11_OVERRIDE
	{
		DoEvent();
	}

	void OnEventHandlerError(int errornum) CXX11_OVERRIDE
	{
		DelayReconnect();
	}

	std::string GetDSN()
	{
		std::ostringstream conninfo("connect_timeout = '5'");
		std::string item;

		if (conf->readString("host", item))
			conninfo << " host = '" << item << "'";

		if (conf->readString("port", item))
			conninfo << " port = '" << item << "'";

		if (conf->readString("name", item))
			conninfo << " dbname = '" << item << "'";

		if (conf->readString("user", item))
			conninfo << " user = '" << item << "'";

		if (conf->readString("pass", item))
			conninfo << " password = '" << item << "'";

		if (conf->getBool("ssl"))
			conninfo << " sslmode = 'require'";
		else
			conninfo << " sslmode = 'disable'";

		return conninfo.str();
	}

	bool DoConnect()
	{
		sql = PQconnectStart(GetDSN().c_str());
		if (!sql)
			return false;

		if(PQstatus(sql) == CONNECTION_BAD)
			return false;

		if(PQsetnonblocking(sql, 1) == -1)
			return false;

		/* OK, we've initalised the connection, now to get it hooked into the socket engine
		* and then start polling it.
		*/
		this->fd = PQsocket(sql);

		if(this->fd <= -1)
			return false;

		if (!SocketEngine::AddFd(this, FD_WANT_NO_WRITE | FD_WANT_NO_READ))
		{
			ServerInstance->Logs->Log(MODNAME, LOG_DEBUG, "BUG: Couldn't add pgsql socket to socket engine");
			return false;
		}

		/* Socket all hooked into the engine, now to tell PgSQL to start connecting */
		return DoPoll();
	}

	bool DoPoll()
	{
		switch(PQconnectPoll(sql))
		{
			case PGRES_POLLING_WRITING:
				SocketEngine::ChangeEventMask(this, FD_WANT_POLL_WRITE | FD_WANT_NO_READ);
				status = CWRITE;
				return true;
			case PGRES_POLLING_READING:
				SocketEngine::ChangeEventMask(this, FD_WANT_POLL_READ | FD_WANT_NO_WRITE);
				status = CREAD;
				return true;
			case PGRES_POLLING_FAILED:
				return false;
			case PGRES_POLLING_OK:
				SocketEngine::ChangeEventMask(this, FD_WANT_POLL_READ | FD_WANT_NO_WRITE);
				status = WWRITE;
				DoConnectedPoll();
			default:
				return true;
		}
	}

	void DoConnectedPoll()
	{
restart:
		while (qinprog.q.empty() && !queue.empty())
		{
			/* There's no query currently in progress, and there's queries in the queue. */
			DoQuery(queue.front());
			queue.pop_front();
		}

		if (PQconsumeInput(sql))
		{
			if (PQisBusy(sql))
			{
				/* Nothing happens here */
			}
			else if (qinprog.c)
			{
				/* Fetch the result.. */
				PGresult* result = PQgetResult(sql);

				/* PgSQL would allow a query string to be sent which has multiple
				 * queries in it, this isn't portable across database backends and
				 * we don't want modules doing it. But just in case we make sure we
				 * drain any results there are and just use the last one.
				 * If the module devs are behaving there will only be one result.
				 */
				while (PGresult* temp = PQgetResult(sql))
				{
					PQclear(result);
					result = temp;
				}

				/* ..and the result */
				PgSQLresult reply(result);
				switch(PQresultStatus(result))
				{
					case PGRES_EMPTY_QUERY:
					case PGRES_BAD_RESPONSE:
					case PGRES_FATAL_ERROR:
					{
						SQLerror err(SQL_QREPLY_FAIL, PQresultErrorMessage(result));
						qinprog.c->OnError(err);
						break;
					}
					default:
						/* Other values are not errors */
						qinprog.c->OnResult(reply);
				}

				delete qinprog.c;
				qinprog = QueueItem(NULL, "");
				goto restart;
			}
			else
			{
				qinprog.q.clear();
			}
		}
		else
		{
			/* I think we'll assume this means the server died...it might not,
			 * but I think that any error serious enough we actually get here
			 * deserves to reconnect [/excuse]
			 * Returning true so the core doesn't try and close the connection.
			 */
			DelayReconnect();
		}
	}

	bool DoResetPoll()
	{
		switch(PQresetPoll(sql))
		{
			case PGRES_POLLING_WRITING:
				SocketEngine::ChangeEventMask(this, FD_WANT_POLL_WRITE | FD_WANT_NO_READ);
				status = CWRITE;
				return DoPoll();
			case PGRES_POLLING_READING:
				SocketEngine::ChangeEventMask(this, FD_WANT_POLL_READ | FD_WANT_NO_WRITE);
				status = CREAD;
				return true;
			case PGRES_POLLING_FAILED:
				return false;
			case PGRES_POLLING_OK:
				SocketEngine::ChangeEventMask(this, FD_WANT_POLL_READ | FD_WANT_NO_WRITE);
				status = WWRITE;
				DoConnectedPoll();
			default:
				return true;
		}
	}

	void DelayReconnect();

	void DoEvent()
	{
		if((status == CREAD) || (status == CWRITE))
		{
			DoPoll();
		}
		else if((status == RREAD) || (status == RWRITE))
		{
			DoResetPoll();
		}
		else
		{
			DoConnectedPoll();
		}
	}

	void submit(SQLQuery *req, const std::string& q) CXX11_OVERRIDE
	{
		if (qinprog.q.empty())
		{
			DoQuery(QueueItem(req,q));
		}
		else
		{
			// wait your turn.
			queue.push_back(QueueItem(req,q));
		}
	}

	void submit(SQLQuery *req, const std::string& q, const ParamL& p) CXX11_OVERRIDE
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
					std::vector<char> buffer(parm.length() * 2 + 1);
					int error;
					size_t escapedsize = PQescapeStringConn(sql, &buffer[0], parm.data(), parm.length(), &error);
					if (error)
						ServerInstance->Logs->Log(MODNAME, LOG_DEBUG, "BUG: Apparently PQescapeStringConn() failed");
					res.append(&buffer[0], escapedsize);
				}
			}
		}
		submit(req, res);
	}

	void submit(SQLQuery *req, const std::string& q, const ParamM& p) CXX11_OVERRIDE
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
					std::vector<char> buffer(parm.length() * 2 + 1);
					int error;
					size_t escapedsize = PQescapeStringConn(sql, &buffer[0], parm.data(), parm.length(), &error);
					if (error)
						ServerInstance->Logs->Log(MODNAME, LOG_DEBUG, "BUG: Apparently PQescapeStringConn() failed");
					res.append(&buffer[0], escapedsize);
				}
			}
		}
		submit(req, res);
	}

	void DoQuery(const QueueItem& req)
	{
		if (status != WREAD && status != WWRITE)
		{
			// whoops, not connected...
			SQLerror err(SQL_BAD_CONN);
			req.c->OnError(err);
			delete req.c;
			return;
		}

		if(PQsendQuery(sql, req.q.c_str()))
		{
			qinprog = req;
		}
		else
		{
			SQLerror err(SQL_QSEND_FAIL, PQerrorMessage(sql));
			req.c->OnError(err);
			delete req.c;
		}
	}

	void Close()
	{
		SocketEngine::DelFd(this);

		if(sql)
		{
			PQfinish(sql);
			sql = NULL;
		}
	}
};

class ModulePgSQL : public Module
{
 public:
	ConnMap connections;
	ReconnectTimer* retimer;

	ModulePgSQL()
		: retimer(NULL)
	{
	}

	~ModulePgSQL()
	{
		delete retimer;
		ClearAllConnections();
	}

	void ReadConfig(ConfigStatus& status) CXX11_OVERRIDE
	{
		ReadConf();
	}

	void ReadConf()
	{
		ConnMap conns;
		ConfigTagList tags = ServerInstance->Config->ConfTags("database");
		for(ConfigIter i = tags.first; i != tags.second; i++)
		{
			if (i->second->getString("module", "pgsql") != "pgsql")
				continue;
			std::string id = i->second->getString("id");
			ConnMap::iterator curr = connections.find(id);
			if (curr == connections.end())
			{
				SQLConn* conn = new SQLConn(this, i->second);
				conns.insert(std::make_pair(id, conn));
				ServerInstance->Modules->AddService(*conn);
			}
			else
			{
				conns.insert(*curr);
				connections.erase(curr);
			}
		}
		ClearAllConnections();
		conns.swap(connections);
	}

	void ClearAllConnections()
	{
		for(ConnMap::iterator i = connections.begin(); i != connections.end(); i++)
		{
			i->second->cull();
			delete i->second;
		}
		connections.clear();
	}

	void OnUnloadModule(Module* mod) CXX11_OVERRIDE
	{
		SQLerror err(SQL_BAD_DBID);
		for(ConnMap::iterator i = connections.begin(); i != connections.end(); i++)
		{
			SQLConn* conn = i->second;
			if (conn->qinprog.c && conn->qinprog.c->creator == mod)
			{
				conn->qinprog.c->OnError(err);
				delete conn->qinprog.c;
				conn->qinprog.c = NULL;
			}
			std::deque<QueueItem>::iterator j = conn->queue.begin();
			while (j != conn->queue.end())
			{
				SQLQuery* q = j->c;
				if (q->creator == mod)
				{
					q->OnError(err);
					delete q;
					j = conn->queue.erase(j);
				}
				else
					j++;
			}
		}
	}

	Version GetVersion() CXX11_OVERRIDE
	{
		return Version("PostgreSQL Service Provider module for all other m_sql* modules, uses v2 of the SQL API", VF_VENDOR);
	}
};

bool ReconnectTimer::Tick(time_t time)
{
	mod->retimer = NULL;
	mod->ReadConf();
	delete this;
	return false;
}

void SQLConn::DelayReconnect()
{
	ModulePgSQL* mod = (ModulePgSQL*)(Module*)creator;
	ConnMap::iterator it = mod->connections.find(conf->getString("id"));
	if (it != mod->connections.end())
	{
		mod->connections.erase(it);
		ServerInstance->GlobalCulls.AddItem((EventHandler*)this);
		if (!mod->retimer)
		{
			mod->retimer = new ReconnectTimer(mod);
			ServerInstance->Timers.AddTimer(mod->retimer);
		}
	}
}

MODULE_INIT(ModulePgSQL)
