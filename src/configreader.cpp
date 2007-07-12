/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2007 InspIRCd Development Team
 * See: http://www.inspircd.org/wiki/index.php/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"
#include "configreader.h"
#include <sstream>
#include <fstream>
#include "xline.h"
#include "exitcodes.h"
#include "commands/cmd_whowas.h"

std::vector<std::string> old_module_names, new_module_names, added_modules, removed_modules;

ServerConfig::ServerConfig(InspIRCd* Instance) : ServerInstance(Instance)
{
	this->ClearStack();
	*ServerName = *Network = *ServerDesc = *AdminName = '\0';
	*HideWhoisServer = *AdminEmail = *AdminNick = *diepass = *restartpass = *FixedQuit = *HideKillsServer = '\0';
	*DefaultModes = *CustomVersion = *motd = *rules = *PrefixQuit = *DieValue = *DNSServer = '\0';
	*UserStats = *ModPath = *MyExecutable = *DisabledCommands = *PID = *SuffixQuit = '\0';
	WhoWasGroupSize = WhoWasMaxGroups = WhoWasMaxKeep = 0;
	log_file = NULL;
	NoUserDns = forcedebug = OperSpyWhois = nofork = HideBans = HideSplits = UndernetMsgPrefix = false;
	CycleHosts = writelog = AllowHalfop = true;
	dns_timeout = DieDelay = 5;
	MaxTargets = 20;
	NetBufferSize = 10240;
	SoftLimit = MAXCLIENTS;
	MaxConn = SOMAXCONN;
	MaxWhoResults = 0;
	debugging = 0;
	MaxChans = 20;
	OperMaxChans = 30;
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

Module* ServerConfig::GetIOHook(InspSocket* is)
{
	std::map<InspSocket*,Module*>::iterator x = SocketIOHookModule.find(is);
	return (x != SocketIOHookModule.end() ? x->second : NULL);
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
		throw ModuleException("Port already hooked by another module");
		return false;
	}
}

bool ServerConfig::AddIOHook(Module* iomod, InspSocket* is)
{
	if (!GetIOHook(is))
	{
		SocketIOHookModule[is] = iomod;
		is->IsIOHooked = true;
		return true;
	}
	else
	{
		throw ModuleException("InspSocket derived class already hooked by another module");
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

bool ServerConfig::DelIOHook(InspSocket* is)
{
	std::map<InspSocket*,Module*>::iterator x = SocketIOHookModule.find(is);
	if (x != SocketIOHookModule.end())
	{
		SocketIOHookModule.erase(x);
		return true;
	}
	return false;
}

void ServerConfig::Update005()
{
	std::stringstream out(data005);
	std::string token;
	std::string line5;
	int token_counter = 0;
	isupport.clear();
	while (out >> token)
	{
		line5 = line5 + token + " ";
		token_counter++;
		if (token_counter >= 13)
		{
			char buf[MAXBUF];
			snprintf(buf, MAXBUF, "%s:are supported by this server", line5.c_str());
			isupport.push_back(buf);
			line5.clear();
			token_counter = 0;
		}
	}
	if (!line5.empty())
	{
		char buf[MAXBUF];
		snprintf(buf, MAXBUF, "%s:are supported by this server", line5.c_str());
		isupport.push_back(buf);
	}
}

void ServerConfig::Send005(userrec* user)
{
	for (std::vector<std::string>::iterator line = ServerInstance->Config->isupport.begin(); line != ServerInstance->Config->isupport.end(); line++)
		user->WriteServ("005 %s %s", user->nick, line->c_str());
}

bool ServerConfig::CheckOnce(char* tag, bool bail, userrec* user)
{
	int count = ConfValueEnum(this->config_data, tag);

	if (count > 1)
	{
		throw CoreException("You have more than one <"+std::string(tag)+"> tag, this is not permitted.");
		return false;
	}
	if (count < 1)
	{
		throw CoreException("You have not defined a <"+std::string(tag)+"> tag, this is required.");
		return false;
	}
	return true;
}

bool NoValidation(ServerConfig* conf, const char* tag, const char* value, ValueItem &data)
{
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
	return true;
}

bool InitializeDisabledCommands(const char* data, InspIRCd* ServerInstance)
{
	std::stringstream dcmds(data);
	std::string thiscmd;

	/* Enable everything first */
	for (command_table::iterator x = ServerInstance->Parser->cmdlist.begin(); x != ServerInstance->Parser->cmdlist.end(); x++)
		x->second->Disable(false);

	/* Now disable all the ones which the user wants disabled */
	while (dcmds >> thiscmd)
	{
		command_table::iterator cm = ServerInstance->Parser->cmdlist.find(thiscmd);
		if (cm != ServerInstance->Parser->cmdlist.end())
		{
			cm->second->Disable(true);
		}
	}
	return true;
}

bool ValidateDnsServer(ServerConfig* conf, const char* tag, const char* value, ValueItem &data)
{
	if (!*(data.GetString()))
	{
		std::string nameserver;
#ifdef WINDOWS
		conf->GetInstance()->Log(DEFAULT,"WARNING: <dns:server> not defined, attempting to find working server in the registry...");
		nameserver = FindNameServerWin();
		/* Windows stacks multiple nameservers in one registry key, seperated by commas.
		 * Spotted by Cataclysm.
		 */
		if (nameserver.find(',') != std::string::npos)
			nameserver = nameserver.substr(0, nameserver.find(','));
		data.Set(nameserver.c_str());
		conf->GetInstance()->Log(DEFAULT,"<dns:server> set to '%s' as first active resolver in registry.", nameserver.c_str());
#else
		// attempt to look up their nameserver from /etc/resolv.conf
		conf->GetInstance()->Log(DEFAULT,"WARNING: <dns:server> not defined, attempting to find working server in /etc/resolv.conf...");
		ifstream resolv("/etc/resolv.conf");
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
#endif
	}
	return true;
}

bool ValidateServerName(ServerConfig* conf, const char* tag, const char* value, ValueItem &data)
{
	/* If we already have a servername, and they changed it, we should throw an exception. */
	if ((strcasecmp(conf->ServerName, data.GetString())) && (*conf->ServerName))
	{
		throw CoreException("Configuration error: You cannot change your servername at runtime! Please restart your server for this change to be applied.");
		/* XXX: We don't actually reach this return of course... */
		return false;
	}
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
	if ((data.GetInteger() > 65535) || (data.GetInteger() < 1))
	{
		conf->GetInstance()->Log(DEFAULT,"<options:maxwhoresults> size out of range, setting to default of 128.");
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

bool ValidateNotEmpty(ServerConfig* conf, const char* tag, const char* value, ValueItem &data)
{
	if (!*data.GetString())
		throw CoreException(std::string("The value for ")+tag+" cannot be empty!");
	return true;
}

bool ValidateRules(ServerConfig* conf, const char* tag, const char* value, ValueItem &data)
{
	conf->ReadFile(conf->RULES, data.GetString());
	return true;
}

bool ValidateModeLists(ServerConfig* conf, const char* tag, const char* value, ValueItem &data)
{
	memset(conf->HideModeLists, 0, 256);
	for (const unsigned char* x = (const unsigned char*)data.GetString(); *x; ++x)
		conf->HideModeLists[*x] = true;
	return true;
}

bool ValidateExemptChanOps(ServerConfig* conf, const char* tag, const char* value, ValueItem &data)
{
	memset(conf->ExemptChanOps, 0, 256);
	for (const unsigned char* x = (const unsigned char*)data.GetString(); *x; ++x)
		conf->ExemptChanOps[*x] = true;
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

	command_t* whowas_command = conf->GetInstance()->Parser->GetHandler("WHOWAS");
	if (whowas_command)
	{
		std::deque<classbase*> params;
		whowas_command->HandleInternal(WHOWAS_PRUNE, params);
	}

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
		ConnectClass c(timeout, flood, allow, pingfreq, password, threshold, sendq, recvq, localmax, globalmax);
		conf->Classes.push_back(c);
	}
	else
	{
		ConnectClass c(deny);
		conf->Classes.push_back(c);
	}

	return true;
}

/* Callback called when there are no more <connect> tags
 */
bool DoneConnect(ServerConfig* conf, const char* tag)
{
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
	const bool silent = values[1].GetBool();
	conf->ulines[server] = silent;
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

void ServerConfig::ReportConfigError(const std::string &errormessage, bool bail, userrec* user)
{
	ServerInstance->Log(DEFAULT, "There were errors in your configuration file: %s", errormessage.c_str());
	if (bail)
	{
		/* Unneeded because of the ServerInstance->Log() aboive? */
		printf("There were errors in your configuration:\n%s\n\n",errormessage.c_str());
		InspIRCd::Exit(EXIT_STATUS_CONFIG);
	}
	else
	{
		std::string errors = errormessage;
		std::string::size_type start;
		unsigned int prefixlen;
		start = 0;
		/* ":ServerInstance->Config->ServerName NOTICE user->nick :" */
		if (user)
		{
			prefixlen = strlen(this->ServerName) + strlen(user->nick) + 11;
			user->WriteServ("NOTICE %s :There were errors in the configuration file:",user->nick);
			while (start < errors.length())
			{
				user->WriteServ("NOTICE %s :%s",user->nick, errors.substr(start, 510 - prefixlen).c_str());
				start += 510 - prefixlen;
			}
		}
		else
		{
			ServerInstance->WriteOpers("There were errors in the configuration file:");
			while (start < errors.length())
			{
				ServerInstance->WriteOpers(errors.substr(start, 360).c_str());
				start += 360;
			}
		}
		return;
	}
}

void ServerConfig::Read(bool bail, userrec* user)
{
	static char debug[MAXBUF];	/* Temporary buffer for debugging value */
	static char maxkeep[MAXBUF];	/* Temporary buffer for WhoWasMaxKeep value */
	static char hidemodes[MAXBUF];	/* Modes to not allow listing from users below halfop */
	static char exemptchanops[MAXBUF];	/* Exempt channel ops from these modes */
	int rem = 0, add = 0;		/* Number of modules added, number of modules removed */
	std::ostringstream errstr;	/* String stream containing the error output */

	/* These tags MUST occur and must ONLY occur once in the config file */
	static char* Once[] = { "server", "admin", "files", "power", "options", NULL };

	/* These tags can occur ONCE or not at all */
	InitialConfig Values[] = {
		{"options",	"softlimit",	MAXCLIENTS_S,		new ValueContainerUInt (&this->SoftLimit),		DT_INTEGER, ValidateSoftLimit},
		{"options",	"somaxconn",	SOMAXCONN_S,		new ValueContainerInt  (&this->MaxConn),		DT_INTEGER, ValidateMaxConn},
		{"options",	"moronbanner",	"Youre banned!",	new ValueContainerChar (this->MoronBanner),		DT_CHARPTR, NoValidation},
		{"server",	"name",		"",			new ValueContainerChar (this->ServerName),		DT_CHARPTR, ValidateServerName},
		{"server",	"description",	"Configure Me",		new ValueContainerChar (this->ServerDesc),		DT_CHARPTR, NoValidation},
		{"server",	"network",	"Network",		new ValueContainerChar (this->Network),			DT_CHARPTR, NoValidation},
		{"admin",	"name",		"",			new ValueContainerChar (this->AdminName),		DT_CHARPTR, NoValidation},
		{"admin",	"email",	"Mis@configu.red",	new ValueContainerChar (this->AdminEmail),		DT_CHARPTR, NoValidation},
		{"admin",	"nick",		"Misconfigured",	new ValueContainerChar (this->AdminNick),		DT_CHARPTR, NoValidation},
		{"files",	"motd",		"",			new ValueContainerChar (this->motd),			DT_CHARPTR, ValidateMotd},
		{"files",	"rules",	"",			new ValueContainerChar (this->rules),			DT_CHARPTR, ValidateRules},
		{"power",	"diepass",	"",			new ValueContainerChar (this->diepass),			DT_CHARPTR, ValidateNotEmpty},
		{"power",	"pause",	"",			new ValueContainerInt  (&this->DieDelay),		DT_INTEGER, NoValidation},
		{"power",	"restartpass",	"",			new ValueContainerChar (this->restartpass),		DT_CHARPTR, ValidateNotEmpty},
		{"options",	"prefixquit",	"",			new ValueContainerChar (this->PrefixQuit),		DT_CHARPTR, NoValidation},
		{"options",	"suffixquit",	"",			new ValueContainerChar (this->SuffixQuit),		DT_CHARPTR, NoValidation},
		{"options",	"fixedquit",	"",			new ValueContainerChar (this->FixedQuit),		DT_CHARPTR, NoValidation},
		{"options",	"loglevel",	"default",		new ValueContainerChar (debug),				DT_CHARPTR, ValidateLogLevel},
		{"options",	"netbuffersize","10240",		new ValueContainerInt  (&this->NetBufferSize),		DT_INTEGER, ValidateNetBufferSize},
		{"options",	"maxwho",	"128",			new ValueContainerInt  (&this->MaxWhoResults),		DT_INTEGER, ValidateMaxWho},
		{"options",	"allowhalfop",	"0",			new ValueContainerBool (&this->AllowHalfop),		DT_BOOLEAN, NoValidation},
		{"dns",		"server",	"",			new ValueContainerChar (this->DNSServer),		DT_CHARPTR, ValidateDnsServer},
		{"dns",		"timeout",	"5",			new ValueContainerInt  (&this->dns_timeout),		DT_INTEGER, NoValidation},
		{"options",	"moduledir",	MOD_PATH,		new ValueContainerChar (this->ModPath),			DT_CHARPTR, NoValidation},
		{"disabled",	"commands",	"",			new ValueContainerChar (this->DisabledCommands),	DT_CHARPTR, NoValidation},
		{"options",	"userstats",	"",			new ValueContainerChar (this->UserStats),		DT_CHARPTR, NoValidation},
		{"options",	"customversion","",			new ValueContainerChar (this->CustomVersion),		DT_CHARPTR, NoValidation},
		{"options",	"hidesplits",	"0",			new ValueContainerBool (&this->HideSplits),		DT_BOOLEAN, NoValidation},
		{"options",	"hidebans",	"0",			new ValueContainerBool (&this->HideBans),		DT_BOOLEAN, NoValidation},
		{"options",	"hidewhois",	"",			new ValueContainerChar (this->HideWhoisServer),		DT_CHARPTR, NoValidation},
		{"options",	"hidekills",	"",			new ValueContainerChar (this->HideKillsServer),		DT_CHARPTR, NoValidation},
		{"options",	"operspywhois",	"0",			new ValueContainerBool (&this->OperSpyWhois),		DT_BOOLEAN, NoValidation},
		{"options",	"nouserdns",	"0",			new ValueContainerBool (&this->NoUserDns),		DT_BOOLEAN, NoValidation},
		{"options",	"syntaxhints",	"0",			new ValueContainerBool (&this->SyntaxHints),		DT_BOOLEAN, NoValidation},
		{"options",	"cyclehosts",	"0",			new ValueContainerBool (&this->CycleHosts),		DT_BOOLEAN, NoValidation},
		{"options",	"ircumsgprefix","0",			new ValueContainerBool (&this->UndernetMsgPrefix),	DT_BOOLEAN, NoValidation},
		{"options",	"announceinvites", "1",			new ValueContainerBool (&this->AnnounceInvites),	DT_BOOLEAN, NoValidation},
		{"options",	"hostintopic",	"1",			new ValueContainerBool (&this->FullHostInTopic),	DT_BOOLEAN, NoValidation},
		{"options",	"hidemodes",	"",			new ValueContainerChar (hidemodes),			DT_CHARPTR, ValidateModeLists},
		{"options",	"exemptchanops","",			new ValueContainerChar (exemptchanops),			DT_CHARPTR, ValidateExemptChanOps},
		{"options",	"defaultmodes", "nt",			new ValueContainerChar (this->DefaultModes),		DT_CHARPTR, NoValidation},
		{"pid",		"file",		"",			new ValueContainerChar (this->PID),			DT_CHARPTR, NoValidation},
		{"whowas",	"groupsize",	"10",			new ValueContainerInt  (&this->WhoWasGroupSize),	DT_INTEGER, NoValidation},
		{"whowas",	"maxgroups",	"10240",		new ValueContainerInt  (&this->WhoWasMaxGroups),	DT_INTEGER, NoValidation},
		{"whowas",	"maxkeep",	"3600",			new ValueContainerChar (maxkeep),			DT_CHARPTR, ValidateWhoWas},
		{"die",		"value",	"",			new ValueContainerChar (this->DieValue),		DT_CHARPTR, NoValidation},
		{"channels",	"users",	"20",			new ValueContainerUInt (&this->MaxChans),		DT_INTEGER, NoValidation},
		{"channels",	"opers",	"60",			new ValueContainerUInt (&this->OperMaxChans),		DT_INTEGER, NoValidation},
		{NULL}
	};

	/* These tags can occur multiple times, and therefore they have special code to read them
	 * which is different to the code for reading the singular tags listed above.
	 */
	MultiConfig MultiValues[] = {

		{"connect",
				{"allow",	"deny",		"password",	"timeout",	"pingfreq",	"flood",
				"threshold",	"sendq",	"recvq",	"localmax",	"globalmax",	"port",
				NULL},
				{"",		"",		"",		"",		"120",		"",
				 "",		"",		"",		"3",		"3",		"0",
				 NULL},
				{DT_CHARPTR,	DT_CHARPTR,	DT_CHARPTR,	DT_INTEGER,	DT_INTEGER,	DT_INTEGER,
				 DT_INTEGER,	DT_INTEGER,	DT_INTEGER,	DT_INTEGER,	DT_INTEGER,	DT_INTEGER},
				InitConnect, DoConnect, DoneConnect},

		{"uline",
				{"server",	"silent",	NULL},
				{"",		"0",		NULL},
				{DT_CHARPTR,	DT_BOOLEAN},
				InitULine,DoULine,DoneULine},

		{"banlist",
				{"chan",	"limit",	NULL},
				{"",		"",		NULL},
				{DT_CHARPTR,	DT_INTEGER},
				InitMaxBans, DoMaxBans, DoneMaxBans},

		{"module",
				{"name",	NULL},
				{"",		NULL},
				{DT_CHARPTR},
				InitModule, DoModule, DoneModule},

		{"badip",
				{"reason",	"ipmask",	NULL},
				{"No reason",	"",		NULL},
				{DT_CHARPTR,	DT_CHARPTR},
				InitXLine, DoZLine, DoneZLine},

		{"badnick",
				{"reason",	"nick",		NULL},
				{"No reason",	"",		NULL},
				{DT_CHARPTR,	DT_CHARPTR},
				InitXLine, DoQLine, DoneQLine},

		{"badhost",
				{"reason",	"host",		NULL},
				{"No reason",	"",		NULL},
				{DT_CHARPTR,	DT_CHARPTR},
				InitXLine, DoKLine, DoneKLine},

		{"exception",
				{"reason",	"host",		NULL},
				{"No reason",	"",		NULL},
				{DT_CHARPTR,	DT_CHARPTR},
				InitXLine, DoELine, DoneELine},

		{"type",
				{"name",	"classes",	NULL},
				{"",		"",		NULL},
				{DT_CHARPTR,	DT_CHARPTR},
				InitTypes, DoType, DoneClassesAndTypes},

		{"class",
				{"name",	"commands",	NULL},
				{"",		"",		NULL},
				{DT_CHARPTR,	DT_CHARPTR},
				InitClasses, DoClass, DoneClassesAndTypes},

		{NULL}
	};

	include_stack.clear();

	/* Load and parse the config file, if there are any errors then explode */

	/* Make a copy here so if it fails then we can carry on running with an unaffected config */
	ConfigDataHash newconfig;

	if (this->LoadConf(newconfig, ServerInstance->ConfigFileName, errstr))
	{
		/* If we succeeded, set the ircd config to the new one */
		this->config_data = newconfig;
	}
	else
	{
		ReportConfigError(errstr.str(), bail, user);
		return;
	}

	/* The stuff in here may throw CoreException, be sure we're in a position to catch it. */
	try
	{
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
			int dt = Values[Index].datatype;
			bool allow_newlines =  ((dt & DT_ALLOW_NEWLINE) > 0);
			dt &= ~DT_ALLOW_NEWLINE;

			ConfValue(this->config_data, Values[Index].tag, Values[Index].value, Values[Index].default_value, 0, item, MAXBUF, allow_newlines);
			ValueItem vi(item);

			if (!Values[Index].validation_function(this, Values[Index].tag, Values[Index].value, vi))
				throw CoreException("One or more values in your configuration file failed to validate. Please see your ircd.log for more information.");

			switch (Values[Index].datatype)
			{
				case DT_CHARPTR:
				{
					ValueContainerChar* vcc = (ValueContainerChar*)Values[Index].val;
					/* Make sure we also copy the null terminator */
					vcc->Set(vi.GetString(), strlen(vi.GetString()) + 1);
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
					int dt = MultiValues[Index].datatype[valuenum];
					bool allow_newlines =  ((dt & DT_ALLOW_NEWLINE) > 0);
					dt &= ~DT_ALLOW_NEWLINE;

					switch (dt)
					{
						case DT_CHARPTR:
						{
							char item[MAXBUF];
							if (ConfValue(this->config_data, MultiValues[Index].tag, MultiValues[Index].items[valuenum], MultiValues[Index].items_default[valuenum], tagnum, item, MAXBUF, allow_newlines))
								vl.push_back(ValueItem(item));
							else
								vl.push_back(ValueItem(""));
						}
						break;
						case DT_INTEGER:
						{
							int item = 0;
							if (ConfValueInteger(this->config_data, MultiValues[Index].tag, MultiValues[Index].items[valuenum], MultiValues[Index].items_default[valuenum], tagnum, item))
								vl.push_back(ValueItem(item));
							else
								vl.push_back(ValueItem(0));
						}
						break;
						case DT_BOOLEAN:
						{
							bool item = ConfValueBool(this->config_data, MultiValues[Index].tag, MultiValues[Index].items[valuenum], MultiValues[Index].items_default[valuenum], tagnum);
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

	}

	catch (CoreException &ce)
	{
		ReportConfigError(ce.GetReason(), bail, user);
		return;
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
		ServerInstance->BindPorts(false, found_ports, pl);

		if (pl.size())
		{
			user->WriteServ("NOTICE %s :*** Not all your client ports could be bound.", user->nick);
			user->WriteServ("NOTICE %s :*** The following port(s) failed to bind:", user->nick);
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

	if (user)
		user->WriteServ("NOTICE %s :*** Successfully rehashed server.", user->nick);
	else
		ServerInstance->WriteOpers("*** Successfully rehashed server.");
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
	int character_count = 0;

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
		 * Fix for moronic windows issue spotted by Adremelech.
		 * Some windows editors save text files as utf-16, which is
		 * a total pain in the ass to parse. Users should save in the
		 * right config format! If we ever see a file where the first
		 * byte is 0xFF or 0xFE, or the second is 0xFF or 0xFE, then
		 * this is most likely a utf-16 file. Bail out and insult user.
		 */
		if ((character_count++ < 2) && (ch == '\xFF' || ch == '\xFE'))
		{
			errorstream << "File " << filename << " cannot be read, as it is encoded in braindead UTF-16. Save your file as plain ASCII!" << std::endl;
			return false;
		}

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

		switch(ch)
		{
			case '\n':
				if (in_quote)
					line += '\n';
				linenumber++;
			case '\r':
				if (!in_quote)
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
			char real_character;
			if (conf.get(real_character))
			{
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

		if (ch != '\r')
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

	//std::cout << "ParseLine(data, '" << line << "', " << linenumber << ", stream)" << std::endl;

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
				else if ((*c == '\n') && (in_quote))
				{
					/* Got a 'real' \n, treat it as part of the value */
					current_value += '\n';
					continue;
				}
				else if ((*c == '\r') && (in_quote))
					/* Got a \r, drop it */
					continue;

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

	confpath = ServerInstance->ConfigFileName;
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
		if((pos = confpath.rfind("/")) != std::string::npos)
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

bool ServerConfig::ConfValue(ConfigDataHash &target, const char* tag, const char* var, int index, char* result, int length, bool allow_linefeeds)
{
	return ConfValue(target, tag, var, "", index, result, length, allow_linefeeds);
}

bool ServerConfig::ConfValue(ConfigDataHash &target, const char* tag, const char* var, const char* default_value, int index, char* result, int length, bool allow_linefeeds)
{
	std::string value;
	bool r = ConfValue(target, std::string(tag), std::string(var), std::string(default_value), index, value, allow_linefeeds);
	strlcpy(result, value.c_str(), length);
	return r;
}

bool ServerConfig::ConfValue(ConfigDataHash &target, const std::string &tag, const std::string &var, int index, std::string &result, bool allow_linefeeds)
{
	return ConfValue(target, tag, var, "", index, result, allow_linefeeds);
}

bool ServerConfig::ConfValue(ConfigDataHash &target, const std::string &tag, const std::string &var, const std::string &default_value, int index, std::string &result, bool allow_linefeeds)
{
	ConfigDataHash::size_type pos = index;
	if((pos >= 0) && (pos < target.count(tag)))
	{
		ConfigDataHash::iterator iter = target.find(tag);

		for(int i = 0; i < index; i++)
			iter++;

		for(KeyValList::iterator j = iter->second.begin(); j != iter->second.end(); j++)
		{
			if(j->first == var)
			{
 				if ((!allow_linefeeds) && (j->second.find('\n') != std::string::npos))
				{
					ServerInstance->Log(DEFAULT, "Value of <" + tag + ":" + var+ "> contains a linefeed, and linefeeds in this value are not permitted -- stripped to spaces.");
					for (std::string::iterator n = j->second.begin(); n != j->second.end(); n++)
						if (*n == '\n')
							*n = ' ';
				}
				else
				{
					result = j->second;
					return true;
				}
			}
		}
		if (!default_value.empty())
		{
			result = default_value;
			return true;
		}
	}
	else if(pos == 0)
	{
		if (!default_value.empty())
		{
			result = default_value;
			return true;
		}
	}
	return false;
}

bool ServerConfig::ConfValueInteger(ConfigDataHash &target, const char* tag, const char* var, int index, int &result)
{
	return ConfValueInteger(target, std::string(tag), std::string(var), "", index, result);
}

bool ServerConfig::ConfValueInteger(ConfigDataHash &target, const char* tag, const char* var, const char* default_value, int index, int &result)
{
	return ConfValueInteger(target, std::string(tag), std::string(var), std::string(default_value), index, result);
}

bool ServerConfig::ConfValueInteger(ConfigDataHash &target, const std::string &tag, const std::string &var, int index, int &result)
{
	return ConfValueInteger(target, tag, var, "", index, result);
}

bool ServerConfig::ConfValueInteger(ConfigDataHash &target, const std::string &tag, const std::string &var, const std::string &default_value, int index, int &result)
{
	std::string value;
	std::istringstream stream;
	bool r = ConfValue(target, tag, var, default_value, index, value);
	stream.str(value);
	if(!(stream >> result))
		return false;
	else
	{
		if (!value.empty())
		{
			if (value.substr(0,2) == "0x")
			{
				char* endptr;

				value.erase(0,2);
				result = strtol(value.c_str(), &endptr, 16);

				/* No digits found */
				if (endptr == value.c_str())
					return false;
			}
			else
			{
				char denominator = *(value.end() - 1);
				switch (toupper(denominator))
				{
					case 'K':
						/* Kilobytes -> bytes */
						result = result * 1024;
					break;
					case 'M':
						/* Megabytes -> bytes */
						result = result * 1024 * 1024;
					break;
					case 'G':
						/* Gigabytes -> bytes */
						result = result * 1024 * 1024 * 1024;
					break;
				}
			}
		}
	}
	return r;
}


bool ServerConfig::ConfValueBool(ConfigDataHash &target, const char* tag, const char* var, int index)
{
	return ConfValueBool(target, std::string(tag), std::string(var), "", index);
}

bool ServerConfig::ConfValueBool(ConfigDataHash &target, const char* tag, const char* var, const char* default_value, int index)
{
	return ConfValueBool(target, std::string(tag), std::string(var), std::string(default_value), index);
}

bool ServerConfig::ConfValueBool(ConfigDataHash &target, const std::string &tag, const std::string &var, int index)
{
	return ConfValueBool(target, tag, var, "", index);
}

bool ServerConfig::ConfValueBool(ConfigDataHash &target, const std::string &tag, const std::string &var, const std::string &default_value, int index)
{
	std::string result;
	if(!ConfValue(target, tag, var, default_value, index, result))
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

	return 0;
}

/** Read the contents of a file located by `fname' into a file_cache pointed at by `F'.
 */
bool ServerConfig::ReadFile(file_cache &F, const char* fname)
{
	if (!fname || !*fname)
		return false;

	FILE* file = NULL;
	char linebuf[MAXBUF];

	F.clear();

	if ((*fname != '/') && (*fname != '\\'))
	{
		std::string::size_type pos;
		std::string confpath = ServerInstance->ConfigFileName;
		std::string newfile = fname;

		if ((pos = confpath.rfind("/")) != std::string::npos)
			newfile = confpath.substr(0, pos) + std::string("/") + fname;
		else if ((pos = confpath.rfind("\\")) != std::string::npos)
			newfile = confpath.substr(0, pos) + std::string("\\") + fname;

		if (!FileExists(newfile.c_str()))
			return false;
		file =  fopen(newfile.c_str(), "r");
	}
	else
	{
		if (!FileExists(fname))
			return false;
		file =  fopen(fname, "r");
	}

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
	struct stat sb;
	if (stat(file, &sb) == -1)
		return false;

	if ((sb.st_mode & S_IFDIR) > 0)
		return false;
	     
	FILE *input;
	if ((input = fopen (file, "r")) == NULL)
		return false;
	else
	{
		fclose(input);
		return true;
	}
}

char* ServerConfig::CleanFilename(char* name)
{
	char* p = name + strlen(name);
	while ((p != name) && (*p != '/') && (*p != '\\')) p--;
	return (p != name ? ++p : p);
}


bool ServerConfig::DirValid(const char* dirandfile)
{
#ifdef WINDOWS
	return true;
#endif

	char work[1024];
	char buffer[1024];
	char otherdir[1024];
	int p;

	strlcpy(work, dirandfile, 1024);
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
	if (getcwd(buffer, 1024 ) == NULL )
		return false;

	if (chdir(work) == -1)
		return false;

	if (getcwd(otherdir, 1024 ) == NULL )
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

std::string ServerConfig::GetFullProgDir()
{
	char buffer[PATH_MAX+1];
#ifdef WINDOWS
	/* Windows has specific api calls to get the exe path that never fail.
	 * For once, windows has something of use, compared to the POSIX code
	 * for this, this is positively neato.
	 */
	if (GetModuleFileName(NULL, buffer, MAX_PATH))
	{
		std::string fullpath = buffer;
		std::string::size_type n = fullpath.rfind("\\inspircd.exe");
		return std::string(fullpath, 0, n);
	}
#else
	// Get the current working directory
	if (getcwd(buffer, PATH_MAX))
	{
		std::string remainder = this->argv[0];

		/* Does argv[0] start with /? its a full path, use it */
		if (remainder[0] == '/')
		{
			std::string::size_type n = remainder.rfind("/inspircd");
			return std::string(remainder, 0, n);
		}

		std::string fullpath = std::string(buffer) + "/" + remainder;
		std::string::size_type n = fullpath.rfind("/inspircd");
		return std::string(fullpath, 0, n);
	}
#endif
	return "/";
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
	if (v.empty())
		return 0;
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

