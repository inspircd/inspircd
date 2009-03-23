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

std::vector<std::string> old_module_names, new_module_names, added_modules, removed_modules;

/* Needs forward declaration */
bool ValidateDnsServer(ServerConfig* conf, const char* tag, const char* value, ValueItem &data);
bool DoneELine(ServerConfig* conf, const char* tag);

ServerConfig::ServerConfig(InspIRCd* Instance) : ServerInstance(Instance)
{
	this->ClearStack();
	*sid = *ServerName = *Network = *ServerDesc = *AdminName = '\0';
	*HideWhoisServer = *AdminEmail = *AdminNick = *diepass = *restartpass = *FixedQuit = *HideKillsServer = '\0';
	*DefaultModes = *CustomVersion = *motd = *rules = *PrefixQuit = *DieValue = *DNSServer = '\0';
	*UserStats = *ModPath = *MyExecutable = *DisabledCommands = *PID = *SuffixQuit = '\0';
	WhoWasGroupSize = WhoWasMaxGroups = WhoWasMaxKeep = 0;
	log_file = NULL;
	NoUserDns = forcedebug = OperSpyWhois = nofork = HideBans = HideSplits = UndernetMsgPrefix = false;
	CycleHosts = writelog = AllowHalfop = InvBypassModes = true;
	dns_timeout = DieDelay = 5;
	MaxTargets = 20;
	NetBufferSize = 10240;
	SoftLimit = Instance->SE->GetMaxFds();
	MaxConn = SOMAXCONN;
	MaxWhoResults = 0;
	debugging = 0;
	MaxChans = 20;
	OperMaxChans = 30;
	c_ipv4_range = 32;
	c_ipv6_range = 128;
	maxbans.clear();
	DNSServerValidator = &ValidateDnsServer;
}

void ServerConfig::ClearStack()
{
	include_stack.clear();
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

bool ServerConfig::CheckOnce(const char* tag, ConfigDataHash &newconf)
{
	int count = ConfValueEnum(newconf, tag);

	if (count > 1)
		throw CoreException("You have more than one <"+std::string(tag)+"> tag, this is not permitted.");
	if (count < 1)
		throw CoreException("You have not defined a <"+std::string(tag)+"> tag, this is required.");
	return true;
}

bool NoValidation(ServerConfig*, const char*, const char*, ValueItem&)
{
	return true;
}

bool DoneConfItem(ServerConfig* conf, const char* tag)
{
	return true;
}

void ServerConfig::ValidateNoSpaces(const char* p, const std::string &tag, const std::string &val)
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
void ServerConfig::ValidateIP(const char* p, const std::string &tag, const std::string &val, bool wild)
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

void ServerConfig::ValidateHostname(const char* p, const std::string &tag, const std::string &val)
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

bool ValidateMaxTargets(ServerConfig* conf, const char*, const char*, ValueItem &data)
{
	if ((data.GetInteger() < 0) || (data.GetInteger() > 31))
	{
		conf->GetInstance()->Logs->Log("CONFIG",DEFAULT,"WARNING: <security:maxtargets> value is greater than 31 or less than 0, set to 20.");
		data.Set(20);
	}
	return true;
}

bool ValidateSoftLimit(ServerConfig* conf, const char*, const char*, ValueItem &data)
{
	if ((data.GetInteger() < 1) || (data.GetInteger() > conf->GetInstance()->SE->GetMaxFds()))
	{
		conf->GetInstance()->Logs->Log("CONFIG",DEFAULT,"WARNING: <performance:softlimit> value is greater than %d or less than 0, set to %d.",conf->GetInstance()->SE->GetMaxFds(),conf->GetInstance()->SE->GetMaxFds());
		data.Set(conf->GetInstance()->SE->GetMaxFds());
	}
	return true;
}

bool ValidateMaxConn(ServerConfig* conf, const char*, const char*, ValueItem &data)
{
	if (data.GetInteger() > SOMAXCONN)
		conf->GetInstance()->Logs->Log("CONFIG",DEFAULT,"WARNING: <performance:somaxconn> value may be higher than the system-defined SOMAXCONN value!");
	return true;
}

bool InitializeDisabledCommands(const char* data, InspIRCd* ServerInstance)
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

bool ValidateDisabledUModes(ServerConfig* conf, const char*, const char*, ValueItem &data)
{
	memset(conf->DisabledUModes, 0, sizeof(conf->DisabledUModes));
	for (const unsigned char* p = (const unsigned char*)data.GetString(); *p; ++p)
	{
		if (*p < 'A' || *p > ('A' + 64)) throw CoreException(std::string("Invalid usermode ")+(char)*p+" was found.");
		conf->DisabledUModes[*p - 'A'] = 1;
	}
	return true;
}

bool ValidateDisabledCModes(ServerConfig* conf, const char*, const char*, ValueItem &data)
{
	memset(conf->DisabledCModes, 0, sizeof(conf->DisabledCModes));
	for (const unsigned char* p = (const unsigned char*)data.GetString(); *p; ++p)
	{
		if (*p < 'A' || *p > ('A' + 64)) throw CoreException(std::string("Invalid chanmode ")+(char)*p+" was found.");
		conf->DisabledCModes[*p - 'A'] = 1;
	}
	return true;
}

bool ValidateDnsServer(ServerConfig* conf, const char*, const char*, ValueItem &data)
{
	if (!*(data.GetString()))
	{
		std::string nameserver;
		// attempt to look up their nameserver from /etc/resolv.conf
		conf->GetInstance()->Logs->Log("CONFIG",DEFAULT,"WARNING: <dns:server> not defined, attempting to find working server in /etc/resolv.conf...");
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
					conf->GetInstance()->Logs->Log("CONFIG",DEFAULT,"<dns:server> set to '%s' as first resolver in /etc/resolv.conf.",nameserver.c_str());
				}
			}

			if (!found_server)
			{
				conf->GetInstance()->Logs->Log("CONFIG",DEFAULT,"/etc/resolv.conf contains no viable nameserver entries! Defaulting to nameserver '127.0.0.1'!");
				data.Set("127.0.0.1");
			}
		}
		else
		{
			conf->GetInstance()->Logs->Log("CONFIG",DEFAULT,"/etc/resolv.conf can't be opened! Defaulting to nameserver '127.0.0.1'!");
			data.Set("127.0.0.1");
		}
	}
	return true;
}

bool ValidateServerName(ServerConfig* conf, const char*, const char*, ValueItem &data)
{
	conf->GetInstance()->Logs->Log("CONFIG",DEFAULT,"Validating server name");
	/* If we already have a servername, and they changed it, we should throw an exception. */
	if (!strchr(data.GetString(), '.'))
	{
		conf->GetInstance()->Logs->Log("CONFIG",DEFAULT,"WARNING: <server:name> '%s' is not a fully-qualified domain name. Changed to '%s%c'",data.GetString(),data.GetString(),'.');
		std::string moo = std::string(data.GetString()).append(".");
		data.Set(moo.c_str());
	}
	conf->ValidateHostname(data.GetString(), "server", "name");
	return true;
}

bool ValidateNetBufferSize(ServerConfig* conf, const char*, const char*, ValueItem &data)
{
	// 65534 not 65535 because of null terminator
	if ((!data.GetInteger()) || (data.GetInteger() > 65534) || (data.GetInteger() < 1024))
	{
		conf->GetInstance()->Logs->Log("CONFIG",DEFAULT,"No NetBufferSize specified or size out of range, setting to default of 10240.");
		data.Set(10240);
	}
	return true;
}

bool ValidateMaxWho(ServerConfig* conf, const char*, const char*, ValueItem &data)
{
	if ((data.GetInteger() > 65535) || (data.GetInteger() < 1))
	{
		conf->GetInstance()->Logs->Log("CONFIG",DEFAULT,"<performance:maxwho> size out of range, setting to default of 128.");
		data.Set(128);
	}
	return true;
}

bool ValidateHalfOp(ServerConfig* conf, const char*, const char*, ValueItem &data)
{
	ModeHandler* mh = conf->GetInstance()->Modes->FindMode('h', MODETYPE_CHANNEL);
	if (data.GetBool() && !mh)
	{
		conf->GetInstance()->Logs->Log("CONFIG",DEFAULT,"Enabling halfop mode.");
		mh = new ModeChannelHalfOp(conf->GetInstance());
		conf->GetInstance()->Modes->AddMode(mh);
	}
	else if (!data.GetBool() && mh)
	{
		conf->GetInstance()->Logs->Log("CONFIG",DEFAULT,"Disabling halfop mode.");
		conf->GetInstance()->Modes->DelMode(mh);
		delete mh;
	}
	return true;
}

bool ValidateMotd(ServerConfig* conf, const char*, const char*, ValueItem &data)
{
	conf->ReadFile(conf->MOTD, data.GetString());
	return true;
}

bool ValidateNotEmpty(ServerConfig*, const char* tag, const char* val, ValueItem &data)
{
	if (!*data.GetString())
		throw CoreException(std::string("The value for <")+tag+":"+val+"> cannot be empty!");
	return true;
}

bool ValidateRules(ServerConfig* conf, const char*, const char*, ValueItem &data)
{
	conf->ReadFile(conf->RULES, data.GetString());
	return true;
}

bool ValidateModeLists(ServerConfig* conf, const char*, const char*, ValueItem &data)
{
	memset(conf->HideModeLists, 0, sizeof(conf->HideModeLists));
	for (const unsigned char* x = (const unsigned char*)data.GetString(); *x; ++x)
		conf->HideModeLists[*x] = true;
	return true;
}

bool ValidateExemptChanOps(ServerConfig* conf, const char*, const char*, ValueItem &data)
{
	memset(conf->ExemptChanOps, 0, sizeof(conf->ExemptChanOps));
	for (const unsigned char* x = (const unsigned char*)data.GetString(); *x; ++x)
		conf->ExemptChanOps[*x] = true;
	return true;
}

bool ValidateInvite(ServerConfig* conf, const char*, const char*, ValueItem &data)
{
	std::string v = data.GetString();

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

bool ValidateSID(ServerConfig* conf, const char*, const char*, ValueItem &data)
{
	conf->GetInstance()->Logs->Log("CONFIG",DEFAULT,"Validating server id");

	const char *sid = data.GetString();

	if (*sid && !conf->GetInstance()->IsSID(sid))
	{
		throw CoreException(std::string(sid) + " is not a valid server ID. A server ID must be 3 characters long, with the first character a digit and the next two characters a digit or letter.");
	}

	strlcpy(conf->sid, sid, 5);

	return true;
}

bool ValidateWhoWas(ServerConfig* conf, const char*, const char*, ValueItem &data)
{
	conf->WhoWasMaxKeep = conf->GetInstance()->Duration(data.GetString());

	if (conf->WhoWasGroupSize < 0)
		conf->WhoWasGroupSize = 0;

	if (conf->WhoWasMaxGroups < 0)
		conf->WhoWasMaxGroups = 0;

	if (conf->WhoWasMaxKeep < 3600)
	{
		conf->WhoWasMaxKeep = 3600;
		conf->GetInstance()->Logs->Log("CONFIG",DEFAULT,"WARNING: <whowas:maxkeep> value less than 3600, setting to default 3600");
	}

	Command* whowas_command = conf->GetInstance()->Parser->GetHandler("WHOWAS");
	if (whowas_command)
	{
		std::deque<classbase*> params;
		whowas_command->HandleInternal(WHOWAS_PRUNE, params);
	}

	return true;
}

/* Callback called before processing the first <connect> tag
 */
bool InitConnect(ServerConfig* conf, const char*)
{
	conf->GetInstance()->Logs->Log("CONFIG",DEFAULT,"Reading connect classes... class list is:");

	for (ClassVector::iterator i = conf->Classes.begin(); i != conf->Classes.end() ; )
	{
		ConnectClass* c = *i;

		/*
		 * only delete a class with refcount 0.
		 * this is needed to avoid trampling on a wild pointer (User::MyClass)!
		 * it's also the most simple way to do it, given that we're looking at threads..
		 * -- w00t
		 */
		if (c->RefCount == 0)
		{
			conf->GetInstance()->Logs->Log("CONFIG",DEFAULT, "Removing connect class, refcount is 0!");

			/* This was causing a crash, because we'd set i to .begin() just here, but then the for loop's increment would
			 * set it to .begin() + 1. Which if it was already the last thing in the list, wasn't good.
			 * Now the increment is in the else { } below.
			 */
			conf->Classes.erase(i);
			i = conf->Classes.begin(); // start over so we don't trample on a bad iterator
		}
		else
		{
			/* also mark all existing classes disabled, if they still exist in the conf, they will be reenabled. */
			c->SetDisabled(true);
			i++;
		}
	}

	return true;
}

/* Callback called to process a single <connect> tag
 */
bool DoConnect(ServerConfig* conf, const char*, char**, ValueList &values, int*)
{
	ConnectClass c;
	const char* allow = values[0].GetString(); /* Yeah, there are a lot of values. Live with it. */
	const char* deny = values[1].GetString();
	const char* password = values[2].GetString();
	int timeout = values[3].GetInteger();
	int pingfreq = values[4].GetInteger();
	int sendq = values[5].GetInteger();
	int recvq = values[6].GetInteger();
	int localmax = values[7].GetInteger();
	int globalmax = values[8].GetInteger();
	int port = values[9].GetInteger();
	const char* name = values[10].GetString();
	const char* parent = values[11].GetString();
	int maxchans = values[12].GetInteger();
	unsigned long limit = values[13].GetInteger();
	const char* hashtype = values[14].GetString();

	conf->GetInstance()->Logs->Log("CONFIG",DEFAULT,"Adding a connect class!");

	ConnectClass *cc = NULL;

	if (*parent)
	{
		/* Find 'parent' and inherit a new class from it,
		 * then overwrite any values that are set here
		 */
		ClassVector::iterator item = conf->Classes.begin();
		for (; item != conf->Classes.end(); ++item)
		{
			cc = *item;
			conf->GetInstance()->Logs->Log("CONFIG",DEBUG,"Class: %s", cc->GetName().c_str());
			if (cc->GetName() == parent)
			{
				cc = new ConnectClass(name, cc);
				cc->Update(timeout, *allow ? allow : deny, pingfreq, password, sendq, recvq, localmax, globalmax, maxchans, port, limit);
				conf->Classes.push_back(cc);
				break;
			}
		}
		if (item == conf->Classes.end())
			throw CoreException("Class name '" + std::string(name) + "' is configured to inherit from class '" + std::string(parent) + "' which cannot be found.");
	}
	else
	{
		if (*allow)
		{
			/* Find existing class by mask, the mask should be unique */
			for (ClassVector::iterator item = conf->Classes.begin(); item != conf->Classes.end(); ++item)
			{
				if ((*item)->GetHost() == allow && !(*item)->GetDisabled())
				{
					(*item)->Update(timeout, allow, pingfreq, password, sendq, recvq, localmax, globalmax, maxchans, port, limit);
					return true;
				}
			}
			cc = new ConnectClass(name, timeout, allow, pingfreq, password, hashtype, sendq, recvq, localmax, globalmax, maxchans);
			cc->limit = limit;
			cc->SetPort(port);
			conf->Classes.push_back(cc);
		}
		else
		{
			/* Find existing class by mask, the mask should be unique */
			for (ClassVector::iterator item = conf->Classes.begin(); item != conf->Classes.end(); ++item)
			{
				if ((*item)->GetHost() == deny && !(*item)->GetDisabled())
				{
					(*item)->Update(name, deny);
					(*item)->SetPort(port);
					return true;
				}
			}
			cc = new ConnectClass(name, deny);
			cc->SetPort(port);
			conf->Classes.push_back(cc);
		}
	}

	return true;
}

/* Callback called when there are no more <connect> tags
 */
bool DoneConnect(ServerConfig *conf, const char*)
{
	conf->GetInstance()->Logs->Log("CONFIG",DEFAULT, "Done adding connect classes!");
	return true;
}

/* Callback called before processing the first <uline> tag
 */
bool InitULine(ServerConfig* conf, const char*)
{
	conf->ulines.clear();
	return true;
}

/* Callback called to process a single <uline> tag
 */
bool DoULine(ServerConfig* conf, const char*, char**, ValueList &values, int*)
{
	const char* server = values[0].GetString();
	const bool silent = values[1].GetBool();
	conf->ulines[server] = silent;
	return true;
}

/* Callback called when there are no more <uline> tags
 */
bool DoneULine(ServerConfig*, const char*)
{
	return true;
}

/* Callback called before processing the first <module> tag
 */
bool InitModule(ServerConfig* conf, const char*)
{
	old_module_names = conf->GetInstance()->Modules->GetAllModuleNames(0);
	new_module_names.clear();
	added_modules.clear();
	removed_modules.clear();
	return true;
}

/* Callback called to process a single <module> tag
 */
bool DoModule(ServerConfig*, const char*, char**, ValueList &values, int*)
{
	const char* modname = values[0].GetString();
	new_module_names.push_back(modname);
	return true;
}

/* Callback called when there are no more <module> tags
 */
bool DoneModule(ServerConfig*, const char*)
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
bool InitMaxBans(ServerConfig* conf, const char*)
{
	conf->maxbans.clear();
	return true;
}

/* Callback called to process a single <banlist> tag
 */
bool DoMaxBans(ServerConfig* conf, const char*, char**, ValueList &values, int*)
{
	const char* channel = values[0].GetString();
	int limit = values[1].GetInteger();
	conf->maxbans[channel] = limit;
	return true;
}

/* Callback called when there are no more <banlist> tags.
 */
bool DoneMaxBans(ServerConfig*, const char*)
{
	return true;
}

void ServerConfig::ReportConfigError(const std::string &errormessage, bool bail, const std::string &useruid)
{
	ServerInstance->Logs->Log("CONFIG",DEFAULT, "There were errors in your configuration file: %s", errormessage.c_str());
	if (bail)
	{
		/* Unneeded because of the ServerInstance->Log() aboive? */
		printf("There were errors in your configuration:\n%s\n\n",errormessage.c_str());
		ServerInstance->Exit(EXIT_STATUS_CONFIG);
	}
	else
	{
		std::string errors = errormessage;
		std::string::size_type start;
		unsigned int prefixlen;
		start = 0;
		/* ":ServerInstance->Config->ServerName NOTICE user->nick :" */
		if (!useruid.empty())
		{
			User* user = ServerInstance->FindNick(useruid);
			if (user)
			{
				prefixlen = strlen(this->ServerName) + user->nick.length() + 11;
				user->WriteServ("NOTICE %s :There were errors in the configuration file:",user->nick.c_str());
				while (start < errors.length())
				{
					user->WriteServ("NOTICE %s :%s",user->nick.c_str(), errors.substr(start, 510 - prefixlen).c_str());
					start += 510 - prefixlen;
				}
			}
		}
		else
		{
			ServerInstance->SNO->WriteToSnoMask('A', "There were errors in the configuration file:");
			while (start < errors.length())
			{
				ServerInstance->SNO->WriteToSnoMask('A', errors.substr(start, 360));
				start += 360;
			}
		}
		return;
	}
}

void ServerConfig::Read(bool bail, const std::string &useruid)
{
	int rem = 0, add = 0;	   /* Number of modules added, number of modules removed */

	static char maxkeep[MAXBUF];	/* Temporary buffer for WhoWasMaxKeep value */
	static char hidemodes[MAXBUF];	/* Modes to not allow listing from users below halfop */
	static char exemptchanops[MAXBUF];	/* Exempt channel ops from these modes */
	static char announceinvites[MAXBUF];	/* options:announceinvites setting */
	static char disabledumodes[MAXBUF]; /* Disabled usermodes */
	static char disabledcmodes[MAXBUF]; /* Disabled chanmodes */
	/* std::ostringstream::clear() does not clear the string itself, only the error flags. */
	this->errstr = new std::ostringstream(std::stringstream::in | std::stringstream::out);

	include_stack.clear();

	/* These tags MUST occur and must ONLY occur once in the config file */
	static const char* Once[] = { "server", "admin", "files", "power", "options", NULL };

	Deprecated ChangedConfig[] = {
		{"options",	"hidelinks",		"has been moved to <security:hidelinks> as of 1.2a3"},
		{"options",	"hidewhois",		"has been moved to <security:hidewhois> as of 1.2a3"},
		{"options",     "userstats",		"has been moved to <security:userstats> as of 1.2a3"},
		{"options",     "customversion",	"has been moved to <security:customversion> as of 1.2a3"},
		{"options",     "hidesplits",		"has been moved to <security:hidesplits> as of 1.2a3"},
		{"options",     "hidebans",		"has been moved to <security:hidebans> as of 1.2a3"},
		{"options",     "hidekills",		"has been moved to <security:hidekills> as of 1.2a3"},
		{"options",     "operspywhois",		"has been moved to <security:operspywhois> as of 1.2a3"},
		{"options",     "announceinvites",	"has been moved to <security:announceinvites> as of 1.2a3"},
		{"options",     "hidemodes",		"has been moved to <security:hidemodes> as of 1.2a3"},
		{"options",     "maxtargets",		"has been moved to <security:maxtargets> as of 1.2a3"},
		{"options",	"nouserdns",		"has been moved to <performance:nouserdns> as of 1.2a3"},
		{"options",	"maxwho",		"has been moved to <performance:maxwho> as of 1.2a3"},
		{"options",	"softlimit",		"has been moved to <performance:softlimit> as of 1.2a3"},
		{"options",     "somaxconn",		"has been moved to <performance:somaxconn> as of 1.2a3"},
		{"options",     "netbuffersize",	"has been moved to <performance:netbuffersize> as of 1.2a3"},
		{"options",     "maxwho",		"has been moved to <performance:maxwho> as of 1.2a3"},
		{"options",	"loglevel",		"1.2 does not use the loglevel value. Please define <log> tags instead."},
		{NULL,		NULL,			NULL}
	};

	/* These tags can occur ONCE or not at all */
	InitialConfig Values[] = {
		{"performance",	"softlimit",	"0",			new ValueContainerUInt (&this->SoftLimit),		DT_INTEGER,  ValidateSoftLimit},
		{"performance",	"somaxconn",	SOMAXCONN_S,		new ValueContainerInt  (&this->MaxConn),		DT_INTEGER,  ValidateMaxConn},
		{"options",	"moronbanner",	"Youre banned!",	new ValueContainerChar (this->MoronBanner),		DT_CHARPTR,  NoValidation},
		{"server",	"name",		"",			new ValueContainerChar (this->ServerName),		DT_HOSTNAME|DT_BOOTONLY, ValidateServerName},
		{"server",	"description",	"Configure Me",		new ValueContainerChar (this->ServerDesc),		DT_CHARPTR,  NoValidation},
		{"server",	"network",	"Network",		new ValueContainerChar (this->Network),			DT_NOSPACES, NoValidation},
		{"server",	"id",		"",			new ValueContainerChar (this->sid),			DT_CHARPTR|DT_BOOTONLY,  ValidateSID},
		{"admin",	"name",		"",			new ValueContainerChar (this->AdminName),		DT_CHARPTR,  NoValidation},
		{"admin",	"email",	"Mis@configu.red",	new ValueContainerChar (this->AdminEmail),		DT_CHARPTR,  NoValidation},
		{"admin",	"nick",		"Misconfigured",	new ValueContainerChar (this->AdminNick),		DT_CHARPTR,  NoValidation},
		{"files",	"motd",		"",			new ValueContainerChar (this->motd),			DT_CHARPTR,  ValidateMotd},
		{"files",	"rules",	"",			new ValueContainerChar (this->rules),			DT_CHARPTR,  ValidateRules},
		{"power",	"diepass",	"",			new ValueContainerChar (this->diepass),			DT_CHARPTR,  ValidateNotEmpty},
		{"power",	"pause",	"",			new ValueContainerInt  (&this->DieDelay),		DT_INTEGER,  NoValidation},
		{"power",	"hash",		"",			new ValueContainerChar (this->powerhash),		DT_CHARPTR,  NoValidation},
		{"power",	"restartpass",	"",			new ValueContainerChar (this->restartpass),		DT_CHARPTR,  ValidateNotEmpty},
		{"options",	"prefixquit",	"",			new ValueContainerChar (this->PrefixQuit),		DT_CHARPTR,  NoValidation},
		{"options",	"suffixquit",	"",			new ValueContainerChar (this->SuffixQuit),		DT_CHARPTR,  NoValidation},
		{"options",	"fixedquit",	"",			new ValueContainerChar (this->FixedQuit),		DT_CHARPTR,  NoValidation},
		{"options",	"prefixpart",	"",			new ValueContainerChar (this->PrefixPart),		DT_CHARPTR,  NoValidation},
		{"options",	"suffixpart",	"",			new ValueContainerChar (this->SuffixPart),		DT_CHARPTR,  NoValidation},
		{"options",	"fixedpart",	"",			new ValueContainerChar (this->FixedPart),		DT_CHARPTR,  NoValidation},
		{"performance",	"netbuffersize","10240",		new ValueContainerInt  (&this->NetBufferSize),		DT_INTEGER,  ValidateNetBufferSize},
		{"performance",	"maxwho",	"128",			new ValueContainerInt  (&this->MaxWhoResults),		DT_INTEGER,  ValidateMaxWho},
		{"options",	"allowhalfop",	"0",			new ValueContainerBool (&this->AllowHalfop),		DT_BOOLEAN,  ValidateHalfOp},
		{"dns",		"server",	"",			new ValueContainerChar (this->DNSServer),		DT_IPADDRESS,DNSServerValidator},
		{"dns",		"timeout",	"5",			new ValueContainerInt  (&this->dns_timeout),		DT_INTEGER,  NoValidation},
		{"options",	"moduledir",	MOD_PATH,		new ValueContainerChar (this->ModPath),			DT_CHARPTR,  NoValidation},
		{"disabled",	"commands",	"",			new ValueContainerChar (this->DisabledCommands),	DT_CHARPTR,  NoValidation},
		{"disabled",	"usermodes",	"",			new ValueContainerChar (disabledumodes),		DT_CHARPTR,  ValidateDisabledUModes},
		{"disabled",	"chanmodes",	"",			new ValueContainerChar (disabledcmodes),		DT_CHARPTR,  ValidateDisabledCModes},
		{"disabled",	"fakenonexistant",	"0",		new ValueContainerBool (&this->DisabledDontExist),		DT_BOOLEAN,  NoValidation},

		{"security",		"runasuser",	"",		new ValueContainerChar(this->SetUser),				DT_CHARPTR, NoValidation},
		{"security",		"runasgroup",	"",		new ValueContainerChar(this->SetGroup),				DT_CHARPTR, NoValidation},
		{"security",	"userstats",	"",			new ValueContainerChar (this->UserStats),		DT_CHARPTR,  NoValidation},
		{"security",	"customversion","",			new ValueContainerChar (this->CustomVersion),		DT_CHARPTR,  NoValidation},
		{"security",	"hidesplits",	"0",			new ValueContainerBool (&this->HideSplits),		DT_BOOLEAN,  NoValidation},
		{"security",	"hidebans",	"0",			new ValueContainerBool (&this->HideBans),		DT_BOOLEAN,  NoValidation},
		{"security",	"hidewhois",	"",			new ValueContainerChar (this->HideWhoisServer),		DT_NOSPACES, NoValidation},
		{"security",	"hidekills",	"",			new ValueContainerChar (this->HideKillsServer),		DT_NOSPACES,  NoValidation},
		{"security",	"operspywhois",	"0",			new ValueContainerBool (&this->OperSpyWhois),		DT_BOOLEAN,  NoValidation},
		{"security",	"restrictbannedusers",	"1",		new ValueContainerBool (&this->RestrictBannedUsers),		DT_BOOLEAN,  NoValidation},
		{"security",	"genericoper",	"0",			new ValueContainerBool (&this->GenericOper),		DT_BOOLEAN,  NoValidation},
		{"performance",	"nouserdns",	"0",			new ValueContainerBool (&this->NoUserDns),		DT_BOOLEAN,  NoValidation},
		{"options",	"syntaxhints",	"0",			new ValueContainerBool (&this->SyntaxHints),		DT_BOOLEAN,  NoValidation},
		{"options",	"cyclehosts",	"0",			new ValueContainerBool (&this->CycleHosts),		DT_BOOLEAN,  NoValidation},
		{"options",	"ircumsgprefix","0",			new ValueContainerBool (&this->UndernetMsgPrefix),	DT_BOOLEAN,  NoValidation},
		{"security",	"announceinvites", "1",			new ValueContainerChar (announceinvites),		DT_CHARPTR,  ValidateInvite},
		{"options",	"hostintopic",	"1",			new ValueContainerBool (&this->FullHostInTopic),	DT_BOOLEAN,  NoValidation},
		{"security",	"hidemodes",	"",			new ValueContainerChar (hidemodes),			DT_CHARPTR,  ValidateModeLists},
		{"options",	"exemptchanops","",			new ValueContainerChar (exemptchanops),			DT_CHARPTR,  ValidateExemptChanOps},
		{"security",	"maxtargets",	"20",			new ValueContainerUInt (&this->MaxTargets),		DT_INTEGER,  ValidateMaxTargets},
		{"options",	"defaultmodes", "nt",			new ValueContainerChar (this->DefaultModes),		DT_CHARPTR,  NoValidation},
		{"pid",		"file",		"",			new ValueContainerChar (this->PID),			DT_CHARPTR,  NoValidation},
		{"whowas",	"groupsize",	"10",			new ValueContainerInt  (&this->WhoWasGroupSize),	DT_INTEGER,  NoValidation},
		{"whowas",	"maxgroups",	"10240",		new ValueContainerInt  (&this->WhoWasMaxGroups),	DT_INTEGER,  NoValidation},
		{"whowas",	"maxkeep",	"3600",			new ValueContainerChar (maxkeep),			DT_CHARPTR,  ValidateWhoWas},
		{"die",		"value",	"",			new ValueContainerChar (this->DieValue),		DT_CHARPTR,  NoValidation},
		{"channels",	"users",	"20",			new ValueContainerUInt (&this->MaxChans),		DT_INTEGER,  NoValidation},
		{"channels",	"opers",	"60",			new ValueContainerUInt (&this->OperMaxChans),		DT_INTEGER,  NoValidation},
		{"cidr",	"ipv4clone",	"32",			new ValueContainerInt (&this->c_ipv4_range),		DT_INTEGER,  NoValidation},
		{"cidr",	"ipv6clone",	"128",			new ValueContainerInt (&this->c_ipv6_range),		DT_INTEGER,  NoValidation},
		{"limits",	"maxnick",	"32",			new ValueContainerST (&this->Limits.NickMax),		DT_INTEGER,  NoValidation},
		{"limits",	"maxchan",	"64",			new ValueContainerST (&this->Limits.ChanMax),		DT_INTEGER,  NoValidation},
		{"limits",	"maxmodes",	"20",			new ValueContainerST (&this->Limits.MaxModes),		DT_INTEGER,  NoValidation},
		{"limits",	"maxident",	"11",			new ValueContainerST (&this->Limits.IdentMax),		DT_INTEGER,  NoValidation},
		{"limits",	"maxquit",	"255",			new ValueContainerST (&this->Limits.MaxQuit),		DT_INTEGER,  NoValidation},
		{"limits",	"maxtopic",	"307",			new ValueContainerST (&this->Limits.MaxTopic),		DT_INTEGER,  NoValidation},
		{"limits",	"maxkick",	"255",			new ValueContainerST (&this->Limits.MaxKick),		DT_INTEGER,  NoValidation},
		{"limits",	"maxgecos",	"128",			new ValueContainerST (&this->Limits.MaxGecos),		DT_INTEGER,  NoValidation},
		{"limits",	"maxaway",	"200",			new ValueContainerST (&this->Limits.MaxAway),		DT_INTEGER,  NoValidation},
		{"options",	"invitebypassmodes",	"1",			new ValueContainerBool (&this->InvBypassModes),		DT_BOOLEAN,  NoValidation},
		{NULL,		NULL,		NULL,			NULL,							DT_NOTHING,  NoValidation}
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
				InitConnect, DoConnect, DoneConnect},

		{"uline",
				{"server",	"silent",	NULL},
				{"",		"0",		NULL},
				{DT_HOSTNAME,	DT_BOOLEAN},
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
				{DT_CHARPTR,	DT_IPADDRESS|DT_ALLOW_WILD},
				InitXLine, DoZLine, DoneConfItem},

		{"badnick",
				{"reason",	"nick",		NULL},
				{"No reason",	"",		NULL},
				{DT_CHARPTR,	DT_CHARPTR},
				InitXLine, DoQLine, DoneConfItem},

		{"badhost",
				{"reason",	"host",		NULL},
				{"No reason",	"",		NULL},
				{DT_CHARPTR,	DT_CHARPTR},
				InitXLine, DoKLine, DoneConfItem},

		{"exception",
				{"reason",	"host",		NULL},
				{"No reason",	"",		NULL},
				{DT_CHARPTR,	DT_CHARPTR},
				InitXLine, DoELine, DoneELine},

		{"type",
				{"name",	"classes",	NULL},
				{"",		"",		NULL},
				{DT_NOSPACES,	DT_CHARPTR},
				InitTypes, DoType, DoneClassesAndTypes},

		{"class",
				{"name",	"commands",	"usermodes",	"chanmodes",	"privs",	NULL},
				{"",		"",				"",				"",			"",			NULL},
				{DT_NOSPACES,	DT_CHARPTR,	DT_CHARPTR,	DT_CHARPTR, DT_CHARPTR},
				InitClasses, DoClass, DoneClassesAndTypes},

		{NULL,
				{NULL},
				{NULL},
				{0},
				NULL, NULL, NULL}
	};

	/* Load and parse the config file, if there are any errors then explode */

	/* Make a copy here so if it fails then we can carry on running with an unaffected config */
	newconfig.clear();

	if (!this->DoInclude(newconfig, ServerInstance->ConfigFileName, *errstr))
	{
		ReportConfigError(errstr->str(), bail, useruid);
		delete errstr;
		return;
	}

	delete errstr;

	/* The stuff in here may throw CoreException, be sure we're in a position to catch it. */
	try
	{
		/* Check we dont have more than one of singular tags, or any of them missing
		 */
		for (int Index = 0; Once[Index]; Index++)
			if (!CheckOnce(Once[Index], newconfig))
				return;

		for (int Index = 0; ChangedConfig[Index].tag; Index++)
		{
			char item[MAXBUF];
			*item = 0;
			if (ConfValue(newconfig, ChangedConfig[Index].tag, ChangedConfig[Index].value, "", 0, item, MAXBUF, true) || *item)
				throw CoreException(std::string("Your configuration contains a deprecated value: <") + ChangedConfig[Index].tag + ":" + ChangedConfig[Index].value + "> - " + ChangedConfig[Index].reason);
		}

		/* Read the values of all the tags which occur once or not at all, and call their callbacks.
		 */
		for (int Index = 0; Values[Index].tag; ++Index)
		{
			char item[MAXBUF];
			int dt = Values[Index].datatype;
			bool allow_newlines = ((dt & DT_ALLOW_NEWLINE) > 0);
			bool allow_wild = ((dt & DT_ALLOW_WILD) > 0);
			bool bootonly = ((dt & DT_BOOTONLY) > 0);
			dt &= ~DT_ALLOW_NEWLINE;
			dt &= ~DT_ALLOW_WILD;
			dt &= ~DT_BOOTONLY;

			/* Silently ignore boot only values */
			if (bootonly && !bail)
			{
				delete Values[Index].val;
				continue;
			}

			ConfValue(newconfig, Values[Index].tag, Values[Index].value, Values[Index].default_value, 0, item, MAXBUF, allow_newlines);
			ValueItem vi(item);

			if (!Values[Index].validation_function(this, Values[Index].tag, Values[Index].value, vi))
				throw CoreException("One or more values in your configuration file failed to validate. Please see your ircd.log for more information.");

			// XXX: ServerInstance->Threads->Lock();
			switch (dt)
			{
				case DT_NOSPACES:
				{
					ValueContainerChar* vcc = (ValueContainerChar*)Values[Index].val;
					this->ValidateNoSpaces(vi.GetString(), Values[Index].tag, Values[Index].value);
					vcc->Set(vi.GetString(), strlen(vi.GetString()) + 1);
				}
				break;
				case DT_HOSTNAME:
				{
					ValueContainerChar* vcc = (ValueContainerChar*)Values[Index].val;
					this->ValidateHostname(vi.GetString(), Values[Index].tag, Values[Index].value);
					vcc->Set(vi.GetString(), strlen(vi.GetString()) + 1);
				}
				break;
				case DT_IPADDRESS:
				{
					ValueContainerChar* vcc = (ValueContainerChar*)Values[Index].val;
					this->ValidateIP(vi.GetString(), Values[Index].tag, Values[Index].value, allow_wild);
					vcc->Set(vi.GetString(), strlen(vi.GetString()) + 1);
				}
				break;
				case DT_CHANNEL:
				{
					ValueContainerChar* vcc = (ValueContainerChar*)Values[Index].val;
					if (*(vi.GetString()) && !ServerInstance->IsChannel(vi.GetString(), MAXBUF))
					{
						// XXX: ServerInstance->Threads->Unlock();
						throw CoreException("The value of <"+std::string(Values[Index].tag)+":"+Values[Index].value+"> is not a valid channel name");
					}
					vcc->Set(vi.GetString(), strlen(vi.GetString()) + 1);
				}
				break;
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
			// XXX: ServerInstance->Threads->Unlock();
		}

		/* Read the multiple-tag items (class tags, connect tags, etc)
		 * and call the callbacks associated with them. We have three
		 * callbacks for these, a 'start', 'item' and 'end' callback.
		 */
		for (int Index = 0; MultiValues[Index].tag; ++Index)
		{
			// XXX: ServerInstance->Threads->Lock();
			MultiValues[Index].init_function(this, MultiValues[Index].tag);
			// XXX: ServerInstance->Threads->Unlock();

			int number_of_tags = ConfValueEnum(newconfig, MultiValues[Index].tag);

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

					// XXX: ServerInstance->Threads->Lock();
					/* We catch and rethrow any exception here just so we can free our mutex
					 */
					try
					{
						switch (dt)
						{
							case DT_NOSPACES:
							{
								char item[MAXBUF];
								if (ConfValue(newconfig, MultiValues[Index].tag, MultiValues[Index].items[valuenum], MultiValues[Index].items_default[valuenum], tagnum, item, MAXBUF, allow_newlines))
									vl.push_back(ValueItem(item));
								else
									vl.push_back(ValueItem(""));
								this->ValidateNoSpaces(vl[vl.size()-1].GetString(), MultiValues[Index].tag, MultiValues[Index].items[valuenum]);
							}
							break;
							case DT_HOSTNAME:
							{
								char item[MAXBUF];
								if (ConfValue(newconfig, MultiValues[Index].tag, MultiValues[Index].items[valuenum], MultiValues[Index].items_default[valuenum], tagnum, item, MAXBUF, allow_newlines))
									vl.push_back(ValueItem(item));
								else
									vl.push_back(ValueItem(""));
								this->ValidateHostname(vl[vl.size()-1].GetString(), MultiValues[Index].tag, MultiValues[Index].items[valuenum]);
							}
							break;
							case DT_IPADDRESS:
							{
								char item[MAXBUF];
								if (ConfValue(newconfig, MultiValues[Index].tag, MultiValues[Index].items[valuenum], MultiValues[Index].items_default[valuenum], tagnum, item, MAXBUF, allow_newlines))
									vl.push_back(ValueItem(item));
								else
									vl.push_back(ValueItem(""));
								this->ValidateIP(vl[vl.size()-1].GetString(), MultiValues[Index].tag, MultiValues[Index].items[valuenum], allow_wild);
							}
							break;
							case DT_CHANNEL:
							{
								char item[MAXBUF];
								if (ConfValue(newconfig, MultiValues[Index].tag, MultiValues[Index].items[valuenum], MultiValues[Index].items_default[valuenum], tagnum, item, MAXBUF, allow_newlines))
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
								if (ConfValue(newconfig, MultiValues[Index].tag, MultiValues[Index].items[valuenum], MultiValues[Index].items_default[valuenum], tagnum, item, MAXBUF, allow_newlines))
									vl.push_back(ValueItem(item));
								else
									vl.push_back(ValueItem(""));
							}
							break;
							case DT_INTEGER:
							{
								int item = 0;
								if (ConfValueInteger(newconfig, MultiValues[Index].tag, MultiValues[Index].items[valuenum], MultiValues[Index].items_default[valuenum], tagnum, item))
									vl.push_back(ValueItem(item));
								else
									vl.push_back(ValueItem(0));
							}
							break;
							case DT_BOOLEAN:
							{
								bool item = ConfValueBool(newconfig, MultiValues[Index].tag, MultiValues[Index].items[valuenum], MultiValues[Index].items_default[valuenum], tagnum);
								vl.push_back(ValueItem(item));
							}
							break;
							default:
								/* Someone was smoking craq if we got here, and we're all gonna die. */
							break;
						}
					}
					catch (CoreException &e)
					{
						// XXX: ServerInstance->Threads->Unlock();
						throw e;
					}
					// XXX: ServerInstance->Threads->Unlock();
				}
				MultiValues[Index].validation_function(this, MultiValues[Index].tag, (char**)MultiValues[Index].items, vl, MultiValues[Index].datatype);
			}
			MultiValues[Index].finish_function(this, MultiValues[Index].tag);
		}

		/* Finalise the limits, increment them all by one so that we can just put assign(str, 0, val)
		 * rather than assign(str, 0, val + 1)
		 */
		Limits.Finalise();

	}

	catch (CoreException &ce)
	{
		ReportConfigError(ce.GetReason(), bail, useruid);
		return;
	}

	// XXX: ServerInstance->Threads->Lock();
	for (int i = 0; i < ConfValueEnum(newconfig, "type"); ++i)
	{
		char item[MAXBUF], classn[MAXBUF], classes[MAXBUF];
		std::string classname;
		ConfValue(newconfig, "type", "classes", "", i, classes, MAXBUF, false);
		irc::spacesepstream str(classes);
		ConfValue(newconfig, "type", "name", "", i, item, MAXBUF, false);
		while (str.GetToken(classname))
		{
			std::string lost;
			bool foundclass = false;
			for (int j = 0; j < ConfValueEnum(newconfig, "class"); ++j)
			{
				ConfValue(newconfig, "class", "name", "", j, classn, MAXBUF, false);
				if (!strcmp(classn, classname.c_str()))
				{
					foundclass = true;
					break;
				}
			}
			if (!foundclass)
			{
				if (!useruid.empty())
				{
					User* user = ServerInstance->FindNick(useruid);
					if (user)
						user->WriteServ("NOTICE %s :*** Warning: Oper type '%s' has a missing class named '%s', this does nothing!", user->nick.c_str(), item, classname.c_str());
				}
				else
				{
					if (bail)
						printf("Warning: Oper type '%s' has a missing class named '%s', this does nothing!\n", item, classname.c_str());
					else
						ServerInstance->SNO->WriteToSnoMask('A', "Warning: Oper type '%s' has a missing class named '%s', this does nothing!", item, classname.c_str());
				}
			}
		}
	}

	/* If we succeeded, set the ircd config to the new one */
	this->config_data = newconfig;

	// XXX: ServerInstance->Threads->Unlock();

	// write once here, to try it out and make sure its ok
	ServerInstance->WritePID(this->PID);

	/* If we're rehashing, let's load any new modules, and unload old ones
	 */
	if (!bail)
	{
		int found_ports = 0;
		FailedPortList pl;
		ServerInstance->BindPorts(false, found_ports, pl);

		if (pl.size() && !useruid.empty())
		{
			// XXX: ServerInstance->Threads->Lock();
			User* user = ServerInstance->FindNick(useruid);
			if (user)
			{
				user->WriteServ("NOTICE %s :*** Not all your client ports could be bound.", user->nick.c_str());
				user->WriteServ("NOTICE %s :*** The following port(s) failed to bind:", user->nick.c_str());
				int j = 1;
				for (FailedPortList::iterator i = pl.begin(); i != pl.end(); i++, j++)
				{
					user->WriteServ("NOTICE %s :*** %d.   Address: %s        Reason: %s", user->nick.c_str(), j, i->first.empty() ? "<all>" : i->first.c_str(), i->second.c_str());
				}
			}
			// XXX: ServerInstance->Threads->Unlock();
		}

		// XXX: ServerInstance->Threads->Lock();
		if (!removed_modules.empty())
		{
			for (std::vector<std::string>::iterator removing = removed_modules.begin(); removing != removed_modules.end(); removing++)
			{
				if (ServerInstance->Modules->Unload(removing->c_str()))
				{
					ServerInstance->SNO->WriteToSnoMask('A', "*** REHASH UNLOADED MODULE: %s",removing->c_str());

					if (!useruid.empty())
					{
						User* user = ServerInstance->FindNick(useruid);
						if (user)
							user->WriteNumeric(RPL_UNLOADEDMODULE, "%s %s :Module %s successfully unloaded.",user->nick.c_str(), removing->c_str(), removing->c_str());
					}
					else
						ServerInstance->SNO->WriteToSnoMask('A', "Module %s successfully unloaded.", removing->c_str());

					rem++;
				}
				else
				{
					if (!useruid.empty())
					{
						User* user = ServerInstance->FindNick(useruid);
						if (user)
							user->WriteNumeric(ERR_CANTUNLOADMODULE, "%s %s :Failed to unload module %s: %s",user->nick.c_str(), removing->c_str(), removing->c_str(), ServerInstance->Modules->LastError().c_str());
					}
					else
						 ServerInstance->SNO->WriteToSnoMask('A', "Failed to unload module %s: %s", removing->c_str(), ServerInstance->Modules->LastError().c_str());
				}
			}
		}

		if (!added_modules.empty())
		{
			for (std::vector<std::string>::iterator adding = added_modules.begin(); adding != added_modules.end(); adding++)
			{
				if (ServerInstance->Modules->Load(adding->c_str()))
				{
					ServerInstance->SNO->WriteToSnoMask('A', "*** REHASH LOADED MODULE: %s",adding->c_str());
					if (!useruid.empty())
					{
						User* user = ServerInstance->FindNick(useruid);
						if (user)
							user->WriteNumeric(RPL_LOADEDMODULE, "%s %s :Module %s successfully loaded.",user->nick.c_str(), adding->c_str(), adding->c_str());
					}
					else
						ServerInstance->SNO->WriteToSnoMask('A', "Module %s successfully loaded.", adding->c_str());

					add++;
				}
				else
				{
					if (!useruid.empty())
					{
						User* user = ServerInstance->FindNick(useruid);
						if (user)
							user->WriteNumeric(ERR_CANTLOADMODULE, "%s %s :Failed to load module %s: %s",user->nick.c_str(), adding->c_str(), adding->c_str(), ServerInstance->Modules->LastError().c_str());
					}
					else
						ServerInstance->SNO->WriteToSnoMask('A', "Failed to load module %s: %s", adding->c_str(), ServerInstance->Modules->LastError().c_str());
				}
			}
		}
		// XXX: ServerInstance->Threads->Unlock();

	}

	if (!bail)
	{
		if (!useruid.empty())
		{
			User* user = ServerInstance->FindNick(useruid);
			if (user)
				user->WriteServ("NOTICE %s :*** Successfully rehashed server.", user->nick.c_str());
		}
		else
			ServerInstance->SNO->WriteToSnoMask('A', "*** Successfully rehashed server.");
	}

}


bool ServerConfig::LoadConf(ConfigDataHash &target, FILE* &conf, const char* filename, std::ostringstream &errorstream)
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
		errorstream << "LoadConf: Couldn't open config file: " << filename << std::endl;
		return false;
	}

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
				errorstream << "End of file after a \\, what did you want to escape?: " << filename << ":" << linenumber << std::endl;
				return false;
			}
		}

		if (ch != '\r')
			line += ch;

		if ((ch != '<') && (!in_tag) && (!in_comment) && (ch > ' ') && (ch != 9))
		{
			errorstream << "You have stray characters beyond the tag which starts at " << filename << ":" << last_successful_parse << std::endl;
			return false;
		}

		if (ch == '<')
		{
			if (in_tag)
			{
				if (!in_quote)
				{
					errorstream << "The tag at location " << filename << ":" << last_successful_parse << " was valid, but there is an error in the tag which comes after it. You are possibly missing a \" or >. Please check this." << std::endl;
					return false;
				}
			}
			else
			{
				if (in_quote)
				{
					errorstream << "Parser error: Inside a quote but not within the last valid tag, which was opened at: " << filename << ":" << last_successful_parse << std::endl;
					return false;
				}
				else
				{
					// errorstream << "Opening new config tag on line " << linenumber << std::endl;
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
				if (in_quote)
				{
					errorstream << "The tag immediately after the one at " << filename << ":" << last_successful_parse << " has a missing closing \" symbol. Please check this." << std::endl;
				}
				else
				{
					errorstream << "You have opened a quote (\") beyond the tag at " << filename << ":" << last_successful_parse << " without opening a new tag. Please check this." << std::endl;
				}
			}
		}
		else if (ch == '>')
		{
			if (!in_quote)
			{
				if (in_tag)
				{
					// errorstream << "Closing config tag on line " << linenumber << std::endl;
					in_tag = false;

					/*
					 * If this finds an <include> then ParseLine can simply call
					 * LoadConf() and load the included config into the same ConfigDataHash
					 */
					long bl = linenumber;
					if (!this->ParseLine(target, filename, line, linenumber, errorstream))
						return false;
					last_successful_parse = linenumber;

					linenumber = bl;

					line.clear();
				}
				else
				{
					errorstream << "You forgot to close the tag which comes immediately after the one at " << filename << ":" << last_successful_parse << std::endl;
					return false;
				}
			}
		}
	}

	/* Fix for bug #392 - if we reach the end of a file and we are still in a quote or comment, most likely the user fucked up */
	if (in_comment || in_quote)
	{
		errorstream << "Reached end of file whilst still inside a quoted section or tag. This is most likely an error or there \
			is a newline missing from the end of the file: " << filename << ":" << linenumber << std::endl;
	}

	return true;
}


bool ServerConfig::LoadConf(ConfigDataHash &target, FILE* &conf, const std::string &filename, std::ostringstream &errorstream)
{
	return this->LoadConf(target, conf, filename.c_str(), errorstream);
}

bool ServerConfig::ParseLine(ConfigDataHash &target, const std::string &filename, std::string &line, long &linenumber, std::ostringstream &errorstream)
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
						errorstream << "Invalid character in value name of tag: '" << *c << "' in value '" << tagname << "' in filename: " << filename << ":" << linenumber << std::endl;
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
							errorstream << "Invalid character in key: '" << *c << "' in key '" << current_key << "' in filename: " << filename << ":" << linenumber << std::endl;
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
					errorstream << "You can't have an escape sequence outside of a quoted section: " << filename << ":" << linenumber << std::endl;
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
							if (!this->DoInclude(target, current_value, errorstream))
								return false;
						}
						else if ((tagname == "include") && (current_key == "executable"))
						{
							/* Pipe an executable and use its stdout as config data */
							if (!this->DoPipe(target, current_value, errorstream))
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
	target.insert(std::pair<std::string, KeyValList > (tagname, results));

	return true;
}

bool ServerConfig::DoPipe(ConfigDataHash &target, const std::string &file, std::ostringstream &errorstream)
{
	FILE* conf = popen(file.c_str(), "r");
	bool ret = false;

	if (conf)
	{
		ret = LoadConf(target, conf, file.c_str(), errorstream);
		pclose(conf);
	}
	else
		errorstream << "Couldn't execute: " << file << std::endl;

	return ret;
}

bool ServerConfig::StartsWithWindowsDriveLetter(const std::string &path)
{
	return (path.length() > 2 && isalpha(path[0]) && path[1] == ':');
}

bool ServerConfig::DoInclude(ConfigDataHash &target, const std::string &file, std::ostringstream &errorstream)
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
			errorstream << "Couldn't get config path from: " << ServerInstance->ConfigFileName << std::endl;
			return false;
		}
	}

	FILE* conf = fopen(newfile.c_str(), "r");
	bool ret = false;

	if (conf)
	{
		ret = LoadConf(target, conf, newfile, errorstream);
		fclose(conf);
	}
	else
		errorstream << "Couldn't open config file: " << file << std::endl;

	return ret;
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
	if (pos < target.count(tag))
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

	if (pos < target.count(tag))
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
#else

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
#endif
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

InspIRCd* ServerConfig::GetInstance()
{
	return ServerInstance;
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

ValueItem::ValueItem(const char* value)
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




/*
 * XXX should this be in a class? -- w00t
 */
bool InitTypes(ServerConfig* conf, const char*)
{
	if (conf->opertypes.size())
	{
		for (opertype_t::iterator n = conf->opertypes.begin(); n != conf->opertypes.end(); n++)
		{
			if (n->second)
				delete[] n->second;
		}
	}

	conf->opertypes.clear();
	return true;
}

/*
 * XXX should this be in a class? -- w00t
 */
bool InitClasses(ServerConfig* conf, const char*)
{
	if (conf->operclass.size())
	{
		for (operclass_t::iterator n = conf->operclass.begin(); n != conf->operclass.end(); n++)
		{
			if (n->second.commandlist)
				delete[] n->second.commandlist;
			if (n->second.cmodelist)
				delete[] n->second.cmodelist;
			if (n->second.umodelist)
				delete[] n->second.umodelist;
			if (n->second.privs)
				delete[] n->second.privs;
		}
	}

	conf->operclass.clear();
	return true;
}

/*
 * XXX should this be in a class? -- w00t
 */
bool DoType(ServerConfig* conf, const char*, char**, ValueList &values, int*)
{
	const char* TypeName = values[0].GetString();
	const char* Classes = values[1].GetString();

	conf->opertypes[TypeName] = strnewdup(Classes);
	return true;
}

/*
 * XXX should this be in a class? -- w00t
 */
bool DoClass(ServerConfig* conf, const char* tag, char**, ValueList &values, int*)
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

	conf->operclass[ClassName].commandlist = strnewdup(CommandList);
	conf->operclass[ClassName].umodelist = strnewdup(UModeList);
	conf->operclass[ClassName].cmodelist = strnewdup(CModeList);
	conf->operclass[ClassName].privs = strnewdup(PrivsList);
	return true;
}

/*
 * XXX should this be in a class? -- w00t
 */
bool DoneClassesAndTypes(ServerConfig*, const char*)
{
	return true;
}



bool InitXLine(ServerConfig* conf, const char* tag)
{
	return true;
}

bool DoZLine(ServerConfig* conf, const char* tag, char** entries, ValueList &values, int* types)
{
	const char* reason = values[0].GetString();
	const char* ipmask = values[1].GetString();

	ZLine* zl = new ZLine(conf->GetInstance(), conf->GetInstance()->Time(), 0, "<Config>", reason, ipmask);
	if (!conf->GetInstance()->XLines->AddLine(zl, NULL))
		delete zl;

	return true;
}

bool DoQLine(ServerConfig* conf, const char* tag, char** entries, ValueList &values, int* types)
{
	const char* reason = values[0].GetString();
	const char* nick = values[1].GetString();

	QLine* ql = new QLine(conf->GetInstance(), conf->GetInstance()->Time(), 0, "<Config>", reason, nick);
	if (!conf->GetInstance()->XLines->AddLine(ql, NULL))
		delete ql;

	return true;
}

bool DoKLine(ServerConfig* conf, const char* tag, char** entries, ValueList &values, int* types)
{
	const char* reason = values[0].GetString();
	const char* host = values[1].GetString();

	XLineManager* xlm = conf->GetInstance()->XLines;

	IdentHostPair ih = xlm->IdentSplit(host);

	KLine* kl = new KLine(conf->GetInstance(), conf->GetInstance()->Time(), 0, "<Config>", reason, ih.first.c_str(), ih.second.c_str());
	if (!xlm->AddLine(kl, NULL))
		delete kl;
	return true;
}

bool DoELine(ServerConfig* conf, const char* tag, char** entries, ValueList &values, int* types)
{
	const char* reason = values[0].GetString();
	const char* host = values[1].GetString();

	XLineManager* xlm = conf->GetInstance()->XLines;

	IdentHostPair ih = xlm->IdentSplit(host);

	ELine* el = new ELine(conf->GetInstance(), conf->GetInstance()->Time(), 0, "<Config>", reason, ih.first.c_str(), ih.second.c_str());
	if (!xlm->AddLine(el, NULL))
		delete el;
	return true;
}

// this should probably be moved to configreader, but atm it relies on CheckELines above.
bool DoneELine(ServerConfig* conf, const char* tag)
{
	for (std::vector<User*>::const_iterator u2 = conf->GetInstance()->Users->local_users.begin(); u2 != conf->GetInstance()->Users->local_users.end(); u2++)
	{
		User* u = (User*)(*u2);
		u->exempt = false;
	}

	conf->GetInstance()->XLines->CheckELines();
	return true;
}

void ConfigReaderThread::Run()
{
	ServerInstance->Config->Read(do_bail, TheUserUID);
	done = true;
}
