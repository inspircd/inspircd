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

/* $ModDesc: Allows storage of oper credentials in an SQL table */

Server *Srv;

class ModuleSQLOper : public Module
{
	ConfigReader* Conf;
	unsigned long dbid;
	Module* SQLModule;

 public:
	bool ReadConfig()
	{
		dbid = Conf->ReadInteger("sqloper","dbid",0,true);	// database id of a database configured in m_sql (see m_sql config)
		SQLModule = Srv->FindModule("m_sql.so");
		if (!SQLModule)
			Srv->Log(DEFAULT,"WARNING: m_sqloper.so could not initialize because m_sql.so is not loaded. Load the module and rehash your server.");
		return (SQLModule);
	}

	ModuleSQLOper(Server* Me)
		: Module::Module(Me)
	{
		Srv = Me;
		Conf = new ConfigReader();
		ReadConfig();
	}

	virtual void OnRehash(std::string parameter)
	{
		delete Conf;
		Conf = new ConfigReader();
		ReadConfig();
	}

	void Implements(char* List)
	{
		List[I_OnRehash] = List[I_OnPreCommand] = 1;
	}

	virtual int OnPreCommand(std::string command, char **parameters, int pcnt, userrec *user)
	{
		if (command == "OPER")
		{
			if (LookupOper(parameters[0],parameters[1],user))
				return 1;
		}
		return 0;
	}

	bool LookupOper(std::string username, std::string password, userrec* user)
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
		temp = "";
		for (unsigned int v = 0; v < username.length(); v++)
		{
			if (username[v] == '\'')
			{
				temp = temp + "\'";
			}
			if (username[v] == '"')
			{
				temp = temp + "\\\"";
			}
			else temp = temp + username[v];
		}
		username = temp;

		// Create a request containing the SQL query and send it to m_sql.so
		SQLRequest* query = new SQLRequest(SQL_RESULT,dbid,"SELECT username,password,hostname,type FROM ircd_opers WHERE username='"+username+"' AND password=md5('"+password+"')");
		Request queryrequest((char*)query, this, SQLModule);
		SQLResult* result = (SQLResult*)queryrequest.Send();

		// Did we get "OK" as a result?
		if (result->GetType() == SQL_OK)
		{
			Srv->Log(DEBUG,"An SQL based oper exists");
			// if we did, this means we may now request a row... there should be only one row for each user, so,
			// we don't need to loop to fetch multiple rows.
			SQLRequest* rowrequest = new SQLRequest(SQL_ROW,dbid,"");
			Request rowquery((char*)rowrequest, this, SQLModule);
			SQLResult* rowresult = (SQLResult*)rowquery.Send();

			// did we get a row? If we did, we can now do something with the fields
			if (rowresult->GetType() == SQL_ROW)
			{
				if (rowresult->GetField("username") == username)
				{
					found = true;
					// oper up the user.
		                        for (int j =0; j < Conf->Enumerate("type"); j++)
		                        {
		                                std::string TypeName = Conf->ReadValue("type","name",j);
						std::string pattern = std::string(user->ident) + "@" + std::string(user->host);
		                                if ((TypeName == rowresult->GetField("type")) && (Srv->MatchText(pattern,rowresult->GetField("hostname"))));
		                                {
		                                        /* found this oper's opertype */
							std::string HostName = Conf->ReadValue("type","host",j);
							if (HostName != "")
			                                        Srv->ChangeHost(user,HostName);
		                                        strlcpy(user->oper,TypeName.c_str(),NICKMAX);
							WriteOpers("*** %s (%s@%s) is now an IRC operator of type %s",user->nick,user->ident,user->host,rowresult->GetField("type").c_str());
							WriteServ(user->fd,"381 %s :You are now an IRC operator of type %s",user->nick,rowresult->GetField("type").c_str());
					                if (!strchr(user->modes,'o'))
					                {
					                        strcat(user->modes,"o");
					                        WriteServ(user->fd,"MODE %s :+o",user->nick);
								Module* Logger = Srv->FindModule("m_sqllog.so");
								if (Logger)
									Logger->OnOper(user,rowresult->GetField("type"));
								AddOper(user);
					                        log(DEFAULT,"OPER: %s!%s@%s opered as type: %s",user->nick,user->ident,user->host,rowresult->GetField("type").c_str());
					                }
		                                        break;
                		                }
		                        }

				}
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
		query->SetConnID(dbid);
		Request donerequest((char*)query, this, SQLModule);
		donerequest.Send();
		delete query;
		return found;
	}

	virtual ~ModuleSQLOper()
	{
		delete Conf;
	}
	
	virtual Version GetVersion()
	{
		return Version(1,0,0,1,VF_VENDOR);
	}
	
};

class ModuleSQLOperFactory : public ModuleFactory
{
 public:
	ModuleSQLOperFactory()
	{
	}
	
	~ModuleSQLOperFactory()
	{
	}
	
	virtual Module * CreateModule(Server* Me)
	{
		return new ModuleSQLOper(Me);
	}
	
};


extern "C" void * init_module( void )
{
	return new ModuleSQLOperFactory;
}

