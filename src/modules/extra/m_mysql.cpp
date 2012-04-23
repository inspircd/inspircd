/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
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
#include "m_sqlv2.h"

#ifdef WINDOWS
#pragma comment(lib, "mysqlclient.lib")
#endif

/* VERSION 2 API: With nonblocking (threaded) requests */

/* $ModDesc: SQL Service Provider module for all other m_sql* modules */
/* $CompileFlags: exec("mysql_config --include") */
/* $LinkerFlags: exec("mysql_config --libs_r") rpath("mysql_config --libs_r") */
/* $ModDep: m_sqlv2.h */

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
 *
 * For a diagram of this system please see http://wiki.inspircd.org/Mysql2
 */


class SQLConnection;
class DispatcherThread;

typedef std::map<std::string, SQLConnection*> ConnMap;
typedef std::deque<SQLresult*> ResultQueue;

unsigned long count(const char * const str, char a)
{
	unsigned long n = 0;
	for (const char *p = reinterpret_cast<const char *>(str); *p; ++p)
	{
		if (*p == '?')
			++n;
	}
	return n;
}


/** MySQL module
 *  */
class ModuleSQL : public Module
{
 public:

	 ConfigReader *Conf;
	 InspIRCd* PublicServerInstance;
	 int currid;
	 bool rehashing;
	 DispatcherThread* Dispatcher;
	 Mutex ResultsMutex;
	 Mutex LoggingMutex;
	 Mutex ConnMutex;

	 ModuleSQL(InspIRCd* Me);
	 ~ModuleSQL();
	 unsigned long NewID();
	 const char* OnRequest(Request* request);
	 void OnRehash(User* user);
	 Version GetVersion();
};


#if !defined(MYSQL_VERSION_ID) || MYSQL_VERSION_ID<32224
#define mysql_field_count mysql_num_fields
#endif

/** Represents a mysql result set
 */
class MySQLresult : public SQLresult
{
	int currentrow;
	std::vector<std::string> colnames;
	std::vector<SQLfieldList> fieldlists;
	SQLfieldMap* fieldmap;
	SQLfieldMap fieldmap2;
	SQLfieldList emptyfieldlist;
	int rows;
 public:

	MySQLresult(Module* self, Module* to, MYSQL_RES* res, int affected_rows, unsigned int rid) : SQLresult(self, to, rid), currentrow(0), fieldmap(NULL)
	{
		/* A number of affected rows from from mysql_affected_rows.
		 */
		fieldlists.clear();
		rows = 0;
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
						std::string b = (row[field_count] ? row[field_count] : "");
						SQLfield sqlf(b, !row[field_count]);
						colnames.push_back(a);
						fieldlists[n].push_back(sqlf);
						field_count++;
					}
					n++;
				}
				rows++;
			}
			mysql_free_result(res);
			res = NULL;
		}
	}

	MySQLresult(Module* self, Module* to, SQLerror e, unsigned int rid) : SQLresult(self, to, rid), currentrow(0)
	{
		rows = 0;
		error = e;
	}

	~MySQLresult()
	{
	}

	virtual int Rows()
	{
		return rows;
	}

	virtual int Cols()
	{
		return colnames.size();
	}

	virtual std::string ColName(int column)
	{
		if (column < (int)colnames.size())
		{
			return colnames[column];
		}
		else
		{
			throw SQLbadColName();
		}
		return "";
	}

	virtual int ColNum(const std::string &column)
	{
		for (unsigned int i = 0; i < colnames.size(); i++)
		{
			if (column == colnames[i])
				return i;
		}
		throw SQLbadColName();
		return 0;
	}

	virtual SQLfield GetValue(int row, int column)
	{
		if ((row >= 0) && (row < rows) && (column >= 0) && (column < Cols()))
		{
			return fieldlists[row][column];
		}

		throw SQLbadColName();

		/* XXX: We never actually get here because of the throw */
		return SQLfield("",true);
	}

	virtual SQLfieldList& GetRow()
	{
		if (currentrow < rows)
			return fieldlists[currentrow++];
		else
			return emptyfieldlist;
	}

	virtual SQLfieldMap& GetRowMap()
	{
		fieldmap2.clear();

		if (currentrow < rows)
		{
			for (int i = 0; i < Cols(); i++)
			{
				fieldmap2.insert(std::make_pair(colnames[i],GetValue(currentrow, i)));
			}
			currentrow++;
		}

		return fieldmap2;
	}

	virtual SQLfieldList* GetRowPtr()
	{
		SQLfieldList* fieldlist = new SQLfieldList();

		if (currentrow < rows)
		{
			for (int i = 0; i < Rows(); i++)
			{
				fieldlist->push_back(fieldlists[currentrow][i]);
			}
			currentrow++;
		}
		return fieldlist;
	}

	virtual SQLfieldMap* GetRowMapPtr()
	{
		fieldmap = new SQLfieldMap();

		if (currentrow < rows)
		{
			for (int i = 0; i < Cols(); i++)
			{
				fieldmap->insert(std::make_pair(colnames[i],GetValue(currentrow, i)));
			}
			currentrow++;
		}

		return fieldmap;
	}

	virtual void Free(SQLfieldMap* fm)
	{
		delete fm;
	}

	virtual void Free(SQLfieldList* fl)
	{
		delete fl;
	}
};

/** Represents a connection to a mysql database
 */
class SQLConnection : public classbase
{
 protected:
	MYSQL *connection;
	MYSQL_RES *res;
	MYSQL_ROW *row;
	SQLhost host;
	std::map<std::string,std::string> thisrow;
	bool Enabled;
	ModuleSQL* Parent;

 public:

	QueryQueue queue;
	ResultQueue rq;

	// This constructor creates an SQLConnection object with the given credentials, but does not connect yet.
	SQLConnection(const SQLhost &hi, ModuleSQL* Creator) : connection(NULL), host(hi), Enabled(false), Parent(Creator)
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
		return mysql_real_connect(connection, host.host.c_str(), host.user.c_str(), host.pass.c_str(), host.name.c_str(), host.port, NULL, 0);
	}

	void DoLeadingQuery()
	{
		if (!CheckConnection())
			return;

		/* Parse the command string and dispatch it to mysql */
		SQLrequest& req = queue.front();

		/* Pointer to the buffer we screw around with substitution in */
		char* query;

		/* Pointer to the current end of query, where we append new stuff */
		char* queryend;

		/* Total length of the unescaped parameters */
		unsigned long maxparamlen, paramcount;

		/* The length of the longest parameter */
		maxparamlen = 0;

		for(ParamL::iterator i = req.query.p.begin(); i != req.query.p.end(); i++)
		{
			if (i->size() > maxparamlen)
				maxparamlen = i->size();
		}

		/* How many params are there in the query? */
		paramcount = count(req.query.q.c_str(), '?');

		/* This stores copy of params to be inserted with using numbered params 1;3B*/
		ParamL paramscopy(req.query.p);

		/* To avoid a lot of allocations, allocate enough memory for the biggest the escaped query could possibly be.
		 * sizeofquery + (maxtotalparamlength*2) + 1
		 *
		 * The +1 is for null-terminating the string for mysql_real_escape_string
		 */

		query = new char[req.query.q.length() + (maxparamlen*paramcount*2) + 1];
		queryend = query;

		/* Okay, now we have a buffer large enough we need to start copying the query into it and escaping and substituting
		 * the parameters into it...
		 */

		for(unsigned long i = 0; i < req.query.q.length(); i++)
		{
			if(req.query.q[i] == '?')
			{
				/* We found a place to substitute..what fun.
				 * use mysql calls to escape and write the
				 * escaped string onto the end of our query buffer,
				 * then we "just" need to make sure queryend is
				 * pointing at the right place.
				 */

				/* Is it numbered parameter?
				 */

				bool numbered;
				numbered = false;

				/* Numbered parameter number :|
				 */
				unsigned int paramnum;
				paramnum = 0;

				/* Let's check if it's a numbered param. And also calculate it's number.
				 */

				while ((i < req.query.q.length() - 1) && (req.query.q[i+1] >= '0') && (req.query.q[i+1] <= '9'))
				{
					numbered = true;
					++i;
					paramnum = paramnum * 10 + req.query.q[i] - '0';
				}

				if (paramnum > paramscopy.size() - 1)
				{
					/* index is out of range!
					 */
					numbered = false;
				}

				if (numbered)
				{
					unsigned long len = mysql_real_escape_string(connection, queryend, paramscopy[paramnum].c_str(), paramscopy[paramnum].length());

					queryend += len;
				}
				else if (req.query.p.size())
				{
					unsigned long len = mysql_real_escape_string(connection, queryend, req.query.p.front().c_str(), req.query.p.front().length());

					queryend += len;
					req.query.p.pop_front();
				}
				else
					break;
			}
			else
			{
				*queryend = req.query.q[i];
				queryend++;
			}
		}

		*queryend = 0;

		req.query.q = query;

		if (!mysql_real_query(connection, req.query.q.data(), req.query.q.length()))
		{
			/* Successfull query */
			res = mysql_use_result(connection);
			unsigned long rows = mysql_affected_rows(connection);
			MySQLresult* r = new MySQLresult(Parent, req.GetSource(), res, rows, req.id);
			r->dbid = this->GetID();
			r->query = req.query.q;
			/* Put this new result onto the results queue.
			 * XXX: Remember to mutex the queue!
			 */
			Parent->ResultsMutex.Lock();
			rq.push_back(r);
			Parent->ResultsMutex.Unlock();
		}
		else
		{
			/* XXX: See /usr/include/mysql/mysqld_error.h for a list of
			 * possible error numbers and error messages */
			SQLerror e(SQL_QREPLY_FAIL, ConvToStr(mysql_errno(connection)) + std::string(": ") + mysql_error(connection));
			MySQLresult* r = new MySQLresult(Parent, req.GetSource(), e, req.id);
			r->dbid = this->GetID();
			r->query = req.query.q;

			Parent->ResultsMutex.Lock();
			rq.push_back(r);
			Parent->ResultsMutex.Unlock();
		}

		delete[] query;
	}

	bool ConnectionLost()
	{
		if (&connection)
		{
			return (mysql_ping(connection) != 0);
		}
		else return false;
	}

	bool CheckConnection()
	{
		if (ConnectionLost())
		{
			return Connect();
		}
		else return true;
	}

	std::string GetError()
	{
		return mysql_error(connection);
	}

	const std::string& GetID()
	{
		return host.id;
	}

	std::string GetHost()
	{
		return host.host;
	}

	void SetEnable(bool Enable)
	{
		Enabled = Enable;
	}

	bool IsEnabled()
	{
		return Enabled;
	}

	void Close()
	{
		mysql_close(connection);
	}

	const SQLhost& GetConfHost()
	{
		return host;
	}

};

ConnMap Connections;

bool HasHost(const SQLhost &host)
{
	for (ConnMap::iterator iter = Connections.begin(); iter != Connections.end(); iter++)
	{
		if (host == iter->second->GetConfHost())
			return true;
	}
	return false;
}

bool HostInConf(ConfigReader* conf, const SQLhost &h)
{
	for(int i = 0; i < conf->Enumerate("database"); i++)
	{
		SQLhost host;
		host.id		= conf->ReadValue("database", "id", i);
		host.host	= conf->ReadValue("database", "hostname", i);
		host.port	= conf->ReadInteger("database", "port", i, true);
		host.name	= conf->ReadValue("database", "name", i);
		host.user	= conf->ReadValue("database", "username", i);
		host.pass	= conf->ReadValue("database", "password", i);
		host.ssl	= conf->ReadFlag("database", "ssl", i);
		if (h == host)
			return true;
	}
	return false;
}

void ClearOldConnections(ConfigReader* conf)
{
	ConnMap::iterator i,safei;
	for (i = Connections.begin(); i != Connections.end(); i++)
	{
		if (!HostInConf(conf, i->second->GetConfHost()))
		{
			delete i->second;
			safei = i;
			--i;
			Connections.erase(safei);
		}
	}
}

void ClearAllConnections()
{
	ConnMap::iterator i;
	while ((i = Connections.begin()) != Connections.end())
	{
		Connections.erase(i);
		delete i->second;
	}
}

void ConnectDatabases(InspIRCd* ServerInstance, ModuleSQL* Parent)
{
	for (ConnMap::iterator i = Connections.begin(); i != Connections.end(); i++)
	{
		if (i->second->IsEnabled())
			continue;

		i->second->SetEnable(true);
		if (!i->second->Connect())
		{
			/* XXX: MUTEX */
			Parent->LoggingMutex.Lock();
			ServerInstance->Logs->Log("m_mysql",DEFAULT,"SQL: Failed to connect database "+i->second->GetHost()+": Error: "+i->second->GetError());
			i->second->SetEnable(false);
			Parent->LoggingMutex.Unlock();
		}
	}
}

void LoadDatabases(ConfigReader* conf, InspIRCd* ServerInstance, ModuleSQL* Parent)
{
	Parent->ConnMutex.Lock();
	ClearOldConnections(conf);
	for (int j =0; j < conf->Enumerate("database"); j++)
	{
		SQLhost host;
		host.id		= conf->ReadValue("database", "id", j);
		host.host	= conf->ReadValue("database", "hostname", j);
		host.port	= conf->ReadInteger("database", "port", j, true);
		host.name	= conf->ReadValue("database", "name", j);
		host.user	= conf->ReadValue("database", "username", j);
		host.pass	= conf->ReadValue("database", "password", j);
		host.ssl	= conf->ReadFlag("database", "ssl", j);

		if (HasHost(host))
			continue;

		if (!host.id.empty() && !host.host.empty() && !host.name.empty() && !host.user.empty() && !host.pass.empty())
		{
			SQLConnection* ThisSQL = new SQLConnection(host, Parent);
			Connections[host.id] = ThisSQL;
		}
	}
	ConnectDatabases(ServerInstance, Parent);
	Parent->ConnMutex.Unlock();
}

char FindCharId(const std::string &id)
{
	char i = 1;
	for (ConnMap::iterator iter = Connections.begin(); iter != Connections.end(); ++iter, ++i)
	{
		if (iter->first == id)
		{
			return i;
		}
	}
	return 0;
}

ConnMap::iterator GetCharId(char id)
{
	char i = 1;
	for (ConnMap::iterator iter = Connections.begin(); iter != Connections.end(); ++iter, ++i)
	{
		if (i == id)
			return iter;
	}
	return Connections.end();
}

class ModuleSQL;

class DispatcherThread : public SocketThread
{
 private:
	ModuleSQL* Parent;
	InspIRCd* ServerInstance;
 public:
	DispatcherThread(InspIRCd* Instance, ModuleSQL* CreatorModule) : SocketThread(Instance), Parent(CreatorModule), ServerInstance(Instance) { }
	~DispatcherThread() { }
	virtual void Run();
	virtual void OnNotify();
};

ModuleSQL::ModuleSQL(InspIRCd* Me) : Module(Me), rehashing(false)
{
	ServerInstance->Modules->UseInterface("SQLutils");

	Conf = new ConfigReader(ServerInstance);
	PublicServerInstance = ServerInstance;
	currid = 0;

	Dispatcher = new DispatcherThread(ServerInstance, this);
	ServerInstance->Threads->Start(Dispatcher);

	if (!ServerInstance->Modules->PublishFeature("SQL", this))
	{
		Dispatcher->join();
		delete Dispatcher;
		ServerInstance->Modules->DoneWithInterface("SQLutils");
		throw ModuleException("m_mysql: Unable to publish feature 'SQL'");
	}

	ServerInstance->Modules->PublishInterface("SQL", this);
	Implementation eventlist[] = { I_OnRehash, I_OnRequest };
	ServerInstance->Modules->Attach(eventlist, this, 2);
}

ModuleSQL::~ModuleSQL()
{
	delete Dispatcher;
	ClearAllConnections();
	delete Conf;
	ServerInstance->Modules->UnpublishInterface("SQL", this);
	ServerInstance->Modules->UnpublishFeature("SQL");
	ServerInstance->Modules->DoneWithInterface("SQLutils");
}

unsigned long ModuleSQL::NewID()
{
	if (currid+1 == 0)
		currid++;
	return ++currid;
}

const char* ModuleSQL::OnRequest(Request* request)
{
	if(strcmp(SQLREQID, request->GetId()) == 0)
	{
		SQLrequest* req = (SQLrequest*)request;

		ConnMap::iterator iter;

		const char* returnval = NULL;

		Dispatcher->LockQueue();
		ConnMutex.Lock();
		if((iter = Connections.find(req->dbid)) != Connections.end())
		{
			req->id = NewID();
			iter->second->queue.push(*req);
			returnval = SQLSUCCESS;
		}
		else
		{
			req->error.Id(SQL_BAD_DBID);
		}

		ConnMutex.Unlock();
		Dispatcher->UnlockQueueWakeup();
		/* Yes, it's possible this will generate a spurious wakeup.
		 * That's fine, it'll just get ignored.
		 */

		return returnval;
	}

	return NULL;
}

void ModuleSQL::OnRehash(User* user)
{
	Dispatcher->LockQueue();
	rehashing = true;
	Dispatcher->UnlockQueueWakeup();
}

Version ModuleSQL::GetVersion()
{
	return Version("$Id$", VF_VENDOR | VF_SERVICEPROVIDER, API_VERSION);
}

void DispatcherThread::Run()
{
	LoadDatabases(Parent->Conf, Parent->PublicServerInstance, Parent);

	SQLConnection* conn = NULL;

	this->LockQueue();
	while (!this->GetExitFlag())
	{
		if (Parent->rehashing)
		{
			Parent->rehashing = false;
			LoadDatabases(Parent->Conf, Parent->PublicServerInstance, Parent);
		}

		conn = NULL;
		Parent->ConnMutex.Lock();
		for (ConnMap::iterator i = Connections.begin(); i != Connections.end(); i++)
		{
			if (i->second->queue.totalsize())
			{
				conn = i->second;
				break;
			}
		}
		Parent->ConnMutex.Unlock();

		if (conn)
		{
			/* There's an item! */
			this->UnlockQueue();
			conn->DoLeadingQuery();
			this->NotifyParent();
			this->LockQueue();
			conn->queue.pop();
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
	SQLConnection* conn;
	while (1)
	{
		conn = NULL;
		Parent->ConnMutex.Lock();
		for (ConnMap::iterator iter = Connections.begin(); iter != Connections.end(); iter++)
		{
			if (!iter->second->rq.empty())
			{
				conn = iter->second;
				break;
			}
		}
		Parent->ConnMutex.Unlock();

		if (!conn)
			break;

		Parent->ResultsMutex.Lock();
		ResultQueue::iterator n = conn->rq.begin();
		Parent->ResultsMutex.Unlock();

		(*n)->Send();
		delete (*n);

		Parent->ResultsMutex.Lock();
		conn->rq.pop_front();
		Parent->ResultsMutex.Unlock();
	}
}

MODULE_INIT(ModuleSQL)
