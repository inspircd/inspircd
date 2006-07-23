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

#include <string>

#include "users.h"
#include "channels.h"
#include "modules.h"
#include "inspircd.h"
#include "configreader.h"
#include "helperfuncs.h"
#include "m_sqlv2.h"
#include "m_sqlutils.h"
#include "commands/cmd_oper.h"

/* $ModDesc: Allows storage of oper credentials in an SQL table */

/* Required for the FOREACH_MOD alias (OnOper event) */
extern int MODCOUNT;
extern ServerConfig* Config;
extern std::vector<Module*> modules;
extern std::vector<ircd_module*> factory;

class ModuleSQLOper : public Module
{
	Server* Srv;
	Module* SQLutils;
	std::string databaseid;

public:
	ModuleSQLOper(Server* Me)
	: Module::Module(Me), Srv(Me)
	{
		SQLutils = Srv->FindFeature("SQLutils");
		
		if(SQLutils)
		{
			log(DEBUG, "Successfully got SQLutils pointer");
		}
		else
		{
			log(DEFAULT, "ERROR: This module requires a module offering the 'SQLutils' feature (usually m_sqlutils.so). Please load it and try again.");
			throw ModuleException("This module requires a module offering the 'SQLutils' feature (usually m_sqlutils.so). Please load it and try again.");
		}
		
		OnRehash("");
	}

	virtual void OnRehash(const std::string &parameter)
	{
		ConfigReader Conf;
		
		databaseid = Conf.ReadValue("sqloper", "dbid", 0); /* Database ID of a database configured for the service provider module */
	}

	void Implements(char* List)
	{
		List[I_OnRequest] = List[I_OnRehash] = List[I_OnPreCommand] = 1;
	}

	virtual int OnPreCommand(const std::string &command, const char** parameters, int pcnt, userrec *user, bool validated)
	{
		if (validated && (command == "OPER"))
		{
			if(LookupOper(user, parameters[0], parameters[1]))
			{	
				/* Returning true here just means the query is in progress, or on it's way to being
				 * in progress. Nothing about the /oper actually being successful..
				 */
				return 1;
			}
		}

		return 0;
	}

	bool LookupOper(userrec* user, const std::string &username, const std::string &password)
	{
		Module* target;
		
		target = Srv->FindFeature("SQL");
		
		if(target)
		{
			SQLrequest req = SQLreq(this, target, databaseid, "SELECT username, password, hostname, type FROM ircd_opers WHERE username = '?' AND password=md5('?')", username, password);
			
			if(req.Send())
			{
				/* When we get the query response from the service provider we will be given an ID to play with,
				 * just an ID number which is unique to this query. We need a way of associating that ID with a userrec
				 * so we insert it into a map mapping the IDs to users.
				 * Thankfully m_sqlutils provides this, it will associate a ID with a user or channel, and if the user quits it removes the
				 * association. This means that if the user quits during a query we will just get a failed lookup from m_sqlutils - telling
				 * us to discard the query.
			 	 */
				log(DEBUG, "Sent query, got given ID %lu", req.id);
				
				AssociateUser(this, SQLutils, req.id, user).Send();
					
				return true;
			}
			else
			{
				log(DEBUG, "SQLrequest failed: %s", req.error.Str());
			
				return false;
			}
		}
		else
		{
			log(SPARSE, "WARNING: Couldn't find SQL provider module. NOBODY will be able to oper up unless their o:line is statically configured");
			return false;
		}
	}
	
	virtual char* OnRequest(Request* request)
	{
		if(strcmp(SQLRESID, request->GetData()) == 0)
		{
			SQLresult* res;
		
			res = static_cast<SQLresult*>(request);
			
			log(DEBUG, "Got SQL result (%s) with ID %lu", res->GetData(), res->id);
			
			userrec* user = GetAssocUser(this, SQLutils, res->id).S().user;
			UnAssociate(this, SQLutils, res->id).S();
			
			if(user)
			{
				if(res->error.Id() == NO_ERROR)
				{				
					log(DEBUG, "Associated query ID %lu with user %s", res->id, user->nick);			
					log(DEBUG, "Got result with %d rows and %d columns", res->Rows(), res->Cols());
			
					if(res->Rows())
					{
						/* We got a row in the result, this means there was a record for the oper..
						 * now we just need to check if their host matches, and if it does then
						 * oper them up.
						 * 
						 * We now (previous versions of the module didn't) support multiple SQL
						 * rows per-oper in the same way the config file does, all rows will be tried
						 * until one is found which matches. This is useful to define several different
						 * hosts for a single oper.
						 * 
						 * The for() loop works as SQLresult::GetRowMap() returns an empty map when there
						 * are no more rows to return.
						 */
						
						for(SQLfieldMap& row = res->GetRowMap(); row.size(); row = res->GetRowMap())
						{
							log(DEBUG, "Trying to oper user %s with username = '%s', passhash = '%s', hostname = '%s', type = '%s'", user->nick, row["username"].d.c_str(), row["password"].d.c_str(), row["hostname"].d.c_str(), row["type"].d.c_str());
							
							if(OperUser(user, row["username"].d, row["password"].d, row["hostname"].d, row["type"].d))
							{
								/* If/when one of the rows matches, stop checking and return */
								return SQLSUCCESS;
							}
						}
					}
					else
					{
						/* No rows in result, this means there was no oper line for the user,
						 * we should have already checked the o:lines so now we need an
						 * "insufficient awesomeness" (invalid credentials) error
						 */
						
						WriteServ(user->fd, "491 %s :Invalid oper credentials", user->nick);
						WriteOpers("*** WARNING! Failed oper attempt by %s!%s@%s!", user->nick, user->ident, user->host);
						log(DEFAULT,"OPER: Failed oper attempt by %s!%s@%s: user, host or password did not match.", user->nick, user->ident, user->host);
					}
				}
				else
				{
					/* This one shouldn't happen, the query failed for some reason.
					 * We have to fail the /oper request and give them the same error
					 * as above.
					 */
					log(DEBUG, "Query failed: %s", res->error.Str());

					WriteServ(user->fd, "491 %s :Invalid oper credentials", user->nick);
					WriteOpers("*** WARNING! Failed oper attempt by %s!%s@%s! (SQL query failed: %s)", user->nick, user->ident, user->host, res->error.Str());
					log(DEFAULT,"OPER: Failed oper attempt by %s!%s@%s: user, host or password did not match.", user->nick, user->ident, user->host);
				}
			}
			else
			{
				log(DEBUG, "Got query with unknown ID, this probably means the user quit while the query was in progress");
			}
		
			return SQLSUCCESS;
		}
		
		log(DEBUG, "Got unsupported API version string: %s", request->GetData());
		
		return NULL;
	}	

	bool OperUser(userrec* user, const std::string &username, const std::string &password, const std::string &hostname, const std::string &type)
	{
		ConfigReader Conf;
		
		for (int j = 0; j < Conf.Enumerate("type"); j++)
		{
			std::string TypeName = Conf.ReadValue("type","name",j);
			
			Srv->Log(DEBUG,"Scanning opertype: "+TypeName);
			
			std::string pattern = std::string(user->ident) + "@" + std::string(user->host);
							
			if((TypeName == type) && OneOfMatches(pattern.c_str(), hostname.c_str()))
			{
				/* found this oper's opertype */
				Srv->Log(DEBUG,"Host and type match: "+TypeName+" "+type);
				
				std::string HostName = Conf.ReadValue("type","host",j);
							
				if(HostName.size())
					Srv->ChangeHost(user, HostName);
								
				strlcpy(user->oper, type.c_str(), NICKMAX-1);
				
				WriteOpers("*** %s (%s@%s) is now an IRC operator of type %s", user->nick, user->ident, user->host, type.c_str());
				WriteServ(user->fd,"381 %s :You are now an IRC operator of type %s", user->nick, type.c_str());
				
				if(user->modes[UM_OPERATOR])
				{
					user->modes[UM_OPERATOR] = 1;
					WriteServ(user->fd,"MODE %s :+o",user->nick);
					FOREACH_MOD(I_OnOper,OnOper(user, type));
					AddOper(user);
					FOREACH_MOD(I_OnPostOper,OnPostOper(user, type));
					log(DEFAULT,"OPER: %s!%s@%s opered as type: %s", user->nick, user->ident, user->host, type.c_str());
				}
								
				return true;
			}
		}
		
		return false;
	}

	virtual ~ModuleSQLOper()
	{
	}
	
	virtual Version GetVersion()
	{
		return Version(1,0,1,0,VF_VENDOR);
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
