/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2024 Larry Williamson <l422y@l422y.com>
 *   Copyright (C) 2015 Daniel Vassdal <shutter@canternet.org>
 *   Copyright (C) 2013, 2017, 2019-2025 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012-2015 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2008 Thomas Stagner <aquanight@gmail.com>
 *   Copyright (C) 2007, 2009 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2007, 2009 Craig Edwards <brain@inspircd.org>
 *   Copyright (C) 2007 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2006 Oliver Lupton <om@inspircd.org>
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

/// $CompilerFlags: find_compiler_flags("libpq")
/// $LinkerFlags: find_linker_flags("libpq")

/// $PackageInfo: require_system("alpine") pcre2-dev pkgconf
/// $PackageInfo: require_system("arch") pkgconf postgresql-libs
/// $PackageInfo: require_system("darwin") libpq pkg-config
/// $PackageInfo: require_system("debian~") libpq-dev pkg-config
/// $PackageInfo: require_system("rhel~") pkg-config postgresql-devel


#include <sstream>

#include <libpq-fe.h>
#include <pg_config.h>

#include "inspircd.h"
#include "modules/sql.h"
#include "utility/string.h"

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

enum SQLstatus
{
	// The connection has died.
	DEAD,

	// Connecting and wants read event.
	CREAD,

	// Connecting and wants write event.
	CWRITE,

	// Connected/working and wants read event.
	WREAD,

	// Connected/working and wants write event.
	WWRITE
};

class ReconnectTimer final
	: public Timer
{
private:
	ModulePgSQL* mod;
public:
	ReconnectTimer(ModulePgSQL* m)
		: Timer(5, false)
		, mod(m)
	{
	}
	bool Tick() override;
};

struct QueueItem final
{
	SQL::Query* c;
	std::string q;
	QueueItem(SQL::Query* C, const std::string& Q)
		: c(C)
		, q(Q)
	{
	}
};

/** PgSQLresult is a subclass of the mostly-pure-virtual class SQLresult.
 * All SQL providers must create their own subclass and define it's methods using that
 * database library's data retrieval functions. The aim is to avoid a slow and inefficient process
 * of converting all data to a common format before it reaches the result structure. This way
 * data is passes to the module nearly as directly as if it was using the API directly itself.
 */

class PgSQLresult final
	: public SQL::Result
{
	PGresult* res;
	int currentrow = 0;
	int rows = 0;
	std::vector<std::string> colnames;

	void getColNames()
	{
		colnames.resize(PQnfields(res));
		for(unsigned int i=0; i < colnames.size(); i++)
		{
			colnames[i] = PQfname(res, i);
		}
	}
public:
	PgSQLresult(PGresult* result)
		: res(result)
	{
		rows = PQntuples(res);
		if (!rows)
			rows = ConvToNum<int>(PQcmdTuples(res));
	}

	~PgSQLresult() override
	{
		PQclear(res);
	}

	int Rows() override
	{
		return rows;
	}

	void GetCols(std::vector<std::string>& result) override
	{
		if (colnames.empty())
			getColNames();
		result = colnames;
	}

	bool HasColumn(const std::string& column, size_t& index) override
	{
		if (colnames.empty())
			getColNames();

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
		char* v = PQgetvalue(res, row, column);
		if (!v || PQgetisnull(res, row, column))
			return std::nullopt;

		return std::string(v, PQgetlength(res, row, column));
	}

	bool GetRow(SQL::Row& result) override
	{
		if (currentrow >= PQntuples(res))
			return false;
		int ncols = PQnfields(res);
		
		result.clear();
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
class SQLConn final
	: public SQL::Provider
	, public EventHandler
{
public:
	std::shared_ptr<ConfigTag> conf; /* The <database> entry */
	std::deque<QueueItem> queue;
	PGconn* sql = nullptr; /* PgSQL database connection handle */
	SQLstatus status = CWRITE; /* PgSQL database connection status */
	QueueItem qinprog; /* If there is currently a query in progress */

	SQLConn(Module* Creator, const std::shared_ptr<ConfigTag>& tag)
		: SQL::Provider(Creator, tag->getString("id"))
		, conf(tag)
		, qinprog(nullptr, "")
	{
		if (!DoConnect())
			DelayReconnect();
	}

	Cullable::Result Cull() override
	{
		this->SQL::Provider::Cull();
		ServerInstance->Modules.DelService(*this);
		return this->EventHandler::Cull();
	}

	~SQLConn() override
	{
		SQL::Error err(SQL::BAD_DBID);
		if (qinprog.c)
		{
			qinprog.c->OnError(err);
			delete qinprog.c;
		}
		for (const auto& item : queue)
		{
			SQL::Query* q = item.c;
			q->OnError(err);
			delete q;
		}
		Close();
	}

	void OnEventHandlerRead() override
	{
		DoEvent();
	}

	void OnEventHandlerWrite() override
	{
		DoEvent();
	}

	void OnEventHandlerError(int errornum) override
	{
		DelayReconnect();
	}

	std::string EscapeDSN(const std::string& str)
	{
		std::string out;
		out.reserve(str.size());

		for (std::string::const_iterator it = str.begin(); it != str.end(); ++it)
		{
			char chr = *it;
			switch (chr)
			{
				case '\\':
					out.append("\\\\");
					break;

				case '\'':
					out.append("\\'");
					break;

				default:
					out.push_back(chr);
					break;
			}
		}

		return out;
	}

	std::string GetDSN()
	{
		std::ostringstream conninfo("connect_timeout = '5'");
		std::string item;

		if (conf->readString("host", item))
			conninfo << " host = '" << EscapeDSN(item) << "'";

		if (conf->readString("port", item))
			conninfo << " port = '" << EscapeDSN(item) << "'";

		if (conf->readString("name", item))
			conninfo << " dbname = '" << EscapeDSN(item) << "'";

		if (conf->readString("user", item))
			conninfo << " user = '" << EscapeDSN(item) << "'";

		if (conf->readString("pass", item))
			conninfo << " password = '" << EscapeDSN(item) << "'";

		if (conf->getBool("tls", conf->getBool("ssl", true)))
			conninfo << " sslmode = 'require'";
		else
			conninfo << " sslmode = 'disable'";

		return conninfo.str();
	}

	bool HandleConnectError(const char* reason)
	{
		ServerInstance->Logs.Critical(MODNAME, "Could not connect to the \"{}\" database: {}",
			GetId(), reason);
		return false;
	}

	bool DoConnect()
	{
		sql = PQconnectStart(GetDSN().c_str());
		if (!sql)
			return HandleConnectError("PQconnectStart returned NULL");

		if(PQstatus(sql) == CONNECTION_BAD)
			return HandleConnectError("connection status is bad");

		if(PQsetnonblocking(sql, 1) == -1)
			return HandleConnectError("unable to mark fd as non-blocking");

		/* OK, we've initialised the connection, now to get it hooked into the socket engine
		* and then start polling it.
		*/
		SetFd(PQsocket(sql));
		if(!HasFd())
			return HandleConnectError("PQsocket returned an invalid fd");

		if (!SocketEngine::AddFd(this, FD_WANT_NO_WRITE | FD_WANT_NO_READ))
			return HandleConnectError("could not add the pgsql socket to the socket engine");

		/* Socket all hooked into the engine, now to tell PgSQL to start connecting */
		if (!DoPoll())
			return HandleConnectError("could not poll the connection state");

		return true;
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
				SocketEngine::ChangeEventMask(this, FD_WANT_NO_READ | FD_WANT_NO_WRITE);
				status = DEAD;
				return false;
			case PGRES_POLLING_OK:
				SocketEngine::ChangeEventMask(this, FD_WANT_POLL_READ | FD_WANT_NO_WRITE);
				status = WWRITE;
				DoConnectedPoll();
				return true;
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
						std::string errmsg = PQresultErrorMessage(result);
						for (size_t pos = 0; ((pos = errmsg.find_first_of("\r\n", pos)) != std::string::npos); )
							errmsg[pos] = ' ';
						SQL::Error err(SQL::QREPLY_FAIL, errmsg);
						qinprog.c->OnError(err);
						break;
					}
					default:
						/* Other values are not errors */
						qinprog.c->OnResult(reply);
				}

				delete qinprog.c;
				qinprog = QueueItem(nullptr, "");
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

	void DelayReconnect();

	void DoEvent()
	{
		if((status == CREAD) || (status == CWRITE))
		{
			DoPoll();
		}
		else if (status == WREAD || status == WWRITE)
		{
			DoConnectedPoll();
		}
	}

	void Submit(SQL::Query* req, const std::string& q) override
	{
		ServerInstance->Logs.Debug(MODNAME, "Executing PostgreSQL query: {}", q);
		if (qinprog.q.empty())
		{
			DoQuery(QueueItem(req, q));
		}
		else
		{
			// wait your turn.
			queue.emplace_back(req, q);
		}
	}

	void Submit(SQL::Query* req, const std::string& q, const SQL::ParamList& p) override
	{
		std::string res;
		unsigned int param = 0;
		for (const auto chr : q)
		{
			if (chr != '?')
				res.push_back(chr);
			else
			{
				if (param < p.size())
				{
					std::string parm = p[param++];
					std::vector<char> buffer(parm.length() * 2 + 1);
					int error;
					size_t escapedsize = PQescapeStringConn(sql, buffer.data(), parm.data(), parm.length(), &error);
					if (error)
						ServerInstance->Logs.Debug(MODNAME, "BUG: Apparently PQescapeStringConn() failed");
					res.append(buffer.data(), escapedsize);
				}
			}
		}
		Submit(req, res);
	}

	void Submit(SQL::Query* req, const std::string& q, const SQL::ParamMap& p) override
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
				if (it != p.end())
				{
					std::string parm = it->second;
					std::vector<char> buffer(parm.length() * 2 + 1);
					int error;
					size_t escapedsize = PQescapeStringConn(sql, buffer.data(), parm.data(), parm.length(), &error);
					if (error)
						ServerInstance->Logs.Debug(MODNAME, "BUG: Apparently PQescapeStringConn() failed");
					res.append(buffer.data(), escapedsize);
				}
			}
		}
		Submit(req, res);
	}

	void DoQuery(const QueueItem& req)
	{
		if (status != WREAD && status != WWRITE)
		{
			// whoops, not connected...
			SQL::Error err(SQL::BAD_CONN);
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
			SQL::Error err(SQL::QSEND_FAIL, PQerrorMessage(sql));
			req.c->OnError(err);
			delete req.c;
		}
	}

	void Close()
	{
		status = DEAD;

		if (HasFd() && SocketEngine::HasFd(GetFd()))
			SocketEngine::DelFd(this);

		if(sql)
		{
			PQfinish(sql);
			sql = nullptr;
		}
	}
};

class ModulePgSQL final
	: public Module
{
public:
	ConnMap connections;
	ReconnectTimer* retimer = nullptr;

	ModulePgSQL()
		: Module(VF_VENDOR, "Provides the ability for SQL modules to query a PostgreSQL database.")
	{
	}

	~ModulePgSQL() override
	{
		delete retimer;
		ClearAllConnections();
	}

	void init() override
	{
		int pqversion = PQlibVersion();
		int minor = pqversion / 100 % 100;
		int revision = pqversion % 100;
		if (pqversion >= 10'00'00)
		{
			// Ref: https://www.postgresql.org/docs/current/libpq-misc.html#LIBPQ-PQLIBVERSION
			minor = revision;
			revision = 0;
		}
		ServerInstance->Logs.Normal(MODNAME, "Module was compiled against libpq version {} and is running against version {}.{}.{}",
			PG_VERSION, pqversion / 10000, minor, revision);
	}

	void ReadConfig(ConfigStatus& status) override
	{
		ReadConf();
	}

	void ReadConf()
	{
		ConnMap conns;

		for (const auto& [_, tag] : ServerInstance->Config->ConfTags("database"))
		{
			if (!insp::equalsci(tag->getString("module"), "pgsql"))
				continue;

			std::string id = tag->getString("id");
			ConnMap::iterator curr = connections.find(id);
			if (curr == connections.end())
			{
				auto* conn = new SQLConn(this, tag);
				if (conn->status != DEAD)
				{
					conns.emplace(id, conn);
					ServerInstance->Modules.AddService(*conn);
				}
				// If the connection is dead it has already been queued for culling
				// at the end of the main loop so we don't need to delete it here.
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
		for (const auto& [_, conn] : connections)
		{
			conn->Cull();
			delete conn;
		}
		connections.clear();
	}

	void OnUnloadModule(Module* mod) override
	{
		SQL::Error err(SQL::BAD_DBID);
		for (const auto& [_, conn] : connections)
		{
			if (conn->qinprog.c && conn->qinprog.c->creator == mod)
			{
				conn->qinprog.c->OnError(err);
				delete conn->qinprog.c;
				conn->qinprog.c = nullptr;
			}
			std::deque<QueueItem>::iterator j = conn->queue.begin();
			while (j != conn->queue.end())
			{
				SQL::Query* q = j->c;
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
};

bool ReconnectTimer::Tick()
{
	mod->retimer = nullptr;
	mod->ReadConf();
	delete this;
	return false;
}

void SQLConn::DelayReconnect()
{
	status = DEAD;
	auto* mod = static_cast<ModulePgSQL*>(creator.ptr());

	ConnMap::iterator it = mod->connections.find(conf->getString("id"));
	if (it != mod->connections.end())
		mod->connections.erase(it);
	ServerInstance->GlobalCulls.AddItem((EventHandler*)this);
	if (!mod->retimer)
	{
		mod->retimer = new ReconnectTimer(mod);
		ServerInstance->Timers.AddTimer(mod->retimer);
	}
}

MODULE_INIT(ModulePgSQL)
