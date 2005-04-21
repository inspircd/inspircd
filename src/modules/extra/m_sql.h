#ifndef __M_SQL_H__
#define __M_SQL_H__

#include <string>
#include <vector>

#define SQL_RESULT 1
#define SQL_COUNT  2
#define SQL_ROW    3
#define SQL_ERROR  4
#define SQL_END    5

// SQLRequest is inherited from a basic Request object
// so that we can neatly pass information around the
// system.

class SQLRequest : public Request
{
 protected:
	long conn_id;
	int request_type;
	std::string thisquery;
 public:
	void SetConnID(long id)
	{
		conn_id = id;
	}

	long GetConnID(long id)
	{
		return conn_id;
	}

	void SetQueryType(int t)
	{
		request_type = t;
	}

	int GetQueryType(int t)
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

class SQLResponse
{
 protected:
	int resptype;
	unsigned long count;
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
		return restype;
	}

	std::string GetError()
	{
		return error;
	}

	void SetCount(unsigned long c)
	{
		count = c;
	}

	unsigned long GetCount()
	{
		return count;
	}
};

#endif
