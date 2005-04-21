/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  Inspire is copyright (C) 2002-2004 ChatSpike-Dev.
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


#include <stdio.h>
#include <string>
#include <mysql/mysql.h>
#include "users.h"
#include "channels.h"
#include "modules.h"
#include "m_sql.h"

/* $ModDesc: m_filter with regexps */
/* $CompileFlags: -I/usr/local/include -I/usr/include -L/usr/local/lib/mysql -L/usr/lib/mysql -lmysqlclient */

/** SQLConnection represents one mysql session.
 * Each session has its own persistent connection to the database.
 */
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
		mysql_init(&connection);
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
		char escaped_query[query.length()+1];
		mysql_real_escape_string(&connection, escaped_query, query.c_str(), query.length());
		int r = mysql_query(&connection, escaped_query);
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
		char escaped_query[query.length()+1];
		mysql_real_escape_string(&connection, escaped_query, query.c_str(), query.length());
	        int r = mysql_query(&connection, escaped_query);
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
				if(mysql_field_count(&connection) == 0)
					return thisrow;
				MYSQL_FIELD *fields = mysql_fetch_fields(res);
				while (field_count < mysql_field_count(&connection))
				{
					thisrow[std::string(fields[field_count].name)] = std::string(row[field_count]);
					field_count++;
				}
				return thisrow;
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

	~SQLConnection()
	{
		mysql_close(&connection);
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

	void LoadDatabases(ConfigReader* Conf)
	{
		Srv->Log(DEFAULT,"SQL: Loading database settings");
		Connections.clear();
		for (int j =0; j < Conf->Enumerate("database"); j++)
		{
			std::string db = Conf->ReadValue("database","name",j);
			std::string user = Conf->ReadValue("database","username",j);
			std::string pass = Conf->ReadValue("database","password",j);
			std::string host = Conf->ReadValue("database","hostname",j);
			std::string id = Conf->ReadValue("database","id",j);
			if ((db != "") && (host != "") && (user != "") && (id != "") && (pass != ""))
			{
				SQLConnection ThisSQL(host,user,pass,db,atoi(id.c_str()));
				Srv->Log(DEFAULT,"Loaded database: "+ThisSQL.GetHost());
				Connections.push_back(ThisSQL);
			}
		}
		ConnectDatabases();
	}

	void ResultType(SQLRequest *r, SQLResult *res)
	{
		for (ConnectionList::iterator i = Connections.begin(); i != Connections.end(); i++)
		{
			if ((i->GetID() == r->GetConnID()) && (i->Enabled()))
			{
				bool xr = i->QueryResult(r->GetQuery());
				if (!xr)
				{
					res->SetType(SQL_ERROR);
					res->SetError(r->GetError());
					return;
				}
			}
		}
	}

	void CountType(SQLRequest *r, SQLResult* res)
	{
		for (ConnectionList::iterator i = Connections.begin(); i != Connections.end(); i++)
		{
			if ((i->GetID() == r->GetConnID()) && (i->Enabled()))
			{
				res->SetType(SQL_COUNT);
				res->SetCount(i->QueryCount(r->GetQuery()));
				return;
			}
		}
	}

	void RowType(SQLRequest *r, SQLResult* res)
	{
		for (ConnectionList::iterator i = Connections.begin(); i != Connections.end(); i++)
		{
			if ((i->GetID() == r->GetConnID()) && (i->Enabled()))
			{
				std::map<std::string,std::string> row = i->GetRow();
				res->SetRow(row);
				res->SetType(SQL_ROW);
				if (!row.size())
					res->SetType(SQL_END);
				return;
			}
		}
	}

	char* OnRequest(Request* request)
	{
		SQLResult Result = new SQLResult();
		SQLRequest *r = (SQLRequest*)request;
		switch (r->GetRequest())
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
		}
		return Result;
	}

	ModuleSQL()
	{
		Srv = new Server();
		Conf = new ConfigReader();
		LoadDatabases(Conf);
	}
	
	virtual ~ModuleSQL()
	{
		Connections.clear();
		delete Conf;
		delete Srv;
	}
	
	virtual void OnRehash()
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
	
	virtual Module * CreateModule()
	{
		return new ModuleSQL;
	}
	
};


extern "C" void * init_module( void )
{
	return new ModuleSQLFactory;
}

