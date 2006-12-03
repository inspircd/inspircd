/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd is copyright (C) 2002-2006 ChatSpike-Dev.
 *		       E-mail:
 *		<brain@chatspike.net>
 *		<Craig@chatspike.net>
 *     
 * Written by Craig Edwards, Craig McLure, and others.
 * This program is free but copyrighted software; see
 *	    the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "configreader.h"
#include <sstream>
#include <fstream>
#include "inspircd.h"
#include "xline.h"

std::vector<std::string> old_module_names, new_module_names, added_modules, removed_modules;

ServerConfig::ServerConfig(InspIRCd* Instance) : ServerInstance(Instance)
{
	this->ClearStack();
	*ServerName = *Network = *ServerDesc = *AdminName = '\0';
	*HideWhoisServer = *AdminEmail = *AdminNick = *diepass = *restartpass = '\0';
	*CustomVersion = *motd = *rules = *PrefixQuit = *DieValue = *DNSServer = '\0';
	*UserStats = *ModPath = *MyExecutable = *DisabledCommands = *PID = '\0';
	log_file = NULL;
	NoUserDns = forcedebug = OperSpyWhois = nofork = HideBans = HideSplits = false;
	CycleHosts = writelog = AllowHalfop = true;
	dns_timeout = DieDelay = 5;
	MaxTargets = 20;
	NetBufferSize = 10240;
	SoftLimit = MAXCLIENTS;
	MaxConn = SOMAXCONN;
	MaxWhoResults = 100;
	debugging = 0;
	LogLevel = DEFAULT;
	maxbans.clear();
	WhoWasGroupSize = 10;
	WhoWasMaxGroups = WhoWasGroupSize * MAXCLIENTS;
	WhoWasMaxKeep = 3600*24*3; // 3 days
}

void ServerConfig::ClearStack()
{
	include_stack.clear();
}

Module* ServerConfig::GetIOHook(int port)
{
	std::map<int,Module*>::iterator x = IOHookModule.find(port);
	return (x != IOHookModule.end() ? x->second : NULL);
}

bool ServerConfig::AddIOHook(int port, Module* iomod)
{
	if (!GetIOHook(port))
	{
		IOHookModule[port] = iomod;
		return true;
	}
	else
	{
		ModuleException err("Port already hooked by another module");
		throw(err);
		return false;
	}
}

bool ServerConfig::DelIOHook(int port)
{
	std::map<int,Module*>::iterator x = IOHookModule.find(port);
	if (x != IOHookModule.end())
	{
		IOHookModule.erase(x);
		return true;
	}
	return false;
}

bool ServerConfig::CheckOnce(char* tag, bool bail, userrec* user)
{
	int count = ConfValueEnum(this->config_data, tag);
	
	if (count > 1)
	{
		if (bail)
		{
			printf("There were errors in your configuration:\nYou have more than one <%s> tag, this is not permitted.\n",tag);
			InspIRCd::Exit(ERROR);
		}
		else
		{
			if (user)
			{
				user->WriteServ("There were errors in your configuration:");
				user->WriteServ("You have more than one <%s> tag, this is not permitted.\n",tag);
			}
			else
			{
				ServerInstance->WriteOpers("There were errors in the configuration file:");
				ServerInstance->WriteOpers("You have more than one <%s> tag, this is not permitted.\n",tag);
			}
		}
		return false;
	}
	if (count < 1)
	{
		if (bail)
		{
			printf("There were errors in your configuration:\nYou have not defined a <%s> tag, this is required.\n",tag);
			InspIRCd::Exit(ERROR);
		}
		else
		{
			if (user)
			{
				user->WriteServ("There were errors in your configuration:");
				user->WriteServ("You have not defined a <%s> tag, this is required.",tag);
			}
			else
			{
				ServerInstance->WriteOpers("There were errors in the configuration file:");
				ServerInstance->WriteOpers("You have not defined a <%s> tag, this is required.",tag);
			}
		}
		return false;
	}
	return true;
}

bool NoValidation(ServerConfig* conf, const char* tag, const char* value, ValueItem &data)
{
	conf->GetInstance()->Log(DEBUG,"No validation for <%s:%s>",tag,value);
	return true;
}

bool ValidateMaxTargets(ServerConfig* conf, const char* tag, const char* value, ValueItem &data)
{
	if ((data.GetInteger() < 0) || (data.GetInteger() > 31))
	{
		conf->GetInstance()->Log(DEFAULT,"WARNING: <options:maxtargets> value is greater than 31 or less than 0, set to 20.");
		data.Set(20);
	}
	return true;
}

bool ValidateSoftLimit(ServerConfig* conf, const char* tag, const char* value, ValueItem &data)
{
	if ((data.GetInteger() < 1) || (data.GetInteger() > MAXCLIENTS))
	{
		conf->GetInstance()->Log(DEFAULT,"WARNING: <options:softlimit> value is greater than %d or less than 0, set to %d.",MAXCLIENTS,MAXCLIENTS);
		data.Set(MAXCLIENTS);
	}
	return true;
}

bool ValidateMaxConn(ServerConfig* conf, const char* tag, const char* value, ValueItem &data)
{
	if (data.GetInteger() > SOMAXCONN)
		conf->GetInstance()->Log(DEFAULT,"WARNING: <options:somaxconn> value may be higher than the system-defined SOMAXCONN value!");
	if (!data.GetInteger())
		data.Set(SOMAXCONN);
	return true;
}

bool ValidateDnsTimeout(ServerConfig* conf, const char* tag, const char* value, ValueItem &data)
{
	if (!data.GetInteger())
		data.Set(5);
	return true;
}

bool InitializeDisabledCommands(const char* data, InspIRCd* ServerInstance)
{
	std::stringstream dcmds(data);
	std::string thiscmd;

	/* Enable everything first */
	for (nspace::hash_map<std::string,command_t*>::iterator x = ServerInstance->Parser->cmdlist.begin(); x != ServerInstance->Parser->cmdlist.end(); x++)
		x->second->Disable(false);

	/* Now disable all the ones which the user wants disabled */
	while (dcmds >> thiscmd)
	{
		nspace::hash_map<std::string,command_t*>::iterator cm = ServerInstance->Parser->cmdlist.find(thiscmd);
		if (cm != ServerInstance->Parser->cmdlist.end())
		{
			ServerInstance->Log(DEBUG,"Disabling command '%s'",cm->second->command.c_str());
			cm->second->Disable(true);
		}
	}
	return true;
}

bool ValidateDnsServer(ServerConfig* conf, const char* tag, const char* value, ValueItem &data)
{
	if (!*(data.GetString()))
	{
		// attempt to look up their nameserver from /etc/resolv.conf
		conf->GetInstance()->Log(DEFAULT,"WARNING: <dns:server> not defined, attempting to find working server in /etc/resolv.conf...");
		ifstream resolv("/etc/resolv.conf");
		std::string nameserver;
		bool found_server = false;

		if (resolv.is_open())
		{
			while (resolv >> nameserver)
			{
				if ((nameserver == "nameserver") && (!found_server))
				{
					resolv >> nameserver;
					data.Set(nameserver.c_str());
					found_server = true;
					conf->GetInstance()->Log(DEFAULT,"<dns:server> set to '%s' as first resolver in /etc/resolv.conf.",nameserver.c_str());
				}
			}

			if (!found_server)
			{
				conf->GetInstance()->Log(DEFAULT,"/etc/resolv.conf contains no viable nameserver entries! Defaulting to nameserver '127.0.0.1'!");
				data.Set("127.0.0.1");
			}
		}
		else
		{
			conf->GetInstance()->Log(DEFAULT,"/etc/resolv.conf can't be opened! Defaulting to nameserver '127.0.0.1'!");
			data.Set("127.0.0.1");
		}
	}
	return true;
}

bool ValidateModPath(ServerConfig* conf, const char* tag, const char* value, ValueItem &data)
{
	if (!*(data.GetString()))
		data.Set(MOD_PATH);
	return true;
}


bool ValidateServerName(ServerConfig* conf, const char* tag, const char* value, ValueItem &data)
{
	if (!strchr(data.GetString(),'.'))
	{
		conf->GetInstance()->Log(DEFAULT,"WARNING: <server:name> '%s' is not a fully-qualified domain name. Changed to '%s%c'",data.GetString(),data.GetString(),'.');
		std::string moo = std::string(data.GetString()).append(".");
		data.Set(moo.c_str());
	}
	return true;
}

bool ValidateNetBufferSize(ServerConfig* conf, const char* tag, const char* value, ValueItem &data)
{
	if ((!data.GetInteger()) || (data.GetInteger() > 65535) || (data.GetInteger() < 1024))
	{
		conf->GetInstance()->Log(DEFAULT,"No NetBufferSize specified or size out of range, setting to default of 10240.");
		data.Set(10240);
	}
	return true;
}

bool ValidateMaxWho(ServerConfig* conf, const char* tag, const char* value, ValueItem &data)
{
	if ((!data.GetInteger()) || (data.GetInteger() > 65535) || (data.GetInteger() < 1))
	{
		conf->GetInstance()->Log(DEFAULT,"No MaxWhoResults specified or size out of range, setting to default of 128.");
		data.Set(128);
	}
	return true;
}

bool ValidateLogLevel(ServerConfig* conf, const char* tag, const char* value, ValueItem &data)
{
	std::string dbg = data.GetString();
	conf->LogLevel = DEFAULT;

	if (dbg == "debug")
		conf->LogLevel = DEBUG;
	else if (dbg  == "verbose")
		conf->LogLevel = VERBOSE;
	else if (dbg == "default")
		conf->LogLevel = DEFAULT;
	else if (dbg == "sparse")
		conf->LogLevel = SPARSE;
	else if (dbg == "none")
		conf->LogLevel = NONE;

	conf->debugging = (conf->LogLevel == DEBUG);

	return true;
}

bool ValidateMotd(ServerConfig* conf, const char* tag, const char* value, ValueItem &data)
{
	conf->ReadFile(conf->MOTD, data.GetString());
	return true;
}

bool ValidateRules(ServerConfig* conf, const char* tag, const char* value, ValueItem &data)
{
	conf->ReadFile(conf->RULES, data.GetString());
	return true;
}

bool ValidateWhoWas(ServerConfig* conf, const char* tag, const char* value, ValueItem &data)
{
	conf->WhoWasMaxKeep = conf->GetInstance()->Duration(data.GetString());

	if (conf->WhoWasGroupSize < 0)
		conf->WhoWasGroupSize = 0;

	if (conf->WhoWasMaxGroups < 0)
		conf->WhoWasMaxGroups = 0;

	if (conf->WhoWasMaxKeep < 3600)
	{
		conf->WhoWasMaxKeep = 3600;
		conf->GetInstance()->Log(DEFAULT,"WARNING: <whowas:maxkeep> value less than 3600, setting to default 3600");
	}
	conf->GetInstance()->Log(DEBUG,"whowas:groupsize:%d maxgroups:%d maxkeep:%d",conf->WhoWasGroupSize,conf->WhoWasMaxGroups,conf->WhoWasMaxKeep);
	irc::whowas::PruneWhoWas(conf->GetInstance(), conf->GetInstance()->Time());
	return true;
}

/* Callback called before processing the first <connect> tag
 */
bool InitConnect(ServerConfig* conf, const char* tag)
{
	conf->GetInstance()->Log(DEFAULT,"Reading connect classes...");
	conf->Classes.clear();
	return true;
}

/* Callback called to process a single <connect> tag
 */
bool DoConnect(ServerConfig* conf, const char* tag, char** entries, ValueList &values, int* types)
{
	ConnectClass c;
	const char* allow = values[0].GetString(); /* Yeah, there are a lot of values. Live with it. */
	const char* deny = values[1].GetString();
	const char* password = values[2].GetString();
	int timeout = values[3].GetInteger();
	int pingfreq = values[4].GetInteger();
	int flood = values[5].GetInteger();
	int threshold = values[6].GetInteger();
	int sendq = values[7].GetInteger();
	int recvq = values[8].GetInteger();
	int localmax = values[9].GetInteger();
	int globalmax = values[10].GetInteger();

	if (*allow)
	{
		c.host = allow;
		c.type = CC_ALLOW;
		c.pass = password;
		c.registration_timeout = timeout;
		c.pingtime = pingfreq;
		c.flood = flood;
		c.threshold = threshold;
		c.sendqmax = sendq;
		c.recvqmax = recvq;
		c.maxlocal = localmax;
		c.maxglobal = globalmax;


		if (c.maxlocal == 0)
			c.maxlocal = 3;
		if (c.maxglobal == 0)
			c.maxglobal = 3;
		if (c.threshold == 0)
		{
			c.threshold = 1;
			c.flood = 999;
			conf->GetInstance()->Log(DEFAULT,"Warning: Connect allow line '%s' has no flood/threshold settings. Setting this tag to 999 lines in 1 second.",c.host.c_str());
		}
		if (c.sendqmax == 0)
			c.sendqmax = 262114;
		if (c.recvqmax == 0)
			c.recvqmax = 4096;
		if (c.registration_timeout == 0)
			c.registration_timeout = 90;
		if (c.pingtime == 0)
			c.pingtime = 120;
		conf->Classes.push_back(c);
	}
	else
	{
		c.host = deny;
		c.type = CC_DENY;
		conf->Classes.push_back(c);
		conf->GetInstance()->Log(DEBUG,"Read connect class type DENY, host=%s",deny);
	}

	return true;
}

/* Callback called when there are no more <connect> tags
 */
bool DoneConnect(ServerConfig* conf, const char* tag)
{
	conf->GetInstance()->Log(DEBUG,"DoneConnect called for tag: %s",tag);
	return true;
}

/* Callback called before processing the first <uline> tag
 */
bool InitULine(ServerConfig* conf, const char* tag)
{
	conf->ulines.clear();
	return true;
}

/* Callback called to process a single <uline> tag
 */
bool DoULine(ServerConfig* conf, const char* tag, char** entries, ValueList &values, int* types)
{
	const char* server = values[0].GetString();
	conf->GetInstance()->Log(DEBUG,"Read ULINE '%s'",server);
	conf->ulines.push_back(server);
	return true;
}

/* Callback called when there are no more <uline> tags
 */
bool DoneULine(ServerConfig* conf, const char* tag)
{
	return true;
}

/* Callback called before processing the first <module> tag
 */
bool InitModule(ServerConfig* conf, const char* tag)
{
	old_module_names.clear();
	new_module_names.clear();
	added_modules.clear();
	removed_modules.clear();
	for (std::vector<std::string>::iterator t = conf->module_names.begin(); t != conf->module_names.end(); t++)
	{
		old_module_names.push_back(*t);
	}
	return true;
}

/* Callback called to process a single <module> tag
 */
bool DoModule(ServerConfig* conf, const char* tag, char** entries, ValueList &values, int* types)
{
	const char* modname = values[0].GetString();
	new_module_names.push_back(modname);
	return true;
}

/* Callback called when there are no more <module> tags
 */
bool DoneModule(ServerConfig* conf, const char* tag)
{
	// now create a list of new modules that are due to be loaded
	// and a seperate list of modules which are due to be unloaded
	for (std::vector<std::string>::iterator _new = new_module_names.begin(); _new != new_module_names.end(); _new++)
	{
		bool added = true;

		for (std::vector<std::string>::iterator old = old_module_names.begin(); old != old_module_names.end(); old++)
		{
			if (*old == *_new)
				added = false;
		}

		if (added)
			added_modules.push_back(*_new);
	}

	for (std::vector<std::string>::iterator oldm = old_module_names.begin(); oldm != old_module_names.end(); oldm++)
	{
		bool removed = true;
		for (std::vector<std::string>::iterator newm = new_module_names.begin(); newm != new_module_names.end(); newm++)
		{
			if (*newm == *oldm)
				removed = false;
		}

		if (removed)
			removed_modules.push_back(*oldm);
	}
	return true;
}

/* Callback called before processing the first <banlist> tag
 */
bool InitMaxBans(ServerConfig* conf, const char* tag)
{
	conf->maxbans.clear();
	return true;
}

/* Callback called to process a single <banlist> tag
 */
bool DoMaxBans(ServerConfig* conf, const char* tag, char** entries, ValueList &values, int* types)
{
	const char* channel = values[0].GetString();
	int limit = values[1].GetInteger();
	conf->maxbans[channel] = limit;
	return true;
}

/* Callback called when there are no more <banlist> tags.
 */
bool DoneMaxBans(ServerConfig* conf, const char* tag)
{
	return true;
}

void ServerConfig::Read(bool bail, userrec* user)
{
	static char debug[MAXBUF];	/* Temporary buffer for debugging value */
	static char maxkeep[MAXBUF];	/* Temporary buffer for WhoWasMaxKeep value */
	int rem = 0, add = 0;		/* Number of modules added, number of modules removed */
	std::ostringstream errstr;	/* String stream containing the error output */

	/* These tags MUST occur and must ONLY occur once in the config file */
	static char* Once[] = { "server", "admin", "files", "power", "options", NULL };

	/* These tags can occur ONCE or not at all */
	InitialConfig Values[] = {
		{"options",		"softlimit",			new ValueContainerUInt (&this->SoftLimit),		DT_INTEGER, ValidateSoftLimit},
		{"options",		"somaxconn",			new ValueContainerInt  (&this->MaxConn),		DT_INTEGER, ValidateMaxConn},
		{"server",		"name",				new ValueContainerChar (this->ServerName),		DT_CHARPTR, ValidateServerName},
		{"server",		"description",			new ValueContainerChar (this->ServerDesc),		DT_CHARPTR, NoValidation},
		{"server",		"network",			new ValueContainerChar (this->Network),			DT_CHARPTR, NoValidation},
		{"admin",		"name",				new ValueContainerChar (this->AdminName),		DT_CHARPTR, NoValidation},
		{"admin",		"email",			new ValueContainerChar (this->AdminEmail),		DT_CHARPTR, NoValidation},
		{"admin",		"nick",				new ValueContainerChar (this->AdminNick),		DT_CHARPTR, NoValidation},
		{"files",		"motd",				new ValueContainerChar (this->motd),			DT_CHARPTR, ValidateMotd},
		{"files",		"rules",			new ValueContainerChar (this->rules),			DT_CHARPTR, ValidateRules},
		{"power",		"diepass",			new ValueContainerChar (this->diepass),			DT_CHARPTR, NoValidation},	
		{"power",		"pause",			new ValueContainerInt  (&this->DieDelay),		DT_INTEGER, NoValidation},
		{"power",		"restartpass",			new ValueContainerChar (this->restartpass),		DT_CHARPTR, NoValidation},
		{"options",		"prefixquit",			new ValueContainerChar (this->PrefixQuit),		DT_CHARPTR, NoValidation},
		{"options",		"loglevel",			new ValueContainerChar (debug),				DT_CHARPTR, ValidateLogLevel},
		{"options",		"netbuffersize",		new ValueContainerInt  (&this->NetBufferSize),		DT_INTEGER, ValidateNetBufferSize},
		{"options",		"maxwho",			new ValueContainerInt  (&this->MaxWhoResults),		DT_INTEGER, ValidateMaxWho},
		{"options",		"allowhalfop",			new ValueContainerBool (&this->AllowHalfop),		DT_BOOLEAN, NoValidation},
		{"dns",			"server",			new ValueContainerChar (this->DNSServer),		DT_CHARPTR, ValidateDnsServer},
		{"dns",			"timeout",			new ValueContainerInt  (&this->dns_timeout),		DT_INTEGER, ValidateDnsTimeout},
		{"options",		"moduledir",			new ValueContainerChar (this->ModPath),			DT_CHARPTR, ValidateModPath},
		{"disabled",		"commands",			new ValueContainerChar (this->DisabledCommands),	DT_CHARPTR, NoValidation},
		{"options",		"userstats",			new ValueContainerChar (this->UserStats),		DT_CHARPTR, NoValidation},
		{"options",		"customversion",		new ValueContainerChar (this->CustomVersion),		DT_CHARPTR, NoValidation},
		{"options",		"hidesplits",			new ValueContainerBool (&this->HideSplits),		DT_BOOLEAN, NoValidation},
		{"options",		"hidebans",			new ValueContainerBool (&this->HideBans),		DT_BOOLEAN, NoValidation},
		{"options",		"hidewhois",			new ValueContainerChar (this->HideWhoisServer),		DT_CHARPTR, NoValidation},
		{"options",		"operspywhois",			new ValueContainerBool (&this->OperSpyWhois),		DT_BOOLEAN, NoValidation},
		{"options",		"nouserdns",			new ValueContainerBool (&this->NoUserDns),		DT_BOOLEAN, NoValidation},
		{"options",		"syntaxhints",			new ValueContainerBool (&this->SyntaxHints),		DT_BOOLEAN, NoValidation},
		{"options",		"cyclehosts",			new ValueContainerBool (&this->CycleHosts),		DT_BOOLEAN, NoValidation},
		{"pid",			"file",				new ValueContainerChar (this->PID),			DT_CHARPTR, NoValidation},
		{"whowas",		"groupsize",			new ValueContainerInt  (&this->WhoWasGroupSize),	DT_INTEGER, NoValidation},
		{"whowas",		"maxgroups",			new ValueContainerInt  (&this->WhoWasMaxGroups),	DT_INTEGER, NoValidation},
		{"whowas",		"maxkeep",			new ValueContainerChar (maxkeep),			DT_CHARPTR, ValidateWhoWas},
		{"die",			"value",			new ValueContainerChar (this->DieValue),		DT_CHARPTR, NoValidation},
		{NULL}
	};

	/* These tags can occur multiple times, and therefore they have special code to read them
	 * which is different to the code for reading the singular tags listed above.
	 */
	MultiConfig MultiValues[] = {

		{"connect",
				{"allow",	"deny",		"password",	"timeout",	"pingfreq",	"flood",
				"threshold",	"sendq",	"recvq",	"localmax",	"globalmax",	NULL},
				{DT_CHARPTR,	DT_CHARPTR,	DT_CHARPTR,	DT_INTEGER,	DT_INTEGER,	DT_INTEGER,
				 DT_INTEGER,	DT_INTEGER,	DT_INTEGER,	DT_INTEGER,	DT_INTEGER},
				InitConnect, DoConnect, DoneConnect},

		{"uline",
				{"server",	NULL},
				{DT_CHARPTR},
				InitULine,DoULine,DoneULine},

		{"banlist",
				{"chan",	"limit",	NULL},
				{DT_CHARPTR,	DT_INTEGER},
				InitMaxBans, DoMaxBans, DoneMaxBans},

		{"module",
				{"name",	NULL},
				{DT_CHARPTR},
				InitModule, DoModule, DoneModule},

		{"badip",
				{"reason",	"ipmask",	NULL},
				{DT_CHARPTR,	DT_CHARPTR},
				InitXLine, DoZLine, DoneXLine},

		{"badnick",
				{"reason",	"nick",		NULL},
				{DT_CHARPTR,	DT_CHARPTR},
				InitXLine, DoQLine, DoneXLine},

		{"badhost",
				{"reason",	"host",		NULL},
				{DT_CHARPTR,	DT_CHARPTR},
				InitXLine, DoKLine, DoneXLine},

		{"exception",
				{"reason",	"host",		NULL},
				{DT_CHARPTR,	DT_CHARPTR},
				InitXLine, DoELine, DoneXLine},

		{"type",
				{"name",	"classes",	NULL},
				{DT_CHARPTR,	DT_CHARPTR},
				InitTypes, DoType, DoneClassesAndTypes},

		{"class",
				{"name",	"commands",	NULL},
				{DT_CHARPTR,	DT_CHARPTR},
				InitClasses, DoClass, DoneClassesAndTypes},

		{NULL}
	};

	include_stack.clear();

	/* Load and parse the config file, if there are any errors then explode */
	
	/* Make a copy here so if it fails then we can carry on running with an unaffected config */
	ConfigDataHash newconfig;
	
	if (this->LoadConf(newconfig, CONFIG_FILE, errstr))
	{
		/* If we succeeded, set the ircd config to the new one */
		this->config_data = newconfig;	
	}
	else
	{
		ServerInstance->Log(DEFAULT, "There were errors in your configuration:\n%s", errstr.str().c_str());

		if (bail)
		{
			/* Unneeded because of the ServerInstance->Log() aboive? */
			printf("There were errors in your configuration:\n%s",errstr.str().c_str());
			InspIRCd::Exit(ERROR);
		}
		else
		{
			std::string errors = errstr.str();
			std::string::size_type start;
			unsigned int prefixlen;
			
			start = 0;
			/* ":ServerInstance->Config->ServerName NOTICE user->nick :" */
			prefixlen = strlen(this->ServerName) + strlen(user->nick) + 11;
	
			if (user)
			{
				user->WriteServ("NOTICE %s :There were errors in the configuration file:",user->nick);
				
				while(start < errors.length())
				{
					user->WriteServ("NOTICE %s :%s",user->nick, errors.substr(start, 510 - prefixlen).c_str());
					start += 510 - prefixlen;
				}
			}
			else
			{
				ServerInstance->WriteOpers("There were errors in the configuration file:");
				
				while(start < errors.length())
				{
					ServerInstance->WriteOpers(errors.substr(start, 360).c_str());
					start += 360;
				}
			}

			return;
		}
	}

	/* Check we dont have more than one of singular tags, or any of them missing
	 */
	for (int Index = 0; Once[Index]; Index++)
		if (!CheckOnce(Once[Index], bail, user))
			return;

	/* Read the values of all the tags which occur once or not at all, and call their callbacks.
	 */
	for (int Index = 0; Values[Index].tag; Index++)
	{
		char item[MAXBUF];
		ConfValue(this->config_data, Values[Index].tag, Values[Index].value, 0, item, MAXBUF);
		ValueItem vi(item);

		Values[Index].validation_function(this, Values[Index].tag, Values[Index].value, vi);

		switch (Values[Index].datatype)
		{
			case DT_CHARPTR:
				{
					ValueContainerChar* vcc = (ValueContainerChar*)Values[Index].val;
					vcc->Set(vi.GetString(), strlen(vi.GetString()));
				}
			break;
			case DT_INTEGER:
				{
					int val = vi.GetInteger();
					ValueContainerInt* vci = (ValueContainerInt*)Values[Index].val;
					vci->Set(&val, sizeof(int));
				}
			break;
			case DT_BOOLEAN:
				{
					bool val = vi.GetBool();
					ValueContainerBool* vcb = (ValueContainerBool*)Values[Index].val;
					vcb->Set(&val, sizeof(bool));
				}
			break;
			default:
				/* You don't want to know what happens if someones bad code sends us here. */
			break;
		}

		/* We're done with this now */
		delete Values[Index].val;
	}

	/* Read the multiple-tag items (class tags, connect tags, etc)
	 * and call the callbacks associated with them. We have three
	 * callbacks for these, a 'start', 'item' and 'end' callback.
	 */
	for (int Index = 0; MultiValues[Index].tag; Index++)
	{
		MultiValues[Index].init_function(this, MultiValues[Index].tag);

		int number_of_tags = ConfValueEnum(this->config_data, MultiValues[Index].tag);

		for (int tagnum = 0; tagnum < number_of_tags; tagnum++)
		{
			ValueList vl;
			for (int valuenum = 0; MultiValues[Index].items[valuenum]; valuenum++)
			{
				switch (MultiValues[Index].datatype[valuenum])
				{
					case DT_CHARPTR:
						{
							char item[MAXBUF];
							ConfValue(this->config_data, MultiValues[Index].tag, MultiValues[Index].items[valuenum], tagnum, item, MAXBUF);
							vl.push_back(ValueItem(item));
						}
					break;
					case DT_INTEGER:
						{
							int item;
							ConfValueInteger(this->config_data, MultiValues[Index].tag, MultiValues[Index].items[valuenum], tagnum, item);
							vl.push_back(ValueItem(item));
						}
					break;
					case DT_BOOLEAN:
						{
							bool item = ConfValueBool(this->config_data, MultiValues[Index].tag, MultiValues[Index].items[valuenum], tagnum);
							vl.push_back(ValueItem(item));
						}
					break;
					default:
						/* Someone was smoking craq if we got here, and we're all gonna die. */
					break;
				}
			}

			MultiValues[Index].validation_function(this, MultiValues[Index].tag, (char**)MultiValues[Index].items, vl, MultiValues[Index].datatype);
		}

		MultiValues[Index].finish_function(this, MultiValues[Index].tag);
	}

	// write once here, to try it out and make sure its ok
	ServerInstance->WritePID(this->PID);

	ServerInstance->Log(DEFAULT,"Done reading configuration file.");

	/* If we're rehashing, let's load any new modules, and unload old ones
	 */
	if (!bail)
	{
		int found_ports = 0;
		FailedPortList pl;
		ServerInstance->stats->BoundPortCount = ServerInstance->BindPorts(false, found_ports, pl);

		if (pl.size())
		{
			user->WriteServ("NOTICE %s :*** Not all your client ports could be bound.", user->nick);
			user->WriteServ("NOTICE %s :*** The following port%s failed to bind:", user->nick, found_ports - ServerInstance->stats->BoundPortCount != 1 ? "s" : "");
			int j = 1;
			for (FailedPortList::iterator i = pl.begin(); i != pl.end(); i++, j++)
			{
				user->WriteServ("NOTICE %s :*** %d.   IP: %s     Port: %lu", user->nick, j, i->first.empty() ? "<all>" : i->first.c_str(), (unsigned long)i->second);
			}
		}

		if (!removed_modules.empty())
		{
			for (std::vector<std::string>::iterator removing = removed_modules.begin(); removing != removed_modules.end(); removing++)
			{
				if (ServerInstance->UnloadModule(removing->c_str()))
				{
					ServerInstance->WriteOpers("*** REHASH UNLOADED MODULE: %s",removing->c_str());

					if (user)
						user->WriteServ("973 %s %s :Module %s successfully unloaded.",user->nick, removing->c_str(), removing->c_str());

					rem++;
				}
				else
				{
					if (user)
						user->WriteServ("972 %s %s :Failed to unload module %s: %s",user->nick, removing->c_str(), removing->c_str(), ServerInstance->ModuleError());
				}
			}
		}

		if (!added_modules.empty())
		{
			for (std::vector<std::string>::iterator adding = added_modules.begin(); adding != added_modules.end(); adding++)
			{
				if (ServerInstance->LoadModule(adding->c_str()))
				{
					ServerInstance->WriteOpers("*** REHASH LOADED MODULE: %s",adding->c_str());
	
					if (user)
						user->WriteServ("975 %s %s :Module %s successfully loaded.",user->nick, adding->c_str(), adding->c_str());
	
					add++;
				}
				else
				{
					if (user)
						user->WriteServ("974 %s %s :Failed to load module %s: %s",user->nick, adding->c_str(), adding->c_str(), ServerInstance->ModuleError());
				}
			}
		}

		ServerInstance->Log(DEFAULT,"Successfully unloaded %lu of %lu modules and loaded %lu of %lu modules.",(unsigned long)rem,(unsigned long)removed_modules.size(),(unsigned long)add,(unsigned long)added_modules.size());
	}
}

bool ServerConfig::LoadConf(ConfigDataHash &target, const char* filename, std::ostringstream &errorstream)
{
	std::ifstream conf(filename);
	std::string line;
	char ch;
	long linenumber;
	bool in_tag;
	bool in_quote;
	bool in_comment;
	
	linenumber = 1;
	in_tag = false;
	in_quote = false;
	in_comment = false;
	
	/* Check if the file open failed first */
	if (!conf)
	{
		errorstream << "LoadConf: Couldn't open config file: " << filename << std::endl;
		return false;
	}
	
	/* Fix the chmod of the file to restrict it to the current user and group */
	chmod(filename,0600);
	
	for (unsigned int t = 0; t < include_stack.size(); t++)
	{
		if (std::string(filename) == include_stack[t])
		{
			errorstream << "File " << filename << " is included recursively (looped inclusion)." << std::endl;
			return false;
		}
	}
	
	/* It's not already included, add it to the list of files we've loaded */
	include_stack.push_back(filename);
	
	/* Start reading characters... */	
	while(conf.get(ch))
	{
		/*
		 * Here we try and get individual tags on separate lines,
		 * this would be so easy if we just made people format
		 * their config files like that, but they don't so...
		 * We check for a '<' and then know the line is over when
		 * we get a '>' not inside quotes. If we find two '<' and
		 * no '>' then die with an error.
		 */
		
		if((ch == '#') && !in_quote)
			in_comment = true;
		
		if(((ch == '\n') || (ch == '\r')) && in_quote)
		{
			errorstream << "Got a newline within a quoted section, this is probably a typo: " << filename << ":" << linenumber << std::endl;
			return false;
		}
		
		switch(ch)
		{
			case '\n':
				linenumber++;
			case '\r':
				in_comment = false;
			case '\0':
				continue;
			case '\t':
				ch = ' ';
		}
		
		if(in_comment)
			continue;

		/* XXX: Added by Brain, May 1st 2006 - Escaping of characters.
		 * Note that this WILL NOT usually allow insertion of newlines,
		 * because a newline is two characters long. Use it primarily to
		 * insert the " symbol.
		 *
		 * Note that this also involves a further check when parsing the line,
		 * which can be found below.
		 */
		if ((ch == '\\') && (in_quote) && (in_tag))
		{
			line += ch;
			ServerInstance->Log(DEBUG,"Escape sequence in config line.");
			char real_character;
			if (conf.get(real_character))
			{
				ServerInstance->Log(DEBUG,"Escaping %c", real_character);
				if (real_character == 'n')
					real_character = '\n';
				line += real_character;
				continue;
			}
			else
			{
				errorstream << "End of file after a \\, what did you want to escape?: " << filename << ":" << linenumber << std::endl;
				return false;
			}
		}

		line += ch;
		
		if(ch == '<')
		{
			if(in_tag)
			{
				if(!in_quote)
				{
					errorstream << "Got another opening < when the first one wasn't closed: " << filename << ":" << linenumber << std::endl;
					return false;
				}
			}
			else
			{
				if(in_quote)
				{
					errorstream << "We're in a quote but outside a tag, interesting. " << filename << ":" << linenumber << std::endl;
					return false;
				}
				else
				{
					// errorstream << "Opening new config tag on line " << linenumber << std::endl;
					in_tag = true;
				}
			}
		}
		else if(ch == '"')
		{
			if(in_tag)
			{
				if(in_quote)
				{
					// errorstream << "Closing quote in config tag on line " << linenumber << std::endl;
					in_quote = false;
				}
				else
				{
					// errorstream << "Opening quote in config tag on line " << linenumber << std::endl;
					in_quote = true;
				}
			}
			else
			{
				if(in_quote)
				{
					errorstream << "Found a (closing) \" outside a tag: " << filename << ":" << linenumber << std::endl;
				}
				else
				{
					errorstream << "Found a (opening) \" outside a tag: " << filename << ":" << linenumber << std::endl;
				}
			}
		}
		else if(ch == '>')
		{
			if(!in_quote)
			{
				if(in_tag)
				{
					// errorstream << "Closing config tag on line " << linenumber << std::endl;
					in_tag = false;

					/*
					 * If this finds an <include> then ParseLine can simply call
					 * LoadConf() and load the included config into the same ConfigDataHash
					 */
					
					if(!this->ParseLine(target, line, linenumber, errorstream))
						return false;
					
					line.clear();
				}
				else
				{
					errorstream << "Got a closing > when we weren't inside a tag: " << filename << ":" << linenumber << std::endl;
					return false;
				}
			}
		}
	}
	
	return true;
}

bool ServerConfig::LoadConf(ConfigDataHash &target, const std::string &filename, std::ostringstream &errorstream)
{
	return this->LoadConf(target, filename.c_str(), errorstream);
}

bool ServerConfig::ParseLine(ConfigDataHash &target, std::string &line, long linenumber, std::ostringstream &errorstream)
{
	std::string tagname;
	std::string current_key;
	std::string current_value;
	KeyValList results;
	bool got_name;
	bool got_key;
	bool in_quote;
	
	got_name = got_key = in_quote = false;
	
	// std::cout << "ParseLine(data, '" << line << "', " << linenumber << ", stream)" << std::endl;
	
	for(std::string::iterator c = line.begin(); c != line.end(); c++)
	{
		if(!got_name)
		{
			/* We don't know the tag name yet. */
			
			if(*c != ' ')
			{
				if(*c != '<')
				{
					tagname += *c;
				}
			}
			else
			{
				/* We got to a space, we should have the tagname now. */
				if(tagname.length())
				{
					got_name = true;
				}
			}
		}
		else
		{
			/* We have the tag name */
			if (!got_key)
			{
				/* We're still reading the key name */
				if (*c != '=')
				{
					if (*c != ' ')
					{
						current_key += *c;
					}
				}
				else
				{
					/* We got an '=', end of the key name. */
					got_key = true;
				}
			}
			else
			{
				/* We have the key name, now we're looking for quotes and the value */

				/* Correctly handle escaped characters here.
				 * See the XXX'ed section above.
				 */
				if ((*c == '\\') && (in_quote))
				{
					c++;
					if (*c == 'n')
						current_value += '\n';
					else
						current_value += *c;
					continue;
				}
				if (*c == '"')
				{
					if (!in_quote)
					{
						/* We're not already in a quote. */
						in_quote = true;
					}
					else
					{
						/* Leaving quotes, we have the value */
						results.push_back(KeyVal(current_key, current_value));
						
						// std::cout << "<" << tagname << ":" << current_key << "> " << current_value << std::endl;
						
						in_quote = false;
						got_key = false;
						
						if((tagname == "include") && (current_key == "file"))
						{
							if(!this->DoInclude(target, current_value, errorstream))
								return false;
						}
						
						current_key.clear();
						current_value.clear();
					}
				}
				else
				{
					if(in_quote)
					{
						current_value += *c;
					}
				}
			}
		}
	}
	
	/* Finished parsing the tag, add it to the config hash */
	target.insert(std::pair<std::string, KeyValList > (tagname, results));
	
	return true;
}

bool ServerConfig::DoInclude(ConfigDataHash &target, const std::string &file, std::ostringstream &errorstream)
{
	std::string confpath;
	std::string newfile;
	std::string::size_type pos;
	
	confpath = CONFIG_FILE;
	newfile = file;
	
	for (std::string::iterator c = newfile.begin(); c != newfile.end(); c++)
	{
		if (*c == '\\')
		{
			*c = '/';
		}
	}

	if (file[0] != '/')
	{
		if((pos = confpath.find("/inspircd.conf")) != std::string::npos)
		{
			/* Leaves us with just the path */
			newfile = confpath.substr(0, pos) + std::string("/") + newfile;
		}
		else
		{
			errorstream << "Couldn't get config path from: " << confpath << std::endl;
			return false;
		}
	}
	
	return LoadConf(target, newfile, errorstream);
}

bool ServerConfig::ConfValue(ConfigDataHash &target, const char* tag, const char* var, int index, char* result, int length)
{
	std::string value;
	bool r = ConfValue(target, std::string(tag), std::string(var), index, value);
	strlcpy(result, value.c_str(), length);
	return r;
}

bool ServerConfig::ConfValue(ConfigDataHash &target, const std::string &tag, const std::string &var, int index, std::string &result)
{
	ConfigDataHash::size_type pos = index;
	if((pos >= 0) && (pos < target.count(tag)))
	{
		ConfigDataHash::const_iterator iter = target.find(tag);
		
		for(int i = 0; i < index; i++)
			iter++;
		
		for(KeyValList::const_iterator j = iter->second.begin(); j != iter->second.end(); j++)
		{
			if(j->first == var)
			{
				result = j->second;
				return true;
			}
		}
	}
	else if(pos == 0)
	{
		ServerInstance->Log(DEBUG, "No <%s> tags in config file.", tag.c_str());
	}
	else
	{
		ServerInstance->Log(DEBUG, "ConfValue got an out-of-range index %d, there are only %d occurences of %s", pos, target.count(tag), tag.c_str());
	}
	
	return false;
}
	
bool ServerConfig::ConfValueInteger(ConfigDataHash &target, const char* tag, const char* var, int index, int &result)
{
	return ConfValueInteger(target, std::string(tag), std::string(var), index, result);
}

bool ServerConfig::ConfValueInteger(ConfigDataHash &target, const std::string &tag, const std::string &var, int index, int &result)
{
	std::string value;
	std::istringstream stream;
	bool r = ConfValue(target, tag, var, index, value);
	stream.str(value);
	if(!(stream >> result))
		return false;
	return r;
}
	
bool ServerConfig::ConfValueBool(ConfigDataHash &target, const char* tag, const char* var, int index)
{
	return ConfValueBool(target, std::string(tag), std::string(var), index);
}

bool ServerConfig::ConfValueBool(ConfigDataHash &target, const std::string &tag, const std::string &var, int index)
{
	std::string result;
	if(!ConfValue(target, tag, var, index, result))
		return false;
	
	return ((result == "yes") || (result == "true") || (result == "1"));
}
	
int ServerConfig::ConfValueEnum(ConfigDataHash &target, const char* tag)
{
	return target.count(tag);
}

int ServerConfig::ConfValueEnum(ConfigDataHash &target, const std::string &tag)
{
	return target.count(tag);
}
	
int ServerConfig::ConfVarEnum(ConfigDataHash &target, const char* tag, int index)
{
	return ConfVarEnum(target, std::string(tag), index);
}

int ServerConfig::ConfVarEnum(ConfigDataHash &target, const std::string &tag, int index)
{
	ConfigDataHash::size_type pos = index;
	
	if((pos >= 0) && (pos < target.count(tag)))
	{
		ConfigDataHash::const_iterator iter = target.find(tag);
		
		for(int i = 0; i < index; i++)
			iter++;
		
		return iter->second.size();
	}
	else if(pos == 0)
	{
		ServerInstance->Log(DEBUG, "No <%s> tags in config file.", tag.c_str());
	}
	else
	{
		ServerInstance->Log(DEBUG, "ConfVarEnum got an out-of-range index %d, there are only %d occurences of %s", pos, target.count(tag), tag.c_str());
	}
	
	return 0;
}

/** Read the contents of a file located by `fname' into a file_cache pointed at by `F'.
 */
bool ServerConfig::ReadFile(file_cache &F, const char* fname)
{
	FILE* file = NULL;
	char linebuf[MAXBUF];

	F.clear();
	
	if (*fname != '/')
	{
		std::string::size_type pos;
		std::string confpath = CONFIG_FILE;
		if((pos = confpath.find("/inspircd.conf")) != std::string::npos)
		{
			/* Leaves us with just the path */
			std::string newfile = confpath.substr(0, pos) + std::string("/") + fname;
			file =  fopen(newfile.c_str(), "r");
			
		}
	}
	else
		file =  fopen(fname, "r");

	if (file)
	{
		while (!feof(file))
		{
			if (fgets(linebuf, sizeof(linebuf), file))
				linebuf[strlen(linebuf)-1] = 0;
			else
				*linebuf = 0;

			if (!feof(file))
			{
				F.push_back(*linebuf ? linebuf : " ");
			}
		}

		fclose(file);
	}
	else
		return false;

	return true;
}

bool ServerConfig::FileExists(const char* file)
{
	FILE *input;
	if ((input = fopen (file, "r")) == NULL)
	{
		return false;
	}
	else
	{
		fclose(input);
		return true;
	}
}

char* ServerConfig::CleanFilename(char* name)
{
	char* p = name + strlen(name);
	while ((p != name) && (*p != '/')) p--;
	return (p != name ? ++p : p);
}


bool ServerConfig::DirValid(const char* dirandfile)
{
	char work[MAXBUF];
	char buffer[MAXBUF];
	char otherdir[MAXBUF];
	int p;

	strlcpy(work, dirandfile, MAXBUF);
	p = strlen(work);

	// we just want the dir
	while (*work)
	{
		if (work[p] == '/')
		{
			work[p] = '\0';
			break;
		}

		work[p--] = '\0';
	}

	// Get the current working directory
	if (getcwd(buffer, MAXBUF ) == NULL )
		return false;

	if (chdir(work) == -1)
		return false;

	if (getcwd(otherdir, MAXBUF ) == NULL )
		return false;

	if (chdir(buffer) == -1)
		return false;

	size_t t = strlen(work);

	if (strlen(otherdir) >= t)
	{
		otherdir[t] = '\0';

		if (!strcmp(otherdir,work))
		{
			return true;
		}

		return false;
	}
	else
	{
		return false;
	}
}

std::string ServerConfig::GetFullProgDir(char** argv, int argc)
{
	char work[MAXBUF];
	char buffer[MAXBUF];
	char otherdir[MAXBUF];
	int p;

	strlcpy(work,argv[0],MAXBUF);
	p = strlen(work);

	// we just want the dir
	while (*work)
	{
		if (work[p] == '/')
		{
			work[p] = '\0';
			break;
		}

		work[p--] = '\0';
	}

	// Get the current working directory
	if (getcwd(buffer, MAXBUF) == NULL)
		return "";

	if (chdir(work) == -1)
		return "";

	if (getcwd(otherdir, MAXBUF) == NULL)
		return "";

	if (chdir(buffer) == -1)
		return "";

	return otherdir;
}

InspIRCd* ServerConfig::GetInstance()
{
	return ServerInstance;
}


ValueItem::ValueItem(int value)
{
	std::stringstream n;
	n << value;
	v = n.str();
}

ValueItem::ValueItem(bool value)
{
	std::stringstream n;
	n << value;
	v = n.str();
}

ValueItem::ValueItem(char* value)
{
	v = value;
}

void ValueItem::Set(char* value)
{
	v = value;
}

void ValueItem::Set(const char* value)
{
	v = value;
}

void ValueItem::Set(int value)
{
	std::stringstream n;
	n << value;
	v = n.str();
}

int ValueItem::GetInteger()
{
	return atoi(v.c_str());
}

char* ValueItem::GetString()
{
	return (char*)v.c_str();
}

bool ValueItem::GetBool()
{
	return (GetInteger() || v == "yes" || v == "true");
}

