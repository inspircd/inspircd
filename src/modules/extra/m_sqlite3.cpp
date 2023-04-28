/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2019 linuxdaemon <linuxdaemon.irc@gmail.com>
 *   Copyright (C) 2015 Daniel Vassdal <shutter@canternet.org>
 *   Copyright (C) 2013-2014, 2016-2019, 2023 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2013-2014, 2016 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012, 2019 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2012, 2016 Adam <Adam@anope.org>
 *   Copyright (C) 2012 ChrisTX <xpipe@hotmail.de>
 *   Copyright (C) 2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2007, 2009 Craig Edwards <brain@inspircd.org>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
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

/// $CompilerFlags: find_compiler_flags("sqlite3")
/// $LinkerFlags: find_linker_flags("sqlite3" "-lsqlite3")

/// $PackageInfo: require_system("arch") pkgconf sqlite
/// $PackageInfo: require_system("centos") pkgconfig sqlite-devel
/// $PackageInfo: require_system("darwin") pkg-config sqlite
/// $PackageInfo: require_system("debian") libsqlite3-dev pkg-config
/// $PackageInfo: require_system("rocky") pkgconfig sqlite-devel
/// $PackageInfo: require_system("ubuntu") libsqlite3-dev pkg-config

#include "inspircd.h"
#include "modules/sql.h"

#ifdef __GNUC__
# pragma GCC diagnostic push
#endif

// Fix warnings about the use of `long long` on C++03.
#if defined __clang__
# pragma clang diagnostic ignored "-Wc++11-long-long"
#elif defined __GNUC__
# pragma GCC diagnostic ignored "-Wlong-long"
#endif

#include <sqlite3.h>

#ifdef __GNUC__
# pragma GCC diagnostic pop
#endif

#ifdef _WIN32
# pragma comment(lib, "sqlite3.lib")
#endif

class SQLConn;
typedef insp::flat_map<std::string, SQLConn*> ConnMap;

class SQLite3Result : public SQL::Result
{
 public:
	int currentrow;
	int rows;
	std::vector<std::string> columns;
	std::vector<SQL::Row> fieldlists;

	SQLite3Result() : currentrow(0), rows(0)
	{
	}

	int Rows() CXX11_OVERRIDE
	{
		return rows;
	}

	bool GetRow(SQL::Row& result) CXX11_OVERRIDE
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

	void GetCols(std::vector<std::string>& result) CXX11_OVERRIDE
	{
		result.assign(columns.begin(), columns.end());
	}

	bool HasColumn(const std::string& column, size_t& index) CXX11_OVERRIDE
	{
		for (size_t i = 0; i < columns.size(); ++i)
		{
			if (columns[i] == column)
			{
				index = i;
				return true;
			}
		}
		return false;
	}
};

class SQLConn : public SQL::Provider
{
	sqlite3* conn;
	reference<ConfigTag> config;

 public:
	SQLConn(Module* Parent, ConfigTag* tag)
		: SQL::Provider(Parent, tag->getString("id"))
		, config(tag)
	{
		std::string host = tag->getString("hostname");
		if (sqlite3_open_v2(host.c_str(), &conn, SQLITE_OPEN_READWRITE, 0) != SQLITE_OK)
		{
			// Even in case of an error conn must be closed
			sqlite3_close(conn);
			conn = NULL;
			ServerInstance->Logs->Log(MODNAME, LOG_DEFAULT, "WARNING: Could not open DB with id: " + tag->getString("id"));
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

	void Query(SQL::Query* query, const std::string& q)
	{
		SQLite3Result res;
		sqlite3_stmt *stmt;
		int err = sqlite3_prepare_v2(conn, q.c_str(), q.length(), &stmt, NULL);
		if (err != SQLITE_OK)
		{
			SQL::Error error(SQL::QSEND_FAIL, sqlite3_errmsg(conn));
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
						res.fieldlists[res.rows][i] = SQL::Field(txt);
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
				SQL::Error error(SQL::QREPLY_FAIL, sqlite3_errmsg(conn));
				query->OnError(error);
				break;
			}
		}
		sqlite3_finalize(stmt);
	}

	void Submit(SQL::Query* query, const std::string& q) CXX11_OVERRIDE
	{
		ServerInstance->Logs->Log(MODNAME, LOG_DEBUG, "Executing SQLite3 query: " + q);
		Query(query, q);
		delete query;
	}

	void Submit(SQL::Query* query, const std::string& q, const SQL::ParamList& p) CXX11_OVERRIDE
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
		Submit(query, res);
	}

	void Submit(SQL::Query* query, const std::string& q, const SQL::ParamMap& p) CXX11_OVERRIDE
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

				SQL::ParamMap::const_iterator it = p.find(field);
				if (it != p.end())
				{
					char* escaped = sqlite3_mprintf("%q", it->second.c_str());
					res.append(escaped);
					sqlite3_free(escaped);
				}
			}
		}
		Submit(query, res);
	}
};

class ModuleSQLite3 : public Module
{
	ConnMap conns;

 public:
	~ModuleSQLite3()
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

	void ReadConfig(ConfigStatus& status) CXX11_OVERRIDE
	{
		ClearConns();
		ConfigTagList tags = ServerInstance->Config->ConfTags("database");
		for(ConfigIter i = tags.first; i != tags.second; i++)
		{
			if (!stdalgo::string::equalsci(i->second->getString("module"), "sqlite"))
				continue;
			SQLConn* conn = new SQLConn(this, i->second);
			conns.insert(std::make_pair(i->second->getString("id"), conn));
			ServerInstance->Modules->AddService(*conn);
		}
	}

	Version GetVersion() CXX11_OVERRIDE
	{
		return Version("Provides the ability for SQL modules to query a SQLite 3 database.", VF_VENDOR);
	}
};

MODULE_INIT(ModuleSQLite3)
