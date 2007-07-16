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
#include <sqlite3.h>
#include "users.h"
#include "channels.h"
#include "modules.h"

#include "m_sqlv2.h"

/* $ModDesc: sqlite3 provider */
/* $CompileFlags: pkgconfversion("sqlite3","3.3") pkgconfincludes("sqlite3","/sqlite3.h","") */
/* $LinkerFlags: pkgconflibs("sqlite3","/libsqlite3.so","-lsqlite3") */
/* $ModDep: m_sqlv2.h */


class SQLConn;
class SQLite3Result;
class ResultNotifier;

typedef std::map<std::string, SQLConn*> ConnMap;
typedef std::deque<classbase*> paramlist;
typedef std::deque<SQLite3Result*> ResultQueue;

ResultNotifier* resultnotify = NULL;


class ResultNotifier : public InspSocket
{
	Module* mod;
	insp_sockaddr sock_us;
	socklen_t uslen;

 public:
	/* Create a socket on a random port. Let the tcp stack allocate us an available port */
#ifdef IPV6
	ResultNotifier(InspIRCd* SI, Module* m) : InspSocket(SI, "::1", 0, true, 3000), mod(m)
#else
	ResultNotifier(InspIRCd* SI, Module* m) : InspSocket(SI, "127.0.0.1", 0, true, 3000), mod(m)
#endif
	{
		uslen = sizeof(sock_us);
		if (getsockname(this->fd,(sockaddr*)&sock_us,&uslen))
		{
			throw ModuleException("Could not create random listening port on localhost");
		}
	}

	ResultNotifier(InspIRCd* SI, Module* m, int newfd, char* ip) : InspSocket(SI, newfd, ip), mod(m)
	{
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

	virtual int OnIncomingConnection(int newsock, char* ip)
	{
		Dispatch();
		return false;
	}

	void Dispatch();
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
	SQLite3Result(Module* self, Module* to, unsigned int id)
	: SQLresult(self, to, id), currentrow(0), rows(0), cols(0), fieldlist(NULL), fieldmap(NULL)
	{
	}

	~SQLite3Result()
	{
	}

	void AddRow(int colsnum, char **data, char **colname)
	{
		colnames.clear();
		cols = colsnum;
		for (int i = 0; i < colsnum; i++)
		{
			fieldlists.resize(fieldlists.size()+1);
			colnames.push_back(colname[i]);
			SQLfield sf(data[i] ? data[i] : "", data[i] ? false : true);
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
  	InspIRCd* Instance;
	Module* mod;
	SQLhost host;
	sqlite3* conn;

  public:
	SQLConn(InspIRCd* SI, Module* m, const SQLhost& hi)
	: Instance(SI), mod(m), host(hi)
	{
		if (OpenDB() != SQLITE_OK)
		{
			Instance->Log(DEFAULT, "WARNING: Could not open DB with id: " + host.id);
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
		unsigned long paramlen;

		/* Total length of query, used for binary-safety in mysql_real_query */
		unsigned long querylength = 0;

		paramlen = 0;
		for(ParamL::iterator i = req.query.p.begin(); i != req.query.p.end(); i++)
		{
			paramlen += i->size();
		}

		/* To avoid a lot of allocations, allocate enough memory for the biggest the escaped query could possibly be.
		 * sizeofquery + (totalparamlength*2) + 1
		 *
		 * The +1 is for null-terminating the string for mysql_real_escape_string
		 */
		query = new char[req.query.q.length() + (paramlen*2) + 1];
		queryend = query;

		for(unsigned long i = 0; i < req.query.q.length(); i++)
		{
			if(req.query.q[i] == '?')
			{
				if(req.query.p.size())
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
			querylength++;
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
			return SQLerror(QSEND_FAIL, error);
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
		return sqlite3_open(host.host.c_str(), &conn);
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
		int QueueFD;
		if ((QueueFD = socket(AF_FAMILY, SOCK_STREAM, 0)) == -1)
		{
			/* crap, we're out of sockets... */
			return;
		}

		insp_sockaddr addr;

#ifdef IPV6
		insp_aton("::1", &addr.sin6_addr);
		addr.sin6_family = AF_FAMILY;
		addr.sin6_port = htons(resultnotify->GetPort());
#else
		insp_inaddr ia;
		insp_aton("127.0.0.1", &ia);
		addr.sin_family = AF_FAMILY;
		addr.sin_addr = ia;
		addr.sin_port = htons(resultnotify->GetPort());
#endif

		if (connect(QueueFD, (sockaddr*)&addr,sizeof(addr)) == -1)
		{
			/* wtf, we cant connect to it, but we just created it! */
			return;
		}
	}

};


class ModuleSQLite3 : public Module
{
  private:
	ConnMap connections;
	unsigned long currid;

  public:
	ModuleSQLite3(InspIRCd* Me)
	: Module::Module(Me), currid(0)
	{
		ServerInstance->UseInterface("SQLutils");

		if (!ServerInstance->PublishFeature("SQL", this))
		{
			throw ModuleException("m_sqlite3: Unable to publish feature 'SQL'");
		}

		resultnotify = new ResultNotifier(ServerInstance, this);

		ReadConf();

		ServerInstance->PublishInterface("SQL", this);
	}

	virtual ~ModuleSQLite3()
	{
		ClearQueue();
		ClearAllConnections();
		resultnotify->SetFd(-1);
		resultnotify->state = I_ERROR;
		resultnotify->OnError(I_ERR_SOCKET);
		resultnotify->ClosePending = true;
		delete resultnotify;
		ServerInstance->UnpublishInterface("SQL", this);
		ServerInstance->UnpublishFeature("SQL");
		ServerInstance->DoneWithInterface("SQLutils");
	}

	void Implements(char* List)
	{
		List[I_OnRequest] = List[I_OnRehash] = 1;
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

			host.id		= conf.ReadValue("database", "id", i);
			host.host	= conf.ReadValue("database", "hostname", i);
			host.port	= conf.ReadInteger("database", "port", i, true);
			host.name	= conf.ReadValue("database", "name", i);
			host.user	= conf.ReadValue("database", "username", i);
			host.pass	= conf.ReadValue("database", "password", i);
			host.ssl	= conf.ReadFlag("database", "ssl", "0", i);

			if (HasHost(host))
				continue;

			this->AddConn(host);
		}
	}

	void AddConn(const SQLhost& hi)
	{
		if (HasHost(hi))
		{
			ServerInstance->Log(DEFAULT, "WARNING: A sqlite connection with id: %s already exists. Aborting database open attempt.", hi.id.c_str());
			return;
		}

		SQLConn* newconn;

		newconn = new SQLConn(ServerInstance, this, hi);

		connections.insert(std::make_pair(hi.id, newconn));
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

	virtual void OnRehash(userrec* user, const std::string &parameter)
	{
		ReadConf();
	}

	virtual char* OnRequest(Request* request)
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
				req->error.Id(BAD_DBID);
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
		return Version(1,1,0,0,VF_VENDOR|VF_SERVICEPROVIDER,API_VERSION);
	}

};

void ResultNotifier::Dispatch()
{
	((ModuleSQLite3*)mod)->SendQueue();
}

MODULE_INIT(ModuleSQLite3);

