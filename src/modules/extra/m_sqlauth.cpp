/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd is copyright (C) 2002-2004 ChatSpike-Dev.
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

using namespace std;

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
#include "helperfuncs.h"
#include "m_sql.h"

/* $ModDesc: Allow/Deny connections based upon an arbitary SQL table */

class ModuleSQLAuth : public Module
{
	Server* Srv;
	ConfigReader* Conf;
	std::string usertable;
	std::string userfield;
	std::string passfield;
	std::string encryption;
	std::string killreason;
	std::string allowpattern;
	bool WallOperFail;
	unsigned long dbid;
	Module* SQLModule;

 public:
	bool ReadConfig()
	{
		Conf = new ConfigReader();
		usertable = Conf->ReadValue("sqlauth","usertable",0);	// user table name
		dbid = Conf->ReadInteger("sqlauth","dbid",0,true);	// database id of a database configured in m_sql (see m_sql config)
		userfield = Conf->ReadValue("sqlauth","userfield",0);	// field name where username can be found
		passfield = Conf->ReadValue("sqlauth","passfield",0);	// field name where password can be found
		killreason = Conf->ReadValue("sqlauth","killreason",0);	// reason to give when access is denied to a user (put your reg details here)
		encryption = Conf->ReadValue("sqlauth","encryption",0);	// name of sql function used to encrypt password, e.g. "md5" or "passwd".
									// define, but leave blank if no encryption is to be used.
		WallOperFail = Conf->ReadFlag("sqlauth","verbose",0);	// set to 1 if failed connects should be reported to operators
		allowpattern = Conf->ReadValue("sqlauth","allowpattern",0); 	// allow nicks matching the pattern without requiring auth
		if (encryption.find("(") == std::string::npos)
		{
			encryption.append("(");
		}
		delete Conf;
		SQLModule = Srv->FindModule("m_sql.so");
		if (!SQLModule)
			Srv->Log(DEFAULT,"WARNING: m_sqlauth.so could not initialize because m_sql.so is not loaded. Load the module and rehash your server.");
		return (SQLModule);
	}

	ModuleSQLAuth(Server* Me)
		: Module::Module(Me)
	{
		Srv = Me;
		ReadConfig();
	}

	void Implements(char* List)
	{
		List[I_OnRehash] = List[I_OnUserRegister] = 1;
	}

	virtual void OnRehash(std::string parameter)
	{
		ReadConfig();
	}

	virtual void OnUserRegister(userrec* user)
	{
		if ((allowpattern != "") && (Srv->MatchText(user->nick,allowpattern)))
			return;
		
		if (!CheckCredentials(user->nick,user->password))
		{
			if (WallOperFail)
				WriteOpers("Forbidden connection from %s!%s@%s (invalid login/password)",user->nick,user->ident,user->host);
			Srv->QuitUser(user,killreason);
		}
	}

	bool CheckCredentials(std::string username, std::string password)
	{
		bool found = false;

		// is the sql module loaded? If not, we don't attempt to do anything.
		if (!SQLModule)
			return false;

		// sanitize the password (we dont want any mysql insertion exploits!)
		std::string temp = "";
		for (unsigned int q = 0; q < password.length(); q++)
		{
			if (password[q] == '\'')
			{
				temp = temp + "\'";
			}
			else if (password[q] == '"')
			{
				temp = temp + "\\\"";
			}
			else temp = temp + password[q];
		}
		password = temp;

		// Create a request containing the SQL query and send it to m_sql.so
		std::string querystr("SELECT * FROM "+usertable+" WHERE "+userfield+"='"+username+"' AND "+passfield+"="+encryption+"'"+password+"')");
		
		Srv->Log(DEBUG, "m_sqlauth.so: Query: " + querystr);
		
		SQLRequest* query = new SQLRequest(SQL_RESULT,dbid,querystr);
		Request queryrequest((char*)query, this, SQLModule);
		SQLResult* result = (SQLResult*)queryrequest.Send();

		// Did we get "OK" as a result?
		if (result->GetType() == SQL_OK)
		{
			log(DEBUG, "m_sqlauth.so: Query OK");
			
			// if we did, this means we may now request a row... there should be only one row for each user, so,
			// we don't need to loop to fetch multiple rows.
			SQLRequest* rowrequest = new SQLRequest(SQL_ROW,dbid,"");
			Request rowquery((char*)rowrequest, this, SQLModule);
			SQLResult* rowresult = (SQLResult*)rowquery.Send();

			// did we get a row? If we did, we can now do something with the fields
			if (rowresult->GetType() == SQL_ROW)
			{
				log(DEBUG, "m_sqlauth.so: Got row...user '%s'", rowresult->GetField(userfield).c_str());
				
				if (rowresult->GetField(userfield) == username)
				{
					log(DEBUG, "m_sqlauth.so: Got correct user...");
					// because the query directly asked for the password hash, we do not need to check it -
					// if it didnt match it wont be returned in the first place from the SELECT.
					// This just checks we didnt get an empty row by accident.
					found = true;
				}
			}
			else
			{
				log(DEBUG, "m_sqlauth.so: Couldn't find row");
				// we didn't have a row.
				found = false;
			}
			
			delete rowrequest;
			delete rowresult;
		}
		else
		{
			log(DEBUG, "m_sqlauth.so: Query failed");
			// the query was bad
			found = false;
		}
		
		query->SetQueryType(SQL_DONE);
		query->SetConnID(dbid);
		Request donerequest((char*)query, this, SQLModule);
		donerequest.Send();
		
		delete query;
		delete result;
		
		return found;
	}

	virtual ~ModuleSQLAuth()
	{
	}
	
	virtual Version GetVersion()
	{
		return Version(1,0,0,2,VF_VENDOR);
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
	
	virtual Module * CreateModule(Server* Me)
	{
		return new ModuleSQLAuth(Me);
	}
	
};


extern "C" void * init_module( void )
{
	return new ModuleSQLAuthFactory;
}

