/*		 +------------------------------------+
 *		 | Inspire Internet Relay Chat Daemon |
 *		 +------------------------------------+
 *
 *	InspIRCd: (C) 2002-2009 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *			  the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"
#include <sqlite3.h>
#include "m_sqlv2.h"

/* $ModDesc: sqlite3 provider */
/* $CompileFlags: pkgconfversion("sqlite3","3.3") pkgconfincludes("sqlite3","/sqlite3.h","") */
/* $LinkerFlags: pkgconflibs("sqlite3","/libsqlite3.so","-lsqlite3") */
/* $ModDep: m_sqlv2.h */
/* $NoPedantic */

class SQLConn;
class SQLite3Result;
class ResultNotifier;
class SQLiteListener;
class ModuleSQLite3;

typedef std::map<std::string, SQLConn*> ConnMap;
typedef std::deque<classbase*> paramlist;
typedef std::deque<SQLite3Result*> ResultQueue;

unsigned long count(const char * const str, char a)
{
	unsigned long n = 0;
	for (const char *p = str; *p; ++p)
	{
		if (*p == '?')
			++n;
	}
	return n;
}

ResultNotifier* notifier = NULL;
SQLiteListener* listener = NULL;
int QueueFD = -1;

class ResultNotifier : public BufferedSocket
{
	ModuleSQLite3* mod;

 public:
	ResultNotifier(ModuleSQLite3* m, int newfd, char* ip) : BufferedSocket(SI, newfd, ip), mod(m)
	{
	}

	virtual bool OnDataReady()
	{
		char data = 0;
		if (ServerInstance->SE->Recv(this, &data, 1, 0) > 0)
		{
			Dispatch();
			return true;
		}
		return false;
	}

	void Dispatch();
};

class SQLiteListener : public ListenSocketBase
{
	ModuleSQLite3* Parent;
	irc::sockets::insp_sockaddr sock_us;
	socklen_t uslen;
	FileReader* index;

 public:
	SQLiteListener(ModuleSQLite3* P, int port, const std::string &addr) : ListenSocketBase(port, addr), Parent(P)
	{
		uslen = sizeof(sock_us);
		if (getsockname(this->fd,(sockaddr*)&sock_us,&uslen))
		{
			throw ModuleException("Could not getsockname() to find out port number for ITC port");
		}
	}

	virtual void OnAcceptReady(const std::string &ipconnectedto, int nfd, const std::string &incomingip)
	{
		new ResultNotifier(this->Parent, this->ServerInstance, nfd, (char *)ipconnectedto.c_str()); // XXX unsafe casts suck
	}

	/* Using getsockname and ntohs, we can determine which port number we were allocated */
	int GetPort()
	{
#ifdef IPV6
		return ntohs(sock_us.sin6_port);
#else
		return ntohs(sock_us.sin_port);
#endif
	}
};

class SQLite3Result : public SQLresult
{
 private:
	int currentrow;
	int rows;
	int cols;

	std::vector<std::string> colnames;
	std::vector<SQLfieldList> fieldlists;
	SQLfieldList emptyfieldlist;

	SQLfieldList* fieldlist;
	SQLfieldMap* fieldmap;

 public:
	SQLite3Result(Module* self, Module* to, unsigned int rid)
	: SQLresult(self, to, rid), currentrow(0), rows(0), cols(0), fieldlist(NULL), fieldmap(NULL)
	{
	}

	~SQLite3Result()
	{
	}

	void AddRow(int colsnum, char **dat, char **colname)
	{
		colnames.clear();
		cols = colsnum;
		for (int i = 0; i < colsnum; i++)
		{
			fieldlists.resize(fieldlists.size()+1);
			colnames.push_back(colname[i]);
			SQLfield sf(dat[i] ? dat[i] : "", dat[i] ? false : true);
			fieldlists[rows].push_back(sf);
		}
		rows++;
	}

	void UpdateAffectedCount()
	{
		rows++;
	}

	virtual int Rows()
	{
		return rows;
	}

	virtual int Cols()
	{
		return cols;
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
			return fieldlists[currentrow];
		else
			return emptyfieldlist;
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

		if (currentrow < rows)
		{
			for (int i = 0; i < Cols(); i++)
			{
				fieldmap->insert(std::make_pair(ColName(i), GetValue(currentrow, i)));
			}
			currentrow++;
		}

		return *fieldmap;
	}

	virtual SQLfieldList* GetRowPtr()
	{
		fieldlist = new SQLfieldList();

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

class SQLConn : public classbase
{
 private:
	ResultQueue results;
	Module* mod;
	SQLhost host;
	sqlite3* conn;

 public:
	SQLConn(Module* m, const SQLhost& hi)
	: mod(m), host(hi)
	{
		if (OpenDB() != SQLITE_OK)
		{
			ServerInstance->Logs->Log("m_sqlite3",DEFAULT, "WARNING: Could not open DB with id: " + host.id);
			CloseDB();
		}
	}

	~SQLConn()
	{
		CloseDB();
	}

	SQLerror Query(SQLrequest &req)
	{
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
		 * The +1 is for null-terminating the string
		 */

		query = new char[req.query.q.length() + (maxparamlen*paramcount*2) + 1];
		queryend = query;

		for(unsigned long i = 0; i < req.query.q.length(); i++)
		{
			if(req.query.q[i] == '?')
			{
				/* We found a place to substitute..what fun.
				 * use sqlite calls to escape and write the
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
					char* escaped;
					escaped = sqlite3_mprintf("%q", paramscopy[paramnum].c_str());
					for (char* n = escaped; *n; n++)
					{
						*queryend = *n;
						queryend++;
					}
					sqlite3_free(escaped);
				}
				else if (req.query.p.size())
				{
					char* escaped;
					escaped = sqlite3_mprintf("%q", req.query.p.front().c_str());
					for (char* n = escaped; *n; n++)
					{
						*queryend = *n;
						queryend++;
					}
					sqlite3_free(escaped);
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

		SQLite3Result* res = new SQLite3Result(mod, req.GetSource(), req.id);
		res->dbid = host.id;
		res->query = req.query.q;
		paramlist params;
		params.push_back(this);
		params.push_back(res);

		char *errmsg = 0;
		sqlite3_update_hook(conn, QueryUpdateHook, &params);
		if (sqlite3_exec(conn, req.query.q.data(), QueryResult, &params, &errmsg) != SQLITE_OK)
		{
			std::string error(errmsg);
			sqlite3_free(errmsg);
			delete[] query;
			delete res;
			return SQLerror(SQL_QSEND_FAIL, error);
		}
		delete[] query;

		results.push_back(res);
		SendNotify();
		return SQLerror();
	}

	static int QueryResult(void *params, int argc, char **argv, char **azColName)
	{
		paramlist* p = (paramlist*)params;
		((SQLConn*)(*p)[0])->ResultReady(((SQLite3Result*)(*p)[1]), argc, argv, azColName);
		return 0;
	}

	static void QueryUpdateHook(void *params, int eventid, char const * azSQLite, char const * azColName, sqlite_int64 rowid)
	{
		paramlist* p = (paramlist*)params;
		((SQLConn*)(*p)[0])->AffectedReady(((SQLite3Result*)(*p)[1]));
	}

	void ResultReady(SQLite3Result *res, int cols, char **data, char **colnames)
	{
		res->AddRow(cols, data, colnames);
	}

	void AffectedReady(SQLite3Result *res)
	{
		res->UpdateAffectedCount();
	}

	int OpenDB()
	{
		return sqlite3_open_v2(host.host.c_str(), &conn, SQLITE_OPEN_READWRITE, 0);
	}

	void CloseDB()
	{
		sqlite3_interrupt(conn);
		sqlite3_close(conn);
	}

	SQLhost GetConfHost()
	{
		return host;
	}

	void SendResults()
	{
		while (results.size())
		{
			SQLite3Result* res = results[0];
			if (res->GetDest())
			{
				res->Send();
			}
			else
			{
				/* If the client module is unloaded partway through a query then the provider will set
				 * the pointer to NULL. We cannot just cancel the query as the result will still come
				 * through at some point...and it could get messy if we play with invalid pointers...
				 */
				delete res;
			}
			results.pop_front();
		}
	}

	void ClearResults()
	{
		while (results.size())
		{
			SQLite3Result* res = results[0];
			delete res;
			results.pop_front();
		}
	}

	void SendNotify()
	{
		if (QueueFD < 0)
		{
			if ((QueueFD = socket(AF_FAMILY, SOCK_STREAM, 0)) == -1)
			{
				/* crap, we're out of sockets... */
				return;
			}

			irc::sockets::insp_sockaddr addr;

#ifdef IPV6
			irc::sockets::insp_aton("::1", &addr.sin6_addr);
			addr.sin6_family = AF_FAMILY;
			addr.sin6_port = htons(listener->GetPort());
#else
			irc::sockets::insp_inaddr ia;
			irc::sockets::insp_aton("127.0.0.1", &ia);
			addr.sin_family = AF_FAMILY;
			addr.sin_addr = ia;
			addr.sin_port = htons(listener->GetPort());
#endif

			if (connect(QueueFD, (sockaddr*)&addr,sizeof(addr)) == -1)
			{
				/* wtf, we cant connect to it, but we just created it! */
				return;
			}
		}
		char id = 0;
		send(QueueFD, &id, 1, 0);
	}

};


class ModuleSQLite3 : public Module
{
 private:
	ConnMap connections;
	unsigned long currid;

 public:
	ModuleSQLite3()
	: currid(0)
	{
		ServerInstance->Modules->UseInterface("SQLutils");

		if (!ServerInstance->Modules->PublishFeature("SQL", this))
		{
			throw ModuleException("m_sqlite3: Unable to publish feature 'SQL'");
		}

		/* Create a socket on a random port. Let the tcp stack allocate us an available port */
#ifdef IPV6
		listener = new SQLiteListener(this, ServerInstance, 0, "::1");
#else
		listener = new SQLiteListener(this, ServerInstance, 0, "127.0.0.1");
#endif

		if (listener->GetFd() == -1)
		{
			ServerInstance->Modules->DoneWithInterface("SQLutils");
			throw ModuleException("m_sqlite3: unable to create ITC pipe");
		}
		else
		{
			ServerInstance->Logs->Log("m_sqlite3", DEBUG, "SQLite: Interthread comms port is %d", listener->GetPort());
		}

		ReadConf();

		ServerInstance->Modules->PublishInterface("SQL", this);
		Implementation eventlist[] = { I_OnRequest, I_OnRehash };
		ServerInstance->Modules->Attach(eventlist, this, 2);
	}

	virtual ~ModuleSQLite3()
	{
		ClearQueue();
		ClearAllConnections();

		ServerInstance->SE->DelFd(listener);
		ServerInstance->BufferedSocketCull();

		if (QueueFD >= 0)
		{
			shutdown(QueueFD, 2);
			close(QueueFD);
		}

		if (notifier)
		{
			ServerInstance->SE->DelFd(notifier);
			notifier->Close();
			ServerInstance->BufferedSocketCull();
		}

		ServerInstance->Modules->UnpublishInterface("SQL", this);
		ServerInstance->Modules->UnpublishFeature("SQL");
		ServerInstance->Modules->DoneWithInterface("SQLutils");
	}


	void SendQueue()
	{
		for (ConnMap::iterator iter = connections.begin(); iter != connections.end(); iter++)
		{
			iter->second->SendResults();
		}
	}

	void ClearQueue()
	{
		for (ConnMap::iterator iter = connections.begin(); iter != connections.end(); iter++)
		{
			iter->second->ClearResults();
		}
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
		ConfigReader conf;
		for(int i = 0; i < conf.Enumerate("database"); i++)
		{
			SQLhost host;
			host.id		= conf.ReadValue("database", "id", i);
			host.host	= conf.ReadValue("database", "hostname", i);
			host.port	= conf.ReadInteger("database", "port", i, true);
			host.name	= conf.ReadValue("database", "name", i);
			host.user	= conf.ReadValue("database", "username", i);
			host.pass	= conf.ReadValue("database", "password", i);
			if (h == host)
				return true;
		}
		return false;
	}

	void ReadConf()
	{
		ClearOldConnections();

		ConfigReader conf;
		for(int i = 0; i < conf.Enumerate("database"); i++)
		{
			SQLhost host;

			host.id		= conf.ReadValue("database", "id", i);
			host.host	= conf.ReadValue("database", "hostname", i);
			host.port	= conf.ReadInteger("database", "port", i, true);
			host.name	= conf.ReadValue("database", "name", i);
			host.user	= conf.ReadValue("database", "username", i);
			host.pass	= conf.ReadValue("database", "password", i);

			if (HasHost(host))
				continue;

			this->AddConn(host);
		}
	}

	void AddConn(const SQLhost& hi)
	{
		if (HasHost(hi))
		{
			ServerInstance->Logs->Log("m_sqlite3",DEFAULT, "WARNING: A sqlite connection with id: %s already exists. Aborting database open attempt.", hi.id.c_str());
			return;
		}

		SQLConn* newconn;

		newconn = new SQLConn(this, hi);

		connections.insert(std::make_pair(hi.id, newconn));
	}

	void ClearOldConnections()
	{
		ConnMap::iterator iter,safei;
		for (iter = connections.begin(); iter != connections.end(); iter++)
		{
			if (!HostInConf(iter->second->GetConfHost()))
			{
				delete iter->second;
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
			delete i->second;
		}
	}

	virtual void OnRehash(User* user)
	{
		ReadConf();
	}

	virtual const char* OnRequest(Request* request)
	{
		if(strcmp(SQLREQID, request->GetId()) == 0)
		{
			SQLrequest* req = (SQLrequest*)request;
			ConnMap::iterator iter;
			if((iter = connections.find(req->dbid)) != connections.end())
			{
				req->id = NewID();
				req->error = iter->second->Query(*req);
				return SQLSUCCESS;
			}
			else
			{
				req->error.Id(SQL_BAD_DBID);
				return NULL;
			}
		}
		return NULL;
	}

	unsigned long NewID()
	{
		if (currid+1 == 0)
			currid++;

		return ++currid;
	}

	virtual Version GetVersion()
	{
		return Version("sqlite3 provider", VF_VENDOR | VF_SERVICEPROVIDER, API_VERSION);
	}

};

void ResultNotifier::Dispatch()
{
	mod->SendQueue();
}

MODULE_INIT(ModuleSQLite3)
