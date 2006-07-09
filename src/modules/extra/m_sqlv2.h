#ifndef INSPIRCD_SQLAPI_2
#define INSPIRCD_SQLAPI_2

#define SQLREQID "SQLv2 Request"
#define SQLRESID "SQLv2 Result"
#define SQLSUCCESS "You shouldn't be reading this (success)"

#include <string>
#include "modules.h"

enum SQLerrorNum { NO_ERROR, BAD_DBID, BAD_CONN, QSEND_FAIL };

class SQLerror
{
	SQLerrorNum id;
	std::string str;
public:
	SQLerror(SQLerrorNum i = NO_ERROR, const std::string &s = "")
	: id(i), str(s)
	{	
	}
	
	void Id(SQLerrorNum i)
	{
		id = i;
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
	SQLerror error;
	
	SQLrequest(Module* s, Module* d, const std::string &q, const std::string &id, bool p = false)
	: Request(SQLREQID, s, d), query(q), dbid(id), pri(p)
	{
	}	
};

class SQLresult : public Request
{
	
public:
	SQLresult(Module* s, Module* d)
	: Request(SQLRESID, s, d)
	{
		
	}
	
	virtual int Rows()
	{
		return 0;
	}
};

#endif
