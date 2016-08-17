/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2007-2009 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2007, 2009 Craig Edwards <craigedwards@brainbox.cc>
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
#include <sqlite3.h>
#include "sql.h"

#ifdef _WIN32
# pragma comment(lib, "sqlite3.lib")
#endif

/* $ModDesc: sqlite3 provider */
/* $CompileFlags: pkgconfversion("sqlite3","3.3") pkgconfincludes("sqlite3","/sqlite3.h","") */
/* $LinkerFlags: pkgconflibs("sqlite3","/libsqlite3.so","-lsqlite3") */
/* $NoPedantic */

class SQLConn;
typedef std::map<std::string, SQLConn*> ConnMap;

class SQLite3Result : public SQLResult
{
 public:
	int currentrow;
	int rows;
	std::vector<std::string> columns;
	std::vector<SQLEntries> fieldlists;

	SQLite3Result() : currentrow(0), rows(0)
	{
	}

	~SQLite3Result()
	{
	}

	virtual int Rows()
	{
		return rows;
	}

	virtual bool GetRow(SQLEntries& result)
	{
		if (currentrow < rows)
		{
			result.assign(fieldlists[currentrow].begin(), fieldlists[currentrow].end());
			currentrow++;
			return true;
		}
		else
		{
			result.clear();
			return false;
		}
	}

	virtual void GetCols(std::vector<std::string>& result)
	{
		result.assign(columns.begin(), columns.end());
	}
};

class SQLConn : public SQLProvider
{
 private:
	sqlite3* conn;
	reference<ConfigTag> config;

 public:
	SQLConn(Module* Parent, ConfigTag* tag) : SQLProvider(Parent, "SQL/" + tag->getString("id")), config(tag)
	{
		std::string host = tag->getString("hostname");
		if (sqlite3_open_v2(host.c_str(), &conn, SQLITE_OPEN_READWRITE, 0) != SQLITE_OK)
		{
			// Even in case of an error conn must be closed
			sqlite3_close(conn);
			conn = NULL;
			ServerInstance->Logs->Log("m_sqlite3",DEFAULT, "WARNING: Could not open DB with id: " + tag->getString("id"));
		}
	}

	~SQLConn()
	{
		if (conn)
		{
			sqlite3_interrupt(conn);
			sqlite3_close(conn);
		}
	}

	void Query(SQLQuery* query, const std::string& q)
	{
		SQLite3Result res;
		sqlite3_stmt *stmt;
		int err = sqlite3_prepare_v2(conn, q.c_str(), q.length(), &stmt, NULL);
		if (err != SQLITE_OK)
		{
			SQLerror error(SQL_QSEND_FAIL, sqlite3_errmsg(conn));
			query->OnError(error);
			return;
		}
		int cols = sqlite3_column_count(stmt);
		res.columns.resize(cols);
		for(int i=0; i < cols; i++)
		{
			res.columns[i] = sqlite3_column_name(stmt, i);
		}
		while (1)
		{
			err = sqlite3_step(stmt);
			if (err == SQLITE_ROW)
			{
				// Add the row
				res.fieldlists.resize(res.rows + 1);
				res.fieldlists[res.rows].resize(cols);
				for(int i=0; i < cols; i++)
				{
					const char* txt = (const char*)sqlite3_column_text(stmt, i);
					if (txt)
						res.fieldlists[res.rows][i] = SQLEntry(txt);
				}
				res.rows++;
			}
			else if (err == SQLITE_DONE)
			{
				query->OnResult(res);
				break;
			}
			else
			{
				SQLerror error(SQL_QREPLY_FAIL, sqlite3_errmsg(conn));
				query->OnError(error);
				break;
			}
		}
		sqlite3_finalize(stmt);
	}

	virtual void submit(SQLQuery* query, const std::string& q)
	{
		Query(query, q);
		delete query;
	}

	virtual void submit(SQLQuery* query, const std::string& q, const ParamL& p)
	{
		std::string res;
		unsigned int param = 0;
		for(std::string::size_type i = 0; i < q.length(); i++)
		{
			if (q[i] != '?')
				res.push_back(q[i]);
			else
			{
				if (param < p.size())
				{
					char* escaped = sqlite3_mprintf("%q", p[param++].c_str());
					res.append(escaped);
					sqlite3_free(escaped);
				}
			}
		}
		submit(query, res);
	}

	virtual void submit(SQLQuery* query, const std::string& q, const ParamM& p)
	{
		std::string res;
		for(std::string::size_type i = 0; i < q.length(); i++)
		{
			if (q[i] != '$')
				res.push_back(q[i]);
			else
			{
				std::string field;
				i++;
				while (i < q.length() && isalnum(q[i]))
					field.push_back(q[i++]);
				i--;

				ParamM::const_iterator it = p.find(field);
				if (it != p.end())
				{
					char* escaped = sqlite3_mprintf("%q", it->second.c_str());
					res.append(escaped);
					sqlite3_free(escaped);
				}
			}
		}
		submit(query, res);
	}
};

class ModuleSQLite3 : public Module
{
 private:
	ConnMap conns;

 public:
	ModuleSQLite3()
	{
	}

	void init()
	{
		ReadConf();

		Implementation eventlist[] = { I_OnRehash };
		ServerInstance->Modules->Attach(eventlist, this, sizeof(eventlist)/sizeof(Implementation));
	}

	virtual ~ModuleSQLite3()
	{
		ClearConns();
	}

	void ClearConns()
	{
		for(ConnMap::iterator i = conns.begin(); i != conns.end(); i++)
		{
			SQLConn* conn = i->second;
			ServerInstance->Modules->DelService(*conn);
			delete conn;
		}
		conns.clear();
	}

	void ReadConf()
	{
		ClearConns();
		ConfigTagList tags = ServerInstance->Config->ConfTags("database");
		for(ConfigIter i = tags.first; i != tags.second; i++)
		{
			if (i->second->getString("module", "sqlite") != "sqlite")
				continue;
			SQLConn* conn = new SQLConn(this, i->second);
			conns.insert(std::make_pair(i->second->getString("id"), conn));
			ServerInstance->Modules->AddService(*conn);
		}
	}

	void OnRehash(User* user)
	{
		ReadConf();
	}

	Version GetVersion()
	{
		return Version("sqlite3 provider", VF_VENDOR);
	}
};

MODULE_INIT(ModuleSQLite3)
