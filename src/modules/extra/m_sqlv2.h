#ifndef INSPIRCD_SQLAPI_2
#define INSPIRCD_SQLAPI_2

#define SQLREQID "SQLv2 Request"
#define SQLRESID "SQLv2 Result"
#define SQLSUCCESS "You shouldn't be reading this (success)"

#include <string>
#include "modules.h"

enum SQLerrorNum { BAD_DBID };

class SQLerror
{
	SQLerrorNum id;
public:
	
	SQLerror()
	{	
	}
	
	SQLerror(SQLerrorNum i)
	: id(i)
	{	
	}
	
	void Id(SQLerrorNum i)
	{
		id = i;
	}
	
	const char* Str()
	{
		switch(id)
		{
			case BAD_DBID:
				return "Invalid database ID";
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
