/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd is copyright (C) 2002-2004 ChatSpike-Dev.
 *                       E-mail:
 *                <brain@chatspike.net>
 *           	  <Craig@chatspike.net>
 *     
 * Written by Craig Edwards, Craig McLure, and others.
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

using namespace std;

#include <stdio.h>
#include <string>
#include <mysql.h>
#include "users.h"
#include "channels.h"
#include "modules.h"
#include "helperfuncs.h"
#include "m_sql.h"

/* $ModDesc: SQL Service Provider module for all other m_sql* modules */
/* $CompileFlags: -I/usr/local/include/mysql -I/usr/include/mysql -I/usr/local/include -I/usr/include */
/* $LinkerFlags: -L/usr/local/lib/mysql -Wl,--rpath -Wl,/usr/local/lib/mysql -L/usr/lib/mysql -Wl,--rpath -Wl,/usr/lib/mysql -lmysqlclient */

/** SQLConnection represents one mysql session.
 * Each session has its own persistent connection to the database.
 */

#if !defined(MYSQL_VERSION_ID) || MYSQL_VERSION_ID<32224
#define mysql_field_count mysql_num_fields
#endif

class SQLConnection
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
		unsigned int timeout = 1;
		mysql_init(&connection);
		mysql_options(&connection,MYSQL_OPT_CONNECT_TIMEOUT,(char*)&timeout);
	}

	// This method connects to the database using the credentials supplied to the constructor, and returns
	// true upon success.
	bool Connect()
	{
		return mysql_real_connect(&connection, host.c_str(), user.c_str(), pass.c_str(), db.c_str(), 0, NULL, 0);
	}

	// This method issues a query that expects multiple rows of results. Use GetRow() and QueryDone() to retrieve
	// multiple rows.
	bool QueryResult(std::string query)
	{
		int r = mysql_query(&connection, query.c_str());
		if (!r)
		{
			res = mysql_use_result(&connection);
		}
		return (!r);
	}

	// This method issues a query that just expects a number of 'effected' rows (e.g. UPDATE or DELETE FROM).
	// the number of effected rows is returned in the return value.
	unsigned long QueryCount(std::string query)
	{
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
			return true;
		}
		else return false;
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

typedef std::vector<SQLConnection> ConnectionList;

class ModuleSQL : public Module
{
	Server *Srv;
	ConfigReader *Conf;
	ConnectionList Connections;
 
 public:
	void ConnectDatabases()
	{
		for (ConnectionList::iterator i = Connections.begin(); i != Connections.end(); i++)
		{
			i->Enable();
			if (i->Connect())
			{
				Srv->Log(DEFAULT,"SQL: Successfully connected database "+i->GetHost());
			}
			else
			{
				Srv->Log(DEFAULT,"SQL: Failed to connect database "+i->GetHost()+": Error: "+i->GetError());
				i->Disable();
			}
		}
	}

	void LoadDatabases(ConfigReader* ThisConf)
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
				SQLConnection ThisSQL(host,user,pass,db,atoi(id.c_str()));
				Srv->Log(DEFAULT,"Loaded database: "+ThisSQL.GetHost());
				Connections.push_back(ThisSQL);
				Srv->Log(DEBUG,"Pushed back connection");
			}
		}
		ConnectDatabases();
	}

	void ResultType(SQLRequest *r, SQLResult *res)
	{
		for (ConnectionList::iterator i = Connections.begin(); i != Connections.end(); i++)
		{
			if ((i->GetID() == r->GetConnID()) && (i->IsEnabled()))
			{
				bool xr = i->QueryResult(r->GetQuery());
				if (!xr)
				{
					res->SetType(SQL_ERROR);
					res->SetError(i->GetError());
					return;
				}
				res->SetType(SQL_OK);
				return;
			}
		}
	}

	void CountType(SQLRequest *r, SQLResult* res)
	{
		for (ConnectionList::iterator i = Connections.begin(); i != Connections.end(); i++)
		{
			if ((i->GetID() == r->GetConnID()) && (i->IsEnabled()))
			{
				res->SetType(SQL_COUNT);
				res->SetCount(i->QueryCount(r->GetQuery()));
				return;
			}
		}
	}

	void DoneType(SQLRequest *r, SQLResult* res)
	{
		for (ConnectionList::iterator i = Connections.begin(); i != Connections.end(); i++)
		{
			if ((i->GetID() == r->GetConnID()) && (i->IsEnabled()))
			{
				res->SetType(SQL_DONE);
				if (!i->QueryDone())
					res->SetType(SQL_ERROR);
			}
		}
	}

	void RowType(SQLRequest *r, SQLResult* res)
	{
		for (ConnectionList::iterator i = Connections.begin(); i != Connections.end(); i++)
		{
			if ((i->GetID() == r->GetConnID()) && (i->IsEnabled()))
			{
				log(DEBUG,"*** FOUND MATCHING ROW");
				std::map<std::string,std::string> row = i->GetRow();
				res->SetRow(row);
				res->SetType(SQL_ROW);
				if (!row.size())
				{
					log(DEBUG,"ROW SIZE IS 0");
					res->SetType(SQL_END);
				}
				return;
			}
		}
	}

	void Implements(char* List)
	{
		List[I_OnRehash] = List[I_OnRequest] = 1;
	}

	char* OnRequest(Request* request)
	{
		if (request)
		{
			SQLResult* Result = new SQLResult();
			SQLRequest *r = (SQLRequest*)request->GetData();
			switch (r->GetQueryType())
			{
				case SQL_RESULT:
					ResultType(r,Result);
				break;
				case SQL_COUNT:
					CountType(r,Result);
				break;
				case SQL_ROW:
					RowType(r,Result);
				break;
				case SQL_DONE:
					DoneType(r,Result);
				break;
			}
			return (char*)Result;
		}
		return NULL;
	}

	ModuleSQL(Server* Me)
		: Module::Module(Me)
	{
		Srv = Me;
		Conf = new ConfigReader();
		LoadDatabases(Conf);
	}
	
	virtual ~ModuleSQL()
	{
		Connections.clear();
		delete Conf;
	}
	
	virtual void OnRehash(const std::string &parameter)
	{
		delete Conf;
		Conf = new ConfigReader();
		LoadDatabases(Conf);
	}
	
	virtual Version GetVersion()
	{
		return Version(1,0,0,0,VF_VENDOR|VF_SERVICEPROVIDER);
	}
	
};

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

