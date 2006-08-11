/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd is copyright (C) 2002-2004 ChatSpike-Dev.
 *                       E-mail:
 *                <brain@chatspike.net>
 *           	  <Craig@chatspike.net>
 *                <omster@gmail.com>
 *     
 * Written by Craig Edwards, Craig McLure, and others.
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include <cstdlib>
#include <sstream>
#include <string>
#include <deque>
#include <map>
#include <libpq-fe.h>

#include "users.h"
#include "channels.h"
#include "modules.h"
#include "helperfuncs.h"
#include "inspircd.h"
#include "configreader.h"

#include "m_sqlv2.h"

/* $ModDesc: PostgreSQL Service Provider module for all other m_sql* modules, uses v2 of the SQL API */
/* $CompileFlags: -I`pg_config --includedir` `perl extra/pgsql_config.pl` */
/* $LinkerFlags: -L`pg_config --libdir` -lpq */

/* UGH, UGH, UGH, UGH, UGH, UGH
 * I'm having trouble seeing how I
 * can avoid this. The core-defined
 * constructors for InspSocket just
 * aren't suitable...and if I'm
 * reimplementing them I need this so
 * I can access the socket engine :\
 */

extern time_t TIME;

/* Forward declare, so we can have the typedef neatly at the top */
class SQLConn;
/* Also needs forward declaration, as it's used inside SQLconn */
class ModulePgSQL;

typedef std::map<std::string, SQLConn*> ConnMap;

/* CREAD,	Connecting and wants read event
 * CWRITE,	Connecting and wants write event
 * WREAD,	Connected/Working and wants read event
 * WWRITE,	Connected/Working and wants write event
 * RREAD,	Resetting and wants read event
 * RWRITE,	Resetting and wants write event
 */
enum SQLstatus { CREAD, CWRITE, WREAD, WWRITE, RREAD, RWRITE };

/** SQLhost, simple structure to store information about a SQL-connection-to-be
 * We use this struct simply to make it neater passing around host information
 * when we're creating connections and resolving hosts.
 * Rather than giving SQLresolver a parameter for every field here so it can in
 * turn call SQLConn's constructor with them all, both can simply use a SQLhost.
 */
class SQLhost
{
 public:
	std::string		id;		/* Database handle id */
 	std::string 	host;	/* Database server hostname */
	unsigned int	port;	/* Database server port */
	std::string 	name;	/* Database name */
	std::string 	user;	/* Database username */
	std::string 	pass;	/* Database password */
	bool			ssl;	/* If we should require SSL */
 
	SQLhost()
	{
	}		

	SQLhost(const std::string& i, const std::string& h, unsigned int p, const std::string& n, const std::string& u, const std::string& pa, bool s)
	: id(i), host(h), port(p), name(n), user(u), pass(pa), ssl(s)
	{
	}
};

class SQLresolver : public Resolver
{
 private:
	SQLhost host;
	ModulePgSQL* mod;
 public:
	SQLresolver(ModulePgSQL* m, InspIRCd* Instance, const SQLhost& hi)
	: Resolver(Instance, hi.host, DNS_QUERY_FORWARD), host(hi), mod(m)
	{
	}

	virtual void OnLookupComplete(const std::string &result);

	virtual void OnError(ResolverError e, const std::string &errormessage)
	{
		log(DEBUG, "DNS lookup failed (%s), dying horribly", errormessage.c_str());
	}

	virtual ~SQLresolver()
	{
	}
};

/** QueryQueue, a queue of queries waiting to be executed.
 * This maintains two queues internally, one for 'priority'
 * queries and one for less important ones. Each queue has
 * new queries appended to it and ones to execute are popped
 * off the front. This keeps them flowing round nicely and no
 * query should ever get 'stuck' for too long. If there are
 * queries in the priority queue they will be executed first,
 * 'unimportant' queries will only be executed when the
 * priority queue is empty.
 *
 * We store lists of SQLrequest's here, by value as we want to avoid storing
 * any data allocated inside the client module (in case that module is unloaded
 * while the query is in progress).
 *
 * Because we want to work on the current SQLrequest in-situ, we need a way
 * of accessing the request we are currently processing, QueryQueue::front(),
 * but that call needs to always return the same request until that request
 * is removed from the queue, this is what the 'which' variable is. New queries are
 * always added to the back of one of the two queues, but if when front()
 * is first called then the priority queue is empty then front() will return
 * a query from the normal queue, but if a query is then added to the priority
 * queue then front() must continue to return the front of the *normal* queue
 * until pop() is called.
 */

class QueryQueue : public classbase
{
private:
	typedef std::deque<SQLrequest> ReqDeque;	

	ReqDeque priority;	/* The priority queue */
	ReqDeque normal;	/* The 'normal' queue */
	enum { PRI, NOR, NON } which;	/* Which queue the currently active element is at the front of */

public:
	QueryQueue()
	: which(NON)
	{
	}
	
	void push(const SQLrequest &q)
	{
		log(DEBUG, "QueryQueue::push(): Adding %s query to queue: %s", ((q.pri) ? "priority" : "non-priority"), q.query.q.c_str());
		
		if(q.pri)
			priority.push_back(q);
		else
			normal.push_back(q);
	}
	
	void pop()
	{
		if((which == PRI) && priority.size())
		{
			priority.pop_front();
		}
		else if((which == NOR) && normal.size())
		{
			normal.pop_front();
		}
		
		/* Reset this */
		which = NON;
		
		/* Silently do nothing if there was no element to pop() */
	}
	
	SQLrequest& front()
	{
		switch(which)
		{
			case PRI:
				return priority.front();
			case NOR:
				return normal.front();
			default:
				if(priority.size())
				{
					which = PRI;
					return priority.front();
				}
				
				if(normal.size())
				{
					which = NOR;
					return normal.front();
				}
				
				/* This will probably result in a segfault,
				 * but the caller should have checked totalsize()
				 * first so..meh - moron :p
				 */
				
				return priority.front();
		}
	}
	
	std::pair<int, int> size()
	{
		return std::make_pair(priority.size(), normal.size());
	}
	
	int totalsize()
	{
		return priority.size() + normal.size();
	}
	
	void PurgeModule(Module* mod)
	{
		DoPurgeModule(mod, priority);
		DoPurgeModule(mod, normal);
	}
	
private:
	void DoPurgeModule(Module* mod, ReqDeque& q)
	{
		for(ReqDeque::iterator iter = q.begin(); iter != q.end(); iter++)
		{
			if(iter->GetSource() == mod)
			{
				if(iter->id == front().id)
				{
					/* It's the currently active query.. :x */
					iter->SetSource(NULL);
				}
				else
				{
					/* It hasn't been executed yet..just remove it */
					iter = q.erase(iter);
				}
			}
		}
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
		
		log(DEBUG, "Created new PgSQL result; %d rows, %d columns, %s affected", rows, cols, PQcmdTuples(res));
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
			log(DEBUG, "PQgetvalue returned a null pointer..nobody wants to tell us what this means");
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
 * Each session has its own persistent connection to the database.
 * This is a subclass of InspSocket so it can easily recieve read/write events from the core socket
 * engine, unlike the original MySQL module this module does not block. Ever. It gets a mild stabbing
 * if it dares to.
 */

class SQLConn : public InspSocket
{
private:
	ModulePgSQL* us;		/* Pointer to the SQL provider itself */
	std::string 	dbhost;	/* Database server hostname */
	unsigned int	dbport;	/* Database server port */
	std::string 	dbname;	/* Database name */
	std::string 	dbuser;	/* Database username */
	std::string 	dbpass;	/* Database password */
	bool			ssl;	/* If we should require SSL */
	PGconn* 		sql;	/* PgSQL database connection handle */
	SQLstatus		status;	/* PgSQL database connection status */
	bool			qinprog;/* If there is currently a query in progress */
	QueryQueue		queue;	/* Queue of queries waiting to be executed on this connection */
	time_t			idle;	/* Time we last heard from the database */

public:

	/* This class should only ever be created inside this module, using this constructor, so we don't have to worry about the default ones */

	SQLConn(InspIRCd* SI, ModulePgSQL* self, const SQLhost& hostinfo);

	~SQLConn();

	bool DoConnect();

	virtual void Close();
	
	bool DoPoll();
	
	bool DoConnectedPoll();

	bool DoResetPoll();
	
	void ShowStatus();	
	
	virtual bool OnDataReady();

	virtual bool OnWriteReady();
	
	virtual bool OnConnected();
	
	bool DoEvent();
	
	bool Reconnect();
	
	std::string MkInfoStr();
	
	const char* StatusStr();
	
	SQLerror DoQuery(SQLrequest &req);
	
	SQLerror Query(const SQLrequest &req);
	
	void OnUnloadModule(Module* mod);
};

class ModulePgSQL : public Module
{
private:
	
	ConnMap connections;
	unsigned long currid;
	char* sqlsuccess;

public:
	ModulePgSQL(InspIRCd* Me)
	: Module::Module(Me), currid(0)
	{
		log(DEBUG, "%s 'SQL' feature", ServerInstance->PublishFeature("SQL", this) ? "Published" : "Couldn't publish");
		
		sqlsuccess = new char[strlen(SQLSUCCESS)+1];
		
		strcpy(sqlsuccess, SQLSUCCESS);

		OnRehash("");
	}

	void Implements(char* List)
	{
		List[I_OnUnloadModule] = List[I_OnRequest] = List[I_OnRehash] = List[I_OnUserRegister] = List[I_OnCheckReady] = List[I_OnUserDisconnect] = 1;
	}

	virtual void OnRehash(const std::string &parameter)
	{
		ConfigReader conf(ServerInstance);
		
		/* Delete all the SQLConn objects in the connection lists,
		 * this will call their destructors where they can handle
		 * closing connections and such.
		 */
		for(ConnMap::iterator iter = connections.begin(); iter != connections.end(); iter++)
		{
			DELETE(iter->second);
		}
		
		/* Empty out our list of connections */
		connections.clear();

		for(int i = 0; i < conf.Enumerate("database"); i++)
		{
			SQLhost host;			
			int ipvalid;
			insp_inaddr blargle;
			
			host.id		= conf.ReadValue("database", "id", i);
			host.host	= conf.ReadValue("database", "hostname", i);
			host.port	= conf.ReadInteger("database", "port", i, true);
			host.name	= conf.ReadValue("database", "name", i);
			host.user	= conf.ReadValue("database", "username", i);
			host.pass	= conf.ReadValue("database", "password", i);
			host.ssl	= conf.ReadFlag("database", "ssl", i);
			
			ipvalid = insp_aton(host.host.c_str(), &blargle);
			
			if(ipvalid > 0)
			{
				/* The conversion succeeded, we were given an IP and we can give it straight to SQLConn */
				this->AddConn(host);
			}
			else if(ipvalid == 0)
			{
				/* Conversion failed, assume it's a host */
				SQLresolver* resolver;
				
				resolver = new SQLresolver(this, ServerInstance, host);
				
				ServerInstance->AddResolver(resolver);
			}
			else
			{
				/* Invalid address family, die horribly. */
				log(DEBUG, "insp_aton failed returning -1, oh noes.");
			}
		}	
	}
	
	void AddConn(const SQLhost& hi)
	{
		SQLConn* newconn;
				
		/* The conversion succeeded, we were given an IP and we can give it straight to SQLConn */
		newconn = new SQLConn(ServerInstance, this, hi);
				
		connections.insert(std::make_pair(hi.id, newconn));
	}
	
	virtual char* OnRequest(Request* request)
	{
		if(strcmp(SQLREQID, request->GetId()) == 0)
		{
			SQLrequest* req = (SQLrequest*)request;
			ConnMap::iterator iter;
		
			log(DEBUG, "Got query: '%s' with %d replacement parameters on id '%s'", req->query.q.c_str(), req->query.p.size(), req->dbid.c_str());

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

		log(DEBUG, "Got unsupported API version string: %s", request->GetId());
		
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
		return Version(1, 0, 0, 0, VF_VENDOR|VF_SERVICEPROVIDER);
	}
	
	virtual ~ModulePgSQL()
	{
		DELETE(sqlsuccess);
	}	
};

SQLConn::SQLConn(InspIRCd* SI, ModulePgSQL* self, const SQLhost& hi)
: InspSocket::InspSocket(SI), us(self), dbhost(hi.host), dbport(hi.port), dbname(hi.name), dbuser(hi.user), dbpass(hi.pass), ssl(hi.ssl), sql(NULL), status(CWRITE), qinprog(false)
{
	log(DEBUG, "Creating new PgSQL connection to database %s on %s:%u (%s/%s)", dbname.c_str(), dbhost.c_str(), dbport, dbuser.c_str(), dbpass.c_str());

	/* Some of this could be reviewed, unsure if I need to fill 'host' etc...
	 * just copied this over from the InspSocket constructor.
	 */
	strlcpy(this->host, dbhost.c_str(), MAXBUF);
	strlcpy(this->IP, dbhost.c_str(), MAXBUF);
	this->port = dbport;
	idle = TIME;
	
	this->ClosePending = false;
			
	log(DEBUG,"No need to resolve %s", this->host);
	
		
	if(!this->DoConnect())
	{
		throw ModuleException("Connect failed");
	}
}

SQLConn::~SQLConn()
{
	Close();
}

bool SQLConn::DoConnect()
{
	log(DEBUG, "SQLConn::DoConnect()");
	
	if(!(sql = PQconnectStart(MkInfoStr().c_str())))
	{
		log(DEBUG, "Couldn't allocate PGconn structure, aborting: %s", PQerrorMessage(sql));
		Close();
		return false;
	}
	
	if(PQstatus(sql) == CONNECTION_BAD)
	{
		log(DEBUG, "PQconnectStart failed: %s", PQerrorMessage(sql));
		Close();
		return false;
	}
	
	ShowStatus();
	
	if(PQsetnonblocking(sql, 1) == -1)
	{
		log(DEBUG, "Couldn't set connection nonblocking: %s", PQerrorMessage(sql));
		Close();
		return false;
	}
	
	/* OK, we've initalised the connection, now to get it hooked into the socket engine
	 * and then start polling it.
	 */
	
	log(DEBUG, "Old DNS socket: %d", this->fd);
	this->fd = PQsocket(sql);
	log(DEBUG, "New SQL socket: %d", this->fd);
	
	if(this->fd <= -1)
	{
		log(DEBUG, "PQsocket says we have an invalid FD: %d", this->fd);
		Close();
		return false;
	}
	
	this->state = I_CONNECTING;
	if (!this->Instance->SE->AddFd(this->fd,false,X_ESTAB_MODULE))
	{
		log(DEBUG, "A PQsocket cant be added to the socket engine!");
		Close();
		return false;
	}
	this->Instance->socket_ref[this->fd] = this;
	
	/* Socket all hooked into the engine, now to tell PgSQL to start connecting */
	
	return DoPoll();
}

void SQLConn::Close()
{
	log(DEBUG,"SQLConn::Close");
	
	if(this->fd > 01)
		Instance->socket_ref[this->fd] = NULL;
	this->fd = -1;
	this->state = I_ERROR;
	this->OnError(I_ERR_SOCKET);
	this->ClosePending = true;
	
	if(sql)
	{
		PQfinish(sql);
		sql = NULL;
	}
	
	return;
}

bool SQLConn::DoPoll()
{
	switch(PQconnectPoll(sql))
	{
		case PGRES_POLLING_WRITING:
			log(DEBUG, "PGconnectPoll: PGRES_POLLING_WRITING");
			WantWrite();
			status = CWRITE;
			return DoPoll();
		case PGRES_POLLING_READING:
			log(DEBUG, "PGconnectPoll: PGRES_POLLING_READING");
			status = CREAD;
			return true;
		case PGRES_POLLING_FAILED:
			log(DEBUG, "PGconnectPoll: PGRES_POLLING_FAILED: %s", PQerrorMessage(sql));
			return false;
		case PGRES_POLLING_OK:
			log(DEBUG, "PGconnectPoll: PGRES_POLLING_OK");
			status = WWRITE;
			return DoConnectedPoll();
		default:
			log(DEBUG, "PGconnectPoll: wtf?");
			return true;
	}
}

bool SQLConn::DoConnectedPoll()
{
	if(!qinprog && queue.totalsize())
	{
		/* There's no query currently in progress, and there's queries in the queue. */
		SQLrequest& query = queue.front();
		DoQuery(query);
	}
	
	if(PQconsumeInput(sql))
	{
		log(DEBUG, "PQconsumeInput succeeded");
		
		/* We just read stuff from the server, that counts as it being alive
		 * so update the idle-since time :p
		 */
		idle = TIME;
			
		if(PQisBusy(sql))
		{
			log(DEBUG, "Still busy processing command though");
		}
		else if(qinprog)
		{
			log(DEBUG, "Looks like we have a result to process!");
			
			/* Grab the request we're processing */
			SQLrequest& query = queue.front();
			
			log(DEBUG, "ID is %lu", query.id);
			
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
				
				log(DEBUG, "Got result, status code: %s; error message: %s", PQresStatus(PQresultStatus(result)), PQresultErrorMessage(result));	
				
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
				log(DEBUG, "Looks like we're handling a zombie query from a module which unloaded before it got a result..fun. ID: %lu", query.id);
				PQclear(result);
			}
			
			qinprog = false;
			queue.pop();				
			DoConnectedPoll();
		}
		else
		{
			log(DEBUG, "Eh!? We just got a read event, and connection isn't busy..but no result :(");
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
		log(DEBUG, "PQconsumeInput failed: %s", PQerrorMessage(sql));
		Reconnect();
		return true;
	}
}

bool SQLConn::DoResetPoll()
{
	switch(PQresetPoll(sql))
	{
		case PGRES_POLLING_WRITING:
			log(DEBUG, "PGresetPoll: PGRES_POLLING_WRITING");
			WantWrite();
			status = CWRITE;
			return DoPoll();
		case PGRES_POLLING_READING:
			log(DEBUG, "PGresetPoll: PGRES_POLLING_READING");
			status = CREAD;
			return true;
		case PGRES_POLLING_FAILED:
			log(DEBUG, "PGresetPoll: PGRES_POLLING_FAILED: %s", PQerrorMessage(sql));
			return false;
		case PGRES_POLLING_OK:
			log(DEBUG, "PGresetPoll: PGRES_POLLING_OK");
			status = WWRITE;
			return DoConnectedPoll();
		default:
			log(DEBUG, "PGresetPoll: wtf?");
			return true;
	}
}

void SQLConn::ShowStatus()
{
	switch(PQstatus(sql))
	{
		case CONNECTION_STARTED:
			log(DEBUG, "PQstatus: CONNECTION_STARTED: Waiting for connection to be made.");
			break;

		case CONNECTION_MADE:
			log(DEBUG, "PQstatus: CONNECTION_MADE: Connection OK; waiting to send.");
			break;
		
		case CONNECTION_AWAITING_RESPONSE:
			log(DEBUG, "PQstatus: CONNECTION_AWAITING_RESPONSE: Waiting for a response from the server.");
			break;
		
		case CONNECTION_AUTH_OK:
			log(DEBUG, "PQstatus: CONNECTION_AUTH_OK: Received authentication; waiting for backend start-up to finish.");
			break;
		
		case CONNECTION_SSL_STARTUP:
			log(DEBUG, "PQstatus: CONNECTION_SSL_STARTUP: Negotiating SSL encryption.");
			break;
		
		case CONNECTION_SETENV:
			log(DEBUG, "PQstatus: CONNECTION_SETENV: Negotiating environment-driven parameter settings.");
			break;
		
		default:
			log(DEBUG, "PQstatus: ???");
	}
}

bool SQLConn::OnDataReady()
{
	/* Always return true here, false would close the socket - we need to do that ourselves with the pgsql API */
	log(DEBUG, "OnDataReady(): status = %s", StatusStr());
	
	return DoEvent();
}

bool SQLConn::OnWriteReady()
{
	/* Always return true here, false would close the socket - we need to do that ourselves with the pgsql API */
	log(DEBUG, "OnWriteReady(): status = %s", StatusStr());
	
	return DoEvent();
}

bool SQLConn::OnConnected()
{
	log(DEBUG, "OnConnected(): status = %s", StatusStr());
	
	return DoEvent();
}

bool SQLConn::Reconnect()
{
	log(DEBUG, "Initiating reconnect");
	
	if(PQresetStart(sql))
	{
		/* Successfully initiatied database reconnect,
		 * set flags so PQresetPoll() will be called appropriately
		 */
		status = RWRITE;
		qinprog = false;
		return true;
	}
	else
	{
		log(DEBUG, "Failed to initiate reconnect...fun");
		return false;
	}	
}

bool SQLConn::DoEvent()
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
	
	switch(PQflush(sql))
	{
		case -1:
			log(DEBUG, "Error flushing write queue: %s", PQerrorMessage(sql));
			break;
		case 0:
			log(DEBUG, "Successfully flushed write queue (or there was nothing to write)");
			break;
		case 1:
			log(DEBUG, "Not all of the write queue written, triggering write event so we can have another go");
			WantWrite();
			break;
	}

	return ret;
}

std::string SQLConn::MkInfoStr()
{			
	std::ostringstream conninfo("connect_timeout = '2'");
	
	if(dbhost.length())
		conninfo << " hostaddr = '" << dbhost << "'";
	
	if(dbport)
		conninfo << " port = '" << dbport << "'";
	
	if(dbname.length())
		conninfo << " dbname = '" << dbname << "'";
	
	if(dbuser.length())
		conninfo << " user = '" << dbuser << "'";
	
	if(dbpass.length())
		conninfo << " password = '" << dbpass << "'";
	
	if(ssl)
		conninfo << " sslmode = 'require'";
	
	return conninfo.str();
}

const char* SQLConn::StatusStr()
{
	if(status == CREAD) return "CREAD";
	if(status == CWRITE) return "CWRITE";
	if(status == WREAD) return "WREAD";
	if(status == WWRITE) return "WWRITE";
	return "Err...what, erm..BUG!";
}

SQLerror SQLConn::DoQuery(SQLrequest &req)
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
			
			query = new char[req.query.q.length() + (paramlen*2)];
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
						len = PQescapeStringConn(queryend, req.query.p.front().c_str(), req.query.p.front().length());
						error = 0;
#endif
						if(error)
						{
							log(DEBUG, "Apparently PQescapeStringConn() failed somehow...don't know how or what to do...");
						}
						
						log(DEBUG, "Appended %d bytes of escaped string onto the query", len);
						
						/* Incremenet queryend to the end of the newly escaped parameter */
						queryend += len;
						
						/* Remove the parameter we just substituted in */
						req.query.p.pop_front();
					}
					else
					{
						log(DEBUG, "Found a substitution location but no parameter to substitute :|");
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
	
			log(DEBUG, "Attempting to dispatch query: %s", query);
			
			req.query.q = query;

			if(PQsendQuery(sql, query))
			{
				log(DEBUG, "Dispatched query successfully");
				qinprog = true;
				DELETE(query);
				return SQLerror();
			}
			else
			{
				log(DEBUG, "Failed to dispatch query: %s", PQerrorMessage(sql));
				DELETE(query);
				return SQLerror(QSEND_FAIL, PQerrorMessage(sql));
			}
		}
	}

	log(DEBUG, "Can't query until connection is complete");
	return SQLerror(BAD_CONN, "Can't query until connection is complete");
}

SQLerror SQLConn::Query(const SQLrequest &req)
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

void SQLConn::OnUnloadModule(Module* mod)
{
	queue.PurgeModule(mod);
}

void SQLresolver::OnLookupComplete(const std::string &result)
{
	host.host = result;
	mod->AddConn(host);
}

class ModulePgSQLFactory : public ModuleFactory
{
 public:
	ModulePgSQLFactory()
	{
	}
	
	~ModulePgSQLFactory()
	{
	}
	
	virtual Module * CreateModule(InspIRCd* Me)
	{
		return new ModulePgSQL(Me);
	}
};


extern "C" void * init_module( void )
{
	return new ModulePgSQLFactory;
}
