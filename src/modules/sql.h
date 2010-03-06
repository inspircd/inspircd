/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2010 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *          the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#ifndef INSPIRCD_SQLAPI_3
#define INSPIRCD_SQLAPI_3

/** Defines the error types which SQLerror may be set to
 */
enum SQLerrorNum { SQL_BAD_DBID, SQL_BAD_CONN, SQL_QSEND_FAIL, SQL_QREPLY_FAIL };

/** A list of format parameters for an SQLquery object.
 */
typedef std::vector<std::string> ParamL;

typedef std::map<std::string, std::string> ParamM;

/**
 * Result of an SQL query. Only valid inside OnResult
 */
class SQLResult : public interfacebase
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
	virtual bool GetRow(std::vector<std::string>& result) = 0;
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
	const std::string dbid;
	const std::string query;

	SQLQuery(Module* Creator, const std::string& db, const std::string& q)
		: creator(Creator), dbid(db), query(q) {}
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
	 * @param dbid The database ID to apply the request to
	 * @param query The query string
	 * @param callback The callback that the result is sent to
	 */
	virtual void submit(SQLQuery* query) = 0;

	/** Format a parameterized query string using proper SQL escaping.
	 * @param q The query string, with '?' parameters
	 * @param p The parameters to fill in in the '?' slots
	 */
	virtual std::string FormatQuery(std::string q, ParamL p) = 0;

	/** Format a parameterized query string using proper SQL escaping.
	 * @param q The query string, with '$foo' parameters
	 * @param p The map to look up parameters in
	 */
	virtual std::string FormatQuery(std::string q, ParamM p) = 0;
};

#endif
