/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2015 Daniel Vassdal <shutter@canternet.org>
 *   Copyright (C) 2014, 2016 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2013-2014, 2016-2017, 2019-2023 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012, 2016 Adam <Adam@anope.org>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
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
/// $LinkerFlags: find_linker_flags("sqlite3")

/// $PackageInfo: require_system("arch") pkgconf sqlite
/// $PackageInfo: require_system("centos") pkgconfig sqlite-devel
/// $PackageInfo: require_system("darwin") pkg-config sqlite
/// $PackageInfo: require_system("debian") libsqlite3-dev pkg-config
/// $PackageInfo: require_system("rocky") pkgconfig sqlite-devel
/// $PackageInfo: require_system("ubuntu") libsqlite3-dev pkg-config

#include "inspircd.h"
#include "modules/sql.h"
#include "utility/string.h"

#include <sqlite3.h>

#ifdef _WIN32
# pragma comment(lib, "sqlite3.lib")
#endif

class SQLConn;
typedef insp::flat_map<std::string, SQLConn*> ConnMap;

class SQLite3Result final
	: public SQL::Result
{
public:
	int currentrow = 0;
	int rows = 0;
	std::vector<std::string> columns;
	std::vector<SQL::Row> fieldlists;

	int Rows() override
	{
		return rows;
	}

	bool GetRow(SQL::Row& result) override
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

	void GetCols(std::vector<std::string>& result) override
	{
		result.assign(columns.begin(), columns.end());
	}

	bool HasColumn(const std::string& column, size_t& index) override
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

class SQLConn final
	: public SQL::Provider
{
	sqlite3* conn;
	std::shared_ptr<ConfigTag> config;

public:
	SQLConn(Module* Parent, const std::shared_ptr<ConfigTag>& tag)
		: SQL::Provider(Parent, tag->getString("id"))
		, config(tag)
	{
		std::string host = tag->getString("hostname");
		if (sqlite3_open_v2(host.c_str(), &conn, SQLITE_OPEN_READWRITE, nullptr) != SQLITE_OK)
		{
			// Even in case of an error conn must be closed
			sqlite3_close(conn);
			conn = nullptr;
			ServerInstance->Logs.Critical(MODNAME, "WARNING: Could not open DB with id: " + tag->getString("id"));
		}
	}

	~SQLConn() override
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
		sqlite3_stmt* stmt;
		int err = sqlite3_prepare_v2(conn, q.c_str(), static_cast<int>(q.length()), &stmt, nullptr);
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
		while (true)
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
						res.fieldlists[res.rows][i] = txt;
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

	void Submit(SQL::Query* query, const std::string& q) override
	{
		ServerInstance->Logs.Debug(MODNAME, "Executing SQLite3 query: " + q);
		Query(query, q);
		delete query;
	}

	void Submit(SQL::Query* query, const std::string& q, const SQL::ParamList& p) override
	{
		std::string res;
		unsigned int param = 0;
		for (const auto chr : q)
		{
			if (chr != '?')
				res.push_back(chr);
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

	void Submit(SQL::Query* query, const std::string& q, const SQL::ParamMap& p) override
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

class ModuleSQLite3 final
	: public Module
{
private:
	ConnMap conns;

public:
	ModuleSQLite3()
		: Module(VF_VENDOR, "Provides the ability for SQL modules to query a SQLite 3 database.")
	{
	}

	~ModuleSQLite3() override
	{
		ClearConns();
	}

	void init() override
	{
		ServerInstance->Logs.Normal(MODNAME, "Module was compiled against SQLite version {} and is running against version {}",
			SQLITE_VERSION, sqlite3_libversion());
	}

	void ClearConns()
	{
		for (const auto& [_, conn] : conns)
		{
			ServerInstance->Modules.DelService(*conn);
			delete conn;
		}
		conns.clear();
	}

	void ReadConfig(ConfigStatus& status) override
	{
		ClearConns();

		for (const auto& [_, tag] : ServerInstance->Config->ConfTags("database"))
		{
			if (!insp::equalsci(tag->getString("module"), "sqlite"))
				continue;

			auto* conn = new SQLConn(this, tag);
			conns.emplace(tag->getString("id"), conn);
			ServerInstance->Modules.AddService(*conn);
		}
	}
};

MODULE_INIT(ModuleSQLite3)
