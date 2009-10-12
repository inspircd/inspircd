/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2009 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *	    the file COPYING for details.
 *
 * ---------------------------------------------------
 */

/* $Core */
/* $CopyInstall: conf/inspircd.quotes.example $(CONPATH) */
/* $CopyInstall: conf/inspircd.rules.example $(CONPATH) */
/* $CopyInstall: conf/inspircd.motd.example $(CONPATH) */
/* $CopyInstall: conf/inspircd.helpop-full.example $(CONPATH) */
/* $CopyInstall: conf/inspircd.helpop.example $(CONPATH) */
/* $CopyInstall: conf/inspircd.censor.example $(CONPATH) */
/* $CopyInstall: conf/inspircd.filter.example $(CONPATH) */
/* $CopyInstall: conf/inspircd.conf.example $(CONPATH) */
/* $CopyInstall: conf/modules.conf.example $(CONPATH) */
/* $CopyInstall: conf/opers.conf.example $(CONPATH) */
/* $CopyInstall: conf/links.conf.example $(CONPATH) */
/* $CopyInstall: .gdbargs $(BASE) */

#include "inspircd.h"
#include <fstream>
#include "xline.h"
#include "exitcodes.h"
#include "commands/cmd_whowas.h"
#include "modes/cmode_h.h"

ServerConfig::ServerConfig()
{
	WhoWasGroupSize = WhoWasMaxGroups = WhoWasMaxKeep = 0;
	log_file = NULL;
	NoUserDns = forcedebug = OperSpyWhois = nofork = HideBans = HideSplits = UndernetMsgPrefix = false;
	CycleHosts = writelog = AllowHalfop = InvBypassModes = true;
	dns_timeout = DieDelay = 5;
	MaxTargets = 20;
	NetBufferSize = 10240;
	SoftLimit = ServerInstance->SE->GetMaxFds();
	MaxConn = SOMAXCONN;
	MaxWhoResults = 0;
	debugging = 0;
	MaxChans = 20;
	OperMaxChans = 30;
	c_ipv4_range = 32;
	c_ipv6_range = 128;
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

void ServerConfig::Send005(User* user)
{
	for (std::vector<std::string>::iterator line = ServerInstance->Config->isupport.begin(); line != ServerInstance->Config->isupport.end(); line++)
		user->WriteNumeric(RPL_ISUPPORT, "%s %s", user->nick.c_str(), line->c_str());
}

bool ServerConfig::CheckOnce(const char* tag)
{
	int count = ConfValueEnum(tag);

	if (count > 1)
		throw CoreException("You have more than one <"+std::string(tag)+"> tag, this is not permitted.");
	if (count < 1)
		throw CoreException("You have not defined a <"+std::string(tag)+"> tag, this is required.");
	return true;
}

static void ValidateNoSpaces(const char* p, const std::string &tag, const std::string &val)
{
	for (const char* ptr = p; *ptr; ++ptr)
	{
		if (*ptr == ' ')
			throw CoreException("The value of <"+tag+":"+val+"> cannot contain spaces");
	}
}

/* NOTE: Before anyone asks why we're not using inet_pton for this, it is because inet_pton and friends do not return so much detail,
 * even in strerror(errno). They just return 'yes' or 'no' to an address without such detail as to whats WRONG with the address.
 * Because ircd users arent as technical as they used to be (;)) we are going to give more of a useful error message.
 */
static void ValidateIP(const char* p, const std::string &tag, const std::string &val, bool wild)
{
	int num_dots = 0;
	int num_seps = 0;
	int not_numbers = false;
	int not_hex = false;

	if (*p)
	{
		if (*p == '.')
			throw CoreException("The value of <"+tag+":"+val+"> is not an IP address");

		for (const char* ptr = p; *ptr; ++ptr)
		{
			if (wild && (*ptr == '*' || *ptr == '?' || *ptr == '/'))
				continue;

			if (*ptr != ':' && *ptr != '.')
			{
				if (*ptr < '0' || *ptr > '9')
					not_numbers = true;
				if ((*ptr < '0' || *ptr > '9') && (toupper(*ptr) < 'A' || toupper(*ptr) > 'F'))
					not_hex = true;
			}
			switch (*ptr)
			{
				case ' ':
					throw CoreException("The value of <"+tag+":"+val+"> is not an IP address");
				case '.':
					num_dots++;
				break;
				case ':':
					num_seps++;
				break;
			}
		}

		if (num_dots > 3)
			throw CoreException("The value of <"+tag+":"+val+"> is an IPv4 address with too many fields!");

		if (num_seps > 8)
			throw CoreException("The value of <"+tag+":"+val+"> is an IPv6 address with too many fields!");

		if (num_seps == 0 && num_dots < 3 && !wild)
			throw CoreException("The value of <"+tag+":"+val+"> looks to be a malformed IPv4 address");

		if (num_seps == 0 && num_dots == 3 && not_numbers)
			throw CoreException("The value of <"+tag+":"+val+"> contains non-numeric characters in an IPv4 address");

		if (num_seps != 0 && not_hex)
			throw CoreException("The value of <"+tag+":"+val+"> contains non-hexdecimal characters in an IPv6 address");

		if (num_seps != 0 && num_dots != 3 && num_dots != 0 && !wild)
			throw CoreException("The value of <"+tag+":"+val+"> is a malformed IPv6 4in6 address");
	}
}

static void ValidateHostname(const char* p, const std::string &tag, const std::string &val)
{
	int num_dots = 0;
	if (*p)
	{
		if (*p == '.')
			throw CoreException("The value of <"+tag+":"+val+"> is not a valid hostname");
		for (const char* ptr = p; *ptr; ++ptr)
		{
			switch (*ptr)
			{
				case ' ':
					throw CoreException("The value of <"+tag+":"+val+"> is not a valid hostname");
				case '.':
					num_dots++;
				break;
			}
		}
		if (num_dots == 0)
			throw CoreException("The value of <"+tag+":"+val+"> is not a valid hostname");
	}
}

// Specialized validators

static bool ValidateMaxTargets(ServerConfig* conf, const char*, const char*, ValueItem &data)
{
	if ((data.GetInteger() < 1) || (data.GetInteger() > 31))
	{
		ServerInstance->Logs->Log("CONFIG",DEFAULT,"WARNING: <security:maxtargets> value is greater than 31 or less than 1, set to 20.");
		data.Set(20);
	}
	return true;
}

static bool ValidateSoftLimit(ServerConfig* conf, const char*, const char*, ValueItem &data)
{
	if ((data.GetInteger() < 1) || (data.GetInteger() > ServerInstance->SE->GetMaxFds()))
	{
		ServerInstance->Logs->Log("CONFIG",DEFAULT,"WARNING: <performance:softlimit> value is greater than %d or less than 0, set to %d.",ServerInstance->SE->GetMaxFds(),ServerInstance->SE->GetMaxFds());
		data.Set(ServerInstance->SE->GetMaxFds());
	}
	return true;
}

static bool ValidateMaxConn(ServerConfig* conf, const char*, const char*, ValueItem &data)
{
	if (data.GetInteger() > SOMAXCONN)
		ServerInstance->Logs->Log("CONFIG",DEFAULT,"WARNING: <performance:somaxconn> value may be higher than the system-defined SOMAXCONN value!");
	return true;
}

bool ServerConfig::ApplyDisabledCommands(const std::string& data)
{
	std::stringstream dcmds(data);
	std::string thiscmd;

	/* Enable everything first */
	for (Commandtable::iterator x = ServerInstance->Parser->cmdlist.begin(); x != ServerInstance->Parser->cmdlist.end(); x++)
		x->second->Disable(false);

	/* Now disable all the ones which the user wants disabled */
	while (dcmds >> thiscmd)
	{
		Commandtable::iterator cm = ServerInstance->Parser->cmdlist.find(thiscmd);
		if (cm != ServerInstance->Parser->cmdlist.end())
		{
			cm->second->Disable(true);
		}
	}
	return true;
}

static bool ValidateDisabledUModes(ServerConfig* conf, const char*, const char*, ValueItem &data)
{
	memset(conf->DisabledUModes, 0, sizeof(conf->DisabledUModes));
	for (const unsigned char* p = (const unsigned char*)data.GetString(); *p; ++p)
	{
		if (*p < 'A' || *p > ('A' + 64)) throw CoreException(std::string("Invalid usermode ")+(char)*p+" was found.");
		conf->DisabledUModes[*p - 'A'] = 1;
	}
	return true;
}

static bool ValidateDisabledCModes(ServerConfig* conf, const char*, const char*, ValueItem &data)
{
	memset(conf->DisabledCModes, 0, sizeof(conf->DisabledCModes));
	for (const unsigned char* p = (const unsigned char*)data.GetString(); *p; ++p)
	{
		if (*p < 'A' || *p > ('A' + 64)) throw CoreException(std::string("Invalid chanmode ")+(char)*p+" was found.");
		conf->DisabledCModes[*p - 'A'] = 1;
	}
	return true;
}

#ifdef WINDOWS
// Note: the windows validator is in win32wrapper.cpp
bool ValidateDnsServer(ServerConfig* conf, const char*, const char*, ValueItem &data);
#else
static bool ValidateDnsServer(ServerConfig* conf, const char*, const char*, ValueItem &data)
{
	if (!*(data.GetString()))
	{
		std::string nameserver;
		// attempt to look up their nameserver from /etc/resolv.conf
		ServerInstance->Logs->Log("CONFIG",DEFAULT,"WARNING: <dns:server> not defined, attempting to find working server in /etc/resolv.conf...");
		std::ifstream resolv("/etc/resolv.conf");
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
					ServerInstance->Logs->Log("CONFIG",DEFAULT,"<dns:server> set to '%s' as first resolver in /etc/resolv.conf.",nameserver.c_str());
				}
			}

			if (!found_server)
			{
				ServerInstance->Logs->Log("CONFIG",DEFAULT,"/etc/resolv.conf contains no viable nameserver entries! Defaulting to nameserver '127.0.0.1'!");
				data.Set("127.0.0.1");
			}
		}
		else
		{
			ServerInstance->Logs->Log("CONFIG",DEFAULT,"/etc/resolv.conf can't be opened! Defaulting to nameserver '127.0.0.1'!");
			data.Set("127.0.0.1");
		}
	}
	return true;
}
#endif

static bool ValidateServerName(ServerConfig* conf, const char*, const char*, ValueItem &data)
{
	ServerInstance->Logs->Log("CONFIG",DEFAULT,"Validating server name");
	/* If we already have a servername, and they changed it, we should throw an exception. */
	if (!strchr(data.GetString(), '.'))
	{
		ServerInstance->Logs->Log("CONFIG",DEFAULT,"WARNING: <server:name> '%s' is not a fully-qualified domain name. Changed to '%s.'",
			data.GetString(),data.GetString());
		std::string moo = data.GetValue();
		data.Set(moo.append("."));
	}
	ValidateHostname(data.GetString(), "server", "name");
	return true;
}

static bool ValidateNetBufferSize(ServerConfig* conf, const char*, const char*, ValueItem &data)
{
	// 65534 not 65535 because of null terminator
	if ((!data.GetInteger()) || (data.GetInteger() > 65534) || (data.GetInteger() < 1024))
	{
		ServerInstance->Logs->Log("CONFIG",DEFAULT,"No NetBufferSize specified or size out of range, setting to default of 10240.");
		data.Set(10240);
	}
	return true;
}

static bool ValidateMaxWho(ServerConfig* conf, const char*, const char*, ValueItem &data)
{
	if ((data.GetInteger() > 65535) || (data.GetInteger() < 1))
	{
		ServerInstance->Logs->Log("CONFIG",DEFAULT,"<performance:maxwho> size out of range, setting to default of 1024.");
		data.Set(1024);
	}
	return true;
}

static bool ValidateHalfOp(ServerConfig* conf, const char*, const char*, ValueItem &data)
{
	ModeHandler* mh = ServerInstance->Modes->FindMode('h', MODETYPE_CHANNEL);
	if (data.GetBool() && !mh) {
		ServerInstance->Logs->Log("CONFIG", DEFAULT, "Enabling halfop mode.");
		mh = new ModeChannelHalfOp;
		ServerInstance->Modes->AddMode(mh);
	} else if (!data.GetBool() && mh) {
		ServerInstance->Logs->Log("CONFIG", DEFAULT, "Disabling halfop mode.");
		ServerInstance->Modes->DelMode(mh);
		delete mh;
	}
	return true;
}

static bool ValidateMotd(ServerConfig* conf, const char*, const char*, ValueItem &data)
{
	conf->ReadFile(conf->MOTD, data.GetString());
	return true;
}

static bool ValidateNotEmpty(ServerConfig*, const char* tag, const char* val, ValueItem &data)
{
	if (data.GetValue().empty())
		throw CoreException(std::string("The value for <")+tag+":"+val+"> cannot be empty!");
	return true;
}

static bool ValidateRules(ServerConfig* conf, const char*, const char*, ValueItem &data)
{
	conf->ReadFile(conf->RULES, data.GetString());
	return true;
}

static bool ValidateModeLists(ServerConfig* conf, const char*, const char*, ValueItem &data)
{
	memset(conf->HideModeLists, 0, sizeof(conf->HideModeLists));
	for (const unsigned char* x = (const unsigned char*)data.GetString(); *x; ++x)
		conf->HideModeLists[*x] = true;
	return true;
}

static bool ValidateExemptChanOps(ServerConfig* conf, const char*, const char*, ValueItem &data)
{
	memset(conf->ExemptChanOps, 0, sizeof(conf->ExemptChanOps));
	for (const unsigned char* x = (const unsigned char*)data.GetString(); *x; ++x)
		conf->ExemptChanOps[*x] = true;
	return true;
}

static bool ValidateInvite(ServerConfig* conf, const char*, const char*, ValueItem &data)
{
	const std::string& v = data.GetValue();

	if (v == "ops")
		conf->AnnounceInvites = ServerConfig::INVITE_ANNOUNCE_OPS;
	else if (v == "all")
		conf->AnnounceInvites = ServerConfig::INVITE_ANNOUNCE_ALL;
	else if (v == "dynamic")
		conf->AnnounceInvites = ServerConfig::INVITE_ANNOUNCE_DYNAMIC;
	else
		conf->AnnounceInvites = ServerConfig::INVITE_ANNOUNCE_NONE;

	return true;
}

static bool ValidateSID(ServerConfig* conf, const char*, const char*, ValueItem &data)
{
	ServerInstance->Logs->Log("CONFIG",DEFAULT,"Validating server id");

	const std::string& sid = data.GetValue();

	if (!sid.empty() && !ServerInstance->IsSID(sid))
	{
		throw CoreException(sid + " is not a valid server ID. A server ID must be 3 characters long, with the first character a digit and the next two characters a digit or letter.");
	}

	conf->sid = sid;

	return true;
}

static bool ValidateWhoWas(ServerConfig* conf, const char*, const char*, ValueItem &data)
{
	conf->WhoWasMaxKeep = ServerInstance->Duration(data.GetString());

	if (conf->WhoWasGroupSize < 0)
		conf->WhoWasGroupSize = 0;

	if (conf->WhoWasMaxGroups < 0)
		conf->WhoWasMaxGroups = 0;

	if (conf->WhoWasMaxKeep < 3600)
	{
		conf->WhoWasMaxKeep = 3600;
		ServerInstance->Logs->Log("CONFIG",DEFAULT,"WARNING: <whowas:maxkeep> value less than 3600, setting to default 3600");
	}

	Module* whowas = ServerInstance->Modules->Find("cmd_whowas.so");
	if (whowas)
	{
		WhowasRequest(NULL, whowas, WhowasRequest::WHOWAS_PRUNE).Send();
	}

	return true;
}

/* Callback called to process a single <uline> tag
 */
static bool DoULine(ServerConfig* conf, const char*, const char**, ValueList &values, int*)
{
	const char* server = values[0].GetString();
	const bool silent = values[1].GetBool();
	conf->ulines[server] = silent;
	return true;
}

/* Callback called to process a single <banlist> tag
 */
static bool DoMaxBans(ServerConfig* conf, const char*, const char**, ValueList &values, int*)
{
	const char* channel = values[0].GetString();
	int limit = values[1].GetInteger();
	conf->maxbans[channel] = limit;
	return true;
}

static bool DoZLine(ServerConfig* conf, const char* tag, const char** entries, ValueList &values, int* types)
{
	const char* reason = values[0].GetString();
	const char* ipmask = values[1].GetString();

	ZLine* zl = new ZLine(ServerInstance->Time(), 0, "<Config>", reason, ipmask);
	if (!ServerInstance->XLines->AddLine(zl, NULL))
		delete zl;

	return true;
}

static bool DoQLine(ServerConfig* conf, const char* tag, const char** entries, ValueList &values, int* types)
{
	const char* reason = values[0].GetString();
	const char* nick = values[1].GetString();

	QLine* ql = new QLine(ServerInstance->Time(), 0, "<Config>", reason, nick);
	if (!ServerInstance->XLines->AddLine(ql, NULL))
		delete ql;

	return true;
}

static bool DoKLine(ServerConfig* conf, const char* tag, const char** entries, ValueList &values, int* types)
{
	const char* reason = values[0].GetString();
	const char* host = values[1].GetString();

	XLineManager* xlm = ServerInstance->XLines;

	IdentHostPair ih = xlm->IdentSplit(host);

	KLine* kl = new KLine(ServerInstance->Time(), 0, "<Config>", reason, ih.first.c_str(), ih.second.c_str());
	if (!xlm->AddLine(kl, NULL))
		delete kl;
	return true;
}

static bool DoELine(ServerConfig* conf, const char* tag, const char** entries, ValueList &values, int* types)
{
	const char* reason = values[0].GetString();
	const char* host = values[1].GetString();

	XLineManager* xlm = ServerInstance->XLines;

	IdentHostPair ih = xlm->IdentSplit(host);

	ELine* el = new ELine(ServerInstance->Time(), 0, "<Config>", reason, ih.first.c_str(), ih.second.c_str());
	if (!xlm->AddLine(el, NULL))
		delete el;
	return true;
}

static bool DoType(ServerConfig* conf, const char*, const char**, ValueList &values, int*)
{
	const char* TypeName = values[0].GetString();
	const char* Classes = values[1].GetString();

	conf->opertypes[TypeName] = std::string(Classes);
	return true;
}

static bool DoClass(ServerConfig* conf, const char* tag, const char**, ValueList &values, int*)
{
	const char* ClassName = values[0].GetString();
	const char* CommandList = values[1].GetString();
	const char* UModeList = values[2].GetString();
	const char* CModeList = values[3].GetString();
	const char *PrivsList = values[4].GetString();

	for (const char* c = UModeList; *c; ++c)
	{
		if ((*c < 'A' || *c > 'z') && *c != '*')
		{
			throw CoreException("Character " + std::string(1, *c) + " is not a valid mode in <class:usermodes>");
		}
	}
	for (const char* c = CModeList; *c; ++c)
	{
		if ((*c < 'A' || *c > 'z') && *c != '*')
		{
			throw CoreException("Character " + std::string(1, *c) + " is not a valid mode in <class:chanmodes>");
		}
	}

	conf->operclass[ClassName].commandlist = CommandList;
	conf->operclass[ClassName].umodelist = UModeList;
	conf->operclass[ClassName].cmodelist = CModeList;
	conf->operclass[ClassName].privs = PrivsList;
	return true;
}

void ServerConfig::CrossCheckOperClassType()
{
	for (int i = 0; i < ConfValueEnum("type"); ++i)
	{
		char item[MAXBUF], classn[MAXBUF], classes[MAXBUF];
		std::string classname;
		ConfValue("type", "classes", "", i, classes, MAXBUF, false);
		irc::spacesepstream str(classes);
		ConfValue("type", "name", "", i, item, MAXBUF, false);
		while (str.GetToken(classname))
		{
			std::string lost;
			bool foundclass = false;
			for (int j = 0; j < ConfValueEnum("class"); ++j)
			{
				ConfValue("class", "name", "", j, classn, MAXBUF, false);
				if (!strcmp(classn, classname.c_str()))
				{
					foundclass = true;
					break;
				}
			}
			if (!foundclass)
			{
				char msg[MAXBUF];
				snprintf(msg, MAXBUF, "	Warning: Oper type '%s' has a missing class named '%s', this does nothing!\n",
					item, classname.c_str());
				throw CoreException(msg);
			}
		}
	}
}

void ServerConfig::CrossCheckConnectBlocks(ServerConfig* current)
{
	typedef std::map<std::string, ConnectClass*> ClassMap;
	ClassMap oldBlocksByMask;
	if (current)
	{
		for(ClassVector::iterator i = current->Classes.begin(); i != current->Classes.end(); ++i)
		{
			ConnectClass* c = *i;
			std::string typeMask = (c->type == CC_ALLOW) ? "a" : "d";
			typeMask += c->host;
			oldBlocksByMask[typeMask] = c;
		}
	}

	int block_count = ConfValueEnum("connect");
	ClassMap newBlocksByMask;
	Classes.resize(block_count, NULL);
	std::map<std::string, int> names;

	bool try_again = true;
	for(int tries=0; try_again && tries < block_count + 1; tries++)
	{
		try_again = false;
		for(int i=0; i < block_count; i++)
		{
			if (Classes[i])
				continue;

			ConnectClass* parent = NULL;
			std::string parentName;
			if (ConfValue("connect", "parent", i, parentName, false))
			{
				std::map<std::string,int>::iterator parentIter = names.find(parentName);
				if (parentIter == names.end())
				{
					try_again = true;
					// couldn't find parent this time. If it's the last time, we'll never find it.
					if (tries == block_count)
						throw CoreException("Could not find parent connect class \"" + parentName + "\" for connect block " + ConvToStr(i));
					continue;
				}
				parent = Classes[parentIter->second];
			}

			std::string name;
			if (ConfValue("connect", "name", i, name, false))
			{
				if (names.find(name) != names.end())
					throw CoreException("Two connect classes with name \"" + name + "\" defined!");
				names[name] = i;
			}

			std::string mask, typeMask;
			char type;

			if (ConfValue("connect", "allow", i, mask, false))
			{
				type = CC_ALLOW;
				typeMask = 'a' + mask;
			}
			else if (ConfValue("connect", "deny", i, mask, false))
			{
				type = CC_DENY;
				typeMask = 'd' + mask;
			}
			else
			{
				throw CoreException("Connect class must have an allow or deny mask (#" + ConvToStr(i) + ")");
			}
			ClassMap::iterator dupMask = newBlocksByMask.find(typeMask);
			if (dupMask != newBlocksByMask.end())
				throw CoreException("Two connect classes cannot have the same mask (" + mask + ")");

			ConnectClass* me = parent ? 
				new ConnectClass(type, mask, *parent) :
				new ConnectClass(type, mask);

			if (!name.empty())
				me->name = name;

			std::string tmpv;
			if (ConfValue("connect", "password", i, tmpv, false))
				me->pass= tmpv;
			if (ConfValue("connect", "hash", i, tmpv, false))
				me->hash = tmpv;
			if (ConfValue("connect", "timeout", i, tmpv, false))
				me->registration_timeout = atol(tmpv.c_str());
			if (ConfValue("connect", "pingfreq", i, tmpv, false))
				me->pingtime = atol(tmpv.c_str());
			if (ConfValue("connect", "sendq", i, tmpv, false))
			{
				// attempt to guess a good hard/soft sendq from a single value
				long value = atol(tmpv.c_str());
				if (value > 16384)
					me->softsendqmax = value / 16;
				else
					me->softsendqmax = value;
				me->hardsendqmax = value * 8;
			}
			if (ConfValue("connect", "softsendq", i, tmpv, false))
				me->softsendqmax = atol(tmpv.c_str());
			if (ConfValue("connect", "hardsendq", i, tmpv, false))
				me->hardsendqmax = atol(tmpv.c_str());
			if (ConfValue("connect", "recvq", i, tmpv, false))
				me->recvqmax = atol(tmpv.c_str());
			if (ConfValue("connect", "localmax", i, tmpv, false))
				me->maxlocal = atol(tmpv.c_str());
			if (ConfValue("connect", "globalmax", i, tmpv, false))
				me->maxglobal = atol(tmpv.c_str());
			if (ConfValue("connect", "port", i, tmpv, false))
				me->port = atol(tmpv.c_str());
			if (ConfValue("connect", "maxchans", i, tmpv, false))
				me->maxchans = atol(tmpv.c_str());
			if (ConfValue("connect", "limit", i, tmpv, false))
				me->limit = atol(tmpv.c_str());

			ClassMap::iterator oldMask = oldBlocksByMask.find(typeMask);
			if (oldMask != oldBlocksByMask.end())
			{
				ConnectClass* old = oldMask->second;
				oldBlocksByMask.erase(oldMask);
				old->Update(me);
				delete me;
				me = old;
			}
			newBlocksByMask[typeMask] = me;
			Classes[i] = me;
		}
	}
}


static const Deprecated ChangedConfig[] = {
	{"options", "hidelinks",		"has been moved to <security:hidelinks> as of 1.2a3"},
	{"options", "hidewhois",		"has been moved to <security:hidewhois> as of 1.2a3"},
	{"options", "userstats",		"has been moved to <security:userstats> as of 1.2a3"},
	{"options", "customversion",	"has been moved to <security:customversion> as of 1.2a3"},
	{"options", "hidesplits",		"has been moved to <security:hidesplits> as of 1.2a3"},
	{"options", "hidebans",		"has been moved to <security:hidebans> as of 1.2a3"},
	{"options", "hidekills",		"has been moved to <security:hidekills> as of 1.2a3"},
	{"options", "operspywhois",		"has been moved to <security:operspywhois> as of 1.2a3"},
	{"options", "announceinvites",	"has been moved to <security:announceinvites> as of 1.2a3"},
	{"options", "hidemodes",		"has been moved to <security:hidemodes> as of 1.2a3"},
	{"options", "maxtargets",		"has been moved to <security:maxtargets> as of 1.2a3"},
	{"options",	"nouserdns",		"has been moved to <performance:nouserdns> as of 1.2a3"},
	{"options",	"maxwho",		"has been moved to <performance:maxwho> as of 1.2a3"},
	{"options",	"softlimit",		"has been moved to <performance:softlimit> as of 1.2a3"},
	{"options", "somaxconn",		"has been moved to <performance:somaxconn> as of 1.2a3"},
	{"options", "netbuffersize",	"has been moved to <performance:netbuffersize> as of 1.2a3"},
	{"options", "maxwho",		"has been moved to <performance:maxwho> as of 1.2a3"},
	{"options",	"loglevel",		"1.2 does not use the loglevel value. Please define <log> tags instead."},
	{"die",     "value",            "has always been deprecated"},
};

/* These tags can occur ONCE or not at all */
static const InitialConfig Values[] = {
	{"performance",	"softlimit",	"0",			new ValueContainerUInt (&ServerConfig::SoftLimit),		DT_INTEGER,  ValidateSoftLimit},
	{"performance",	"somaxconn",	SOMAXCONN_S,		new ValueContainerInt  (&ServerConfig::MaxConn),		DT_INTEGER,  ValidateMaxConn},
	{"options",	"moronbanner",	"You're banned!",	new ValueContainerString (&ServerConfig::MoronBanner),		DT_CHARPTR,  NULL},
	{"server",	"name",		"",			new ValueContainerString (&ServerConfig::ServerName),		DT_HOSTNAME, ValidateServerName},
	{"server",	"description",	"Configure Me",		new ValueContainerString (&ServerConfig::ServerDesc),		DT_CHARPTR,  NULL},
	{"server",	"network",	"Network",		new ValueContainerString (&ServerConfig::Network),			DT_NOSPACES, NULL},
	{"server",	"id",		"",			new ValueContainerString (&ServerConfig::sid),			DT_CHARPTR,  ValidateSID},
	{"admin",	"name",		"",			new ValueContainerString (&ServerConfig::AdminName),		DT_CHARPTR,  NULL},
	{"admin",	"email",	"Mis@configu.red",	new ValueContainerString (&ServerConfig::AdminEmail),		DT_CHARPTR,  NULL},
	{"admin",	"nick",		"Misconfigured",	new ValueContainerString (&ServerConfig::AdminNick),		DT_CHARPTR,  NULL},
	{"files",	"motd",		"",			new ValueContainerString (&ServerConfig::motd),			DT_CHARPTR,  ValidateMotd},
	{"files",	"rules",	"",			new ValueContainerString (&ServerConfig::rules),			DT_CHARPTR,  ValidateRules},
	{"power",	"diepass",	"",			new ValueContainerString (&ServerConfig::diepass),			DT_CHARPTR,  ValidateNotEmpty},
	{"power",	"pause",	"",			new ValueContainerInt  (&ServerConfig::DieDelay),		DT_INTEGER,  NULL},
	{"power",	"hash",		"",			new ValueContainerString (&ServerConfig::powerhash),		DT_CHARPTR,  NULL},
	{"power",	"restartpass",	"",			new ValueContainerString (&ServerConfig::restartpass),		DT_CHARPTR,  ValidateNotEmpty},
	{"options",	"prefixquit",	"",			new ValueContainerString (&ServerConfig::PrefixQuit),		DT_CHARPTR,  NULL},
	{"options",	"suffixquit",	"",			new ValueContainerString (&ServerConfig::SuffixQuit),		DT_CHARPTR,  NULL},
	{"options",	"fixedquit",	"",			new ValueContainerString (&ServerConfig::FixedQuit),		DT_CHARPTR,  NULL},
	{"options",	"prefixpart",	"",			new ValueContainerString (&ServerConfig::PrefixPart),		DT_CHARPTR,  NULL},
	{"options",	"suffixpart",	"",			new ValueContainerString (&ServerConfig::SuffixPart),		DT_CHARPTR,  NULL},
	{"options",	"fixedpart",	"",			new ValueContainerString (&ServerConfig::FixedPart),		DT_CHARPTR,  NULL},
	{"performance",	"netbuffersize","10240",		new ValueContainerInt  (&ServerConfig::NetBufferSize),		DT_INTEGER,  ValidateNetBufferSize},
	{"performance",	"maxwho",	"1024",			new ValueContainerInt  (&ServerConfig::MaxWhoResults),		DT_INTEGER,  ValidateMaxWho},
	{"options",	"allowhalfop",	"0",			new ValueContainerBool (&ServerConfig::AllowHalfop),		DT_BOOLEAN,  ValidateHalfOp},
	{"dns",		"server",	"",			new ValueContainerString (&ServerConfig::DNSServer),		DT_IPADDRESS,ValidateDnsServer},
	{"dns",		"timeout",	"5",			new ValueContainerInt  (&ServerConfig::dns_timeout),		DT_INTEGER,  NULL},
	{"options",	"moduledir",	MOD_PATH,		new ValueContainerString (&ServerConfig::ModPath),			DT_CHARPTR,  NULL},
	{"disabled",	"commands",	"",			new ValueContainerString (&ServerConfig::DisabledCommands),	DT_CHARPTR,  NULL},
	{"disabled",	"usermodes",	"",			NULL, DT_NOTHING,  ValidateDisabledUModes},
	{"disabled",	"chanmodes",	"",			NULL, DT_NOTHING,  ValidateDisabledCModes},
	{"disabled",	"fakenonexistant",	"0",		new ValueContainerBool (&ServerConfig::DisabledDontExist),		DT_BOOLEAN,  NULL},

	{"security",		"runasuser",	"",		new ValueContainerString(&ServerConfig::SetUser),				DT_CHARPTR, NULL},
	{"security",		"runasgroup",	"",		new ValueContainerString(&ServerConfig::SetGroup),				DT_CHARPTR, NULL},
	{"security",	"userstats",	"",			new ValueContainerString (&ServerConfig::UserStats),		DT_CHARPTR,  NULL},
	{"security",	"customversion","",			new ValueContainerString (&ServerConfig::CustomVersion),		DT_CHARPTR,  NULL},
	{"security",	"hidesplits",	"0",			new ValueContainerBool (&ServerConfig::HideSplits),		DT_BOOLEAN,  NULL},
	{"security",	"hidebans",	"0",			new ValueContainerBool (&ServerConfig::HideBans),		DT_BOOLEAN,  NULL},
	{"security",	"hidewhois",	"",			new ValueContainerString (&ServerConfig::HideWhoisServer),		DT_NOSPACES, NULL},
	{"security",	"hidekills",	"",			new ValueContainerString (&ServerConfig::HideKillsServer),		DT_NOSPACES,  NULL},
	{"security",	"operspywhois",	"0",			new ValueContainerBool (&ServerConfig::OperSpyWhois),		DT_BOOLEAN,  NULL},
	{"security",	"restrictbannedusers",	"1",		new ValueContainerBool (&ServerConfig::RestrictBannedUsers),		DT_BOOLEAN,  NULL},
	{"security",	"genericoper",	"0",			new ValueContainerBool (&ServerConfig::GenericOper),		DT_BOOLEAN,  NULL},
	{"performance",	"nouserdns",	"0",			new ValueContainerBool (&ServerConfig::NoUserDns),		DT_BOOLEAN,  NULL},
	{"options",	"syntaxhints",	"0",			new ValueContainerBool (&ServerConfig::SyntaxHints),		DT_BOOLEAN,  NULL},
	{"options",	"cyclehosts",	"0",			new ValueContainerBool (&ServerConfig::CycleHosts),		DT_BOOLEAN,  NULL},
	{"options",	"ircumsgprefix","0",			new ValueContainerBool (&ServerConfig::UndernetMsgPrefix),	DT_BOOLEAN,  NULL},
	{"security",	"announceinvites", "1",			NULL, DT_NOTHING,  ValidateInvite},
	{"options",	"hostintopic",	"1",			new ValueContainerBool (&ServerConfig::FullHostInTopic),	DT_BOOLEAN,  NULL},
	{"security",	"hidemodes",	"",			NULL, DT_NOTHING,  ValidateModeLists},
	{"options",	"exemptchanops","",			NULL, DT_NOTHING,  ValidateExemptChanOps},
	{"security",	"maxtargets",	"20",			new ValueContainerUInt (&ServerConfig::MaxTargets),		DT_INTEGER,  ValidateMaxTargets},
	{"options",	"defaultmodes", "nt",			new ValueContainerString (&ServerConfig::DefaultModes),		DT_CHARPTR,  NULL},
	{"pid",		"file",		"",			new ValueContainerString (&ServerConfig::PID),			DT_CHARPTR,  NULL},
	{"whowas",	"groupsize",	"10",			new ValueContainerInt  (&ServerConfig::WhoWasGroupSize),	DT_INTEGER,  NULL},
	{"whowas",	"maxgroups",	"10240",		new ValueContainerInt  (&ServerConfig::WhoWasMaxGroups),	DT_INTEGER,  NULL},
	{"whowas",	"maxkeep",	"3600",			NULL, DT_NOTHING,  ValidateWhoWas},
	{"die",		"value",	"",			new ValueContainerString (&ServerConfig::DieValue),		DT_CHARPTR,  NULL},
	{"channels",	"users",	"20",			new ValueContainerUInt (&ServerConfig::MaxChans),		DT_INTEGER,  NULL},
	{"channels",	"opers",	"60",			new ValueContainerUInt (&ServerConfig::OperMaxChans),		DT_INTEGER,  NULL},
	{"cidr",	"ipv4clone",	"32",			new ValueContainerInt (&ServerConfig::c_ipv4_range),		DT_INTEGER,  NULL},
	{"cidr",	"ipv6clone",	"128",			new ValueContainerInt (&ServerConfig::c_ipv6_range),		DT_INTEGER,  NULL},
	{"limits",	"maxnick",	"32",			new ValueContainerLimit (&ServerLimits::NickMax),		DT_LIMIT,  NULL},
	{"limits",	"maxchan",	"64",			new ValueContainerLimit (&ServerLimits::ChanMax),		DT_LIMIT,  NULL},
	{"limits",	"maxmodes",	"20",			new ValueContainerLimit (&ServerLimits::MaxModes),		DT_LIMIT,  NULL},
	{"limits",	"maxident",	"11",			new ValueContainerLimit (&ServerLimits::IdentMax),		DT_LIMIT,  NULL},
	{"limits",	"maxquit",	"255",			new ValueContainerLimit (&ServerLimits::MaxQuit),		DT_LIMIT,  NULL},
	{"limits",	"maxtopic",	"307",			new ValueContainerLimit (&ServerLimits::MaxTopic),		DT_LIMIT,  NULL},
	{"limits",	"maxkick",	"255",			new ValueContainerLimit (&ServerLimits::MaxKick),		DT_LIMIT,  NULL},
	{"limits",	"maxgecos",	"128",			new ValueContainerLimit (&ServerLimits::MaxGecos),		DT_LIMIT,  NULL},
	{"limits",	"maxaway",	"200",			new ValueContainerLimit (&ServerLimits::MaxAway),		DT_LIMIT,  NULL},
	{"options",	"invitebypassmodes",	"1",			new ValueContainerBool (&ServerConfig::InvBypassModes),		DT_BOOLEAN,  NULL},
};

/* These tags can occur multiple times, and therefore they have special code to read them
 * which is different to the code for reading the singular tags listed above.
 */
MultiConfig MultiValues[] = {

	{"connect",
			{"allow",	"deny",		"password",	"timeout",	"pingfreq",
			"sendq",	"recvq",	"localmax",	"globalmax",	"port",
			"name",		"parent",	"maxchans",     "limit",	"hash",
			NULL},
			{"",		"",				"",			"",			"120",
			 "",		"",				"3",		"3",		"0",
			 "",		"",				"0",	    "0",		"",
			 NULL},
			{DT_IPADDRESS|DT_ALLOW_WILD, DT_IPADDRESS|DT_ALLOW_WILD, DT_CHARPTR,	DT_INTEGER,	DT_INTEGER,
			DT_INTEGER,	DT_INTEGER,	DT_INTEGER,	DT_INTEGER,	DT_INTEGER,
			DT_NOSPACES,	DT_NOSPACES,	DT_INTEGER,     DT_INTEGER,	DT_CHARPTR},
			NULL,},

	{"uline",
			{"server",	"silent",	NULL},
			{"",		"0",		NULL},
			{DT_HOSTNAME,	DT_BOOLEAN},
			DoULine},

	{"banlist",
			{"chan",	"limit",	NULL},
			{"",		"",		NULL},
			{DT_CHARPTR,	DT_INTEGER},
			DoMaxBans},

	{"module",
			{"name",	NULL},
			{"",		NULL},
			{DT_CHARPTR},
			NULL},

	{"badip",
			{"reason",	"ipmask",	NULL},
			{"No reason",	"",		NULL},
			{DT_CHARPTR,	DT_IPADDRESS|DT_ALLOW_WILD},
			DoZLine},

	{"badnick",
			{"reason",	"nick",		NULL},
			{"No reason",	"",		NULL},
			{DT_CHARPTR,	DT_CHARPTR},
			DoQLine},

	{"badhost",
			{"reason",	"host",		NULL},
			{"No reason",	"",		NULL},
			{DT_CHARPTR,	DT_CHARPTR},
			DoKLine},

	{"exception",
			{"reason",	"host",		NULL},
			{"No reason",	"",		NULL},
			{DT_CHARPTR,	DT_CHARPTR},
			DoELine},

	{"type",
			{"name",	"classes",	NULL},
			{"",		"",		NULL},
			{DT_NOSPACES,	DT_CHARPTR},
			DoType},

	{"class",
			{"name",	"commands",	"usermodes",	"chanmodes",	"privs",	NULL},
			{"",		"",				"",				"",			"",			NULL},
			{DT_NOSPACES,	DT_CHARPTR,	DT_CHARPTR,	DT_CHARPTR, DT_CHARPTR},
			DoClass},
};

/* These tags MUST occur and must ONLY occur once in the config file */
static const char* Once[] = { "server", "admin", "files", "power", "options" };

// WARNING: it is not safe to use most of the codebase in this function, as it
// will run in the config reader thread
void ServerConfig::Read()
{
	/* Load and parse the config file, if there are any errors then explode */

	if (!this->DoInclude(ServerInstance->ConfigFileName, true))
	{
		valid = false;
		return;
	}
}

void ServerConfig::Apply(ServerConfig* old, const std::string &useruid)
{
	valid = true;
	/* std::ostringstream::clear() does not clear the string itself, only the error flags. */
	errstr.clear();
	errstr.str().clear();
	include_stack.clear();

	/* The stuff in here may throw CoreException, be sure we're in a position to catch it. */
	try
	{
		/* Check we dont have more than one of singular tags, or any of them missing
		 */
		for (int Index = 0; Index * sizeof(*Once) < sizeof(Once); Index++)
			CheckOnce(Once[Index]);

		for (int Index = 0; Index * sizeof(Deprecated) < sizeof(ChangedConfig); Index++)
		{
			char item[MAXBUF];
			*item = 0;
			if (ConfValue(ChangedConfig[Index].tag, ChangedConfig[Index].value, "", 0, item, MAXBUF, true) || *item)
				throw CoreException(std::string("Your configuration contains a deprecated value: <") + ChangedConfig[Index].tag + ":" + ChangedConfig[Index].value + "> - " + ChangedConfig[Index].reason);
		}

		/* Read the values of all the tags which occur once or not at all, and call their callbacks.
		 */
		for (int Index = 0; Index * sizeof(*Values) < sizeof(Values); ++Index)
		{
			char item[MAXBUF];
			int dt = Values[Index].datatype;
			bool allow_newlines = ((dt & DT_ALLOW_NEWLINE) > 0);
			bool allow_wild = ((dt & DT_ALLOW_WILD) > 0);
			dt &= ~DT_ALLOW_NEWLINE;
			dt &= ~DT_ALLOW_WILD;

			ConfValue(Values[Index].tag, Values[Index].value, Values[Index].default_value, 0, item, MAXBUF, allow_newlines);
			ValueItem vi(item);

			if (Values[Index].validation_function && !Values[Index].validation_function(this, Values[Index].tag, Values[Index].value, vi))
				throw CoreException("One or more values in your configuration file failed to validate. Please see your ircd.log for more information.");

			switch (dt)
			{
				case DT_NOSPACES:
				{
					ValueContainerString* vcc = (ValueContainerString*)Values[Index].val;
					ValidateNoSpaces(vi.GetString(), Values[Index].tag, Values[Index].value);
					vcc->Set(this, vi.GetValue());
				}
				break;
				case DT_HOSTNAME:
				{
					ValueContainerString* vcc = (ValueContainerString*)Values[Index].val;
					ValidateHostname(vi.GetString(), Values[Index].tag, Values[Index].value);
					vcc->Set(this, vi.GetValue());
				}
				break;
				case DT_IPADDRESS:
				{
					ValueContainerString* vcc = (ValueContainerString*)Values[Index].val;
					ValidateIP(vi.GetString(), Values[Index].tag, Values[Index].value, allow_wild);
					vcc->Set(this, vi.GetValue());
				}
				break;
				case DT_CHANNEL:
				{
					ValueContainerString* vcc = (ValueContainerString*)Values[Index].val;
					if (*(vi.GetString()) && !ServerInstance->IsChannel(vi.GetString(), MAXBUF))
					{
						throw CoreException("The value of <"+std::string(Values[Index].tag)+":"+Values[Index].value+"> is not a valid channel name");
					}
					vcc->Set(this, vi.GetValue());
				}
				break;
				case DT_CHARPTR:
				{
					ValueContainerString* vcs = dynamic_cast<ValueContainerString*>(Values[Index].val);
					if (vcs)
						vcs->Set(this, vi.GetValue());
				}
				break;
				case DT_INTEGER:
				{
					int val = vi.GetInteger();
					ValueContainerInt* vci = (ValueContainerInt*)Values[Index].val;
					vci->Set(this, val);
				}
				break;
				case DT_LIMIT:
				{
					int val = vi.GetInteger();
					ValueContainerLimit* vci = (ValueContainerLimit*)Values[Index].val;
					vci->Set(this, val);
				}
				break;
				case DT_BOOLEAN:
				{
					bool val = vi.GetBool();
					ValueContainerBool* vcb = (ValueContainerBool*)Values[Index].val;
					vcb->Set(this, val);
				}
				break;
			}
		}

		/* Read the multiple-tag items (class tags, connect tags, etc)
		 * and call the callbacks associated with them. We have three
		 * callbacks for these, a 'start', 'item' and 'end' callback.
		 */
		for (int Index = 0; Index * sizeof(MultiConfig) < sizeof(MultiValues); ++Index)
		{
			int number_of_tags = ConfValueEnum(MultiValues[Index].tag);

			for (int tagnum = 0; tagnum < number_of_tags; ++tagnum)
			{
				ValueList vl;
				for (int valuenum = 0; (MultiValues[Index].items[valuenum]) && (valuenum < MAX_VALUES_PER_TAG); ++valuenum)
				{
					int dt = MultiValues[Index].datatype[valuenum];
					bool allow_newlines =  ((dt & DT_ALLOW_NEWLINE) > 0);
					bool allow_wild = ((dt & DT_ALLOW_WILD) > 0);
					dt &= ~DT_ALLOW_NEWLINE;
					dt &= ~DT_ALLOW_WILD;

					switch (dt)
					{
						case DT_NOSPACES:
						{
							char item[MAXBUF];
							if (ConfValue(MultiValues[Index].tag, MultiValues[Index].items[valuenum], MultiValues[Index].items_default[valuenum], tagnum, item, MAXBUF, allow_newlines))
								vl.push_back(ValueItem(item));
							else
								vl.push_back(ValueItem(""));
							ValidateNoSpaces(vl[vl.size()-1].GetString(), MultiValues[Index].tag, MultiValues[Index].items[valuenum]);
						}
						break;
						case DT_HOSTNAME:
						{
							char item[MAXBUF];
							if (ConfValue(MultiValues[Index].tag, MultiValues[Index].items[valuenum], MultiValues[Index].items_default[valuenum], tagnum, item, MAXBUF, allow_newlines))
								vl.push_back(ValueItem(item));
							else
								vl.push_back(ValueItem(""));
							ValidateHostname(vl[vl.size()-1].GetString(), MultiValues[Index].tag, MultiValues[Index].items[valuenum]);
						}
						break;
						case DT_IPADDRESS:
						{
							char item[MAXBUF];
							if (ConfValue(MultiValues[Index].tag, MultiValues[Index].items[valuenum], MultiValues[Index].items_default[valuenum], tagnum, item, MAXBUF, allow_newlines))
								vl.push_back(ValueItem(item));
							else
								vl.push_back(ValueItem(""));
							ValidateIP(vl[vl.size()-1].GetString(), MultiValues[Index].tag, MultiValues[Index].items[valuenum], allow_wild);
						}
						break;
						case DT_CHANNEL:
						{
							char item[MAXBUF];
							if (ConfValue(MultiValues[Index].tag, MultiValues[Index].items[valuenum], MultiValues[Index].items_default[valuenum], tagnum, item, MAXBUF, allow_newlines))
								vl.push_back(ValueItem(item));
							else
								vl.push_back(ValueItem(""));
							if (!ServerInstance->IsChannel(vl[vl.size()-1].GetString(), MAXBUF))
								throw CoreException("The value of <"+std::string(MultiValues[Index].tag)+":"+MultiValues[Index].items[valuenum]+"> number "+ConvToStr(tagnum + 1)+" is not a valid channel name");
						}
						break;
						case DT_CHARPTR:
						{
							char item[MAXBUF];
							if (ConfValue(MultiValues[Index].tag, MultiValues[Index].items[valuenum], MultiValues[Index].items_default[valuenum], tagnum, item, MAXBUF, allow_newlines))
								vl.push_back(ValueItem(item));
							else
								vl.push_back(ValueItem(""));
						}
						break;
						case DT_INTEGER:
						{
							int item = 0;
							if (ConfValueInteger(MultiValues[Index].tag, MultiValues[Index].items[valuenum], MultiValues[Index].items_default[valuenum], tagnum, item))
								vl.push_back(ValueItem(item));
							else
								vl.push_back(ValueItem(0));
						}
						break;
						case DT_BOOLEAN:
						{
							bool item = ConfValueBool(MultiValues[Index].tag, MultiValues[Index].items[valuenum], MultiValues[Index].items_default[valuenum], tagnum);
							vl.push_back(ValueItem(item));
						}
						break;
					}
				}
				if (MultiValues[Index].validation_function)
					MultiValues[Index].validation_function(this, MultiValues[Index].tag, MultiValues[Index].items, vl, MultiValues[Index].datatype);
			}
		}

		/* Finalise the limits, increment them all by one so that we can just put assign(str, 0, val)
		 * rather than assign(str, 0, val + 1)
		 */
		Limits.Finalise();

		// Handle special items
		CrossCheckOperClassType();
		CrossCheckConnectBlocks(old);
	}
	catch (CoreException &ce)
	{
		errstr << ce.GetReason();
		valid = false;
	}

	// write once here, to try it out and make sure its ok
	ServerInstance->WritePID(this->PID);

	/*
	 * These values can only be set on boot. Keep their old values. Do it before we send messages so we actually have a servername.
	 */
	if (old)
	{
		this->ServerName = old->ServerName;
		this->sid = old->sid;
		this->argv = old->argv;
		this->argc = old->argc;

		// Same for ports... they're bound later on first run.
		FailedPortList pl;
		ServerInstance->BindPorts(pl);
		if (pl.size())
		{
			errstr << "Not all your client ports could be bound.\nThe following port(s) failed to bind:\n";

			int j = 1;
			for (FailedPortList::iterator i = pl.begin(); i != pl.end(); i++, j++)
			{
				char buf[MAXBUF];
				snprintf(buf, MAXBUF, "%d.   Address: %s   Reason: %s\n", j, i->first.empty() ? "<all>" : i->first.c_str(), i->second.c_str());
				errstr << buf;
			}
		}
	}

	User* user = useruid.empty() ? NULL : ServerInstance->FindNick(useruid);

	valid = errstr.str().empty();
	if (!valid)
		ServerInstance->Logs->Log("CONFIG",DEFAULT, "There were errors in your configuration file:");

	while (errstr.good())
	{
		std::string line;
		getline(errstr, line, '\n');
		if (!line.empty())
		{
			if (user)
				user->WriteServ("NOTICE %s :*** %s", user->nick.c_str(), line.c_str());
			else
				ServerInstance->SNO->WriteGlobalSno('a', line);
		}

		if (!old)
		{
			// Starting up, so print it out so it's seen. XXX this is a bit of a hack.
			printf("%s\n", line.c_str());
		}
	}

	errstr.clear();
	errstr.str(std::string());

	/* No old configuration -> initial boot, nothing more to do here */
	if (!old)
	{
		if (!valid)
		{
			ServerInstance->Exit(EXIT_STATUS_CONFIG);
		}

		return;
	}

	// If there were errors processing configuration, don't touch modules.
	if (!valid)
		return;

	ApplyModules(user);
}

void ServerConfig::ApplyModules(User* user)
{
	const std::vector<std::string> v = ServerInstance->Modules->GetAllModuleNames(0);
	std::vector<std::string> added_modules;
	std::set<std::string> removed_modules(v.begin(), v.end());

	int new_module_count = ConfValueEnum("module");
	for(int i=0; i < new_module_count; i++)
	{
		std::string name;
		if (ConfValue("module", "name", i, name, false))
		{
			// if this module is already loaded, the erase will succeed, so we need do nothing
			// otherwise, we need to add the module (which will be done later)
			if (removed_modules.erase(name) == 0)
				added_modules.push_back(name);
		}
	}

	for (std::set<std::string>::iterator removing = removed_modules.begin(); removing != removed_modules.end(); removing++)
	{
		// Don't remove cmd_*.so, just remove m_*.so
		if (removing->c_str()[0] == 'c')
			continue;
		if (ServerInstance->Modules->Unload(removing->c_str()))
		{
			ServerInstance->SNO->WriteToSnoMask('a', "*** REHASH UNLOADED MODULE: %s",removing->c_str());

			if (user)
				user->WriteNumeric(RPL_UNLOADEDMODULE, "%s %s :Module %s successfully unloaded.",user->nick.c_str(), removing->c_str(), removing->c_str());
			else
				ServerInstance->SNO->WriteToSnoMask('a', "Module %s successfully unloaded.", removing->c_str());
		}
		else
		{
			if (user)
				user->WriteNumeric(ERR_CANTUNLOADMODULE, "%s %s :Failed to unload module %s: %s",user->nick.c_str(), removing->c_str(), removing->c_str(), ServerInstance->Modules->LastError().c_str());
			else
				 ServerInstance->SNO->WriteToSnoMask('a', "Failed to unload module %s: %s", removing->c_str(), ServerInstance->Modules->LastError().c_str());
		}
	}

	for (std::vector<std::string>::iterator adding = added_modules.begin(); adding != added_modules.end(); adding++)
	{
		if (ServerInstance->Modules->Load(adding->c_str()))
		{
			ServerInstance->SNO->WriteToSnoMask('a', "*** REHASH LOADED MODULE: %s",adding->c_str());
			if (user)
				user->WriteNumeric(RPL_LOADEDMODULE, "%s %s :Module %s successfully loaded.",user->nick.c_str(), adding->c_str(), adding->c_str());
			else
				ServerInstance->SNO->WriteToSnoMask('a', "Module %s successfully loaded.", adding->c_str());
		}
		else
		{
			if (user)
				user->WriteNumeric(ERR_CANTLOADMODULE, "%s %s :Failed to load module %s: %s",user->nick.c_str(), adding->c_str(), adding->c_str(), ServerInstance->Modules->LastError().c_str());
			else
				ServerInstance->SNO->WriteToSnoMask('a', "Failed to load module %s: %s", adding->c_str(), ServerInstance->Modules->LastError().c_str());
		}
	}

	if (user)
		user->WriteServ("NOTICE %s :*** Successfully rehashed server.", user->nick.c_str());
	else
		ServerInstance->SNO->WriteToSnoMask('a', "*** Successfully rehashed server.");
}

bool ServerConfig::LoadConf(FILE* &conf, const char* filename, bool allowexeinc)
{
	std::string line;
	char ch;
	long linenumber = 1;
	long last_successful_parse = 1;
	bool in_tag;
	bool in_quote;
	bool in_comment;
	int character_count = 0;

	in_tag = false;
	in_quote = false;
	in_comment = false;

	ServerInstance->Logs->Log("CONFIG", DEBUG, "Reading %s", filename);

	/* Check if the file open failed first */
	if (!conf)
	{
		errstr << "LoadConf: Couldn't open config file: " << filename << std::endl;
		return false;
	}

	for (unsigned int t = 0; t < include_stack.size(); t++)
	{
		if (std::string(filename) == include_stack[t])
		{
			errstr << "File " << filename << " is included recursively (looped inclusion)." << std::endl;
			return false;
		}
	}

	/* It's not already included, add it to the list of files we've loaded */
	include_stack.push_back(filename);

	/* Start reading characters... */
	while ((ch = fgetc(conf)) != EOF)
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
			errstr << "File " << filename << " cannot be read, as it is encoded in braindead UTF-16. Save your file as plain ASCII!" << std::endl;
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

		if ((ch == '#') && !in_quote)
			in_comment = true;

		switch (ch)
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
			if (!feof(conf))
			{
				real_character = fgetc(conf);
				if (real_character == 'n')
					real_character = '\n';
				line += real_character;
				continue;
			}
			else
			{
				errstr << "End of file after a \\, what did you want to escape?: " << filename << ":" << linenumber << std::endl;
				return false;
			}
		}

		if (ch != '\r')
			line += ch;

		if ((ch != '<') && (!in_tag) && (!in_comment) && (ch > ' ') && (ch != 9))
		{
			errstr << "You have stray characters beyond the tag which starts at " << filename << ":" << last_successful_parse << std::endl;
			return false;
		}

		if (ch == '<')
		{
			if (in_tag)
			{
				if (!in_quote)
				{
					errstr << "The tag at location " << filename << ":" << last_successful_parse << " was valid, but there is an error in the tag which comes after it. You are possibly missing a \" or >. Please check this." << std::endl;
					return false;
				}
			}
			else
			{
				if (in_quote)
				{
					errstr << "Parser error: Inside a quote but not within the last valid tag, which was opened at: " << filename << ":" << last_successful_parse << std::endl;
					return false;
				}
				else
				{
					// errstr << "Opening new config tag on line " << linenumber << std::endl;
					in_tag = true;
				}
			}
		}
		else if (ch == '"')
		{
			if (in_tag)
			{
				if (in_quote)
				{
					// errstr << "Closing quote in config tag on line " << linenumber << std::endl;
					in_quote = false;
				}
				else
				{
					// errstr << "Opening quote in config tag on line " << linenumber << std::endl;
					in_quote = true;
				}
			}
			else
			{
				if (in_quote)
				{
					errstr << "The tag immediately after the one at " << filename << ":" << last_successful_parse << " has a missing closing \" symbol. Please check this." << std::endl;
				}
				else
				{
					errstr << "You have opened a quote (\") beyond the tag at " << filename << ":" << last_successful_parse << " without opening a new tag. Please check this." << std::endl;
				}
			}
		}
		else if (ch == '>')
		{
			if (!in_quote)
			{
				if (in_tag)
				{
					// errstr << "Closing config tag on line " << linenumber << std::endl;
					in_tag = false;

					/*
					 * If this finds an <include> then ParseLine can simply call
					 * LoadConf() and load the included config into the same ConfigDataHash
					 */
					long bl = linenumber;
					if (!this->ParseLine(filename, line, linenumber, allowexeinc))
						return false;
					last_successful_parse = linenumber;

					linenumber = bl;

					line.clear();
				}
				else
				{
					errstr << "You forgot to close the tag which comes immediately after the one at " << filename << ":" << last_successful_parse << std::endl;
					return false;
				}
			}
		}
	}

	/* Fix for bug #392 - if we reach the end of a file and we are still in a quote or comment, most likely the user fucked up */
	if (in_comment || in_quote)
	{
		errstr << "Reached end of file whilst still inside a quoted section or tag. This is most likely an error or there \
			is a newline missing from the end of the file: " << filename << ":" << linenumber << std::endl;
	}

	return true;
}


bool ServerConfig::LoadConf(FILE* &conf, const std::string &filename, bool allowexeinc)
{
	return this->LoadConf(conf, filename.c_str(), allowexeinc);
}

bool ServerConfig::ParseLine(const std::string &filename, std::string &line, long &linenumber, bool allowexeinc)
{
	std::string tagname;
	std::string current_key;
	std::string current_value;
	KeyValList results;
	char last_char = 0;
	bool got_name;
	bool got_key;
	bool in_quote;

	got_name = got_key = in_quote = false;

	for(std::string::iterator c = line.begin(); c != line.end(); c++)
	{
		if (!got_name)
		{
			/* We don't know the tag name yet. */

			if (*c != ' ')
			{
				if (*c != '<')
				{
					if ((*c >= 'a' && *c <= 'z') || (*c >= 'A' && *c <='Z') || (*c >= '0' && *c <= '9') || *c == '_')
						tagname += *c;
					else
					{
						errstr << "Invalid character in value name of tag: '" << *c << "' in value '" << tagname << "' in filename: " << filename << ":" << linenumber << std::endl;
						return false;
					}
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
				if ((*c != '=') && (*c != '>'))
				{
					if (*c != ' ')
					{
						if ((*c >= 'a' && *c <= 'z') || (*c >= 'A' && *c <='Z') || (*c >= '0' && *c <= '9') || *c == '_')
							current_key += *c;
						else
						{
							errstr << "Invalid character in key: '" << *c << "' in key '" << current_key << "' in filename: " << filename << ":" << linenumber << std::endl;
							return false;
						}
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
				else if ((*c == '\\') && (!in_quote))
				{
					errstr << "You can't have an escape sequence outside of a quoted section: " << filename << ":" << linenumber << std::endl;
					return false;
				}
				else if ((*c == '\n') && (in_quote))
				{
					/* Got a 'real' \n, treat it as part of the value */
					current_value += '\n';
					continue;
				}
				else if ((*c == '\r') && (in_quote))
				{
					/* Got a \r, drop it */
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
						/* Leaving the quotes, we have the current value */
						results.push_back(KeyVal(current_key, current_value));

						// std::cout << "<" << tagname << ":" << current_key << "> " << current_value << std::endl;

						in_quote = false;
						got_key = false;

						if ((tagname == "include") && (current_key == "file"))
						{
							if (!this->DoInclude(current_value, allowexeinc))
								return false;
						}
						else if ((tagname == "include") && (current_key == "executable"))
						{
							if (!allowexeinc)
							{
								errstr << "Executable includes are not allowed to use <include:executable>\n"
									"This could be an attempt to execute commands from a malicious remote include.\n"
									"If you need multiple levels of remote include, create a script to assemble the "
									"contents locally or include files using <include:file>\n";
								return false;
							}

							/* Pipe an executable and use its stdout as config data */
							if (!this->DoPipe(current_value))
								return false;
						}

						current_key.clear();
						current_value.clear();
					}
				}
				else
				{
					if (in_quote)
					{
						last_char = *c;
						current_value += *c;
					}
				}
			}
		}
	}

	/* Finished parsing the tag, add it to the config hash */
	config_data.insert(std::pair<std::string, KeyValList > (tagname, results));

	return true;
}

bool ServerConfig::DoPipe(const std::string &file)
{
	FILE* conf = popen(file.c_str(), "r");
	bool ret = false;

	if (conf)
	{
		ret = LoadConf(conf, file.c_str(), false);
		pclose(conf);
	}
	else
		errstr << "Couldn't execute: " << file << std::endl;

	return ret;
}

bool ServerConfig::StartsWithWindowsDriveLetter(const std::string &path)
{
	return (path.length() > 2 && isalpha(path[0]) && path[1] == ':');
}

bool ServerConfig::DoInclude(const std::string &file, bool allowexeinc)
{
	std::string confpath;
	std::string newfile;
	std::string::size_type pos;

	confpath = ServerInstance->ConfigFileName;
	newfile = file;

	std::replace(newfile.begin(),newfile.end(),'\\','/');
	std::replace(confpath.begin(),confpath.end(),'\\','/');

	if ((newfile[0] != '/') && (!StartsWithWindowsDriveLetter(newfile)))
	{
		if((pos = confpath.rfind("/")) != std::string::npos)
		{
			/* Leaves us with just the path */
			newfile = confpath.substr(0, pos) + std::string("/") + newfile;
		}
		else
		{
			errstr << "Couldn't get config path from: " << ServerInstance->ConfigFileName << std::endl;
			return false;
		}
	}

	FILE* conf = fopen(newfile.c_str(), "r");
	bool ret = false;

	if (conf)
	{
		ret = LoadConf(conf, newfile, allowexeinc);
		fclose(conf);
	}
	else
		errstr << "Couldn't open config file: " << file << std::endl;

	return ret;
}

bool ServerConfig::ConfValue(const char* tag, const char* var, int index, char* result, int length, bool allow_linefeeds)
{
	return ConfValue(tag, var, "", index, result, length, allow_linefeeds);
}

bool ServerConfig::ConfValue(const char* tag, const char* var, const char* default_value, int index, char* result, int length, bool allow_linefeeds)
{
	std::string value;
	bool r = ConfValue(std::string(tag), std::string(var), std::string(default_value), index, value, allow_linefeeds);
	strlcpy(result, value.c_str(), length);
	return r;
}

bool ServerConfig::ConfValue(const std::string &tag, const std::string &var, int index, std::string &result, bool allow_linefeeds)
{
	return ConfValue(tag, var, "", index, result, allow_linefeeds);
}

bool ServerConfig::ConfValue(const std::string &tag, const std::string &var, const std::string &default_value, int index, std::string &result, bool allow_linefeeds)
{
	ConfigDataHash::size_type pos = index;
	if (pos < config_data.count(tag))
	{
		ConfigDataHash::iterator iter = config_data.find(tag);

		for(int i = 0; i < index; i++)
			iter++;

		for(KeyValList::iterator j = iter->second.begin(); j != iter->second.end(); j++)
		{
			if(j->first == var)
			{
 				if ((!allow_linefeeds) && (j->second.find('\n') != std::string::npos))
				{
					ServerInstance->Logs->Log("CONFIG",DEFAULT, "Value of <" + tag + ":" + var+ "> contains a linefeed, and linefeeds in this value are not permitted -- stripped to spaces.");
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
	else if (pos == 0)
	{
		if (!default_value.empty())
		{
			result = default_value;
			return true;
		}
	}
	return false;
}

bool ServerConfig::ConfValueInteger(const char* tag, const char* var, int index, int &result)
{
	return ConfValueInteger(std::string(tag), std::string(var), "", index, result);
}

bool ServerConfig::ConfValueInteger(const char* tag, const char* var, const char* default_value, int index, int &result)
{
	return ConfValueInteger(std::string(tag), std::string(var), std::string(default_value), index, result);
}

bool ServerConfig::ConfValueInteger(const std::string &tag, const std::string &var, int index, int &result)
{
	return ConfValueInteger(tag, var, "", index, result);
}

bool ServerConfig::ConfValueInteger(const std::string &tag, const std::string &var, const std::string &default_value, int index, int &result)
{
	std::string value;
	std::istringstream stream;
	bool r = ConfValue(tag, var, default_value, index, value);
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


bool ServerConfig::ConfValueBool(const char* tag, const char* var, int index)
{
	return ConfValueBool(std::string(tag), std::string(var), "", index);
}

bool ServerConfig::ConfValueBool(const char* tag, const char* var, const char* default_value, int index)
{
	return ConfValueBool(std::string(tag), std::string(var), std::string(default_value), index);
}

bool ServerConfig::ConfValueBool(const std::string &tag, const std::string &var, int index)
{
	return ConfValueBool(tag, var, "", index);
}

bool ServerConfig::ConfValueBool(const std::string &tag, const std::string &var, const std::string &default_value, int index)
{
	std::string result;
	if(!ConfValue(tag, var, default_value, index, result))
		return false;

	return ((result == "yes") || (result == "true") || (result == "1"));
}

int ServerConfig::ConfValueEnum(const char* tag)
{
	return config_data.count(tag);
}

int ServerConfig::ConfValueEnum(const std::string &tag)
{
	return config_data.count(tag);
}

int ServerConfig::ConfVarEnum(const char* tag, int index)
{
	return ConfVarEnum(std::string(tag), index);
}

int ServerConfig::ConfVarEnum(const std::string &tag, int index)
{
	ConfigDataHash::size_type pos = index;

	if (pos < config_data.count(tag))
	{
		ConfigDataHash::const_iterator iter = config_data.find(tag);

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

	if ((*fname != '/') && (*fname != '\\') && (!StartsWithWindowsDriveLetter(fname)))
	{
		std::string::size_type pos;
		std::string confpath = ServerInstance->ConfigFileName;
		std::string newfile = fname;

		if (((pos = confpath.rfind("/"))) != std::string::npos)
			newfile = confpath.substr(0, pos) + std::string("/") + fname;
		else if (((pos = confpath.rfind("\\"))) != std::string::npos)
			newfile = confpath.substr(0, pos) + std::string("\\") + fname;

		ServerInstance->Logs->Log("config", DEBUG, "Filename: %s", newfile.c_str());

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

			F.push_back(*linebuf ? linebuf : " ");
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

const char* ServerConfig::CleanFilename(const char* name)
{
	const char* p = name + strlen(name);
	while ((p != name) && (*p != '/') && (*p != '\\')) p--;
	return (p != name ? ++p : p);
}


std::string ServerConfig::GetFullProgDir()
{
	char buffer[PATH_MAX];
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

std::string ServerConfig::GetSID()
{
	return sid;
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

void ValueItem::Set(const std::string& value)
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

const char* ValueItem::GetString() const
{
	return v.c_str();
}

bool ValueItem::GetBool()
{
	return (GetInteger() || v == "yes" || v == "true");
}


void ConfigReaderThread::Run()
{
	Config = new ServerConfig;
	Config->Read();
	done = true;
}

void ConfigReaderThread::Finish()
{
	ServerConfig* old = ServerInstance->Config;
	ServerInstance->Logs->Log("CONFIG",DEBUG,"Switching to new configuration...");
	ServerInstance->Logs->CloseLogs();
	ServerInstance->Config = this->Config;
	ServerInstance->Logs->OpenFileLogs();
	Config->Apply(old, TheUserUID);

	if (Config->valid)
	{
		/*
		 * Apply the changed configuration from the rehash.
		 *
		 * XXX: The order of these is IMPORTANT, do not reorder them without testing
		 * thoroughly!!!
		 */
		ServerInstance->XLines->CheckELines();
		ServerInstance->XLines->CheckELines();
		ServerInstance->XLines->ApplyLines();
		ServerInstance->Res->Rehash();
		ServerInstance->ResetMaxBans();
		Config->ApplyDisabledCommands(Config->DisabledCommands);
		User* user = TheUserUID.empty() ? ServerInstance->FindNick(TheUserUID) : NULL;
		FOREACH_MOD(I_OnRehash, OnRehash(user));
		ServerInstance->BuildISupport();

		delete old;
	}
	else
	{
		// whoops, abort!
		ServerInstance->Logs->CloseLogs();
		ServerInstance->Config = old;
		ServerInstance->Logs->OpenFileLogs();
		delete this->Config;
	}
}
