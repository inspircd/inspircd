#ifndef __M_SQL_H__
#define __M_SQL_H__

using namespace std;

#include <string>
#include <vector>

#define SQL_RESULT 1
#define SQL_COUNT  2
#define SQL_ROW    3
#define SQL_ERROR  4
#define SQL_END    5
#define SQL_DONE   6
#define SQL_OK     7

// SQLRequest is inherited from a basic Request object
// so that we can neatly pass information around the
// system.

class SQLRequest : public classbase
{
 protected:
	long conn_id;
	int request_type;
	std::string thisquery;
 public:
	SQLRequest(int qt, long cid, std::string query)
	{
		this->SetQueryType(qt);
		this->SetConnID(cid);
		this->SetQuery(query);
	}

	void SetConnID(long id)
	{
		conn_id = id;
	}

	long GetConnID()
	{
		return conn_id;
	}

	void SetQueryType(int t)
	{
		request_type = t;
	}

	int GetQueryType()
	{
		return request_type;
	}

	void SetQuery(std::string query)
	{
		thisquery = query;
	}

	std::string GetQuery()
	{
		return thisquery;
	}
};

// Upon completion, an SQLRequest returns an SQLResponse.

class SQLResult : public classbase
{
 protected:
	int resptype;
	long count;
	std::string error;
	std::map<std::string,std::string> row;
 public:

	void SetRow(std::map<std::string,std::string> r)
	{
		row = r;
	}

	std::string GetField(std::string field)
	{
		std::map<std::string,std::string>::iterator iter = row.find(field);
		if (iter == row.end()) return "";
		return iter->second;
	}

	void SetType(int rt)
	{
		resptype = rt;
	}

	void SetError(std::string err)
	{
		error = err;
	}

	int GetType()
	{
		return resptype;
	}

	std::string GetError()
	{
		return error;
	}

	void SetCount(long c)
	{
		count = c;
	}

	/* This will return a negative value of the SQL server is down */
	long GetCount()
	{
		return count;
	}
};

class SQLQuery : public classbase
{
 private:
	SQLRequest* rowrequest;
	SQLRequest* query;
	SQLResult* result;
	SQLResult* rowresult;
	Request* rowquery;
	unsigned long dbid;
	Module* parent;
	Module* SQLModule;
	Server* Srv;
	std::string error;

	bool MakeQueryGoNow(std::string qry)
	{
		// Insert Lack of More Original Name here.
		Request queryrequest((char*)query, parent, SQLModule);
		result = (SQLResult*)queryrequest.Send();
		if (result->GetType() != SQL_ERROR)
		{
			// Query Is fine.. Prepare to get first row...
			rowrequest = new SQLRequest(SQL_ROW,dbid,"");
			rowquery = new Request((char*)rowrequest, parent, SQLModule);
			return true;
		}
		// Query Failed. - Coder Fucked up! (Probably me too :/)
		Srv->Log(DEBUG, " ============= SQL Error, Query And Error Follow. ============= ");
		Srv->Log(DEBUG, "Query: "+ qry);
		Srv->Log(DEBUG, "Error: "+ result->GetError());
		Srv->Log(DEBUG, " ============================================================== ");
		error = result->GetError();
		// Destroy Variables that were set..
		delete query;
		query = NULL;
		result = NULL;
		return false;
	}

 public:

	SQLQuery(Server* S) : Srv(S)
	{
	}

	SQLQuery(Module* a, unsigned long b, Server* S) : dbid(b), parent(a), Srv(S)
	{
		// Make a few useful variables..
		SQLModule = Srv->FindModule("m_sql.so");
	}

	~SQLQuery()
	{
	}

	bool Query(std::string qry)
	{
		query = new SQLRequest(SQL_RESULT, dbid, qry);
		return MakeQueryGoNow(qry);
	}

	bool QueryCount(std::string qry)
	{
		query = new SQLRequest(SQL_COUNT, dbid, qry);
		return MakeQueryGoNow(qry);
	}

	bool GetRow()
	{
		rowresult = (SQLResult*)rowquery->Send();
		if (rowresult->GetType() == SQL_ROW)
		{
			// We have got a row.. thats all for now.
			return true;
		}
		// No Row, Error, or end. KILL CALLER! *BANG*
		return false;
	}

	std::string GetField(std::string fname)
	{
		return rowresult->GetField(fname);
	}

	long GetCount()
	{
		rowresult = (SQLResult*)rowquery->Send();
		if (rowresult->GetType() == SQL_COUNT)
		{
			return rowresult->GetCount();
		}
		else
		{
			return 0;
		}
	}

	const std::string &GetError()
	{
		return error;
	}

	void SQLDone()
	{
		// Tell m_sql we are finished..
		query->SetQueryType(SQL_DONE);
		query->SetConnID(dbid);
		Request donerequest((char*)query, parent, SQLModule);
		donerequest.Send();

		// Do Some Clearing up.
		delete query;
		delete rowrequest;
		// Null the variables, so they can be re-used without confusion..
		result = NULL;
		query = NULL;
		rowrequest = NULL;
		rowresult = NULL;
	}

	static std::string Sanitise(const std::string& crap)
	{
		std::string temp = "";
		for (unsigned int q = 0; q < crap.length(); q++)
 		{
			if (crap[q] == '\'')
			{
				temp += "\\'";
			}
			else if (crap[q] == '"')
			{
				temp += "\\\"";
			}
			else if (crap[q] == '\\')
			{
				temp += "\\\\";
			}
			else
			{
				temp += crap[q];
			}
		}
		return temp;
	}
};


#endif
