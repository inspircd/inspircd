/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2007 InspIRCd Development Team
 * See: http://www.inspircd.org/wiki/index.php/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"
#include <cstdlib>
#include <sstream>
#include <libpq-fe.h>
#include "users.h"
#include "channels.h"
#include "modules.h"
#include "configreader.h"
#include "m_sqlv2.h"

/* $ModDesc: PostgreSQL Service Provider module for all other m_sql* modules, uses v2 of the SQL API */
/* $CompileFlags: -Iexec("pg_config --includedir") eval("my $s = `pg_config --version`;$s =~ /^.*?(\d+)\.(\d+)\.(\d+).*?$/;my $v = hex(sprintf("0x%02x%02x%02x", $1, $2, $3));print "-DPGSQL_HAS_ESCAPECONN" if(($v >= 0x080104) || ($v >= 0x07030F && $v < 0x070400) || ($v >= 0x07040D && $v < 0x080000) || ($v >= 0x080008 && $v < 0x080100));") */
/* $LinkerFlags: -Lexec("pg_config --libdir") -lpq */
/* $ModDep: m_sqlv2.h */


/* SQLConn rewritten by peavey to
 * use EventHandler instead of
 * InspSocket. This is much neater
 * and gives total control of destroy
 * and delete of resources.
 */

/* Forward declare, so we can have the typedef neatly at the top */
class SQLConn;

typedef std::map<std::string, SQLConn*> ConnMap;

/* CREAD,	Connecting and wants read event
 * CWRITE,	Connecting and wants write event
 * WREAD,	Connected/Working and wants read event
 * WWRITE,	Connected/Working and wants write event
 * RREAD,	Resetting and wants read event
 * RWRITE,	Resetting and wants write event
 */
enum SQLstatus { CREAD, CWRITE, WREAD, WWRITE, RREAD, RWRITE };

/** SQLhost::GetDSN() - Overload to return correct DSN for PostgreSQL
 */
std::string SQLhost::GetDSN()
{
	std::ostringstream conninfo("connect_timeout = '2'");

	if (ip.length())
		conninfo << " hostaddr = '" << ip << "'";

	if (port)
		conninfo << " port = '" << port << "'";

	if (name.length())
		conninfo << " dbname = '" << name << "'";

	if (user.length())
		conninfo << " user = '" << user << "'";

	if (pass.length())
		conninfo << " password = '" << pass << "'";

	if (ssl)
	{
		conninfo << " sslmode = 'require'";
	}
	else
	{
		conninfo << " sslmode = 'disable'";
	}

	return conninfo.str();
}

class ReconnectTimer : public InspTimer
{
  private:
	Module* mod;
  public:
	ReconnectTimer(InspIRCd* SI, Module* m)
	: InspTimer(5, SI->Time(), false), mod(m)
	{
	}
	virtual void Tick(time_t TIME);
};


/** Used to resolve sql server hostnames
 */
class SQLresolver : public Resolver
{
 private:
	SQLhost host;
	Module* mod;
 public:
	SQLresolver(Module* m, InspIRCd* Instance, const SQLhost& hi, bool &cached)
	: Resolver(Instance, hi.host, DNS_QUERY_FORWARD, cached, (Module*)m), host(hi), mod(m)
	{
	}

	virtual void OnLookupComplete(const std::string &result, unsigned int ttl, bool cached);

	virtual void OnError(ResolverError e, const std::string &errormessage)
	{
		ServerInstance->Log(DEBUG, "PgSQL: DNS lookup failed (%s), dying horribly", errormessage.c_str());
	}
};

/** PgSQLresult is a subclass of the mostly-pure-virtual class SQLresult.
 * All SQL providers must create their own subclass and define it's methods using that
 * database library's data retriveal functions. The aim is to avoid a slow and inefficient process
 * of converting all data to a common format before it reaches the result structure. This way
 * data is passes to the module nearly as directly as if it was using the API directly itself.
 */

class PgSQLresult : public SQLresult
{
	PGresult* res;
	int currentrow;
	int rows;
	int cols;

	SQLfieldList* fieldlist;
	SQLfieldMap* fieldmap;
public:
	PgSQLresult(Module* self, Module* to, unsigned long id, PGresult* result)
	: SQLresult(self, to, id), res(result), currentrow(0), fieldlist(NULL), fieldmap(NULL)
	{
		rows = PQntuples(res);
		cols = PQnfields(res);
	}

	~PgSQLresult()
	{
		/* If we allocated these, free them... */
		if(fieldlist)
			DELETE(fieldlist);

		if(fieldmap)
			DELETE(fieldmap);

		PQclear(res);
	}

	virtual int Rows()
	{
		if(!cols && !rows)
		{
			return atoi(PQcmdTuples(res));
		}
		else
		{
			return rows;
		}
	}

	virtual int Cols()
	{
		return PQnfields(res);
	}

	virtual std::string ColName(int column)
	{
		char* name = PQfname(res, column);

		return (name) ? name : "";
	}

	virtual int ColNum(const std::string &column)
	{
		int n = PQfnumber(res, column.c_str());

		if(n == -1)
		{
			throw SQLbadColName();
		}
		else
		{
			return n;
		}
	}

	virtual SQLfield GetValue(int row, int column)
	{
		char* v = PQgetvalue(res, row, column);

		if(v)
		{
			return SQLfield(std::string(v, PQgetlength(res, row, column)), PQgetisnull(res, row, column));
		}
		else
		{
			throw SQLbadColName();
		}
	}

	virtual SQLfieldList& GetRow()
	{
		/* In an effort to reduce overhead we don't actually allocate the list
		 * until the first time it's needed...so...
		 */
		if(fieldlist)
		{
			fieldlist->clear();
		}
		else
		{
			fieldlist = new SQLfieldList;
		}

		if(currentrow < PQntuples(res))
		{
			int cols = PQnfields(res);

			for(int i = 0; i < cols; i++)
			{
				fieldlist->push_back(GetValue(currentrow, i));
			}

			currentrow++;
		}

		return *fieldlist;
	}

	virtual SQLfieldMap& GetRowMap()
	{
		/* In an effort to reduce overhead we don't actually allocate the map
		 * until the first time it's needed...so...
		 */
		if(fieldmap)
		{
			fieldmap->clear();
		}
		else
		{
			fieldmap = new SQLfieldMap;
		}

		if(currentrow < PQntuples(res))
		{
			int cols = PQnfields(res);

			for(int i = 0; i < cols; i++)
			{
				fieldmap->insert(std::make_pair(ColName(i), GetValue(currentrow, i)));
			}

			currentrow++;
		}

		return *fieldmap;
	}

	virtual SQLfieldList* GetRowPtr()
	{
		SQLfieldList* fl = new SQLfieldList;

		if(currentrow < PQntuples(res))
		{
			int cols = PQnfields(res);

			for(int i = 0; i < cols; i++)
			{
				fl->push_back(GetValue(currentrow, i));
			}

			currentrow++;
		}

		return fl;
	}

	virtual SQLfieldMap* GetRowMapPtr()
	{
		SQLfieldMap* fm = new SQLfieldMap;

		if(currentrow < PQntuples(res))
		{
			int cols = PQnfields(res);

			for(int i = 0; i < cols; i++)
			{
				fm->insert(std::make_pair(ColName(i), GetValue(currentrow, i)));
			}

			currentrow++;
		}

		return fm;
	}

	virtual void Free(SQLfieldMap* fm)
	{
		DELETE(fm);
	}

	virtual void Free(SQLfieldList* fl)
	{
		DELETE(fl);
	}
};

/** SQLConn represents one SQL session.
 */
class SQLConn : public EventHandler
{
  private:
  	InspIRCd*		Instance;
	SQLhost			confhost;	/* The <database> entry */
	Module*			us;			/* Pointer to the SQL provider itself */
	PGconn* 		sql;		/* PgSQL database connection handle */
	SQLstatus		status;		/* PgSQL database connection status */
	bool			qinprog;	/* If there is currently a query in progress */
	QueryQueue		queue;		/* Queue of queries waiting to be executed on this connection */
	time_t			idle;		/* Time we last heard from the database */

  public:
	SQLConn(InspIRCd* SI, Module* self, const SQLhost& hi)
	: EventHandler(), Instance(SI), confhost(hi), us(self), sql(NULL), status(CWRITE), qinprog(false)
	{
		idle = this->Instance->Time();
		if(!DoConnect())
		{
			Instance->Log(DEFAULT, "WARNING: Could not connect to database with id: " + ConvToStr(hi.id));
			DelayReconnect();
		}
	}

	~SQLConn()
	{
		Close();
	}

	virtual void HandleEvent(EventType et, int errornum)
	{
		switch (et)
		{
			case EVENT_READ:
				OnDataReady();
			break;

			case EVENT_WRITE:
				OnWriteReady();
			break;

			case EVENT_ERROR:
				DelayReconnect();
			break;

			default:
			break;
		}
	}

	bool DoConnect()
	{
		if(!(sql = PQconnectStart(confhost.GetDSN().c_str())))
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

		if (!this->Instance->SE->AddFd(this))
		{
			Instance->Log(DEBUG, "BUG: Couldn't add pgsql socket to socket engine");
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
				Instance->SE->WantWrite(this);
				status = CWRITE;
				return true;
			case PGRES_POLLING_READING:
				status = CREAD;
				return true;
			case PGRES_POLLING_FAILED:
				return false;
			case PGRES_POLLING_OK:
				status = WWRITE;
				return DoConnectedPoll();
			default:
				return true;
		}
	}

	bool DoConnectedPoll()
	{
		if(!qinprog && queue.totalsize())
		{
			/* There's no query currently in progress, and there's queries in the queue. */
			SQLrequest& query = queue.front();
			DoQuery(query);
		}

		if(PQconsumeInput(sql))
		{
			/* We just read stuff from the server, that counts as it being alive
			 * so update the idle-since time :p
			 */
			idle = this->Instance->Time();

			if (PQisBusy(sql))
			{
				/* Nothing happens here */
			}
			else if (qinprog)
			{
				/* Grab the request we're processing */
				SQLrequest& query = queue.front();

				/* Get a pointer to the module we're about to return the result to */
				Module* to = query.GetSource();

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

				if(to)
				{
					/* ..and the result */
					PgSQLresult reply(us, to, query.id, result);

					/* Fix by brain, make sure the original query gets sent back in the reply */
					reply.query = query.query.q;

					switch(PQresultStatus(result))
					{
						case PGRES_EMPTY_QUERY:
						case PGRES_BAD_RESPONSE:
						case PGRES_FATAL_ERROR:
							reply.error.Id(QREPLY_FAIL);
							reply.error.Str(PQresultErrorMessage(result));
						default:;
							/* No action, other values are not errors */
					}

					reply.Send();

					/* PgSQLresult's destructor will free the PGresult */
				}
				else
				{
					/* If the client module is unloaded partway through a query then the provider will set
					 * the pointer to NULL. We cannot just cancel the query as the result will still come
					 * through at some point...and it could get messy if we play with invalid pointers...
					 */
					PQclear(result);
				}
				qinprog = false;
				queue.pop();
				DoConnectedPoll();
			}
			return true;
		}
		else
		{
			/* I think we'll assume this means the server died...it might not,
			 * but I think that any error serious enough we actually get here
			 * deserves to reconnect [/excuse]
			 * Returning true so the core doesn't try and close the connection.
			 */
			DelayReconnect();
			return true;
		}
	}

	bool DoResetPoll()
	{
		switch(PQresetPoll(sql))
		{
			case PGRES_POLLING_WRITING:
				Instance->SE->WantWrite(this);
				status = CWRITE;
				return DoPoll();
			case PGRES_POLLING_READING:
				status = CREAD;
				return true;
			case PGRES_POLLING_FAILED:
				return false;
			case PGRES_POLLING_OK:
				status = WWRITE;
				return DoConnectedPoll();
			default:
				return true;
		}
	}

	bool OnDataReady()
	{
		/* Always return true here, false would close the socket - we need to do that ourselves with the pgsql API */
		return DoEvent();
	}

	bool OnWriteReady()
	{
		/* Always return true here, false would close the socket - we need to do that ourselves with the pgsql API */
		return DoEvent();
	}

	bool OnConnected()
	{
		return DoEvent();
	}

	void DelayReconnect();

	bool DoEvent()
	{
		bool ret;

		if((status == CREAD) || (status == CWRITE))
		{
			ret = DoPoll();
		}
		else if((status == RREAD) || (status == RWRITE))
		{
			ret = DoResetPoll();
		}
		else
		{
			ret = DoConnectedPoll();
		}
		return ret;
	}

	SQLerror DoQuery(SQLrequest &req)
	{
		if((status == WREAD) || (status == WWRITE))
		{
			if(!qinprog)
			{
				/* Parse the command string and dispatch it */

				/* Pointer to the buffer we screw around with substitution in */
				char* query;
				/* Pointer to the current end of query, where we append new stuff */
				char* queryend;
				/* Total length of the unescaped parameters */
				unsigned int paramlen;

				paramlen = 0;

				for(ParamL::iterator i = req.query.p.begin(); i != req.query.p.end(); i++)
				{
					paramlen += i->size();
				}

				/* To avoid a lot of allocations, allocate enough memory for the biggest the escaped query could possibly be.
				 * sizeofquery + (totalparamlength*2) + 1
				 *
				 * The +1 is for null-terminating the string for PQsendQuery()
				 */

				query = new char[req.query.q.length() + (paramlen*2) + 1];
				queryend = query;

				/* Okay, now we have a buffer large enough we need to start copying the query into it and escaping and substituting
				 * the parameters into it...
				 */

				for(unsigned int i = 0; i < req.query.q.length(); i++)
				{
					if(req.query.q[i] == '?')
					{
						/* We found a place to substitute..what fun.
						 * Use the PgSQL calls to escape and write the
						 * escaped string onto the end of our query buffer,
						 * then we "just" need to make sure queryend is
						 * pointing at the right place.
						 */

						if(req.query.p.size())
						{
							int error = 0;
							size_t len = 0;

#ifdef PGSQL_HAS_ESCAPECONN
							len = PQescapeStringConn(sql, queryend, req.query.p.front().c_str(), req.query.p.front().length(), &error);
#else
							len = PQescapeString         (queryend, req.query.p.front().c_str(), req.query.p.front().length());
#endif
							if(error)
							{
								Instance->Log(DEBUG, "BUG: Apparently PQescapeStringConn() failed somehow...don't know how or what to do...");
							}

							/* Incremenet queryend to the end of the newly escaped parameter */
							queryend += len;

							/* Remove the parameter we just substituted in */
							req.query.p.pop_front();
						}
						else
						{
							Instance->Log(DEBUG, "BUG: Found a substitution location but no parameter to substitute :|");
							break;
						}
					}
					else
					{
						*queryend = req.query.q[i];
						queryend++;
					}
				}

				/* Null-terminate the query */
				*queryend = 0;
				req.query.q = query;

				if(PQsendQuery(sql, query))
				{
					qinprog = true;
					delete[] query;
					return SQLerror();
				}
				else
				{
					delete[] query;
					return SQLerror(QSEND_FAIL, PQerrorMessage(sql));
				}
			}
		}
		return SQLerror(BAD_CONN, "Can't query until connection is complete");
	}

	SQLerror Query(const SQLrequest &req)
	{
		queue.push(req);

		if(!qinprog && queue.totalsize())
		{
			/* There's no query currently in progress, and there's queries in the queue. */
			SQLrequest& query = queue.front();
			return DoQuery(query);
		}
		else
		{
			return SQLerror();
		}
	}

	void OnUnloadModule(Module* mod)
	{
		queue.PurgeModule(mod);
	}

	const SQLhost GetConfHost()
	{
		return confhost;
	}

	void Close() {
		if (!this->Instance->SE->DelFd(this))
		{
			if (sql && PQstatus(sql) == CONNECTION_BAD)
			{
				this->Instance->SE->DelFd(this, true);
			}
			else
			{
				Instance->Log(DEBUG, "BUG: PQsocket cant be removed from socket engine!");
			}
		}

		if(sql)
		{
			PQfinish(sql);
			sql = NULL;
		}
	}

};

class ModulePgSQL : public Module
{
  private:
	ConnMap connections;
	unsigned long currid;
	char* sqlsuccess;
	ReconnectTimer* retimer;

  public:
	ModulePgSQL(InspIRCd* Me)
	: Module::Module(Me), currid(0)
	{
		ServerInstance->UseInterface("SQLutils");

		sqlsuccess = new char[strlen(SQLSUCCESS)+1];

		strlcpy(sqlsuccess, SQLSUCCESS, strlen(SQLSUCCESS));

		if (!ServerInstance->PublishFeature("SQL", this))
		{
			throw ModuleException("BUG: PgSQL Unable to publish feature 'SQL'");
		}

		ReadConf();

		ServerInstance->PublishInterface("SQL", this);
	}

	virtual ~ModulePgSQL()
	{
		if (retimer)
			ServerInstance->Timers->DelTimer(retimer);
		ClearAllConnections();
		delete[] sqlsuccess;
		ServerInstance->UnpublishInterface("SQL", this);
		ServerInstance->UnpublishFeature("SQL");
		ServerInstance->DoneWithInterface("SQLutils");
	}

	void Implements(char* List)
	{
		List[I_OnUnloadModule] = List[I_OnRequest] = List[I_OnRehash] = List[I_OnUserRegister] = List[I_OnCheckReady] = List[I_OnUserDisconnect] = 1;
	}

	virtual void OnRehash(userrec* user, const std::string &parameter)
	{
		ReadConf();
	}

	bool HasHost(const SQLhost &host)
	{
		for (ConnMap::iterator iter = connections.begin(); iter != connections.end(); iter++)
		{
			if (host == iter->second->GetConfHost())
				return true;
		}
		return false;
	}

	bool HostInConf(const SQLhost &h)
	{
		ConfigReader conf(ServerInstance);
		for(int i = 0; i < conf.Enumerate("database"); i++)
		{
			SQLhost host;
			host.id		= conf.ReadValue("database", "id", i);
			host.host	= conf.ReadValue("database", "hostname", i);
			host.port	= conf.ReadInteger("database", "port", i, true);
			host.name	= conf.ReadValue("database", "name", i);
			host.user	= conf.ReadValue("database", "username", i);
			host.pass	= conf.ReadValue("database", "password", i);
			host.ssl	= conf.ReadFlag("database", "ssl", "0", i);
			if (h == host)
				return true;
		}
		return false;
	}

	void ReadConf()
	{
		ClearOldConnections();

		ConfigReader conf(ServerInstance);
		for(int i = 0; i < conf.Enumerate("database"); i++)
		{
			SQLhost host;
			int ipvalid;

			host.id		= conf.ReadValue("database", "id", i);
			host.host	= conf.ReadValue("database", "hostname", i);
			host.port	= conf.ReadInteger("database", "port", i, true);
			host.name	= conf.ReadValue("database", "name", i);
			host.user	= conf.ReadValue("database", "username", i);
			host.pass	= conf.ReadValue("database", "password", i);
			host.ssl	= conf.ReadFlag("database", "ssl", "0", i);

			if (HasHost(host))
				continue;

#ifdef IPV6
			if (strchr(host.host.c_str(),':'))
			{
				in6_addr blargle;
				ipvalid = inet_pton(AF_INET6, host.host.c_str(), &blargle);
			}
			else
#endif
			{
				in_addr blargle;
				ipvalid = inet_aton(host.host.c_str(), &blargle);
			}

			if(ipvalid > 0)
			{
				/* The conversion succeeded, we were given an IP and we can give it straight to SQLConn */
				host.ip = host.host;
				this->AddConn(host);
			}
			else if(ipvalid == 0)
			{
				/* Conversion failed, assume it's a host */
				SQLresolver* resolver;

				try
				{
					bool cached;
					resolver = new SQLresolver(this, ServerInstance, host, cached);
					ServerInstance->AddResolver(resolver, cached);
				}
				catch(...)
				{
					/* THE WORLD IS COMING TO AN END! */
				}
			}
			else
			{
				/* Invalid address family, die horribly. */
				ServerInstance->Log(DEBUG, "BUG: insp_aton failed returning -1, oh noes.");
			}
		}
	}

	void ClearOldConnections()
	{
		ConnMap::iterator iter,safei;
		for (iter = connections.begin(); iter != connections.end(); iter++)
		{
			if (!HostInConf(iter->second->GetConfHost()))
			{
				DELETE(iter->second);
				safei = iter;
				--iter;
				connections.erase(safei);
			}
		}
	}

	void ClearAllConnections()
	{
		ConnMap::iterator i;
		while ((i = connections.begin()) != connections.end())
		{
			connections.erase(i);
			DELETE(i->second);
		}
	}

	void AddConn(const SQLhost& hi)
	{
		if (HasHost(hi))
		{
			ServerInstance->Log(DEFAULT, "WARNING: A pgsql connection with id: %s already exists, possibly due to DNS delay. Aborting connection attempt.", hi.id.c_str());
			return;
		}

		SQLConn* newconn;

		/* The conversion succeeded, we were given an IP and we can give it straight to SQLConn */
		newconn = new SQLConn(ServerInstance, this, hi);

		connections.insert(std::make_pair(hi.id, newconn));
	}

	void ReconnectConn(SQLConn* conn)
	{
		for (ConnMap::iterator iter = connections.begin(); iter != connections.end(); iter++)
		{
			if (conn == iter->second)
			{
				DELETE(iter->second);
				connections.erase(iter);
				break;
			}
		}
		retimer = new ReconnectTimer(ServerInstance, this);
		ServerInstance->Timers->AddTimer(retimer);
	}

	virtual char* OnRequest(Request* request)
	{
		if(strcmp(SQLREQID, request->GetId()) == 0)
		{
			SQLrequest* req = (SQLrequest*)request;
			ConnMap::iterator iter;
			if((iter = connections.find(req->dbid)) != connections.end())
			{
				/* Execute query */
				req->id = NewID();
				req->error = iter->second->Query(*req);

				return (req->error.Id() == NO_ERROR) ? sqlsuccess : NULL;
			}
			else
			{
				req->error.Id(BAD_DBID);
				return NULL;
			}
		}
		return NULL;
	}

	virtual void OnUnloadModule(Module* mod, const std::string&	name)
	{
		/* When a module unloads we have to check all the pending queries for all our connections
		 * and set the Module* specifying where the query came from to NULL. If the query has already
		 * been dispatched then when it is processed it will be dropped if the pointer is NULL.
		 *
		 * If the queries we find are not already being executed then we can simply remove them immediately.
		 */
		for(ConnMap::iterator iter = connections.begin(); iter != connections.end(); iter++)
		{
			iter->second->OnUnloadModule(mod);
		}
	}

	unsigned long NewID()
	{
		if (currid+1 == 0)
			currid++;

		return ++currid;
	}

	virtual Version GetVersion()
	{
		return Version(1, 1, 0, 0, VF_VENDOR|VF_SERVICEPROVIDER, API_VERSION);
	}
};

/* move this here to use AddConn, rather that than having the whole
 * module above SQLConn, since this is buggin me right now :/
 */
void SQLresolver::OnLookupComplete(const std::string &result, unsigned int ttl, bool cached)
{
	host.ip = result;
	((ModulePgSQL*)mod)->AddConn(host);
	((ModulePgSQL*)mod)->ClearOldConnections();
}

void ReconnectTimer::Tick(time_t time)
{
	((ModulePgSQL*)mod)->ReadConf();
}

void SQLConn::DelayReconnect()
{
	((ModulePgSQL*)us)->ReconnectConn(this);
}

MODULE_INIT(ModulePgSQL);

