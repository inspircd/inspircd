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

#define LT_OPER		1
#define LT_KILL		2
#define LT_SERVLINK	3
#define LT_XLINE	4
#define LT_CONNECT	5
#define LT_DISCONNECT	6
#define LT_FLOOD	7
#define LT_LOADMODULE	8

/* $ModDesc: Allow/Deny connections based upon an arbitary SQL table */

Server *Srv;

class ModuleSQLLog : public Module
{
	ConfigReader* Conf;
	unsigned long dbid;
	Module* SQLModule;

 public:
	bool ReadConfig()
	{
		Conf = new ConfigReader();
		dbid = Conf->ReadInteger("sqllog","dbid",0,true);	// database id of a database configured in m_sql (see m_sql config)
		delete Conf;
		SQLModule = Srv->FindModule("m_sql.so");
		if (!SQLModule)
			Srv->Log(DEFAULT,"WARNING: m_SQLLog.so could not initialize because m_sql.so is not loaded. Load the module and rehash your server.");
		return (SQLModule);
	}

	ModuleSQLLog()
	{
		Srv = new Server;
		ReadConfig();
	}

	virtual void OnRehash()
	{
		ReadConfig();
	}

	long InsertNick(std::string nick)
	{
		long nid = -1;
                SQLRequest* query = new SQLRequest(SQL_RESULT,dbid,"SELECT id,actor FROM ircd_log_actors WHERE actor='"+nick+"'");
                Request queryrequest((char*)query, this, SQLModule);
                SQLResult* result = (SQLResult*)queryrequest.Send();
                if (result->GetType() == SQL_OK)
                {
                        SQLRequest* rowrequest = new SQLRequest(SQL_ROW,dbid,"");
                        Request rowquery((char*)rowrequest, this, SQLModule);
                        SQLResult* rowresult = (SQLResult*)rowquery.Send();
                        if (rowresult->GetType() == SQL_ROW)
                        {
                                nid = atoi(rowresult->GetField("id").c_str());
                                delete rowresult;
                        }
                        delete rowrequest;
                        delete result;
                }
                query->SetQueryType(SQL_DONE);
                query->SetConnID(dbid);
                Request donerequest((char*)query, this, SQLModule);
                donerequest.Send();
                delete query;
		if (nid < 1)
		{
			SQLRequest* query = new SQLRequest(SQL_COUNT,dbid,"INSERT INTO ircd_log_actors VALUES('','"+nick+"')");
			Request queryrequest((char*)query, this, SQLModule);
			SQLResult* result = (SQLResult*)queryrequest.Send();
	                if (result->GetType() == SQL_ERROR)
	                {
	                        Srv->Log(DEFAULT,"SQL log error: " + result->GetError());
	                }
			if (result)
				delete result;
			if (query)
				delete query;
			nid = InsertNick(nick);
		}
		return nid;
	}

	void InsertEntry(long category,long nickid,long hostid,long sourceid,unsigned long date)
	{
		char querybuffer[MAXBUF];
		snprintf(querybuffer,MAXBUF,"INSERT INTO ircd_log VALUES('',%d,%d,%d,%d,%lu)",category,nickid,hostid,sourceid,date);
		SQLRequest* query = new SQLRequest(SQL_COUNT,dbid,querybuffer);
		Request queryrequest((char*)query, this, SQLModule);
		SQLResult* result = (SQLResult*)queryrequest.Send();
		if (result->GetType() == SQL_ERROR)
		{
			Srv->Log(DEFAULT,"SQL log error: " + result->GetError());
		}
		if (result)
			delete result;
		if (query)
			delete query;
		return;
	}

	long InsertHost(std::string host)
	{
                long hid = -1;
                SQLRequest* query = new SQLRequest(SQL_RESULT,dbid,"SELECT id,hostname FROM ircd_log_hosts WHERE hostname='"+host+"'");
                Request queryrequest((char*)query, this, SQLModule);
                SQLResult* result = (SQLResult*)queryrequest.Send();
                if (result->GetType() == SQL_OK)
                {
                        SQLRequest* rowrequest = new SQLRequest(SQL_ROW,dbid,"");
                        Request rowquery((char*)rowrequest, this, SQLModule);
                        SQLResult* rowresult = (SQLResult*)rowquery.Send();
                        if (rowresult->GetType() == SQL_ROW)
                        {
                                hid = atoi(rowresult->GetField("id").c_str());
                                delete rowresult;
                        }
                        delete rowrequest;
                        delete result;
                }
                query->SetQueryType(SQL_DONE);
                query->SetConnID(dbid);
                Request donerequest((char*)query, this, SQLModule);
                donerequest.Send();
                delete query;
                if (hid < 1)
                {
                        SQLRequest* query = new SQLRequest(SQL_COUNT,dbid,"INSERT INTO ircd_log_hosts VALUES('','"+host+"')");
                        Request queryrequest((char*)query, this, SQLModule);
                        SQLResult* result = (SQLResult*)queryrequest.Send();
        	        if (result->GetType() == SQL_ERROR)
	                {
	                        Srv->Log(DEFAULT,"SQL log error: " + result->GetError());
	                }
                        if (result)
                                delete result;
			if (query)
				delete query;
                        hid = InsertHost(host);
                }
                return hid;
	}

	void AddLogEntry(int category, std::string nick, std::string host, std::string source)
	{
		// is the sql module loaded? If not, we don't attempt to do anything.
		if (!SQLModule)
			return;

		long nickid = InsertNick(nick);
		long sourceid = InsertNick(source);
		long hostid = InsertHost(host);
		InsertEntry(category,nickid,hostid,sourceid,time(NULL));
	}

	virtual void OnOper(userrec* user)
	{
		AddLogEntry(LT_OPER,user->nick,user->host,user->server);
	}

	virtual int OnKill(userrec* source, userrec* dest, std::string reason)
	{
		AddLogEntry(LT_KILL,dest->nick,dest->host,source->nick);
		return 0;
	}

	virtual int OnMeshToken(char token,string_list params,serverrec* source,serverrec* reply, std::string tcp_host,std::string ipaddr,int port)
	{
		if ((token == 'U') || (token == 's') || (token == 'S'))
			AddLogEntry(LT_SERVLINK,tcp_host,ipaddr,Srv->GetServerName());
		return 0;
	}

	virtual int OnPreCommand(std::string command, char **parameters, int pcnt, userrec *user)
	{
		if ((command == "GLINE") || (command == "KLINE") || (command == "ELINE") || (command == "ZLINE"))
			AddLogEntry(LT_XLINE,user->nick,parameters[0],user->server);
		return 0;
	}

	virtual void OnUserConnect(userrec* user)
	{
		AddLogEntry(LT_CONNECT,user->nick,user->host,user->server);
	}

	virtual void OnUserQuit(userrec* user)
	{
		AddLogEntry(LT_DISCONNECT,user->nick,user->host,user->server);
	}

	virtual void OnLoadModule(Module* mod,std::string name)
	{
		AddLogEntry(LT_LOADMODULE,name,Srv->GetServerName(),Srv->GetServerName());
	}

	virtual ~ModuleSQLLog()
	{
		delete Srv;
	}
	
	virtual Version GetVersion()
	{
		return Version(1,0,0,1,VF_VENDOR);
	}
	
};

class ModuleSQLLogFactory : public ModuleFactory
{
 public:
	ModuleSQLLogFactory()
	{
	}
	
	~ModuleSQLLogFactory()
	{
	}
	
	virtual Module * CreateModule()
	{
		return new ModuleSQLLog;
	}
	
};


extern "C" void * init_module( void )
{
	return new ModuleSQLLogFactory;
}

