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
/* $CompileFlags: -I`pg_config --includedir` */
/* $LinkerFlags: -L`pg_config --libdir` -lpq */

/* UGH, UGH, UGH, UGH, UGH, UGH
 * I'm having trouble seeing how I
 * can avoid this. The core-defined
 * constructors for InspSocket just
 * aren't suitable...and if I'm
 * reimplementing them I need this so
 * I can access the socket engine :\
 */
extern InspIRCd* ServerInstance;
InspSocket* socket_ref[MAX_DESCRIPTORS];

/* Forward declare, so we can have the typedef neatly at the top */
class SQLConn;

typedef std::map<std::string, SQLConn*> ConnMap;

/* CREAD,	Connecting and wants read event
 * CWRITE,	Connecting and wants write event
 * WREAD,	Connected/Working and wants read event
 * WWRITE, 	Connected/Working and wants write event
 */
enum SQLstatus { CREAD, CWRITE, WREAD, WWRITE };

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
 * These are lists of SQLresult so we can, from the moment the
 * SQLrequest is recieved, be beginning to construct the result
 * object. The copy in the deque can then be submitted in-situ
 * and finally deleted from this queue. No copies of the SQLresult :)
 *
 * Because we work on the SQLresult in-situ, we need a way of accessing the
 * result we are currently processing, QueryQueue::front(), but that call
 * needs to always return the same element until that element is removed
 * from the queue, this is what the 'which' variable is. New queries are
 * always added to the back of one of the two queues, but if when front()
 * is first called then the priority queue is empty then front() will return
 * a query from the normal queue, but if a query is then added to the priority
 * queue then front() must continue to return the front of the *normal* queue
 * until pop() is called.
 */

class QueryQueue : public classbase
{
private:
	std::deque<SQLresult> priority;	/* The priority queue */
	std::deque<SQLresult> normal;	/* The 'normal' queue */
	enum { PRI, NOR, NON } which;	/* Which queue the currently active element is at the front of */

public:
	QueryQueue()
	: which(NON)
	{
	
	}
	
	void push(const Query &q, bool pri = false)
	{
		log(DEBUG, "QueryQueue::push_back(): Adding %s query to queue: %s", ((pri) ? "priority" : "non-priority"), q.c_str());
		
		if(pri)
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
		
		/* Silently do nothing if there was no element to pop() */
	}
	
	SQLresult& front()
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
	Server* Srv;			/* Server* for..uhm..something, maybe */
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
	Query			query;	/* The currently active query on this connection */

public:

	/* This class should only ever be created inside this module, using this constructor, so we don't have to worry about the default ones */

	SQLConn(Server* srv, const std::string &h, unsigned int p, const std::string &d, const std::string &u, const std::string &pwd, bool s)
	: InspSocket::InspSocket(), Srv(srv), dbhost(h), dbport(p), dbname(d), dbuser(u), dbpass(pwd), ssl(s), sql(NULL), status(CWRITE), qinprog(false)
	{
		log(DEBUG, "Creating new PgSQL connection to database %s on %s:%u (%s/%s)", dbname.c_str(), dbhost.c_str(), dbport, dbuser.c_str(), dbpass.c_str());

		/* Some of this could be reviewed, unsure if I need to fill 'host' etc...
		 * just copied this over from the InspSocket constructor.
		 */
		strlcpy(this->host, dbhost.c_str(), MAXBUF);
		this->port = dbport;
		
		this->ClosePending = false;
		
		if(!inet_aton(this->host, &this->addy))
		{
			/* Its not an ip, spawn the resolver.
			 * PgSQL doesn't do nonblocking DNS 
			 * lookups, so we do it for it.
			 */
			
			log(DEBUG,"Attempting to resolve %s", this->host);
			
			this->dns.SetNS(Srv->GetConfig()->DNSServer);
			this->dns.ForwardLookupWithFD(this->host, fd);
			
			this->state = I_RESOLVING;
			socket_ref[this->fd] = this;
			
			return;
		}
		else
		{
			log(DEBUG,"No need to resolve %s", this->host);
			strlcpy(this->IP, this->host, MAXBUF);
			
			if(!this->DoConnect())
			{
				throw ModuleException("Connect failed");
			}
		}
	}
	
	~SQLConn()
	{
		Close();
	}
	
	bool DoResolve()
	{	
		log(DEBUG, "Checking for DNS lookup result");
		
		if(this->dns.HasResult())
		{
			std::string res_ip = dns.GetResultIP();
			
			if(res_ip.length())
			{
				log(DEBUG, "Got result: %s", res_ip.c_str());
				
				strlcpy(this->IP, res_ip.c_str(), MAXBUF);
				dbhost = res_ip;
				
				socket_ref[this->fd] = NULL;
				
				return this->DoConnect();
			}
			else
			{
				log(DEBUG, "DNS lookup failed, dying horribly");
				Close();
				return false;
			}
		}
		else
		{
			log(DEBUG, "No result for lookup yet!");
			return true;
		}
	}

	bool DoConnect()
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
		ServerInstance->SE->AddFd(this->fd,false,X_ESTAB_MODULE);
		socket_ref[this->fd] = this;
		
		/* Socket all hooked into the engine, now to tell PgSQL to start connecting */
		
		return DoPoll();
	}
	
	virtual void Close()
	{
		log(DEBUG,"SQLConn::Close");
		
		if(this->fd > 01)
			socket_ref[this->fd] = NULL;
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
	
	bool DoPoll()
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
				break;
			case PGRES_POLLING_FAILED:
				log(DEBUG, "PGconnectPoll: PGRES_POLLING_FAILED: %s", PQerrorMessage(sql));
				return false;
			case PGRES_POLLING_OK:
				log(DEBUG, "PGconnectPoll: PGRES_POLLING_OK");
				status = WWRITE;
				return DoConnectedPoll()
			default:
				log(DEBUG, "PGconnectPoll: wtf?");
				break;
		}
		
		return true;
	}
	
	bool DoConnectedPoll()
	{
		if(!qinprog && queue.totalsize())
		{
			/* There's no query currently in progress, and there's queries in the queue. */
			query = queue.pop_front();
			DoQuery();
		}
		
		if(PQconsumeInput(sql))
		{
			log(DEBUG, "PQconsumeInput succeeded");
				
			if(PQisBusy(sql))
			{
				log(DEBUG, "Still busy processing command though");
			}
			else
			{
				log(DEBUG, "Looks like we have a result to process!");
				
				while(PGresult* result = PQgetResult(sql))
				{
					int cols = PQnfields(result);
					
					log(DEBUG, "Got result! :D");
					log(DEBUG, "%d rows, %d columns checking now what the column names are", PQntuples(result), cols);
						
					for(int i = 0; i < cols; i++)
					{
						log(DEBUG, "Column name: %s (%d)", PQfname(result, i));
					}
						
					PQclear(result);
				}
				
				qinprog = false;
			}
			
			return true;
		}
		
		log(DEBUG, "PQconsumeInput failed: %s", PQerrorMessage(sql));
		return false;
	}
	
	void ShowStatus()
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
	
	virtual bool OnDataReady()
	{
		/* Always return true here, false would close the socket - we need to do that ourselves with the pgsql API */
		log(DEBUG, "OnDataReady(): status = %s", StatusStr());
		
		return DoEvent();
	}

	virtual bool OnWriteReady()
	{
		/* Always return true here, false would close the socket - we need to do that ourselves with the pgsql API */
		log(DEBUG, "OnWriteReady(): status = %s", StatusStr());
		
		return DoEvent();
	}
	
	virtual bool OnConnected()
	{
		log(DEBUG, "OnConnected(): status = %s", StatusStr());
		
		return DoEvent();
	}
	
	bool DoEvent()
	{
		bool ret;
		
		if((status == CREAD) || (status == CWRITE))
		{
			ret = DoPoll();
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
	
	std::string MkInfoStr()
	{			
		/* XXX - This needs nonblocking DNS lookups */
		
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
	
	const char* StatusStr()
	{
		if(status == CREAD) return "CREAD";
		if(status == CWRITE) return "CWRITE";
		if(status == WREAD) return "WREAD";
		if(status == WWRITE) return "WWRITE";
		return "Err...what, erm..BUG!";
	}
	
	SQLerror Query(const Query &query, bool pri)
	{
		queue.push_back(query, pri);
		
		if((status == WREAD) || (status == WWRITE))
		{
			if(!qinprog)
			{
				if(PQsendQuery(sql, query.c_str()))
				{
					log(DEBUG, "Dispatched query: %s", query.c_str());
					qinprog = true;
					return SQLerror();
				}
				else
				{
					log(DEBUG, "Failed to dispatch query: %s", PQerrorMessage(sql));
					return SQLerror(QSEND_FAIL, PQerrorMessage(sql));
				}
			}
		}

		log(DEBUG, "Can't query until connection is complete");
		return SQLerror(BAD_CONN, "Can't query until connection is complete");
	}
};

class ModulePgSQL : public Module
{
private:
	Server* Srv;
	ConnMap connections;

public:
	ModulePgSQL(Server* Me)
	: Module::Module(Me), Srv(Me)
	{
		log(DEBUG, "%s 'SQL' feature", Srv->PublishFeature("SQL", this) ? "Published" : "Couldn't publish");
		log(DEBUG, "%s 'PgSQL' feature", Srv->PublishFeature("PgSQL", this) ? "Published" : "Couldn't publish");

		OnRehash("");
	}

	void Implements(char* List)
	{
		List[I_OnRequest] = List[I_OnRehash] = List[I_OnUserRegister] = List[I_OnCheckReady] = List[I_OnUserDisconnect] = 1;
	}

	virtual void OnRehash(const std::string &parameter)
	{
		ConfigReader conf;
		
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
			std::string id;
			SQLConn* newconn;
			
			id = conf.ReadValue("database", "id", i);
			newconn = new SQLConn(Srv,	conf.ReadValue("database", "hostname", i),
										conf.ReadInteger("database", "port", i, true),
										conf.ReadValue("database", "name", i),
										conf.ReadValue("database", "username", i),
										conf.ReadValue("database", "password", i),
										conf.ReadFlag("database", "ssl", i));
			
			connections.insert(std::make_pair(id, newconn));
		}	
	}
	
	virtual char* OnRequest(Request* request)
	{
		if(strcmp(SQLREQID, request->GetData()) == 0)
		{
			SQLrequest* req = (SQLrequest*)request;
			ConnMap::iterator iter;
		
			log(DEBUG, "Got query: '%s' on id '%s'", req->query.c_str(), req->dbid.c_str());

			if((iter = connections.find(req->dbid)) != connections.end())
			{
				/* Execute query */
				req->error = iter->second->Query(Query(req->query, req->GetSource(), this), req->pri);
				
				return SQLSUCCESS;
			}
			else
			{
				req->error.Id(BAD_DBID);
				return NULL;
			}
		}

		log(DEBUG, "Got unsupported API version string: %s", request->GetData());
		
		return NULL;
	}
		
	virtual Version GetVersion()
	{
		return Version(1, 0, 0, 0, VF_VENDOR|VF_SERVICEPROVIDER);
	}
	
	virtual ~ModulePgSQL()
	{
	}	
};

class ModulePgSQLFactory : public ModuleFactory
{
 public:
	ModulePgSQLFactory()
	{
	}
	
	~ModulePgSQLFactory()
	{
	}
	
	virtual Module * CreateModule(Server* Me)
	{
		return new ModulePgSQL(Me);
	}
};


extern "C" void * init_module( void )
{
	return new ModulePgSQLFactory;
}
