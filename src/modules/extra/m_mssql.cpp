/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2008-2009 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2008-2009 Craig Edwards <craigedwards@brainbox.cc>
 *   Copyright (C) 2008 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2008 Pippijn van Steenhoven <pip88nl@gmail.com>
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


#include "inspircd.h"
#include <tds.h>
#include <tdsconvert.h>
#include "users.h"
#include "channels.h"
#include "modules.h"

#include "m_sqlv2.h"

/* $ModDesc: MsSQL provider */
/* $CompileFlags: exec("grep VERSION_NO /usr/include/tdsver.h 2>/dev/null | perl -e 'print "-D_TDSVER=".((<> =~ /freetds v(\d+\.\d+)/i) ? $1*100 : 0);'") */
/* $LinkerFlags: -ltds */
/* $ModDep: m_sqlv2.h */

class SQLConn;
class MsSQLResult;
class ModuleMsSQL;

typedef std::map<std::string, SQLConn*> ConnMap;
typedef std::deque<MsSQLResult*> ResultQueue;

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

ConnMap connections;
Mutex* ResultsMutex;
Mutex* LoggingMutex;

class QueryThread : public SocketThread
{
  private:
	ModuleMsSQL* const Parent;
  public:
	QueryThread(ModuleMsSQL* mod) : Parent(mod) { }
	~QueryThread() { }
	virtual void Run();
	virtual void OnNotify();
};

class MsSQLResult : public SQLresult
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
	MsSQLResult(Module* self, Module* to, unsigned int rid)
	: SQLresult(self, to, rid), currentrow(0), rows(0), cols(0), fieldlist(NULL), fieldmap(NULL)
	{
	}

	~MsSQLResult()
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
	TDSLOGIN* login;
	TDSSOCKET* sock;
	TDSCONTEXT* context;

 public:
	QueryQueue queue;

	SQLConn(Module* m, const SQLhost& hi)
	: mod(m), host(hi), login(NULL), sock(NULL), context(NULL)
	{
		if (OpenDB())
		{
			std::string query("USE " + host.name);
			if (tds_submit_query(sock, query.c_str()) == TDS_SUCCEED)
			{
				if (tds_process_simple_query(sock) != TDS_SUCCEED)
				{
					LoggingMutex->Lock();
					ServerInstance->Logs->Log("m_mssql",DEFAULT, "WARNING: Could not select database " + host.name + " for DB with id: " + host.id);
					LoggingMutex->Unlock();
					CloseDB();
				}
			}
			else
			{
				LoggingMutex->Lock();
				ServerInstance->Logs->Log("m_mssql",DEFAULT, "WARNING: Could not select database " + host.name + " for DB with id: " + host.id);
				LoggingMutex->Unlock();
				CloseDB();
			}
		}
		else
		{
			LoggingMutex->Lock();
			ServerInstance->Logs->Log("m_mssql",DEFAULT, "WARNING: Could not connect to DB with id: " + host.id);
			LoggingMutex->Unlock();
			CloseDB();
		}
	}

	~SQLConn()
	{
		CloseDB();
	}

	SQLerror Query(SQLrequest* req)
	{
		if (!sock)
			return SQLerror(SQL_BAD_CONN, "Socket was NULL, check if SQL server is running.");

		/* Pointer to the buffer we screw around with substitution in */
		char* query;

		/* Pointer to the current end of query, where we append new stuff */
		char* queryend;

		/* Total length of the unescaped parameters */
		unsigned long maxparamlen, paramcount;

		/* The length of the longest parameter */
		maxparamlen = 0;

		for(ParamL::iterator i = req->query.p.begin(); i != req->query.p.end(); i++)
		{
			if (i->size() > maxparamlen)
				maxparamlen = i->size();
		}

		/* How many params are there in the query? */
		paramcount = count(req->query.q.c_str(), '?');

		/* This stores copy of params to be inserted with using numbered params 1;3B*/
		ParamL paramscopy(req->query.p);

		/* To avoid a lot of allocations, allocate enough memory for the biggest the escaped query could possibly be.
		 * sizeofquery + (maxtotalparamlength*2) + 1
		 *
		 * The +1 is for null-terminating the string
		 */

		query = new char[req->query.q.length() + (maxparamlen*paramcount*2) + 1];
		queryend = query;

		for(unsigned long i = 0; i < req->query.q.length(); i++)
		{
			if(req->query.q[i] == '?')
			{
				/* We found a place to substitute..what fun.
				 * use mssql calls to escape and write the
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

				while ((i < req->query.q.length() - 1) && (req->query.q[i+1] >= '0') && (req->query.q[i+1] <= '9'))
				{
					numbered = true;
					++i;
					paramnum = paramnum * 10 + req->query.q[i] - '0';
				}

				if (paramnum > paramscopy.size() - 1)
				{
					/* index is out of range!
					 */
					numbered = false;
				}

				if (numbered)
				{
					/* Custom escaping for this one. converting ' to '' should make SQL Server happy. Ugly but fast :]
					 */
					char* escaped = new char[(paramscopy[paramnum].length() * 2) + 1];
					char* escend = escaped;
					for (std::string::iterator p = paramscopy[paramnum].begin(); p < paramscopy[paramnum].end(); p++)
					{
						if (*p == '\'')
						{
							*escend = *p;
							escend++;
							*escend = *p;
						}
						*escend = *p;
						escend++;
					}
					*escend = 0;

					for (char* n = escaped; *n; n++)
					{
						*queryend = *n;
						queryend++;
					}
					delete[] escaped;
				}
				else if (req->query.p.size())
				{
					/* Custom escaping for this one. converting ' to '' should make SQL Server happy. Ugly but fast :]
					 */
					char* escaped = new char[(req->query.p.front().length() * 2) + 1];
					char* escend = escaped;
					for (std::string::iterator p = req->query.p.front().begin(); p < req->query.p.front().end(); p++)
					{
						if (*p == '\'')
						{
							*escend = *p;
							escend++;
							*escend = *p;
						}
						*escend = *p;
						escend++;
					}
					*escend = 0;

					for (char* n = escaped; *n; n++)
					{
						*queryend = *n;
						queryend++;
					}
					delete[] escaped;
					req->query.p.pop_front();
				}
				else
					break;
			}
			else
			{
				*queryend = req->query.q[i];
				queryend++;
			}
		}
		*queryend = 0;
		req->query.q = query;

		MsSQLResult* res = new MsSQLResult((Module*)mod, req->source, req->id);
		res->dbid = host.id;
		res->query = req->query.q;

		char* msquery = strdup(req->query.q.data());
		LoggingMutex->Lock();
		ServerInstance->Logs->Log("m_mssql",DEBUG,"doing Query: %s",msquery);
		LoggingMutex->Unlock();
		if (tds_submit_query(sock, msquery) != TDS_SUCCEED)
		{
			std::string error("failed to execute: "+std::string(req->query.q.data()));
			delete[] query;
			delete res;
			free(msquery);
			return SQLerror(SQL_QSEND_FAIL, error);
		}
		delete[] query;
		free(msquery);

		int tds_res;
		while (tds_process_tokens(sock, &tds_res, NULL, TDS_TOKEN_RESULTS) == TDS_SUCCEED)
		{
			//ServerInstance->Logs->Log("m_mssql",DEBUG,"<******> result type: %d", tds_res);
			//ServerInstance->Logs->Log("m_mssql",DEBUG,"AFFECTED ROWS: %d", sock->rows_affected);
			switch (tds_res)
			{
				case TDS_ROWFMT_RESULT:
					break;

				case TDS_DONE_RESULT:
					if (sock->rows_affected > -1)
					{
						for (int c = 0; c < sock->rows_affected; c++)  res->UpdateAffectedCount();
						continue;
					}
					break;

				case TDS_ROW_RESULT:
					while (tds_process_tokens(sock, &tds_res, NULL, TDS_STOPAT_ROWFMT|TDS_RETURN_DONE|TDS_RETURN_ROW) == TDS_SUCCEED)
					{
						if (tds_res != TDS_ROW_RESULT)
							break;

						if (!sock->current_results)
							continue;

						if (sock->res_info->row_count > 0)
						{
							int cols = sock->res_info->num_cols;
							char** name = new char*[MAXBUF];
							char** data = new char*[MAXBUF];
							for (int j=0; j<cols; j++)
							{
								TDSCOLUMN* col = sock->current_results->columns[j];
								name[j] = col->column_name;

								int ctype;
								int srclen;
								unsigned char* src;
								CONV_RESULT dres;
								ctype = tds_get_conversion_type(col->column_type, col->column_size);
#if _TDSVER >= 82
									src = col->column_data;
#else
									src = &(sock->current_results->current_row[col->column_offset]);
#endif
								srclen = col->column_cur_size;
								tds_convert(sock->tds_ctx, ctype, (TDS_CHAR *) src, srclen, SYBCHAR, &dres);
								data[j] = (char*)dres.ib;
							}
							ResultReady(res, cols, data, name);
						}
					}
					break;

				default:
					break;
			}
		}
		ResultsMutex->Lock();
		results.push_back(res);
		ResultsMutex->Unlock();
		return SQLerror();
	}

	static int HandleMessage(const TDSCONTEXT * pContext, TDSSOCKET * pTdsSocket, TDSMESSAGE * pMessage)
	{
		SQLConn* sc = (SQLConn*)pContext->parent;
		LoggingMutex->Lock();
		ServerInstance->Logs->Log("m_mssql", DEBUG, "Message for DB with id: %s -> %s", sc->host.id.c_str(), pMessage->message);
		LoggingMutex->Unlock();
		return 0;
	}

	static int HandleError(const TDSCONTEXT * pContext, TDSSOCKET * pTdsSocket, TDSMESSAGE * pMessage)
	{
		SQLConn* sc = (SQLConn*)pContext->parent;
		LoggingMutex->Lock();
		ServerInstance->Logs->Log("m_mssql", DEFAULT, "Error for DB with id: %s -> %s", sc->host.id.c_str(), pMessage->message);
		LoggingMutex->Unlock();
		return 0;
	}

	void ResultReady(MsSQLResult *res, int cols, char **data, char **colnames)
	{
		res->AddRow(cols, data, colnames);
	}

	void AffectedReady(MsSQLResult *res)
	{
		res->UpdateAffectedCount();
	}

	bool OpenDB()
	{
		CloseDB();

		TDSCONNECTION* conn = NULL;

		login = tds_alloc_login();
		tds_set_app(login, "TSQL");
		tds_set_library(login,"TDS-Library");
		tds_set_host(login, "");
		tds_set_server(login, host.host.c_str());
		tds_set_server_addr(login, host.host.c_str());
		tds_set_user(login, host.user.c_str());
		tds_set_passwd(login, host.pass.c_str());
		tds_set_port(login, host.port);
		tds_set_packet(login, 512);

		context = tds_alloc_context(this);
		context->msg_handler = HandleMessage;
		context->err_handler = HandleError;

		sock = tds_alloc_socket(context, 512);
		tds_set_parent(sock, NULL);

		conn = tds_read_config_info(NULL, login, context->locale);

		if (tds_connect(sock, conn) == TDS_SUCCEED)
		{
			tds_free_connection(conn);
			return 1;
		}
		tds_free_connection(conn);
		return 0;
	}

	void CloseDB()
	{
		if (sock)
		{
			tds_free_socket(sock);
			sock = NULL;
		}
		if (context)
		{
			tds_free_context(context);
			context = NULL;
		}
		if (login)
		{
			tds_free_login(login);
			login = NULL;
		}
	}

	SQLhost GetConfHost()
	{
		return host;
	}

	void SendResults()
	{
		while (results.size())
		{
			MsSQLResult* res = results[0];
			ResultsMutex->Lock();
			if (res->dest)
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
			ResultsMutex->Unlock();
		}
	}

	void ClearResults()
	{
		while (results.size())
		{
			MsSQLResult* res = results[0];
			delete res;
			results.pop_front();
		}
	}

	void DoLeadingQuery()
	{
		SQLrequest* req = queue.front();
		req->error = Query(req);
	}

};


class ModuleMsSQL : public Module
{
 private:
	unsigned long currid;
	QueryThread* queryDispatcher;
	ServiceProvider sqlserv;

 public:
	ModuleMsSQL()
	: currid(0), sqlserv(this, "SQL/mssql", SERVICE_DATA)
	{
		LoggingMutex = new Mutex();
		ResultsMutex = new Mutex();
		queryDispatcher = new QueryThread(this);
	}

	void init()
	{
		ReadConf();

		ServerInstance->Threads->Start(queryDispatcher);

		Implementation eventlist[] = { I_OnRehash };
		ServerInstance->Modules->Attach(eventlist, this, sizeof(eventlist)/sizeof(Implementation));
		ServerInstance->Modules->AddService(sqlserv);
	}

	virtual ~ModuleMsSQL()
	{
		queryDispatcher->join();
		delete queryDispatcher;
		ClearQueue();
		ClearAllConnections();

		delete LoggingMutex;
		delete ResultsMutex;
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
		ConfigTagList tags = ServerInstance->Config->ConfTags("database");
		for (ConfigIter i = tags.first; i != tags.second; ++i)
		{
			ConfigTag* tag = i->second;
			SQLhost host;
			host.id		= tag->getString("id");
			host.host	= tag->getString("hostname");
			host.port	= tag->getInt("port", 1433);
			host.name	= tag->getString("name");
			host.user	= tag->getString("username");
			host.pass	= tag->getString("password");
			if (h == host)
				return true;
		}
		return false;
	}

	void ReadConf()
	{
		ClearOldConnections();

		ConfigTagList tags = ServerInstance->Config->ConfTags("database");
		for (ConfigIter i = tags.first; i != tags.second; ++i)
		{
			ConfigTag* tag = i->second;
			SQLhost host;

			host.id		= tag->getString("id");
			host.host	= tag->getString("hostname");
			host.port	= tag->getInt("port", 1433);
			host.name	= tag->getString("name");
			host.user	= tag->getString("username");
			host.pass	= tag->getString("password");

			if (HasHost(host))
				continue;

			this->AddConn(host);
		}
	}

	void AddConn(const SQLhost& hi)
	{
		if (HasHost(hi))
		{
			LoggingMutex->Lock();
			ServerInstance->Logs->Log("m_mssql",DEFAULT, "WARNING: A MsSQL connection with id: %s already exists. Aborting database open attempt.", hi.id.c_str());
			LoggingMutex->Unlock();
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
		for(ConnMap::iterator i = connections.begin(); i != connections.end(); ++i)
			delete i->second;
		connections.clear();
	}

	virtual void OnRehash(User* user)
	{
		queryDispatcher->LockQueue();
		ReadConf();
		queryDispatcher->UnlockQueueWakeup();
	}

	void OnRequest(Request& request)
	{
		if(strcmp(SQLREQID, request.id) == 0)
		{
			SQLrequest* req = (SQLrequest*)&request;

			queryDispatcher->LockQueue();

			ConnMap::iterator iter;

			if((iter = connections.find(req->dbid)) != connections.end())
			{
				req->id = NewID();
				iter->second->queue.push(new SQLrequest(*req));
			}
			else
			{
				req->error.Id(SQL_BAD_DBID);
			}
			queryDispatcher->UnlockQueueWakeup();
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
		return Version("MsSQL provider", VF_VENDOR);
	}

};

void QueryThread::OnNotify()
{
	Parent->SendQueue();
}

void QueryThread::Run()
{
	this->LockQueue();
	while (this->GetExitFlag() == false)
	{
		SQLConn* conn = NULL;
		for (ConnMap::iterator i = connections.begin(); i != connections.end(); i++)
		{
			if (i->second->queue.totalsize())
			{
				conn = i->second;
				break;
			}
		}
		if (conn)
		{
			this->UnlockQueue();
			conn->DoLeadingQuery();
			this->NotifyParent();
			this->LockQueue();
			conn->queue.pop();
		}
		else
		{
			this->WaitForQueue();
		}
	}
	this->UnlockQueue();
}

MODULE_INIT(ModuleMsSQL)
