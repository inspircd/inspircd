/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2010 Daniel De Graaf <danieldg@inspircd.org>
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


#ifndef INSPIRCD_SQLAPI_3
#define INSPIRCD_SQLAPI_3

/** Defines the error types which SQLerror may be set to
 */
enum SQLerrorNum { SQL_NO_ERROR, SQL_BAD_DBID, SQL_BAD_CONN, SQL_QSEND_FAIL, SQL_QREPLY_FAIL };

/** A list of format parameters for an SQLquery object.
 */
typedef std::vector<std::string> ParamL;

typedef std::map<std::string, std::string> ParamM;

class SQLEntry
{
 public:
	std::string value;
	bool nul;
	SQLEntry() : nul(true) {}
	SQLEntry(const std::string& v) : value(v), nul(false) {}
	inline operator std::string&() { return value; }
};

typedef std::vector<SQLEntry> SQLEntries;

/**
 * Result of an SQL query. Only valid inside OnResult
 */
class SQLResult : public classbase
{
 public:
	/**
	 * Return the number of rows in the result.
	 *
	 * Note that if you have perfomed an INSERT or UPDATE query or other
	 * query which will not return rows, this will return the number of
	 * affected rows. In this case you SHOULD NEVER access any of the result
	 * set rows, as there aren't any!
	 * @returns Number of rows in the result set.
	 */
	virtual int Rows() = 0;

	/**
	 * Return a single row (result of the query). The internal row counter
	 * is incremented by one.
	 *
	 * @param result Storage for the result data.
	 * @returns true if there was a row, false if no row exists (end of
	 * iteration)
	 */
	virtual bool GetRow(SQLEntries& result) = 0;

	/** Returns column names for the items in this row
	 */
	virtual void GetCols(std::vector<std::string>& result) = 0;
};

/** SQLerror holds the error state of a request.
 * The error string varies from database software to database software
 * and should be used to display informational error messages to users.
 */
class SQLerror
{
 public:
	/** The error id
	 */
	SQLerrorNum id;

	/** The error string
	 */
	std::string str;

	/** Initialize an SQLerror
	 * @param i The error ID to set
	 * @param s The (optional) error string to set
	 */
	SQLerror(SQLerrorNum i, const std::string &s = "")
	: id(i), str(s)
	{
	}

	/** Return the error string for an error
	 */
	const char* Str()
	{
		if(str.length())
			return str.c_str();

		switch(id)
		{
			case SQL_BAD_DBID:
				return "Invalid database ID";
			case SQL_BAD_CONN:
				return "Invalid connection";
			case SQL_QSEND_FAIL:
				return "Sending query failed";
			case SQL_QREPLY_FAIL:
				return "Getting query result failed";
			default:
				return "Unknown error";
		}
	}
};

/**
 * Object representing an SQL query. This should be allocated on the heap and
 * passed to an SQLProvider, which will free it when the query is complete or
 * when the querying module is unloaded.
 *
 * You should store whatever information is needed to have the callbacks work in
 * this object (UID of user, channel name, etc).
 */
class SQLQuery : public classbase
{
 public:
	ModuleRef creator;

	SQLQuery(Module* Creator) : creator(Creator) {}
	virtual ~SQLQuery() {}

	virtual void OnResult(SQLResult& result) = 0;
	/**
	 * Called when the query fails
	 */
	virtual void OnError(SQLerror& error) { }
};

/**
 * Provider object for SQL servers
 */
class SQLProvider : public DataProvider
{
 public:
	SQLProvider(Module* Creator, const std::string& Name) : DataProvider(Creator, Name) {}
	/** Submit an asynchronous SQL request
	 * @param callback The result reporting point
	 * @param query The hardcoded query string. If you have parameters to substitute, see below.
	 */
	virtual void submit(SQLQuery* callback, const std::string& query) = 0;

	/** Submit an asynchronous SQL request
	 * @param callback The result reporting point
	 * @param format The simple parameterized query string ('?' parameters)
	 * @param p Parameters to fill in for the '?' entries
	 */
	virtual void submit(SQLQuery* callback, const std::string& format, const ParamL& p) = 0;

	/** Submit an asynchronous SQL request.
	 * @param callback The result reporting point
	 * @param format The parameterized query string ('$name' parameters)
	 * @param p Parameters to fill in for the '$name' entries
	 */
	virtual void submit(SQLQuery* callback, const std::string& format, const ParamM& p) = 0;

	/** Convenience function to prepare a map from a User* */
	void PopulateUserInfo(User* user, ParamM& userinfo)
	{
		userinfo["nick"] = user->nick;
		userinfo["host"] = user->host;
		userinfo["ip"] = user->GetIPString();
		userinfo["gecos"] = user->fullname;
		userinfo["ident"] = user->ident;
		userinfo["server"] = user->server;
		userinfo["uuid"] = user->uuid;
	}
};

#endif
