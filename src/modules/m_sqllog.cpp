/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2010 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"
#include "m_sqlv2.h"

static Module* SQLModule;
static Module* MyMod;
static std::string dbid;

enum LogTypes { LT_OPER = 1, LT_KILL, LT_SERVLINK, LT_XLINE, LT_CONNECT, LT_DISCONNECT, LT_FLOOD, LT_LOADMODULE };

enum QueryState { FIND_SOURCE, FIND_NICK, FIND_HOST, DONE};

class QueryInfo;

std::map<unsigned long,QueryInfo*> active_queries;

class QueryInfo
{
public:
	QueryState qs;
	unsigned long id;
	std::string nick;
	std::string source;
	std::string hostname;
	int sourceid;
	int nickid;
	int hostid;
	int category;
	time_t date;
	bool insert;

	QueryInfo(const std::string &n, const std::string &s, const std::string &h, unsigned long i, int cat)
	{
		qs = FIND_SOURCE;
		nick = n;
		source = s;
		hostname = h;
		id = i;
		category = cat;
		sourceid = nickid = hostid = -1;
		date = ServerInstance->Time();
		insert = false;
	}

	void Go(SQLresult* res)
	{
		switch (qs)
		{
			case FIND_SOURCE:
				if (res->Rows() && sourceid == -1 && !insert)
				{
					sourceid = atoi(res->GetValue(0,0).d.c_str());
					SQLrequest req = SQLrequest(MyMod, SQLModule, dbid, SQLquery("SELECT id,actor FROM ircd_log_actors WHERE actor='?'") % nick);
					req.Send();
					insert = false;
					qs = FIND_NICK;
					active_queries[req.id] = this;
				}
				else if (res->Rows() && sourceid == -1 && insert)
				{
					SQLrequest req = SQLrequest(MyMod, SQLModule, dbid, SQLquery("SELECT id,actor FROM ircd_log_actors WHERE actor='?'") % source);
					req.Send();
					insert = false;
					qs = FIND_SOURCE;
					active_queries[req.id] = this;
				}
				else
				{
					SQLrequest req = SQLrequest(MyMod, SQLModule, dbid, SQLquery("INSERT INTO ircd_log_actors (actor) VALUES('?')") % source);
					req.Send();
					insert = true;
					qs = FIND_SOURCE;
					active_queries[req.id] = this;
				}
			break;

			case FIND_NICK:
				if (res->Rows() && nickid == -1 && !insert)
				{
					nickid = atoi(res->GetValue(0,0).d.c_str());
					SQLrequest req = SQLrequest(MyMod, SQLModule, dbid, SQLquery("SELECT id,hostname FROM ircd_log_hosts WHERE hostname='?'") % hostname);
					req.Send();
					insert = false;
					qs = FIND_HOST;
					active_queries[req.id] = this;
				}
				else if (res->Rows() && nickid == -1 && insert)
				{
					SQLrequest req = SQLrequest(MyMod, SQLModule, dbid, SQLquery("SELECT id,actor FROM ircd_log_actors WHERE actor='?'") % nick);
					req.Send();
					insert = false;
					qs = FIND_NICK;
					active_queries[req.id] = this;
				}
				else
				{
					SQLrequest req = SQLrequest(MyMod, SQLModule, dbid, SQLquery("INSERT INTO ircd_log_actors (actor) VALUES('?')") % nick);
					req.Send();
					insert = true;
					qs = FIND_NICK;
					active_queries[req.id] = this;
				}
			break;

			case FIND_HOST:
				if (res->Rows() && hostid == -1 && !insert)
				{
					hostid = atoi(res->GetValue(0,0).d.c_str());
					SQLrequest req = SQLrequest(MyMod, SQLModule, dbid,
							SQLquery("INSERT INTO ircd_log (category_id,nick,host,source,dtime) VALUES('?','?','?','?','?')") % category % nickid % hostid % sourceid % date);
					req.Send();
					insert = true;
					qs = DONE;
					active_queries[req.id] = this;
				}
				else if (res->Rows() && hostid == -1 && insert)
				{
					SQLrequest req = SQLrequest(MyMod, SQLModule, dbid, SQLquery("SELECT id,hostname FROM ircd_log_hosts WHERE hostname='?'") % hostname);
					req.Send();
					insert = false;
					qs = FIND_HOST;
					active_queries[req.id] = this;
				}
				else
				{
					SQLrequest req = SQLrequest(MyMod, SQLModule, dbid, SQLquery("INSERT INTO ircd_log_hosts (hostname) VALUES('?')") % hostname);
					req.Send();
					insert = true;
					qs = FIND_HOST;
					active_queries[req.id] = this;
				}
			break;

			case DONE:
			break;
		}
	}
};

/* $ModDesc: Logs network-wide data to an SQL database */

class ModuleSQLLog : public Module
{

 public:
	void init()
	{
		Module* SQLutils = ServerInstance->Modules->Find("m_sqlutils.so");
		if (!SQLutils)
			throw ModuleException("Can't find m_sqlutils.so. Please load m_sqlutils.so before m_sqlauth.so.");

		ServiceProvider* prov = ServerInstance->Modules->FindService(SERVICE_DATA, "SQL");
		if (!prov)
			throw ModuleException("Can't find an SQL provider module. Please load one before attempting to load m_sqlauth.");
		SQLModule = prov->creator;

		OnRehash(NULL);
		MyMod = this;
		active_queries.clear();

		Implementation eventlist[] = { I_OnRehash, I_OnOper, I_OnGlobalOper, I_OnKill,
			I_OnPreCommand, I_OnUserRegister, I_OnUserQuit, I_OnLoadModule };
		ServerInstance->Modules->Attach(eventlist, this, 8);
	}

	void ReadConfig()
	{
		ConfigReader Conf;
		dbid = Conf.ReadValue("sqllog","dbid",0);	// database id of a database configured in sql module
	}

	virtual void OnRehash(User* user)
	{
		ReadConfig();
	}

	void OnRequest(Request& request)
	{
		if(strcmp(SQLRESID, request.id) == 0)
		{
			SQLresult* res = static_cast<SQLresult*>(&request);
			std::map<unsigned long, QueryInfo*>::iterator n;

			n = active_queries.find(res->id);

			if (n != active_queries.end())
			{
				n->second->Go(res);
				active_queries.erase(n);
			}
		}
	}

	void AddLogEntry(int category, const std::string &nick, const std::string &host, const std::string &source)
	{
		// is the sql module loaded? If not, we don't attempt to do anything.
		if (!SQLModule)
			return;

		SQLrequest req = SQLrequest(this, SQLModule, dbid, SQLquery("SELECT id,actor FROM ircd_log_actors WHERE actor='?'") % source);
		req.Send();
		QueryInfo* i = new QueryInfo(nick, source, host, req.id, category);
		i->qs = FIND_SOURCE;
		active_queries[req.id] = i;
	}

	virtual void OnOper(User* user, const std::string &opertype)
	{
		AddLogEntry(LT_OPER,user->nick,user->host,user->server);
	}

	virtual void OnGlobalOper(User* user)
	{
		AddLogEntry(LT_OPER,user->nick,user->host,user->server);
	}

	virtual ModResult OnKill(User* source, User* dest, const std::string &reason)
	{
		AddLogEntry(LT_KILL,dest->nick,dest->host,source->nick);
		return MOD_RES_PASSTHRU;
	}

	virtual ModResult OnPreCommand(std::string &command, std::vector<std::string> &parameters, LocalUser *user, bool validated, const std::string &original_line)
	{
		if ((command == "GLINE" || command == "KLINE" || command == "ELINE" || command == "ZLINE") && validated)
		{
			AddLogEntry(LT_XLINE,user->nick,command[0]+std::string(":")+parameters[0],user->server);
		}
		return MOD_RES_PASSTHRU;
	}

	virtual ModResult OnUserRegister(LocalUser* user)
	{
		AddLogEntry(LT_CONNECT,user->nick,user->host,user->server);
		return MOD_RES_PASSTHRU;
	}

	virtual void OnUserQuit(User* user, const std::string &reason, const std::string &oper_message)
	{
		AddLogEntry(LT_DISCONNECT,user->nick,user->host,user->server);
	}

	virtual void OnLoadModule(Module* mod)
	{
		AddLogEntry(LT_LOADMODULE,mod->ModuleSourceFile,ServerInstance->Config->ServerName.c_str(), ServerInstance->Config->ServerName.c_str());
	}

	virtual Version GetVersion()
	{
		return Version("Logs network-wide data to an SQL database", VF_VENDOR);
	}

};

MODULE_INIT(ModuleSQLLog)
