/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd is copyright (C) 2002-2004 ChatSpike-Dev.
 *		       E-mail:
 *		<brain@chatspike.net>
 *	   	  <Craig@chatspike.net>
 *     
 * Written by Craig Edwards, Craig McLure, and others.
 * This program is free but copyrighted software; see
 *	    the file COPYING for details.
 *
 * ---------------------------------------------------
 */

using namespace std;

#include <stdio.h>
#include <string>
#include <mysql.h>
#include <pthread.h>
#include "users.h"
#include "channels.h"
#include "modules.h"
#include "helperfuncs.h"
#include "m_sqlv2.h"

/* VERSION 2 API: With nonblocking (threaded) requests */

/* $ModDesc: SQL Service Provider module for all other m_sql* modules */
/* $CompileFlags: `mysql_config --include` */
/* $LinkerFlags: `mysql_config --libs_r` `perl ../mysql_rpath.pl` */


class SQLConnection;

extern InspIRCd* ServerInstance;
typedef std::map<std::string, SQLConnection*> ConnMap;
bool giveup = false;


#if !defined(MYSQL_VERSION_ID) || MYSQL_VERSION_ID<32224
#define mysql_field_count mysql_num_fields
#endif

class QueryQueue : public classbase
{
private:
	typedef std::deque<SQLrequest> ReqDeque;

	ReqDeque priority;      /* The priority queue */
	ReqDeque normal;	/* The 'normal' queue */
	enum { PRI, NOR, NON } which;   /* Which queue the currently active element is at the front of */

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

/* A mutex to wrap around queue accesses */
pthread_mutex_t queue_mutex = PTHREAD_MUTEX_INITIALIZER;

class SQLConnection : public classbase
{
 protected:

	MYSQL connection;
	MYSQL_RES *res;
	MYSQL_ROW row;
	std::string host;
	std::string user;
	std::string pass;
	std::string db;
	std::map<std::string,std::string> thisrow;
	bool Enabled;
	long id;

 public:

	QueryQueue queue;

	// This constructor creates an SQLConnection object with the given credentials, and creates the underlying
	// MYSQL struct, but does not connect yet.
	SQLConnection(std::string thishost, std::string thisuser, std::string thispass, std::string thisdb, long myid)
	{
		this->Enabled = true;
		this->host = thishost;
		this->user = thisuser;
		this->pass = thispass;
		this->db = thisdb;
		this->id = myid;
	}

	// This method connects to the database using the credentials supplied to the constructor, and returns
	// true upon success.
	bool Connect()
	{
		unsigned int timeout = 1;
		mysql_init(&connection);
		mysql_options(&connection,MYSQL_OPT_CONNECT_TIMEOUT,(char*)&timeout);
		return mysql_real_connect(&connection, host.c_str(), user.c_str(), pass.c_str(), db.c_str(), 0, NULL, 0);
	}

	void DoLeadingQuery()
	{
		SQLrequest& query = queue.front();
		log(DEBUG,"DO QUERY: %s",query.query.q.c_str());
	}

	// This method issues a query that expects multiple rows of results. Use GetRow() and QueryDone() to retrieve
	// multiple rows.
	bool QueryResult(std::string query)
	{
		if (!CheckConnection()) return false;
		
		int r = mysql_query(&connection, query.c_str());
		if (!r)
		{
			res = mysql_use_result(&connection);
		}
		return (!r);
	}

	// This method issues a query that just expects a number of 'effected' rows (e.g. UPDATE or DELETE FROM).
	// the number of effected rows is returned in the return value.
	long QueryCount(std::string query)
	{
		/* If the connection is down, we return a negative value - New to 1.1 */
		if (!CheckConnection()) return -1;

		int r = mysql_query(&connection, query.c_str());
		if (!r)
		{
			res = mysql_store_result(&connection);
			unsigned long rows = mysql_affected_rows(&connection);
			mysql_free_result(res);
			return rows;
		}
		return 0;
	}

	// This method fetches a row, if available from the database. You must issue a query
	// using QueryResult() first! The row's values are returned as a map of std::string
	// where each item is keyed by the column name.
	std::map<std::string,std::string> GetRow()
	{
		thisrow.clear();
		if (res)
		{
			row = mysql_fetch_row(res);
			if (row)
			{
				unsigned int field_count = 0;
				MYSQL_FIELD *fields = mysql_fetch_fields(res);
				if(mysql_field_count(&connection) == 0)
					return thisrow;
				if (fields && mysql_field_count(&connection))
				{
					while (field_count < mysql_field_count(&connection))
					{
						std::string a = (fields[field_count].name ? fields[field_count].name : "");
						std::string b = (row[field_count] ? row[field_count] : "");
						thisrow[a] = b;
						field_count++;
					}
					return thisrow;
				}
			}
		}
		return thisrow;
	}

	bool QueryDone()
	{
		if (res)
		{
			mysql_free_result(res);
			res = NULL;
			return true;
		}
		else return false;
	}

	bool ConnectionLost()
	{
		if (&connection) {
			return (mysql_ping(&connection) != 0);
		}
		else return false;
	}

	bool CheckConnection()
	{
		if (ConnectionLost()) {
			return Connect();
		}
		else return true;
	}

	std::string GetError()
	{
		return mysql_error(&connection);
	}

	long GetID()
	{
		return id;
	}

	std::string GetHost()
	{
		return host;
	}

	void Enable()
	{
		Enabled = true;
	}

	void Disable()
	{
		Enabled = false;
	}

	bool IsEnabled()
	{
		return Enabled;
	}

};

ConnMap Connections;

void ConnectDatabases(Server* Srv)
{
	for (ConnMap::iterator i = Connections.begin(); i != Connections.end(); i++)
	{
		i->second->Enable();
		if (i->second->Connect())
		{
			Srv->Log(DEFAULT,"SQL: Successfully connected database "+i->second->GetHost());
		}
		else
		{
			Srv->Log(DEFAULT,"SQL: Failed to connect database "+i->second->GetHost()+": Error: "+i->second->GetError());
			i->second->Disable();
		}
	}
}


void LoadDatabases(ConfigReader* ThisConf, Server* Srv)
{
	Srv->Log(DEFAULT,"SQL: Loading database settings");
	Connections.clear();
	Srv->Log(DEBUG,"Cleared connections");
	for (int j =0; j < ThisConf->Enumerate("database"); j++)
	{
		std::string db = ThisConf->ReadValue("database","name",j);
		std::string user = ThisConf->ReadValue("database","username",j);
		std::string pass = ThisConf->ReadValue("database","password",j);
		std::string host = ThisConf->ReadValue("database","hostname",j);
		std::string id = ThisConf->ReadValue("database","id",j);
		Srv->Log(DEBUG,"Read database settings");
		if ((db != "") && (host != "") && (user != "") && (id != "") && (pass != ""))
		{
			SQLConnection* ThisSQL = new SQLConnection(host,user,pass,db,atoi(id.c_str()));
			Srv->Log(DEFAULT,"Loaded database: "+ThisSQL->GetHost());
			Connections[id] = ThisSQL;
			Srv->Log(DEBUG,"Pushed back connection");
		}
	}
	ConnectDatabases(Srv);
}

void* DispatcherThread(void* arg);

class ModuleSQL : public Module
{
 public:
	Server *Srv;
	ConfigReader *Conf;
	pthread_t Dispatcher;
	int currid;

	void Implements(char* List)
	{
		List[I_OnRehash] = List[I_OnRequest] = 1;
	}

        unsigned long NewID()
	{
		if (currid+1 == 0)
			currid++;
		return ++currid;
	}

	char* OnRequest(Request* request)
	{
		if(strcmp(SQLREQID, request->GetData()) == 0)
		{
			SQLrequest* req = (SQLrequest*)request;

			/* XXX: Lock */
			pthread_mutex_lock(&queue_mutex);

			ConnMap::iterator iter;

			char* returnval = NULL;

			log(DEBUG, "Got query: '%s' with %d replacement parameters on id '%s'", req->query.q.c_str(), req->query.p.size(), req->dbid.c_str());

			if((iter = Connections.find(req->dbid)) != Connections.end())
			{
				iter->second->queue.push(*req);
				req->id = NewID();
				returnval = SQLSUCCESS;
			}
			else
			{
				req->error.Id(BAD_DBID);
			}

			pthread_mutex_unlock(&queue_mutex);
			/* XXX: Unlock */

			return returnval;
		}

		log(DEBUG, "Got unsupported API version string: %s", request->GetData());

		return NULL;
	}

	ModuleSQL(Server* Me)
		: Module::Module(Me)
	{
		Srv = Me;
		Conf = new ConfigReader();
		currid = 0;
		pthread_attr_t attribs;
		pthread_attr_init(&attribs);
		pthread_attr_setdetachstate(&attribs, PTHREAD_CREATE_DETACHED);
		if (pthread_create(&this->Dispatcher, &attribs, DispatcherThread, (void *)this) != 0)
		{
			throw ModuleException("m_mysql: Failed to create dispatcher thread: " + std::string(strerror(errno)));
		}
		Srv->PublishFeature("SQL", this);
		Srv->PublishFeature("MySQL", this);
	}
	
	virtual ~ModuleSQL()
	{
		DELETE(Conf);
	}
	
	virtual void OnRehash(const std::string &parameter)
	{
		/* TODO: set rehash bool here, which makes the dispatcher thread rehash at next opportunity */
	}
	
	virtual Version GetVersion()
	{
		return Version(1,1,0,0,VF_VENDOR|VF_SERVICEPROVIDER);
	}
	
};

void* DispatcherThread(void* arg)
{
	ModuleSQL* thismodule = (ModuleSQL*)arg;
	LoadDatabases(thismodule->Conf, thismodule->Srv);

	while (!giveup)
	{
		SQLConnection* conn = NULL;
		/* XXX: Lock here for safety */
		pthread_mutex_lock(&queue_mutex);
		for (ConnMap::iterator i = Connections.begin(); i != Connections.end(); i++)
		{
			if (i->second->queue.totalsize())
			{
				conn = i->second;
				break;
			}
		}
		pthread_mutex_unlock(&queue_mutex);
		/* XXX: Unlock */

		/* Theres an item! */
		if (conn)
		{
			conn->DoLeadingQuery();

			/* XXX: Lock */
			pthread_mutex_lock(&queue_mutex);
			conn->queue.pop();
			pthread_mutex_unlock(&queue_mutex);
			/* XXX: Unlock */
		}

		usleep(50);
	}

	return NULL;
}


// stuff down here is the module-factory stuff. For basic modules you can ignore this.

class ModuleSQLFactory : public ModuleFactory
{
 public:
	ModuleSQLFactory()
	{
	}
	
	~ModuleSQLFactory()
	{
	}
	
	virtual Module * CreateModule(Server* Me)
	{
		return new ModuleSQL(Me);
	}
	
};


extern "C" void * init_module( void )
{
	return new ModuleSQLFactory;
}

