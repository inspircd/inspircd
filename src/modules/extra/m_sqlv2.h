#ifndef INSPIRCD_SQLAPI_2
#define INSPIRCD_SQLAPI_2

#include <string>
#include <deque>
#include <map>
#include "modules.h"

/** SQLreq define.
 * This is the voodoo magic which lets us pass multiple
 * parameters to the SQLrequest constructor... voodoo...
 */
#define SQLreq(a, b, c, d, e...) SQLrequest(a, b, c, (SQLquery(d), ##e))

/** Identifiers used to identify Request types
 */
#define SQLREQID "SQLv2 Request"
#define SQLRESID "SQLv2 Result"
#define SQLSUCCESS "You shouldn't be reading this (success)"

/** Defines the error types which SQLerror may be set to
 */
enum SQLerrorNum { NO_ERROR, BAD_DBID, BAD_CONN, QSEND_FAIL, QREPLY_FAIL };

/** A list of format parameters for an SQLquery object.
 */
typedef std::deque<std::string> ParamL;

/** The base class of SQL exceptions
 */
class SQLexception : public ModuleException
{
};

/** An exception thrown when a bad column or row name or id is requested
 */
class SQLbadColName : public SQLexception
{
public:
	SQLbadColName() { }
};

/** SQLerror holds the error state of any SQLrequest or SQLresult.
 * The error string varies from database software to database software
 * and should be used to display informational error messages to users.
 */
class SQLerror : public classbase
{
	/** The error id
	 */
	SQLerrorNum id;
	/** The error string
	 */
	std::string str;
public:
	/** Initialize an SQLerror
	 * @param i The error ID to set
	 * @param s The (optional) error string to set
	 */
	SQLerror(SQLerrorNum i = NO_ERROR, const std::string &s = "")
	: id(i), str(s)
	{	
	}
	
	/** Return the ID of the error
	 */
	SQLerrorNum Id()
	{
		return id;
	}
	
	/** Set the ID of an error
	 * @param i The new error ID to set
	 * @return the ID which was set
	 */
	SQLerrorNum Id(SQLerrorNum i)
	{
		id = i;
		return id;
	}
	
	/** Set the error string for an error
	 * @param s The new error string to set
	 */
	void Str(const std::string &s)
	{
		str = s;
	}
	
	/** Return the error string for an error
	 */
	const char* Str()
	{
		if(str.length())
			return str.c_str();
		
		switch(id)
		{
			case NO_ERROR:
				return "No error";
			case BAD_DBID:
				return "Invalid database ID";
			case BAD_CONN:
				return "Invalid connection";
			case QSEND_FAIL:
				return "Sending query failed";
			case QREPLY_FAIL:
				return "Getting query result failed";
			default:
				return "Unknown error";				
		}
	}
};

/** SQLquery provides a way to represent a query string, and its parameters in a type-safe way.
 * C++ has no native type-safe way of having a variable number of arguments to a function,
 * the workaround for this isn't easy to describe simply, but in a nutshell what's really
 * happening when - from the above example - you do this:
 *
 * SQLrequest foo = SQLreq(this, target, "databaseid", "SELECT (foo, bar) FROM rawr WHERE foo = '?' AND bar = ?", "Hello", "42");
 *
 * what's actually happening is functionally this:
 *
 * SQLrequest foo = SQLreq(this, target, "databaseid", query("SELECT (foo, bar) FROM rawr WHERE foo = '?' AND bar = ?").addparam("Hello").addparam("42"));
 *
 * with 'query()' returning a reference to an object with a 'addparam()' member function which
 * in turn returns a reference to that object. There are actually four ways you can create a
 * SQLrequest..all have their disadvantages and advantages. In the real implementations the
 * 'query()' function is replaced by the constructor of another class 'SQLquery' which holds
 * the query string and a ParamL (std::deque<std::string>) of query parameters.
 * This is essentially the same as the above example except 'addparam()' is replaced by operator,(). The full syntax for this method is:
 *
 * SQLrequest foo = SQLrequest(this, target, "databaseid", (SQLquery("SELECT.. ?"), parameter, parameter));
 */
class SQLquery
{
public:
	/** The query 'format string'
	 */
	std::string q;
	/** The query parameter list
	 * There should be one parameter for every ? character
	 * within the format string shown above.
	 */
	ParamL p;

	/** Initialize an SQLquery with a given format string only
	 */
	SQLquery(const std::string &query)
	: q(query)
	{
		log(DEBUG, "SQLquery constructor: %s", q.c_str());
	}

	/** Initialize an SQLquery with a format string and parameters.
	 * If you provide parameters, you must initialize the list yourself
	 * if you choose to do it via this method, using std::deque::push_back().
	 */
	SQLquery(const std::string &query, const ParamL &params)
	: q(query), p(params)
	{
		log(DEBUG, "SQLquery constructor with %d params: %s", p.size(), q.c_str());
	}	
	
	/** An overloaded operator for pushing parameters onto the parameter list
	 */
	SQLquery& operator,(const std::string &foo)
	{
		p.push_back(foo);
		return *this;
	}
	
	/** An overloaded operator for pushing parameters onto the parameter list.
	 * This has higher precedence than 'operator,' and can save on parenthesis.
	 */
	SQLquery& operator%(const std::string &foo)
	{
		p.push_back(foo);
		return *this;
	}
};

/** SQLrequest is sent to the SQL API to command it to run a query and return the result.
 * You must instantiate this object with a valid SQLquery object and its parameters, then
 * send it using its Send() method to the module providing the 'SQL' feature. To find this
 * module, use Server::FindFeature().
 */
class SQLrequest : public Request
{
public:
	/** The fully parsed and expanded query string
	 * This is initialized from the SQLquery parameter given in the constructor.
	 */
	SQLquery query;
	/** The database ID to apply the request to
	 */
	std::string dbid;
	/** True if this is a priority query.
	 * Priority queries may 'queue jump' in the request queue.
	 */
	bool pri;
	/** The query ID, assigned by the SQL api.
	 * After your request is processed, this will
	 * be initialized for you by the API to a valid request ID,
	 * except in the case of an error.
	 */
	unsigned long id;
	/** If an error occured, error.id will be any other value than NO_ERROR.
	 */
	SQLerror error;
	
	/** Initialize an SQLrequest.
	 * For example:
	 *
	 * SQLrequest req = SQLreq(MyMod, SQLModule, dbid, "INSERT INTO ircd_log_actors VALUES('','?')", nick);
	 *
	 * @param s A pointer to the sending module, where the result should be routed
	 * @param d A pointer to the receiving module, identified as implementing the 'SQL' feature
	 * @param databaseid The database ID to perform the query on. This must match a valid
	 * database ID from the configuration of the SQL module.
	 * @param q A properly initialized SQLquery object.
	 */
	SQLrequest(Module* s, Module* d, const std::string &databaseid, const SQLquery &q)
	: Request(SQLREQID, s, d), query(q), dbid(databaseid), pri(false), id(0)
	{
	}
	
	/** Set the priority of a request.
	 */
	void Priority(bool p = true)
	{
		pri = p;
	}
	
	/** Set the source of a request. You should not need to use this method.
	 */
	void SetSource(Module* mod)
	{
		source = mod;
	}
};

/**
 * This class contains a field's data plus a way to determine if the field
 * is NULL or not without having to mess around with NULL pointers.
 */
class SQLfield
{
public:
	/**
	 * The data itself
	 */
	std::string d;

	/**
	 * If the field was null
	 */
	bool null;

	/** Initialize an SQLfield
	 */
	SQLfield(const std::string &data, bool n)
	: d(data), null(n)
	{
		
	}
};

/** A list of items which make up a row of a result or table (tuple)
 * This does not include field names.
 */
typedef std::vector<SQLfield> SQLfieldList;
/** A list of items which make up a row of a result or table (tuple)
 * This also includes the field names.
 */
typedef std::map<std::string, SQLfield> SQLfieldMap;

/** SQLresult is a reply to a previous query.
 * If you send a query to the SQL api, the response will arrive at your
 * OnRequest method of your module at some later time, depending on the
 * congestion of the SQL server and complexity of the query. The ID of
 * this result will match the ID assigned to your original request.
 * SQLresult contains its own internal cursor (row counter) which is
 * incremented with each method call which retrieves a single row.
 */
class SQLresult : public Request
{
public:
	/** The original query string passed initially to the SQL API
	 */
	std::string query;
	/** The database ID the query was executed on
	 */
	std::string dbid;
	/**
	 * The error (if any) which occured.
	 * If an error occured the value of error.id will be any
	 * other value than NO_ERROR.
	 */
	SQLerror error;	
	/**
	 * This will match  query ID you were given when sending
	 * the request at an earlier time.
	 */
	unsigned long id;

	/** Used by the SQL API to instantiate an SQLrequest
	 */
	SQLresult(Module* s, Module* d, unsigned long i)
	: Request(SQLRESID, s, d), id(i)
	{
	}
	
	/**
	 * Return the number of rows in the result
	 * Note that if you have perfomed an INSERT
	 * or UPDATE query or other query which will
	 * not return rows, this will return the
	 * number of affected rows, and SQLresult::Cols()
	 * will contain 0. In this case you SHOULD NEVER
	 * access any of the result set rows, as there arent any!
	 * @returns Number of rows in the result set.
	 */
	virtual int Rows() = 0;
	
	/**
	 * Return the number of columns in the result.
	 * If you performed an UPDATE or INSERT which
	 * does not return a dataset, this value will
	 * be 0.
	 * @returns Number of columns in the result set.
	 */
	virtual int Cols() = 0;
	
	/**
	 * Get a string name of the column by an index number
	 * @param column The id number of a column
	 * @returns The column name associated with the given ID
	 */
	virtual std::string ColName(int column) = 0;
	
	/**
	 * Get an index number for a column from a string name.
	 * An exception of type SQLbadColName will be thrown if
	 * the name given is invalid.
	 * @param column The column name to get the ID of
	 * @returns The ID number of the column provided
	 */
	virtual int ColNum(const std::string &column) = 0;
	
	/**
	 * Get a string value in a given row and column
	 * This does not effect the internal cursor.
	 * @returns The value stored at [row,column] in the table
	 */
	virtual SQLfield GetValue(int row, int column) = 0;
	
	/**
	 * Return a list of values in a row, this should
	 * increment an internal counter so you can repeatedly
	 * call it until it returns an empty vector.
	 * This returns a reference to an internal object,
	 * the same object is used for all calls to this function
	 * and therefore the return value is only valid until
	 * you call this function again. It is also invalid if
	 * the SQLresult object is destroyed.
	 * The internal cursor (row counter) is incremented by one.
	 * @returns A reference to the current row's SQLfieldList
	 */
	virtual SQLfieldList& GetRow() = 0;
	
	/**
	 * As above, but return a map indexed by key name.
	 * The internal cursor (row counter) is incremented by one.
	 * @returns A reference to the current row's SQLfieldMap
	 */
	virtual SQLfieldMap& GetRowMap() = 0;
	
	/**
	 * Like GetRow(), but returns a pointer to a dynamically
	 * allocated object which must be explicitly freed. For
	 * portability reasons this must be freed with SQLresult::Free()
	 * The internal cursor (row counter) is incremented by one.
	 * @returns A newly-allocated SQLfieldList
	 */
	virtual SQLfieldList* GetRowPtr() = 0;
	
	/**
	 * As above, but return a map indexed by key name
	 * The internal cursor (row counter) is incremented by one.
	 * @returns A newly-allocated SQLfieldMap
	 */
	virtual SQLfieldMap* GetRowMapPtr() = 0;
	
	/**
	 * Overloaded function for freeing the lists and maps
	 * returned by GetRowPtr or GetRowMapPtr.
	 * @param fm The SQLfieldMap to free
	 */
	virtual void Free(SQLfieldMap* fm) = 0;

	/**
	 * Overloaded function for freeing the lists and maps
	 * returned by GetRowPtr or GetRowMapPtr.
	 * @param fl The SQLfieldList to free
	 */
	virtual void Free(SQLfieldList* fl) = 0;
};

#endif
