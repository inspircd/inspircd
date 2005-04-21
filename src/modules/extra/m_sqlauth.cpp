/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  Inspire is copyright (C) 2002-2004 ChatSpike-Dev.
 *                       E-mail:
 *                <brain@chatspike.net>
 *           	  <Craig@chatspike.net>
 *     
 * Written by Craig Edwards, Craig McLure, and others.
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include <stdio.h>
#include <string>
#include <stdlib.h>
#include <time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include "users.h"
#include "channels.h"
#include "modules.h"
#include "inspircd.h"
#include "m_sql.h"

/* $ModDesc: An SQL test module */

Server *Srv;

class ModuleSQLAuth : public Module
{
	ConfigReader* Conf;
	std::string usertable;
	unsigned long dbid;
	Module* SQLModule;

 public:
	bool ReadConfig()
	{
		Conf = new ConfigReader();
		usertable = Conf->ReadValue("sqlauth","usertable",0);
		dbid = Conf->ReadInteger("sqlauth","dbid",0,true);
		delete Conf;
		SQLModule = Srv->FindModule("m_sql.so");
		return (SQLModule);
	}

	ModuleSQLAuth()
	{
		Srv = new Server;
		ReadConfig();
	}

	virtual void OnRehash()
	{
		ReadConfig();
	}

	bool CheckCredentials(std::string username, std::string password,std::string usertable)
	{
		bool found = false;

		// is the sql module loaded? If not, we don't attempt to do anything.
		if (!SQLModule)
			return false;

		// Create a request containing the SQL query and send it to m_sql.so
		SQLRequest* query = new SQLRequest(SQL_RESULT,1,"SELECT * FROM "+usertable+" WHERE user='"+username+"' AND pass=md5('"+password+"')");
		Request queryrequest((char*)query, this, SQLModule);
		SQLResult* result = (SQLResult*)queryrequest.Send();

		// Did we get "OK" as a result?
		if (result->GetType() == SQL_OK)
		{

			// if we did, this means we may now request a row... there should be only one row for each user, so,
			// we don't need to loop to fetch multiple rows.
			SQLRequest* rowrequest = new SQLRequest(SQL_ROW,1,"");
			Request rowquery((char*)rowrequest, this, SQLModule);
			SQLResult* rowresult = (SQLResult*)rowquery.Send();

			// did we get a row? If we did, we can now do something with the fields
			if (rowresult->GetType() == SQL_ROW)
			{
				Srv->Log(DEBUG,"*********** SQL TEST MODULE - RESULTS *************");
				Srv->Log(DEBUG,"Result, field 'qcount': '" + rowrequest->GetField("qcount"));
				Srv->Log(DEBUG,"Result, field 'asked': '" + rowrequest->GetField("asked"));
				found = true;
				delete rowresult;
			}
			else
			{
				// we didn't have a row.
				found = false;
			}
			delete rowrequest;
			delete result;
		}
		else
		{
			// the query was bad
			found = false;
		}
		query->SetQueryType(SQL_DONE);
		query->SetConnID(1);
		Request donerequest((char*)query, this, SQLModule);
		donerequest.Send();
		delete query;
		return found;
	}

	virtual bool OnCheckReady(userrec* user)
	{
	}

        virtual void OnUserDisconnect(userrec* user)
        {
	}
	
	virtual ~ModuleSQLAuth()
	{
		delete Srv;
	}
	
	virtual Version GetVersion()
	{
		return Version(1,0,0,1,VF_VENDOR);
	}
	
};

class ModuleSQLAuthFactory : public ModuleFactory
{
 public:
	ModuleSQLAuthFactory()
	{
	}
	
	~ModuleSQLAuthFactory()
	{
	}
	
	virtual Module * CreateModule()
	{
		return new ModuleSQLAuth;
	}
	
};


extern "C" void * init_module( void )
{
	return new ModuleSQLAuthFactory;
}

