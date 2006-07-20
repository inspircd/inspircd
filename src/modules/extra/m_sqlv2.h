#ifndef INSPIRCD_SQLAPI_2
#define INSPIRCD_SQLAPI_2

#define SQLREQID "SQLv2 Request"
#define SQLRESID "SQLv2 Result"
#define SQLSUCCESS "You shouldn't be reading this (success)"

#include <string>
#include <vector>
#include <map>
#include "modules.h"

enum SQLerrorNum { NO_ERROR, BAD_DBID, BAD_CONN, QSEND_FAIL };

class SQLexception : public ModuleException
{
};

class SQLbadColName : public SQLexception
{
public:
	SQLbadColName() { }
};

class SQLerror : public classbase
{
	SQLerrorNum id;
	std::string str;
public:
	SQLerror(SQLerrorNum i = NO_ERROR, const std::string &s = "")
	: id(i), str(s)
	{	
	}
	
	SQLerrorNum Id()
	{
		return id;
	}
	
	SQLerrorNum Id(SQLerrorNum i)
	{
		id = i;
		return id;
	}
	
	void Str(const std::string &s)
	{
		str = s;
	}
	
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
			default:
				return "Unknown error";				
		}
	}
};

class SQLrequest : public Request
{
public:
	std::string query;
	std::string dbid;
	bool pri;
	unsigned long id;
	SQLerror error;
	
	SQLrequest(Module* s, Module* d, const std::string &q, const std::string &id, bool p = false)
	: Request(SQLREQID, s, d), query(q), dbid(id), pri(p), id(0)
	{
	}
	
	void SetSource(Module* mod)
	{
		source = mod;
	}
};

class SQLfield
{
public:
	/* The data itself */
	std::string d;

	/* If the field was null */
	bool null;

	SQLfield(const std::string &data, bool n)
	: d(data), null(n)
	{
		
	}
};

typedef std::vector<SQLfield> SQLfieldList;
typedef std::map<std::string, SQLfield> SQLfieldMap;

class SQLresult : public Request
{
public:
	std::string query;
	std::string dbid;
	SQLerror error;	

	SQLresult(Module* s, Module* d)
	: Request(SQLRESID, s, d)
	{
	}
	
	/* Return the number of rows in the result */
	virtual int Rows() = 0;
	
	/* Return the number of columns in the result */
	virtual int Cols() = 0;
	
	/* Get a string name of the column by an index number */
	virtual std::string ColName(int column) = 0;
	
	/* Get an index number for a column from a string name.
	 * An exception of type SQLbadColName will be thrown if
	 * the name given is invalid.
	 */
	virtual int ColNum(const std::string &column) = 0;
	
	/* Get a string value in a given row and column */
	virtual SQLfield GetValue(int row, int column) = 0;
	
	/* Return a list of values in a row, this should
	 * increment an internal counter so you can repeatedly
	 * call it until it returns an empty vector.
	 * This returns a reference to an internal object,
	 * the same object is used for all calls to this function
	 * and therefore the return value is only valid until
	 * you call this function again. It is also invalid if
	 * the SQLresult object is destroyed.
	 */
	virtual SQLfieldList& GetRow() = 0;
	
	/* As above, but return a map indexed by key name */
	virtual SQLfieldMap& GetRowMap() = 0;
	
	/* Like GetRow(), but returns a pointer to a dynamically
	 * allocated object which must be explicitly freed. For
	 * portability reasons this must be freed with SQLresult::Free()
	 */
	virtual SQLfieldList* GetRowPtr() = 0;
	
	/* As above, but return a map indexed by key name */
	virtual SQLfieldMap* GetRowMapPtr() = 0;
	
	/* Overloaded function for freeing the lists and maps returned
	 * above.
	 */
	virtual void Free(SQLfieldMap* fm) = 0;
	virtual void Free(SQLfieldList* fl) = 0;
};

#endif
