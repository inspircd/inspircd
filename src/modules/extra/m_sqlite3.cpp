/*		 +------------------------------------+
 *		 | Inspire Internet Relay Chat Daemon |
 *		 +------------------------------------+
 *
 *	InspIRCd: (C) 2002-2010 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *			  the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"
#include <sqlite3.h>
#include "sql.h"

/* $ModDesc: sqlite3 provider */
/* $CompileFlags: pkgconfversion("sqlite3","3.3") pkgconfincludes("sqlite3","/sqlite3.h","") */
/* $LinkerFlags: pkgconflibs("sqlite3","/libsqlite3.so","-lsqlite3") */
/* $NoPedantic */

namespace m_sqlite {

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
			ServerInstance->Logs->Log("m_sqlite3",DEFAULT, "WARNING: Could not open DB with id: " + tag->getString("id"));
			conn = NULL;
		}
	}

	~SQLConn()
	{
		sqlite3_interrupt(conn);
		sqlite3_close(conn);
	}

	void Query(SQLQuery* query, const std::string& q)
	{
		ServerInstance->Logs->Log("m_sqlite3",DEBUG, "Query(%s): %s", config->getString("id").c_str(), q.c_str());
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

	class SQLFormat : public FormatSubstitute
	{
	 public:
		const ParamM& map;
		SQLFormat(const ParamM& Map) : map(Map) {}
		std::string lookup(const std::string& key)
		{
			ParamM::const_iterator it = map.find(key);
			if (it == map.end())
				return "";
			char* escaped = sqlite3_mprintf("%q", it->second.c_str());
			std::string rv(escaped);
			sqlite3_free(escaped);
			return rv;
		}
	};

	virtual void submit(SQLQuery* query, const std::string& q, const ParamM& p)
	{
		SQLFormat subst(p);
		std::string res = subst.format(q);
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

	void ReadConfig(ConfigReadStatus&)
	{
		ClearConns();
		ConfigTagList tags = ServerInstance->Config->GetTags("database");
		for(ConfigIter i = tags.first; i != tags.second; i++)
		{
			if (i->second->getString("module", "sqlite") != "sqlite")
				continue;
			SQLConn* conn = new SQLConn(this, i->second);
			conns.insert(std::make_pair(i->second->getString("id"), conn));
			ServerInstance->Modules->AddService(*conn);
		}
	}

	Version GetVersion()
	{
		return Version("sqlite3 provider", VF_VENDOR);
	}
};

}

using m_sqlite::ModuleSQLite3;

MODULE_INIT(ModuleSQLite3)
