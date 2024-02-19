/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2015 Daniel Vassdal <shutter@canternet.org>
 *   Copyright (C) 2014 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2013, 2017-2019, 2021-2022 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
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


#pragma once

namespace SQL
{
	class Error;
	class Provider;
	class Query;
	class Result;

	/** A list of parameter replacement values. */
	typedef std::vector<std::string> ParamList;

	/** A map of parameter replacement values. */
	typedef std::map<std::string, std::string> ParamMap;

	/** A single SQL field. */
	typedef std::optional<std::string> Field;

	/** A list of SQL fields from a specific row. */
	typedef std::vector<Field> Row;

	/** An enumeration of possible error codes. */
	enum ErrorCode
	{
		/** No error has occurred. */
		SUCCESS,

		/** The database identifier is invalid. */
		BAD_DBID,

		/** The database connection has failed. */
		BAD_CONN,

		/** Executing the query failed. */
		QSEND_FAIL,

		/** Reading the response failed. */
		QREPLY_FAIL
	};

	/** Populates a parameter map with information about a user.
	 * @param user The user to collect information from.
	 * @param userinfo The map to populate.
	 */
	void PopulateUserInfo(User* user, ParamMap& userinfo);
}

/** Represents the result of an SQL query. */
class SQL::Result
	: public Cullable
{
public:
	/**
	 * Return the number of rows in the result.
	 *
	 * Note that if you have performed an INSERT or UPDATE query or other
	 * query which will not return rows, this will return the number of
	 * affected rows. In this case you SHOULD NEVER access any of the result
	 * set rows, as there aren't any!
	 * @returns Number of rows in the result set.
	 */
	virtual int Rows() = 0;

	/** Retrieves the next available row from the database.
	 * @param result A list to store the fields from this row in.
	 * @return True if a row could be retrieved; otherwise, false.
	 */
	virtual bool GetRow(Row& result) = 0;

	/** Retrieves a list of SQL columns in the result.
	 * @param result A reference to the vector to store column names in.
	 */
	virtual void GetCols(std::vector<std::string>& result) = 0;

	/**
	 * Check if there's a column with the specified name in the result
	 *
	 * @param column The column name.
	 * @param index The place to store the column index if it exists.
	 * @returns If the column exists then true; otherwise, false.
	 */
	virtual bool HasColumn(const std::string& column, size_t& index) = 0;
};

/** SQL::Error holds the error state of a request.
 * The error string varies from database software to database software
 * and should be used to display informational error messages to users.
 */
class SQL::Error final
{
private:
	/** The custom error message if one has been specified. */
	const std::string message;

public:
	/** The code which represents this error. */
	const ErrorCode code;

	/** Initialize an SQL::Error from an error code.
	 * @param c A code which represents this error.
	 */
	Error(ErrorCode c)
		: code(c)
	{
	}

	/** Initialize an SQL::Error from an error code and a custom error message.
	 * @param c A code which represents this error.
	 * @param m A custom error message.
	 */
	Error(ErrorCode c, const std::string& m)
		: message(m)
		, code(c)
	{
	}

	/** Retrieves the error message. */
	const char* ToString() const
	{
		if (!message.empty())
			return message.c_str();

		switch (code)
		{
			case BAD_DBID:
				return "Invalid database identifier";
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

/**
 * Object representing an SQL query. This should be allocated on the heap and
 * passed to an SQL::Provider, which will free it when the query is complete or
 * when the querying module is unloaded.
 *
 * You should store whatever information is needed to have the callbacks work in
 * this object (UID of user, channel name, etc).
 */
class SQL::Query
	: public Cullable
{
protected:
	/** Creates a new SQL query. */
	Query(Module* Creator)
		: creator(Creator)
	{
	}

public:
	const ModuleRef creator;

	/** Called when an SQL error happens.
	 * @param error The error that occurred.
	 */
	virtual void OnError(const SQL::Error& error) = 0;

	/** Called when a SQL result is received.
	 * @param result The result of the SQL query.
	 */
	virtual void OnResult(SQL::Result& result) = 0;
};

/**
 * Provider object for SQL servers
 */
class SQL::Provider
	: public DataProvider
{
private:
	/** The name of the database tag in the config. */
	const std::string dbid;

public:
	Provider(Module* Creator, const std::string& Name)
		: DataProvider(Creator, "SQL/" + Name)
		, dbid(Name)
	{
	}

	/** Retrieves the name of the database tag in the config. */
	const std::string& GetId() const { return dbid; }

	/** Submit an asynchronous SQL query.
	 * @param callback The result reporting point
	 * @param query The hardcoded query string. If you have parameters to substitute, see below.
	 */
	virtual void Submit(Query* callback, const std::string& query) = 0;

	/** Submit an asynchronous SQL query.
	 * @param callback The result reporting point
	 * @param format The simple parameterized query string ('?' parameters)
	 * @param p Parameters to fill in for the '?' entries
	 */
	virtual void Submit(Query* callback, const std::string& format, const SQL::ParamList& p) = 0;

	/** Submit an asynchronous SQL query.
	 * @param callback The result reporting point
	 * @param format The parameterized query string ('$name' parameters)
	 * @param p Parameters to fill in for the '$name' entries
	 */
	virtual void Submit(Query* callback, const std::string& format, const ParamMap& p) = 0;
};

inline void SQL::PopulateUserInfo(User* user, ParamMap& userinfo)
{
	userinfo.insert({
		{ "address", user->GetAddress()       },
		{ "dhost",   user->GetDisplayedHost() },
		{ "duser",   user->GetDisplayedUser() },
		{ "host",    user->GetRealHost()      },
		{ "nick",    user->nick               },
		{ "real",    user->GetRealName()      },
		{ "server",  user->server->GetName()  },
		{ "sid",     user->server->GetId()    },
		{ "user",    user->GetRealUser()      },
		{ "uuid",    user->uuid               },
	});

	// Deprecated keys.
	userinfo.insert({
		{ "ident", userinfo["user"]    },
		{ "ip" ,   userinfo["address"] },
	});
};
