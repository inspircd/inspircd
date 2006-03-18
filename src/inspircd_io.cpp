/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd is copyright (C) 2002-2006 ChatSpike-Dev.
 *                       E-mail:
 *                <brain@chatspike.net>
 *                <Craig@chatspike.net>
 *     
 * Written by Craig Edwards, Craig McLure, and others.
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

using namespace std;

#include "inspircd_config.h"
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <string>
#include <unistd.h>
#include <sstream>
#include <iostream>
#include <fstream>
#include "message.h"
#include "inspircd.h"
#include "inspircd_io.h"
#include "inspstring.h"
#include "helperfuncs.h"
#include "userprocess.h"
#include "xline.h"

extern ServerConfig *Config;
extern InspIRCd* ServerInstance;
extern int openSockfd[MAX_DESCRIPTORS];
extern time_t TIME;

extern int MODCOUNT;
extern std::vector<Module*> modules;
extern std::vector<ircd_module*> factory;

std::vector<std::string> old_module_names, new_module_names, added_modules, removed_modules;

ServerConfig::ServerConfig()
{
	this->ClearStack();
	*TempDir = *ServerName = *Network = *ServerDesc = *AdminName = '\0';
	*HideWhoisServer = *AdminEmail = *AdminNick = *diepass = *restartpass = '\0';
	*CustomVersion = *motd = *rules = *PrefixQuit = *DieValue = *DNSServer = '\0';
	*OperOnlyStats = *ModPath = *MyExecutable = *DisabledCommands = *PID = '\0';
	log_file = NULL;
	nofork = HideBans = HideSplits = unlimitcore = false;
	AllowHalfop = true;
	dns_timeout = DieDelay = 5;
	MaxTargets = 20;
	NetBufferSize = 10240;
	SoftLimit = MAXCLIENTS;
	MaxConn = SOMAXCONN;
	MaxWhoResults = 100;
	debugging = 0;
	LogLevel = DEFAULT;
	maxbans.clear();
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
	int count = ConfValueEnum(tag,&Config->config_f);
	if (count > 1)
	{
		if (bail)
		{
			printf("There were errors in your configuration:\nYou have more than one <%s> tag, this is not permitted.\n",tag);
			Exit(0);
		}
		else
		{
			if (user)
			{
				WriteServ(user->fd,"There were errors in your configuration:");
				WriteServ(user->fd,"You have more than one <%s> tag, this is not permitted.\n",tag);
			}
			else
			{
				WriteOpers("There were errors in the configuration file:");
				WriteOpers("You have more than one <%s> tag, this is not permitted.\n",tag);
			}
		}
		return false;
	}
	if (count < 1)
	{
		if (bail)
		{
			printf("There were errors in your configuration:\nYou have not defined a <%s> tag, this is required.\n",tag);
			Exit(0);
		}
		else
		{
			if (user)
			{
				WriteServ(user->fd,"There were errors in your configuration:");
				WriteServ(user->fd,"You have not defined a <%s> tag, this is required.",tag);
			}
			else
			{
				WriteOpers("There were errors in the configuration file:");
				WriteOpers("You have not defined a <%s> tag, this is required.",tag);
			}
		}
		return false;
	}
	return true;
}

bool NoValidation(const char* tag, const char* value, void* data)
{
	log(DEBUG,"No validation for <%s:%s>",tag,value);
	return true;
}

bool ValidateTempDir(const char* tag, const char* value, void* data)
{
	char* x = (char*)data;
	if (!*x)
		strlcpy(x,"/tmp",1024);
	return true;
}
 
bool ValidateMaxTargets(const char* tag, const char* value, void* data)
{
	int* x = (int*)data;
	if ((*x < 0) || (*x > 31))
	{
		log(DEFAULT,"WARNING: <options:maxtargets> value is greater than 31 or less than 0, set to 20.");
		*x = 20;
	}
	return true;
}

bool ValidateSoftLimit(const char* tag, const char* value, void* data)
{
	int* x = (int*)data;	
	if ((*x < 1) || (*x > MAXCLIENTS))
	{
		log(DEFAULT,"WARNING: <options:softlimit> value is greater than %d or less than 0, set to %d.",MAXCLIENTS,MAXCLIENTS);
		*x = MAXCLIENTS;
	}
	return true;
}

bool ValidateMaxConn(const char* tag, const char* value, void* data)
{
	int* x = (int*)data;	
	if (*x > SOMAXCONN)
		log(DEFAULT,"WARNING: <options:somaxconn> value may be higher than the system-defined SOMAXCONN value!");
	if (!*x)
		*x = SOMAXCONN;
	return true;
}

bool ValidateDnsTimeout(const char* tag, const char* value, void* data)
{
	int* x = (int*)data;
	if (!*x)
		*x = 5;
	return true;
}

bool ValidateDnsServer(const char* tag, const char* value, void* data)
{
	char* x = (char*)data;
	if (!*x)
	{
		// attempt to look up their nameserver from /etc/resolv.conf
		log(DEFAULT,"WARNING: <dns:server> not defined, attempting to find working server in /etc/resolv.conf...");
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
					strlcpy(x,nameserver.c_str(),MAXBUF);
					found_server = true;
					log(DEFAULT,"<dns:server> set to '%s' as first resolver in /etc/resolv.conf.",nameserver.c_str());
				}
			}

			if (!found_server)
			{
				log(DEFAULT,"/etc/resolv.conf contains no viable nameserver entries! Defaulting to nameserver '127.0.0.1'!");
				strlcpy(x,"127.0.0.1",MAXBUF);
			}
		}
		else
		{
			log(DEFAULT,"/etc/resolv.conf can't be opened! Defaulting to nameserver '127.0.0.1'!");
			strlcpy(x,"127.0.0.1",MAXBUF);
		}
	}
	return true;
}

bool ValidateModPath(const char* tag, const char* value, void* data)
{
	char* x = (char*)data;	
	if (!*x)
		strlcpy(x,MOD_PATH,MAXBUF);
	return true;
}


bool ValidateServerName(const char* tag, const char* value, void* data)
{
	char* x = (char*)data;
	if (!strchr(x,'.'))
	{
		log(DEFAULT,"WARNING: <server:name> '%s' is not a fully-qualified domain name. Changed to '%s%c'",x,x,'.');
		charlcat(x,'.',MAXBUF);
	}
	//strlower(x);
	return true;
}

bool ValidateNetBufferSize(const char* tag, const char* value, void* data)
{
	if ((!Config->NetBufferSize) || (Config->NetBufferSize > 65535) || (Config->NetBufferSize < 1024))
	{
		log(DEFAULT,"No NetBufferSize specified or size out of range, setting to default of 10240.");
		Config->NetBufferSize = 10240;
	}
	return true;
}

bool ValidateMaxWho(const char* tag, const char* value, void* data)
{
	if ((!Config->MaxWhoResults) || (Config->MaxWhoResults > 65535) || (Config->MaxWhoResults < 1))
	{
		log(DEFAULT,"No MaxWhoResults specified or size out of range, setting to default of 128.");
		Config->MaxWhoResults = 128;
	}
	return true;
}

bool ValidateLogLevel(const char* tag, const char* value, void* data)
{
	const char* dbg = (const char*)data;
	Config->LogLevel = DEFAULT;
	if (!strcmp(dbg,"debug"))
	{
		Config->LogLevel = DEBUG;
		Config->debugging = 1;
	}
	else if (!strcmp(dbg,"verbose"))
		Config->LogLevel = VERBOSE;
	else if (!strcmp(dbg,"default"))
		Config->LogLevel = DEFAULT;
	else if (!strcmp(dbg,"sparse"))
		Config->LogLevel = SPARSE;
	else if (!strcmp(dbg,"none"))
		Config->LogLevel = NONE;
	return true;
}

bool ValidateMotd(const char* tag, const char* value, void* data)
{
	readfile(Config->MOTD,Config->motd);
	return true;
}

bool ValidateRules(const char* tag, const char* value, void* data)
{
	readfile(Config->RULES,Config->rules);
	return true;
}

/* Callback called before processing the first <connect> tag
 */
bool InitConnect(const char* tag)
{
	log(DEFAULT,"Reading connect classes...");
	Config->Classes.clear();
	return true;
}

/* Callback called to process a single <connect> tag
 */
bool DoConnect(const char* tag, char** entries, void** values, int* types)
{
	ConnectClass c;
	char* allow = (char*)values[0]; /* Yeah, there are a lot of values. Live with it. */
	char* deny = (char*)values[1];
	char* password = (char*)values[2];
	int* timeout = (int*)values[3];
	int* pingfreq = (int*)values[4];
	int* flood = (int*)values[5];
	int* threshold = (int*)values[6];
	int* sendq = (int*)values[7];
	int* recvq = (int*)values[8];
	int* localmax = (int*)values[9];
	int* globalmax = (int*)values[10];

	if (*allow)
	{
		c.host = allow;
		c.type = CC_ALLOW;
		c.pass = password;
		c.registration_timeout = *timeout;
		c.pingtime = *pingfreq;
		c.flood = *flood;
		c.threshold = *threshold;
		c.sendqmax = *sendq;
		c.recvqmax = *recvq;
		c.maxlocal = *localmax;
		c.maxglobal = *globalmax;


		if (c.maxlocal == 0)
			c.maxlocal = 3;
		if (c.maxglobal == 0)
			c.maxglobal = 3;
		if (c.threshold == 0)
		{
			c.threshold = 1;
			c.flood = 999;
			log(DEFAULT,"Warning: Connect allow line '%s' has no flood/threshold settings. Setting this tag to 999 lines in 1 second.",c.host.c_str());
		}
		if (c.sendqmax == 0)
			c.sendqmax = 262114;
		if (c.recvqmax == 0)
			c.recvqmax = 4096;
		if (c.registration_timeout == 0)
			c.registration_timeout = 90;
		if (c.pingtime == 0)
			c.pingtime = 120;
		Config->Classes.push_back(c);
	}
	else
	{
		c.host = deny;
		c.type = CC_DENY;
		Config->Classes.push_back(c);
		log(DEBUG,"Read connect class type DENY, host=%s",deny);
	}

	return true;
}

/* Callback called when there are no more <connect> tags
 */
bool DoneConnect(const char* tag)
{
	log(DEBUG,"DoneConnect called for tag: %s",tag);
	return true;
}

/* Callback called before processing the first <uline> tag
 */
bool InitULine(const char* tag)
{
	Config->ulines.clear();
	return true;
}

/* Callback called to process a single <uline> tag
 */
bool DoULine(const char* tag, char** entries, void** values, int* types)
{
	char* server = (char*)values[0];
	log(DEBUG,"Read ULINE '%s'",server);
	Config->ulines.push_back(server);
	return true;
}

/* Callback called when there are no more <uline> tags
 */
bool DoneULine(const char* tag)
{
	return true;
}

/* Callback called before processing the first <module> tag
 */
bool InitModule(const char* tag)
{
	old_module_names.clear();
	new_module_names.clear();
	added_modules.clear();
	removed_modules.clear();
	for (std::vector<std::string>::iterator t = Config->module_names.begin(); t != Config->module_names.end(); t++)
	{
		old_module_names.push_back(*t);
	}
	return true;
}

/* Callback called to process a single <module> tag
 */
bool DoModule(const char* tag, char** entries, void** values, int* types)
{
	char* modname = (char*)values[0];
	new_module_names.push_back(modname);
	return true;
}

/* Callback called when there are no more <module> tags
 */
bool DoneModule(const char* tag)
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
bool InitMaxBans(const char* tag)
{
	Config->maxbans.clear();
	return true;
}

/* Callback called to process a single <banlist> tag
 */
bool DoMaxBans(const char* tag, char** entries, void** values, int* types)
{
	char* channel = (char*)values[0];
	int* limit = (int*)values[1];
	Config->maxbans[channel] = *limit;
	return true;
}

/* Callback called when there are no more <banlist> tags.
 */
bool DoneMaxBans(const char* tag)
{
	return true;
}

void ServerConfig::Read(bool bail, userrec* user)
{
	char debug[MAXBUF];		/* Temporary buffer for debugging value */
	char dataline[1024];		/* Temporary buffer for error output */
	char* convert;			/* Temporary buffer used for reading singular values into */
	char* data[12];			/* Temporary buffers for reading multiple occurance tags into */
	void* ptr[12];			/* Temporary pointers for passing to callbacks */
	int r_i[12];			/* Temporary array for casting */
	int rem = 0, add = 0;		/* Number of modules added, number of modules removed */
	std::stringstream errstr;	/* String stream containing the error output */

	/* These tags MUST occur and must ONLY occur once in the config file */
	static char* Once[] = { "server", "admin", "files", "power", "options", "pid", NULL };

	/* These tags can occur ONCE or not at all */
	static InitialConfig Values[] = {
		{"options",	"softlimit",		&this->SoftLimit,		DT_INTEGER, ValidateSoftLimit},
		{"options",	"somaxconn",		&this->MaxConn,			DT_INTEGER, ValidateMaxConn},
		{"server",	"name",			&this->ServerName,		DT_CHARPTR, ValidateServerName},
		{"server",	"description",		&this->ServerDesc,		DT_CHARPTR, NoValidation},
		{"server",	"network",		&this->Network,			DT_CHARPTR, NoValidation},
		{"admin",	"name",			&this->AdminName,		DT_CHARPTR, NoValidation},
		{"admin",	"email",		&this->AdminEmail,		DT_CHARPTR, NoValidation},
		{"admin",	"nick",			&this->AdminNick,		DT_CHARPTR, NoValidation},
		{"files",	"motd",			&this->motd,			DT_CHARPTR, ValidateMotd},
		{"files",	"rules",		&this->rules,			DT_CHARPTR, ValidateRules},
		{"power",	"diepass",		&this->diepass,			DT_CHARPTR, NoValidation},	
		{"power",	"pauseval",		&this->DieDelay,		DT_INTEGER, NoValidation},
		{"power",	"restartpass",		&this->restartpass,		DT_CHARPTR, NoValidation},
		{"options",	"prefixquit",		&this->PrefixQuit,		DT_CHARPTR, NoValidation},
		{"die",		"value",		&this->DieValue,		DT_CHARPTR, NoValidation},
		{"options",	"loglevel",		&debug,				DT_CHARPTR, ValidateLogLevel},
		{"options",	"netbuffersize",	&this->NetBufferSize,		DT_INTEGER, ValidateNetBufferSize},
		{"options",	"maxwho",		&this->MaxWhoResults,		DT_INTEGER, ValidateMaxWho},
		{"options",	"allowhalfop",		&this->AllowHalfop,		DT_BOOLEAN, NoValidation},
		{"dns",		"server",		&this->DNSServer,		DT_CHARPTR, ValidateDnsServer},
		{"dns",		"timeout",		&this->dns_timeout,		DT_INTEGER, ValidateDnsTimeout},
		{"options",	"moduledir",		&this->ModPath,			DT_CHARPTR, ValidateModPath},
		{"disabled",	"commands",		&this->DisabledCommands,	DT_CHARPTR, NoValidation},
		{"options",	"operonlystats",	&this->OperOnlyStats,		DT_CHARPTR, NoValidation},
		{"options",	"customversion",	&this->CustomVersion,		DT_CHARPTR, NoValidation},
		{"options",	"hidesplits",		&this->HideSplits,		DT_BOOLEAN, NoValidation},
		{"options",	"hidebans",		&this->HideBans,		DT_BOOLEAN, NoValidation},
		{"options",	"hidewhois",		&this->HideWhoisServer,		DT_CHARPTR, NoValidation},
		{"options",	"tempdir",		&this->TempDir,			DT_CHARPTR, ValidateTempDir},
		{"pid",		"file",			&this->PID,			DT_CHARPTR, NoValidation},
		{NULL}
	};

	/* These tags can occur multiple times, and therefore they have special code to read them
	 * which is different to the code for reading the singular tags listed above.
	 */
	static MultiConfig MultiValues[] = {

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

	/* Initially, load the config into memory, bail if there are errors
	 */
	if (!LoadConf(CONFIG_FILE,&Config->config_f,&errstr))
	{
		errstr.seekg(0);
		log(DEFAULT,"There were errors in your configuration:\n%s",errstr.str().c_str());

		if (bail)
		{
			printf("There were errors in your configuration:\n%s",errstr.str().c_str());
			Exit(0);
		}
		else
		{
			if (user)
			{
				WriteServ(user->fd,"NOTICE %s :There were errors in the configuration file:",user->nick);
				while (!errstr.eof())
				{
					errstr.getline(dataline,1024);
					WriteServ(user->fd,"NOTICE %s :%s",user->nick,dataline);
				}
			}
			else
			{
				WriteOpers("There were errors in the configuration file:");
				while (!errstr.eof())
				{
					errstr.getline(dataline,1024);
					WriteOpers(dataline);
				}
			}

			return;
		}
	}

	/* Check we dont have more than one of singular tags, or any of them missing
	 */
	for (int Index = 0; Once[Index]; Index++)
		if (!CheckOnce(Once[Index],bail,user))
			return;

	/* Read the values of all the tags which occur once or not at all, and call their callbacks.
	 */
	for (int Index = 0; Values[Index].tag; Index++)
	{
		int* val_i = (int*) Values[Index].val;
		char* val_c = (char*) Values[Index].val;

		switch (Values[Index].datatype)
		{
			case DT_CHARPTR:
				ConfValue(Values[Index].tag, Values[Index].value, 0, val_c, &this->config_f);
			break;

			case DT_INTEGER:
				convert = new char[MAXBUF];
				ConfValue(Values[Index].tag, Values[Index].value, 0, convert, &this->config_f);
				*val_i = atoi(convert);
				delete[] convert;
			break;

			case DT_BOOLEAN:
				convert = new char[MAXBUF];
				ConfValue(Values[Index].tag, Values[Index].value, 0, convert, &this->config_f);
				*val_i = ((*convert == tolower('y')) || (*convert == tolower('t')) || (*convert == '1'));
				delete[] convert;
			break;

			case DT_NOTHING:
			break;
		}

		Values[Index].validation_function(Values[Index].tag, Values[Index].value, Values[Index].val);
	}

	/* Claim memory for use when reading multiple tags
	 */
	for (int n = 0; n < 12; n++)
		data[n] = new char[MAXBUF];

	/* Read the multiple-tag items (class tags, connect tags, etc)
	 * and call the callbacks associated with them. We have three
	 * callbacks for these, a 'start', 'item' and 'end' callback.
	 */
	for (int Index = 0; MultiValues[Index].tag; Index++)
	{
		MultiValues[Index].init_function(MultiValues[Index].tag);

		int number_of_tags = ConfValueEnum((char*)MultiValues[Index].tag, &this->config_f);

		for (int tagnum = 0; tagnum < number_of_tags; tagnum++)
		{
			for (int valuenum = 0; MultiValues[Index].items[valuenum]; valuenum++)
			{
				ConfValue((char*)MultiValues[Index].tag,(char*)MultiValues[Index].items[valuenum], tagnum, data[valuenum], &this->config_f);

				switch (MultiValues[Index].datatype[valuenum])
				{
					case DT_CHARPTR:
						ptr[valuenum] = data[valuenum];
					break;
					case DT_INTEGER:
						r_i[valuenum] = atoi(data[valuenum]);
						ptr[valuenum] = &r_i[valuenum];
					break;
					case DT_BOOLEAN:
						r_i[valuenum] = ((*data[valuenum] == tolower('y')) || (*data[valuenum] == tolower('t')) || (*data[valuenum] == '1'));
						ptr[valuenum] = &r_i[valuenum];
					break;
					default:
					break;
				}
			}
			MultiValues[Index].validation_function(MultiValues[Index].tag, (char**)MultiValues[Index].items, ptr, MultiValues[Index].datatype);
		}

		MultiValues[Index].finish_function(MultiValues[Index].tag);
	}

	/* Free any memory we claimed
	 */
	for (int n = 0; n < 12; n++)
		delete[] data[n];

	// write once here, to try it out and make sure its ok
	WritePID(Config->PID);

	log(DEFAULT,"Done reading configuration file, InspIRCd is now starting.");

	/* If we're rehashing, let's load any new modules, and unload old ones
	 */
	if (!bail)
	{
		ServerInstance->stats->BoundPortCount = BindPorts(false);

		if (!removed_modules.empty())
			for (std::vector<std::string>::iterator removing = removed_modules.begin(); removing != removed_modules.end(); removing++)
			{
				if (ServerInstance->UnloadModule(removing->c_str()))
				{
					WriteOpers("*** REHASH UNLOADED MODULE: %s",removing->c_str());

					if (user)
						WriteServ(user->fd,"973 %s %s :Module %s successfully unloaded.",user->nick, removing->c_str(), removing->c_str());

					rem++;
				}
				else
				{
					if (user)
						WriteServ(user->fd,"972 %s %s :Failed to unload module %s: %s",user->nick, removing->c_str(), removing->c_str(), ServerInstance->ModuleError());
				}
			}

		if (!added_modules.empty())
		for (std::vector<std::string>::iterator adding = added_modules.begin(); adding != added_modules.end(); adding++)
		{
			if (ServerInstance->LoadModule(adding->c_str()))
			{
				WriteOpers("*** REHASH LOADED MODULE: %s",adding->c_str());

				if (user)
					WriteServ(user->fd,"975 %s %s :Module %s successfully loaded.",user->nick, adding->c_str(), adding->c_str());

				add++;
			}
			else
			{
				if (user)
					WriteServ(user->fd,"974 %s %s :Failed to load module %s: %s",user->nick, adding->c_str(), adding->c_str(), ServerInstance->ModuleError());
			}
		}

		log(DEFAULT,"Successfully unloaded %lu of %lu modules and loaded %lu of %lu modules.",(unsigned long)rem,(unsigned long)removed_modules.size(),(unsigned long)add,(unsigned long)added_modules.size());
	}
}

void Exit(int status)
{
	if (Config->log_file)
		fclose(Config->log_file);
	send_error("Server shutdown.");
	exit (status);
}

void Killed(int status)
{
	if (Config->log_file)
		fclose(Config->log_file);
	send_error("Server terminated.");
	exit(status);
}

char* CleanFilename(char* name)
{
	char* p = name + strlen(name);
	while ((p != name) && (*p != '/')) p--;
	return (p != name ? ++p : p);
}


void Rehash(int status)
{
	WriteOpers("Rehashing config file %s due to SIGHUP",CleanFilename(CONFIG_FILE));
	fclose(Config->log_file);
	OpenLog(NULL,0);
	Config->Read(false,NULL);
	FOREACH_MOD(I_OnRehash,OnRehash(""));
}



void Start()
{
	printf("\033[1;32mInspire Internet Relay Chat Server, compiled %s at %s\n",__DATE__,__TIME__);
	printf("(C) ChatSpike Development team.\033[0m\n\n");
	printf("Developers:\t\t\033[1;32mBrain, FrostyCoolSlug, w00t, Om\033[0m\n");
	printf("Others:\t\t\t\033[1;32mSee /INFO Output\033[0m\n");
	printf("Name concept:\t\t\033[1;32mLord_Zathras\033[0m\n\n");
}

void WritePID(const std::string &filename)
{
	ofstream outfile(filename.c_str());
	if (outfile.is_open())
	{
		outfile << getpid();
		outfile.close();
	}
	else
	{
		printf("Failed to write PID-file '%s', exiting.\n",filename.c_str());
		log(DEFAULT,"Failed to write PID-file '%s', exiting.",filename.c_str());
		Exit(0);
	}
}

void SetSignals()
{
	signal (SIGALRM, SIG_IGN);
	signal (SIGHUP, Rehash);
	signal (SIGPIPE, SIG_IGN);
	signal (SIGTERM, Exit);
	signal (SIGSEGV, Error);
}


bool DaemonSeed()
{
	int childpid;
	if ((childpid = fork ()) < 0)
		return (ERROR);
	else if (childpid > 0)
	{
		/* We wait a few seconds here, so that the shell prompt doesnt come back over the output */
		sleep(6);
		exit (0);
	}
	setsid ();
	umask (007);
	printf("InspIRCd Process ID: \033[1;32m%lu\033[0m\n",(unsigned long)getpid());

	if (Config->unlimitcore)
	{
		rlimit rl;
		if (getrlimit(RLIMIT_CORE, &rl) == -1)
		{
			log(DEFAULT,"Failed to getrlimit()!");
			return false;
		}
		else
		{
			rl.rlim_cur = rl.rlim_max;
			if (setrlimit(RLIMIT_CORE, &rl) == -1)
				log(DEFAULT,"setrlimit() failed, cannot increase coredump size.");
		}
	}
  
	return true;
}


/* Make Sure Modules Are Avaliable!
 * (BugFix By Craig.. See? I do work! :p)
 * Modified by brain, requires const char*
 * to work with other API functions
 */

bool FileExists (const char* file)
{
	FILE *input;
	if ((input = fopen (file, "r")) == NULL)
	{
		return(false);
	}
	else
	{
		fclose (input);
		return(true);
	}
}

/* ConfProcess does the following things to a config line in the following order:
 *
 * Processes the line for syntax errors as shown below
 *  (1) Line void of quotes or equals (a malformed, illegal tag format)
 *  (2) Odd number of quotes on the line indicating a missing quote
 *  (3) number of equals signs not equal to number of quotes / 2 (missing an equals sign)
 *  (4) Spaces between the opening bracket (<) and the keyword
 *  (5) Spaces between a keyword and an equals sign
 *  (6) Spaces between an equals sign and a quote
 * Removes trailing spaces
 * Removes leading spaces
 * Converts tabs to spaces
 * Turns multiple spaces that are outside of quotes into single spaces
 */

std::string ServerConfig::ConfProcess(char* buffer, long linenumber, std::stringstream* errorstream, bool &error, std::string filename)
{
	long number_of_quotes = 0;
	long number_of_equals = 0;
	bool has_open_bracket = false;
	bool in_quotes = false;
	char* trailing;

	error = false;
	if (!buffer)
	{
		return "";
	}
	// firstly clean up the line by stripping spaces from the start and end and converting tabs to spaces
	for (char* d = buffer; *d; d++)
		if (*d == 9)
			*d = ' ';
	while (*buffer == ' ') buffer++;
	trailing = buffer + strlen(buffer) - 1;
	while (*trailing == ' ') *trailing-- = '\0';

	// empty lines are syntactically valid, as are comments
	if (!(*buffer) || buffer[0] == '#')
		return "";

	for (char* c = buffer; *c; c++)
	{
		// convert all spaces that are OUTSIDE quotes into hardspace (0xA0) as this will make them easier to
		// search and replace later :)
		if ((!in_quotes) && (*c == ' '))
			*c = '\xA0';
		if ((*c == '<') && (!in_quotes))
		{
			has_open_bracket = true;
			if (!(*(buffer+1)))
			{
				*errorstream << "Tag without identifier at " << filename << ":" << linenumber << endl;
				error = true;
				return "";
			}
			else if ((tolower(*(c+1)) < 'a') || (tolower(*(c+1)) > 'z'))
			{
				*errorstream << "Invalid characters in identifier at " << filename << ":" << linenumber << endl;
				error = true;
				return "";
			}
		}
		if (*c == '"')
		{
			number_of_quotes++;
			in_quotes = (!in_quotes);
		}
		if ((*c == '=') && (!in_quotes))
		{
			number_of_equals++;
			if (*(c+1) == 0)
			{
				*errorstream << "Variable without a value at " << filename << ":" << linenumber << endl;
				error = true;
				return "";
			}
			else if (*(c+1) != '"')
			{
				*errorstream << "Variable name not followed immediately by its value at " << filename << ":" << linenumber << endl;
				error = true;
				return "";
			}
			else if (c == buffer)
			{
				*errorstream << "Value without a variable (line starts with '=') at " << filename << ":" << linenumber << endl;
				error = true;
				return "";
			}
			else if (*(c-1) == '\xA0')
			{
				*errorstream << "Variable name not followed immediately by its value at " << filename << ":" << linenumber << endl;
				error = true;
				return "";
			}
		}
	}
	// no quotes, and no equals. something freaky.
	if ((!number_of_quotes) || (!number_of_equals) && (strlen(buffer)>2) && (*buffer == '<'))
	{
		*errorstream << "Malformed tag at " << filename << ":" << linenumber << endl;
		error = true;
		return "";
	}
	// odd number of quotes. thats just wrong.
	if ((number_of_quotes % 2) != 0)
	{
		*errorstream << "Missing \" at " << filename << ":" << linenumber << endl;
		error = true;
		return "";
	}
	if (number_of_equals < (number_of_quotes/2))
	{
		*errorstream << "Missing '=' at " << filename << ":" << linenumber << endl;
	}
	if (number_of_equals > (number_of_quotes/2))
	{
		*errorstream << "Too many '=' at " << filename << ":" << linenumber << endl;
	}

	std::string parsedata = buffer;
	// turn multispace into single space
	while (parsedata.find("\xA0\xA0") != std::string::npos)
	{
		parsedata.erase(parsedata.find("\xA0\xA0"),1);
	}

	// turn our hardspace back into softspace
	for (unsigned int d = 0; d < parsedata.length(); d++)
	{
		if (parsedata[d] == '\xA0')
			parsedata[d] = ' ';
	}

	// and we're done, the line is fine!
	return parsedata;
}

int ServerConfig::fgets_safe(char* buffer, size_t maxsize, FILE* &file)
{
	char c_read = 0;
	size_t n = 0;
	char* bufptr = buffer;
	while ((!feof(file)) && (c_read != '\n') && (c_read != '\r') && (n < maxsize))
	{
		c_read = fgetc(file);
		if ((c_read != '\n') && (c_read != '\r'))
		{
			*bufptr++ = c_read;
			n++;
		}
	}
	*bufptr = 0;
	return bufptr - buffer;
}

bool ServerConfig::LoadConf(const char* filename, std::stringstream *target, std::stringstream* errorstream)
{
	target->str("");
	errorstream->str("");
	long linenumber = 1;
	// first, check that the file exists before we try to do anything with it
	if (!FileExists(filename))
	{
		*errorstream << "File " << filename << " not found." << endl;
		return false;
	}
	// Fix the chmod of the file to restrict it to the current user and group
	chmod(filename,0600);
	for (unsigned int t = 0; t < include_stack.size(); t++)
	{
		if (std::string(filename) == include_stack[t])
		{
			*errorstream << "File " << filename << " is included recursively (looped inclusion)." << endl;
			return false;
		}
	}
	include_stack.push_back(filename);
	// now open it
	FILE* conf = fopen(filename,"r");
	char buffer[MAXBUF];
	if (conf)
	{
		while (!feof(conf))
		{
			if (fgets_safe(buffer, MAXBUF, conf))
			{
				if ((!feof(conf)) && (buffer) && (*buffer))
				{
					if ((buffer[0] != '#') && (buffer[0] != '\r')  && (buffer[0] != '\n'))
					{
						if (!strncmp(buffer,"<include file=\"",15))
						{
							char* buf = buffer;
							char confpath[10240],newconf[10240];
							// include file directive
							buf += 15;	// advance to filename
							for (char* j = buf; *j; j++)
							{
								if (*j == '\\')
									*j = '/';
								if (*j == '"')
								{
									*j = 0;
									break;
								}
							}
							log(DEBUG,"Opening included file '%s'",buf);
							if (*buf != '/')
							{
								strlcpy(confpath,CONFIG_FILE,10240);
								if (strstr(confpath,"/inspircd.conf"))
								{
									// leaves us with just the path
									*(strstr(confpath,"/inspircd.conf")) = '\0';
								}
								snprintf(newconf,10240,"%s/%s",confpath,buf);
							}
							else strlcpy(newconf,buf,10240);
							std::stringstream merge(stringstream::in | stringstream::out);
							// recursively call LoadConf and get the new data, use the same errorstream
							if (LoadConf(newconf, &merge, errorstream))
							{
								// append to the end of the file
								std::string newstuff = merge.str();
								*target << newstuff;
							}
							else
							{
								// the error propogates up to its parent recursively
								// causing the config reader to bail at the top level.
								fclose(conf);
								return false;
							}
						}
						else
						{
							bool error = false;
							std::string data = this->ConfProcess(buffer,linenumber++,errorstream,error,filename);
							if (error)
							{
								return false;
							}
							*target << data;
						}
					}
					else linenumber++;
				}
			}
		}
		fclose(conf);
	}
	target->seekg(0);
	return true;
}

/* Counts the number of tags of a certain type within the config file, e.g. to enumerate opers */

int ServerConfig::EnumConf(std::stringstream *config, const char* tag)
{
	int ptr = 0;
	char buffer[MAXBUF], c_tag[MAXBUF], c, lastc;
	int in_token, in_quotes, tptr, idx = 0;

	std::string x = config->str();
	const char* buf = x.c_str();
	char* bptr = (char*)buf;
	
	ptr = 0;
	in_token = 0;
	in_quotes = 0;
	lastc = '\0';
	while (*bptr)
	{
		lastc = c;
		c = *bptr++;
		if ((c == '#') && (lastc == '\n'))
		{
			while ((c != '\n') && (*bptr))
			{
				lastc = c;
				c = *bptr++;
			}
		}
		if ((c == '<') && (!in_quotes))
		{
			tptr = 0;
			in_token = 1;
			do {
				c = *bptr++;
				if (c != ' ')
				{
					c_tag[tptr++] = c;
					c_tag[tptr] = '\0';
				}
			} while (c != ' ');
		}
		if (c == '"')
		{
			in_quotes = (!in_quotes);
		}
		if ((c == '>') && (!in_quotes))
		{
			in_token = 0;
			if (!strcmp(c_tag,tag))
			{
				/* correct tag, but wrong index */
				idx++;
			}
			c_tag[0] = '\0';
			buffer[0] = '\0';
			ptr = 0;
			tptr = 0;
		}
		if (c != '>')
		{
			if ((in_token) && (c != '\n') && (c != '\r'))
			{
				buffer[ptr++] = c;
				buffer[ptr] = '\0';
			}
		}
	}
	return idx;
}

/* Counts the number of values within a certain tag */

int ServerConfig::EnumValues(std::stringstream *config, const char* tag, int index)
{
	int ptr = 0;
	char buffer[MAXBUF], c_tag[MAXBUF], c, lastc;
	int in_token, in_quotes, tptr, idx = 0;
	bool correct_tag = false;
	int num_items = 0;
	const char* buf = config->str().c_str();
	char* bptr = (char*)buf;

	ptr = 0;
	in_token = 0;
	in_quotes = 0;
	lastc = 0;

	while (*bptr)
	{
		lastc = c;
		c = *bptr++;
		if ((c == '#') && (lastc == '\n'))
		{
			while ((c != '\n') && (*bptr))
			{
				lastc = c;
				c = *bptr++;
			}
		}
		if ((c == '<') && (!in_quotes))
		{
			tptr = 0;
			in_token = 1;
			do {
				c = *bptr++;
				if (c != ' ')
				{
					c_tag[tptr++] = c;
					c_tag[tptr] = '\0';
					
					if ((!strcmp(c_tag,tag)) && (idx == index))
					{
						correct_tag = true;
					}
				}
			} while (c != ' ');
		}
		if (c == '"')
		{
			in_quotes = (!in_quotes);
		}
		
		if ( (correct_tag) && (!in_quotes) && ( (c == ' ') || (c == '\n') || (c == '\r') ) )
		{
			num_items++;
		}
		if ((c == '>') && (!in_quotes))
		{
			in_token = 0;
			if (correct_tag)
				correct_tag = false;
			if (!strcmp(c_tag,tag))
			{
				/* correct tag, but wrong index */
				idx++;
			}
			c_tag[0] = '\0';
			buffer[0] = '\0';
			ptr = 0;
			tptr = 0;
		}
		if (c != '>')
		{
			if ((in_token) && (c != '\n') && (c != '\r'))
			{
				buffer[ptr++] = c;
				buffer[ptr] = '\0';
			}
		}
	}
	return num_items+1;
}


int ServerConfig::ConfValueEnum(char* tag, std::stringstream* config)
{
	return EnumConf(config,tag);
}


int ServerConfig::ReadConf(std::stringstream *config, const char* tag, const char* var, int index, char *result)
{
	int ptr = 0;
	char buffer[65535], c_tag[MAXBUF], c, lastc;
	int in_token, in_quotes, tptr, idx = 0;
	char* key;
	std::string x = config->str();
	const char* buf = x.c_str();
	char* bptr = (char*)buf;
	
	ptr = 0;
	in_token = 0;
	in_quotes = 0;
	lastc = 0;
	c_tag[0] = 0;
	buffer[0] = 0;

	while (*bptr)
	{
		lastc = c;
		c = *bptr++;
		// FIX: Treat tabs as spaces
		if (c == 9)
			c = 32;
		if ((c == '<') && (!in_quotes))
		{
			tptr = 0;
			in_token = 1;
			do {
				c = *bptr++;
				if (c != ' ')
				{
					c_tag[tptr++] = c;
					c_tag[tptr] = '\0';
				}
			// FIX: Tab can follow a tagname as well as space.
			} while ((c != ' ') && (c != 9));
		}
		if (c == '"')
		{
			in_quotes = (!in_quotes);
		}
		if ((c == '>') && (!in_quotes))
		{
			in_token = 0;
			if (idx == index)
			{
				if (!strcmp(c_tag,tag))
				{
					if ((buffer) && (c_tag) && (var))
					{
						key = strstr(buffer,var);
						if (!key)
						{
							/* value not found in tag */
							*result = 0;
							return 0;
						}
						else
						{
							key+=strlen(var);
							while (*key !='"')
							{
								if (!*key)
								{
									/* missing quote */
									*result = 0;
									return 0;
								}
								key++;
							}
							key++;
							for (char* j = key; *j; j++)
							{
								if (*j == '"')
								{
									*j = 0;
									break;
								}
							}
							strlcpy(result,key,MAXBUF);
							return 1;
						}
					}
				}
			}
			if (!strcmp(c_tag,tag))
			{
				/* correct tag, but wrong index */
				idx++;
			}
			c_tag[0] = '\0';
			buffer[0] = '\0';
			ptr = 0;
			tptr = 0;
		}
		if (c != '>')
		{
			if ((in_token) && (c != '\n') && (c != '\r'))
			{
				buffer[ptr++] = c;
				buffer[ptr] = '\0';
			}
		}
	}
	*result = 0; // value or its tag not found at all
	return 0;
}



int ServerConfig::ConfValue(char* tag, char* var, int index, char *result,std::stringstream *config)
{
	ReadConf(config, tag, var, index, result);
	return 0;
}

int ServerConfig::ConfValueInteger(char* tag, char* var, int index, std::stringstream *config)
{
	char result[MAXBUF];
	ReadConf(config, tag, var, index, result);
	return atoi(result);
}

/** This will bind a socket to a port. It works for UDP/TCP.
 * If a hostname is given to bind to, the function will first
 * attempt to resolve the hostname, then bind to the IP the 
 * hostname resolves to. This is a blocking lookup blocking for
 * a maximum of one second before it times out, using the DNS
 * server specified in the configuration file.
 */ 
bool BindSocket (int sockfd, struct sockaddr_in client, struct sockaddr_in server, int port, char* addr)
{
	memset((char *)&server,0,sizeof(server));
	struct in_addr addy;
	bool resolved = false;
	char resolved_addr[128];

	if (*addr == '*')
		*addr = 0;

	if (*addr && !inet_aton(addr,&addy))
	{
		/* If they gave a hostname, bind to the IP it resolves to */
		if (CleanAndResolve(resolved_addr, addr, true))
		{
			inet_aton(resolved_addr,&addy);
			log(DEFAULT,"Resolved binding '%s' -> '%s'",addr,resolved_addr);
			server.sin_addr = addy;
			resolved = true;
		}
		else
		{
			log(DEFAULT,"WARNING: Could not resolve '%s' to an IP for binding to on port %d",addr,port);
			return false;
		}
	}
	server.sin_family = AF_INET;
	if (!resolved)
	{
		if (!*addr)
		{
			server.sin_addr.s_addr = htonl(INADDR_ANY);
		}
		else
		{
			server.sin_addr = addy;
		}
	}
	server.sin_port = htons(port);
	if (bind(sockfd,(struct sockaddr*)&server,sizeof(server)) < 0)
	{
		return false;
	}
	else
	{
		log(DEBUG,"Bound port %s:%d",*addr ? addr : "*",port);
		if (listen(sockfd, Config->MaxConn) == -1)
		{
			log(DEFAULT,"ERROR in listen(): %s",strerror(errno));
			return false;
		}
		else
		{
			return true;
		}
	}
}


// Open a TCP Socket
int OpenTCPSocket()
{
	int sockfd;
	int on = 1;
	struct linger linger = { 0 };
  
	if ((sockfd = socket (AF_INET, SOCK_STREAM, 0)) < 0)
	{
		log(DEFAULT,"Error creating TCP socket: %s",strerror(errno));
		return (ERROR);
	}
	else
	{
		setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (const char*)&on, sizeof(on));
		/* This is BSD compatible, setting l_onoff to 0 is *NOT* http://web.irc.org/mla/ircd-dev/msg02259.html */
		linger.l_onoff = 1;
		linger.l_linger = 1;
		setsockopt(sockfd, SOL_SOCKET, SO_LINGER, (const char*)&linger,sizeof(linger));
		return (sockfd);
	}
}

bool HasPort(int port, char* addr)
{
	for (int count = 0; count < ServerInstance->stats->BoundPortCount; count++)
	{
		if ((port == Config->ports[count]) && (!strcasecmp(Config->addrs[count],addr)))
		{
			return true;
		}
	}
	return false;
}

int BindPorts(bool bail)
{
	char configToken[MAXBUF], Addr[MAXBUF], Type[MAXBUF];
	sockaddr_in client,server;
	int clientportcount = 0;
	int BoundPortCount = 0;

	if (!bail)
	{
		int InitialPortCount = ServerInstance->stats->BoundPortCount;
		log(DEBUG,"Initial port count: %d",InitialPortCount);

		for (int count = 0; count < Config->ConfValueEnum("bind",&Config->config_f); count++)
		{
			Config->ConfValue("bind","port",count,configToken,&Config->config_f);
			Config->ConfValue("bind","address",count,Addr,&Config->config_f);
			Config->ConfValue("bind","type",count,Type,&Config->config_f);
			if (((!*Type) || (!strcmp(Type,"clients"))) && (!HasPort(atoi(configToken),Addr)))
			{
				// modules handle server bind types now
				Config->ports[clientportcount+InitialPortCount] = atoi(configToken);
				if (*Addr == '*')
					*Addr = 0;

				strlcpy(Config->addrs[clientportcount+InitialPortCount],Addr,256);
				clientportcount++;
				log(DEBUG,"NEW binding %s:%s [%s] from config",Addr,configToken, Type);
			}
		}
		int PortCount = clientportcount;
		if (PortCount)
		{
			for (int count = InitialPortCount; count < InitialPortCount + PortCount; count++)
			{
				if ((openSockfd[count] = OpenTCPSocket()) == ERROR)
				{
					log(DEBUG,"Bad fd %d binding port [%s:%d]",openSockfd[count],Config->addrs[count],Config->ports[count]);
					return ERROR;
				}
				if (!BindSocket(openSockfd[count],client,server,Config->ports[count],Config->addrs[count]))
				{
					log(DEFAULT,"Failed to bind port [%s:%d]: %s",Config->addrs[count],Config->ports[count],strerror(errno));
				}
				else
				{
					/* Associate the new open port with a slot in the socket engine */
					ServerInstance->SE->AddFd(openSockfd[count],true,X_LISTEN);
					BoundPortCount++;
				}
			}
			return InitialPortCount + BoundPortCount;
		}
		else
		{
			log(DEBUG,"There is nothing new to bind!");
		}
		return InitialPortCount;
	}

	for (int count = 0; count < Config->ConfValueEnum("bind",&Config->config_f); count++)
	{
		Config->ConfValue("bind","port",count,configToken,&Config->config_f);
		Config->ConfValue("bind","address",count,Addr,&Config->config_f);
		Config->ConfValue("bind","type",count,Type,&Config->config_f);

		if ((!*Type) || (!strcmp(Type,"clients")))
		{
			// modules handle server bind types now
			Config->ports[clientportcount] = atoi(configToken);

			// If the client put bind "*", this is an unrealism.
			// We don't actually support this as documented, but
			// i got fed up of people trying it, so now it converts
			// it to an empty string meaning the same 'bind to all'.
			if (*Addr == '*')
				*Addr = 0;

			strlcpy(Config->addrs[clientportcount],Addr,256);
			clientportcount++;
			log(DEBUG,"Binding %s:%s [%s] from config",Addr,configToken, Type);
		}
	}

	int PortCount = clientportcount;

	for (int count = 0; count < PortCount; count++)
	{
		if ((openSockfd[BoundPortCount] = OpenTCPSocket()) == ERROR)
		{
			log(DEBUG,"Bad fd %d binding port [%s:%d]",openSockfd[BoundPortCount],Config->addrs[count],Config->ports[count]);
			return ERROR;
		}

		if (!BindSocket(openSockfd[BoundPortCount],client,server,Config->ports[count],Config->addrs[count]))
		{
			log(DEFAULT,"Failed to bind port [%s:%d]: %s",Config->addrs[count],Config->ports[count],strerror(errno));
		}
		else
		{
			/* well we at least bound to one socket so we'll continue */
			BoundPortCount++;
		}
	}

	/* if we didn't bind to anything then abort */
	if (!BoundPortCount)
	{
		log(DEFAULT,"No ports bound, bailing!");
		printf("\nERROR: Could not bind any of %d ports! Please check your configuration.\n\n", PortCount);
		return ERROR;
	}

	return BoundPortCount;
}

