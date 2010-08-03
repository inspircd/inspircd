/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2010 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *	    the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"
#include <fstream>
#include "xline.h"
#include "exitcodes.h"
#include "commands/cmd_whowas.h"
#include "configparser.h"

ServerConfig::ServerConfig()
{
	WhoWasGroupSize = WhoWasMaxGroups = WhoWasMaxKeep = 0;
	RawLog = NoUserDns = HideBans = HideSplits = UndernetMsgPrefix = NameOnlyModes = false;
	WildcardIPv6 = CycleHosts = InvBypassModes = true;
	dns_timeout = 5;
	MaxTargets = 20;
	NetBufferSize = 10240;
	SoftLimit = ServerInstance->SE->GetMaxFds();
	MaxConn = SOMAXCONN;
	MaxChans = 20;
	OperMaxChans = 30;
	c_ipv4_range = 32;
	c_ipv6_range = 128;
}

void ServerConfig::Update005()
{
	std::stringstream out(data005);
	std::vector<std::string> data;
	std::string token;
	while (out >> token)
		data.push_back(token);
	sort(data.begin(), data.end());

	std::string line5;
	isupport.clear();
	for(unsigned int i=0; i < data.size(); i++)
	{
		token = data[i];
		line5 = line5 + token + " ";
		if (i % 13 == 12)
		{
			line5.append(":are supported by this server");
			isupport.push_back(line5);
			line5.clear();
		}
	}
	if (!line5.empty())
	{
		line5.append(":are supported by this server");
		isupport.push_back(line5);
	}
}

void ServerConfig::Send005(User* user)
{
	for (std::vector<std::string>::iterator line = ServerInstance->Config->isupport.begin(); line != ServerInstance->Config->isupport.end(); line++)
		user->WriteNumeric(RPL_ISUPPORT, "%s %s", user->nick.c_str(), line->c_str());
}

template<typename T, typename V>
static void range(T& value, V min, V max, V def, const char* msg)
{
	if (value >= (T)min && value <= (T)max)
		return;
	ServerInstance->Logs->Log("CONFIG", DEFAULT,
		"WARNING: %s value of %ld is not between %ld and %ld; set to %ld.",
		msg, (long)value, (long)min, (long)max, (long)def);
	value = def;
}


static void ValidIP(const std::string& ip, const std::string& key)
{
	irc::sockets::sockaddrs dummy;
	if (!irc::sockets::aptosa(ip, 0, dummy))
		throw CoreException("The value of "+key+" is not an IP address");
}

static void ValidHost(const std::string& p, const std::string& msg)
{
	int num_dots = 0;
	if (p.empty() || p[0] == '.')
		throw CoreException("The value of "+msg+" is not a valid hostname");
	for (unsigned int i=0;i < p.length();i++)
	{
		switch (p[i])
		{
			case ' ':
				throw CoreException("The value of "+msg+" is not a valid hostname");
			case '.':
				num_dots++;
			break;
		}
	}
	if (num_dots == 0)
		throw CoreException("The value of "+msg+" is not a valid hostname");
}

void ServerConfig::ApplyDisabled()
{
	std::stringstream dcmds(ConfValue("disabled")->getString("commands", ""));
	std::stringstream dmodes(ConfValue("disabled")->getString("modes", ""));
	std::string word;

	/* Enable everything first */
	for (Commandtable::iterator x = ServerInstance->Parser->cmdlist.begin(); x != ServerInstance->Parser->cmdlist.end(); x++)
		x->second->Disable(false);

	for (ModeIDIter id; id; id++)
	{
		ModeHandler* mh = ServerInstance->Modes->FindMode(id);
		if (mh)
			mh->disabled = false;
	}

	/* Now disable all the ones which the user wants disabled */
	while (dcmds >> word)
	{
		Commandtable::iterator cm = ServerInstance->Parser->cmdlist.find(word);
		if (cm != ServerInstance->Parser->cmdlist.end())
		{
			cm->second->Disable(true);
		}
	}

	while (dmodes >> word)
	{
		ModeHandler* mh = ServerInstance->Modes->FindMode(word);
		if (mh)
			mh->disabled = true;
	}

	for (const unsigned char* p = (const unsigned char*)ConfValue("disabled")->getString("usermodes").c_str(); *p; ++p)
	{
		ModeHandler* mh = ServerInstance->Modes->FindMode(*p, MODETYPE_USER);
		if (mh)
			mh->disabled = true;
	}

	for (const unsigned char* p = (const unsigned char*)ConfValue("disabled")->getString("chanmodes").c_str(); *p; ++p)
	{
		ModeHandler* mh = ServerInstance->Modes->FindMode(*p, MODETYPE_CHANNEL);
		if (mh)
			mh->disabled = true;
	}
}

#ifdef WINDOWS
// Note: the windows validator is in win32wrapper.cpp
void FindDNS(std::string& server);
#else
static void FindDNS(std::string& server)
{
	if (!server.empty())
		return;

	// attempt to look up their nameserver from /etc/resolv.conf
	ServerInstance->Logs->Log("CONFIG",DEFAULT,"WARNING: <dns:server> not defined, attempting to find working server in /etc/resolv.conf...");

	std::ifstream resolv("/etc/resolv.conf");

	while (resolv >> server)
	{
		if (server == "nameserver")
		{
			resolv >> server;
			if (server.find_first_not_of("0123456789.") == std::string::npos)
			{
				ServerInstance->Logs->Log("CONFIG",DEFAULT,"<dns:server> set to '%s' as first resolver in /etc/resolv.conf.",server.c_str());
				return;
			}
		}
	}

	ServerInstance->Logs->Log("CONFIG",DEFAULT,"/etc/resolv.conf contains no viable nameserver entries! Defaulting to nameserver '127.0.0.1'!");
	server = "127.0.0.1";
}
#endif

static void ReadXLine(ServerConfig* conf, const std::string& tag, const std::string& key, XLineFactory* make)
{
	ConfigTagList tags = conf->ConfTags(tag);
	for(ConfigIter i = tags.first; i != tags.second; ++i)
	{
		ConfigTag* ctag = i->second;
		std::string mask;
		if (!ctag->readString(key, mask))
			throw CoreException("<"+tag+":"+key+"> missing at " + ctag->getTagLocation());
		std::string reason = ctag->getString("reason", "<Config>");
		XLine* xl = make->Generate(ServerInstance->Time(), 0, "<Config>", reason, mask);
		if (!ServerInstance->XLines->AddLine(xl, NULL))
			delete xl;
	}
}

void ServerConfig::CrossCheckOperClassType()
{
	ConfigTagList tags = ConfTags("class");
	for(ConfigIter i = tags.first; i != tags.second; ++i)
	{
		ConfigTag* tag = i->second;
		std::string name = tag->getString("name");
		if (name.empty())
			throw CoreException("<class:name> missing from tag at " + tag->getTagLocation());
		if (oper_classes.find(name) != oper_classes.end())
			throw CoreException("Duplicate class block with name " + name + " at " + tag->getTagLocation());
		oper_classes[name] = tag;
	}

	tags = ConfTags("type");
	for(ConfigIter i = tags.first; i != tags.second; ++i)
	{
		ConfigTag* tag = i->second;
		std::string name = tag->getString("name");
		if (name.empty())
			throw CoreException("<type:name> is missing from tag at " + tag->getTagLocation());
		if (!ServerInstance->IsNick(name.c_str(), Limits.NickMax))
			throw CoreException("<type:name> is invalid (value '" + name + "')");
		if (oper_blocks.find(" " + name) != oper_blocks.end())
			throw CoreException("Duplicate type block with name " + name + " at " + tag->getTagLocation());

		oper_blocks[" " + name] = new OperInfo(tag);
	}

	tags = ConfTags("oper");
	for(ConfigIter i = tags.first; i != tags.second; ++i)
	{
		ConfigTag* tag = i->second;

		std::string name = tag->getString("name");
		if (name.empty())
			throw CoreException("<oper:name> missing from tag at " + tag->getTagLocation());

		if (oper_blocks.find(name) != oper_blocks.end())
			throw CoreException("Duplicate oper block with name " + name + " at " + tag->getTagLocation());

		oper_blocks[name] = new OperInfo(tag);
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
			if (c->name.substr(0, 8) != "unnamed-")
			{
				oldBlocksByMask["n" + c->name] = c;
			}
			else if (c->type == CC_ALLOW || c->type == CC_DENY)
			{
				std::string typeMask = (c->type == CC_ALLOW) ? "a" : "d";
				typeMask += c->host;
				oldBlocksByMask[typeMask] = c;
			}
		}
	}

	int blk_count = config_data.count("connect");
	if (blk_count == 0)
	{
		// No connect blocks found; make a trivial default block
		std::vector<KeyVal>* items;
		ConfigTag* tag = ConfigTag::create("connect", "<auto>", 0, items);
		items->push_back(std::make_pair("allow", "*"));
		config_data.insert(std::make_pair("connect", tag));
		blk_count = 1;
	}

	Classes.resize(blk_count);
	std::map<std::string, int> names;

	bool try_again = true;
	for(int tries=0; try_again; tries++)
	{
		try_again = false;
		ConfigTagList tags = ConfTags("connect");
		int i=0;
		for(ConfigIter it = tags.first; it != tags.second; ++it, ++i)
		{
			ConfigTag* tag = it->second;
			if (Classes[i])
				continue;

			ConnectClass* parent = NULL;
			std::string parentName = tag->getString("parent");
			if (!parentName.empty())
			{
				std::map<std::string,int>::iterator parentIter = names.find(parentName);
				if (parentIter == names.end())
				{
					try_again = true;
					// couldn't find parent this time. If it's the last time, we'll never find it.
					if (tries >= blk_count)
						throw CoreException("Could not find parent connect class \"" + parentName + "\" for connect block at " + tag->getTagLocation());
					continue;
				}
				parent = Classes[parentIter->second];
			}

			reference<ConnectClass> me = new ConnectClass(tag, parent);

			std::string typeMask;
			if (me->name.empty())
			{
				me->name = "unnamed-" + ConvToStr(i);
				if (me->type == CC_ALLOW)
					typeMask = 'a' + me->host;
				else
					typeMask = 'd' + me->host;
			}
			else
			{
				typeMask = 'n' + me->name;
			}

			if (names.find(me->name) != names.end())
				throw CoreException("Two connect classes with name \"" + me->name + "\" defined!");
			names[me->name] = i;

			ClassMap::iterator oldMask = oldBlocksByMask.find(typeMask);
			if (oldMask != oldBlocksByMask.end())
			{
				ConnectClass* old = oldMask->second;
				oldBlocksByMask.erase(oldMask);
				old->Update(me);
				me = old;
			}
			Classes[i] = me;
		}
	}
}

/** Represents a deprecated configuration tag.
 */
struct Deprecated
{
	/** Tag name
	 */
	const char* tag;
	/** Tag value
	 */
	const char* value;
	/** Reason for deprecation
	 */
	const char* reason;
};

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
	{"die",     "value",            "you need to reread your config"},
};

void ServerConfig::Fill()
{
	ConfigTag* options = ConfValue("options");
	ConfigTag* security = ConfValue("security");
	diepass = ConfValue("power")->getString("diepass");
	restartpass = ConfValue("power")->getString("restartpass");
	powerhash = ConfValue("power")->getString("hash");
	PrefixQuit = options->getString("prefixquit");
	SuffixQuit = options->getString("suffixquit");
	FixedQuit = options->getString("fixedquit");
	PrefixPart = options->getString("prefixpart");
	SuffixPart = options->getString("suffixpart");
	FixedPart = options->getString("fixedpart");
	SoftLimit = ConfValue("performance")->getInt("softlimit", ServerInstance->SE->GetMaxFds());
	MaxConn = ConfValue("performance")->getInt("somaxconn", SOMAXCONN);
	MoronBanner = options->getString("moronbanner", "You're banned!");
	ServerName = ConfValue("server")->getString("name");
	ServerDesc = ConfValue("server")->getString("description", "Configure Me");
	Network = ConfValue("server")->getString("network", "Network");
	sid = ConfValue("server")->getString("id", "");
	AdminName = ConfValue("admin")->getString("name", "");
	AdminEmail = ConfValue("admin")->getString("email", "null@example.com");
	AdminNick = ConfValue("admin")->getString("nick", "admin");
	ModPath = ConfValue("path")->getString("moduledir", MOD_PATH);
	NetBufferSize = ConfValue("performance")->getInt("netbuffersize", 10240);
	dns_timeout = ConfValue("dns")->getInt("timeout", 5);
	DisabledDontExist = ConfValue("disabled")->getBool("fakenonexistant");
	UserStats = security->getString("userstats");
	CustomVersion = security->getString("customversion", Network + " IRCd");
	HideSplits = security->getBool("hidesplits");
	HideBans = security->getBool("hidebans");
	HideWhoisServer = security->getString("hidewhois");
	HideKillsServer = security->getString("hidekills");
	RestrictBannedUsers = security->getBool("restrictbannedusers", true);
	GenericOper = security->getBool("genericoper");
	NoUserDns = ConfValue("performance")->getBool("nouserdns");
	SyntaxHints = options->getBool("syntaxhints");
	CycleHosts = options->getBool("cyclehosts");
	CycleHostsFromUser = options->getBool("cyclehostsfromuser");
	UndernetMsgPrefix = options->getBool("ircumsgprefix");
	FullHostInTopic = options->getBool("hostintopic");
	MaxTargets = security->getInt("maxtargets", 20);
	DefaultModes = options->getString("defaultmodes", "ont");
	PID = ConfValue("pid")->getString("file");
	WhoWasGroupSize = ConfValue("whowas")->getInt("groupsize");
	WhoWasMaxGroups = ConfValue("whowas")->getInt("maxgroups");
	WhoWasMaxKeep = ServerInstance->Duration(ConfValue("whowas")->getString("maxkeep"));
	MaxChans = ConfValue("channels")->getInt("users", 20);
	OperMaxChans = ConfValue("channels")->getInt("opers", 60);
	c_ipv4_range = ConfValue("cidr")->getInt("ipv4clone", 32);
	c_ipv6_range = ConfValue("cidr")->getInt("ipv6clone", 128);
	Limits.NickMax = ConfValue("limits")->getInt("maxnick", 32);
	Limits.ChanMax = ConfValue("limits")->getInt("maxchan", 64);
	Limits.MaxModes = ConfValue("limits")->getInt("maxmodes", 20);
	Limits.IdentMax = ConfValue("limits")->getInt("maxident", 11);
	Limits.MaxQuit = ConfValue("limits")->getInt("maxquit", 255);
	Limits.MaxTopic = ConfValue("limits")->getInt("maxtopic", 307);
	Limits.MaxKick = ConfValue("limits")->getInt("maxkick", 255);
	Limits.MaxGecos = ConfValue("limits")->getInt("maxgecos", 128);
	Limits.MaxAway = ConfValue("limits")->getInt("maxaway", 200);
	InvBypassModes = options->getBool("invitebypassmodes", true);
	NameOnlyModes = options->getBool("nameonlymodes", true);

	range(SoftLimit, 10, ServerInstance->SE->GetMaxFds(), ServerInstance->SE->GetMaxFds(), "<performance:softlimit>");
	range(MaxConn, 0, SOMAXCONN, SOMAXCONN, "<performance:somaxconn>");
	range(MaxTargets, 1, 31, 20, "<security:maxtargets>");
	range(NetBufferSize, 1024, 65534, 10240, "<performance:netbuffersize>");
	range(WhoWasGroupSize, 0, 10000, 10, "<whowas:groupsize>");
	range(WhoWasMaxGroups, 0, 1000000, 10240, "<whowas:maxgroups>");
	range(WhoWasMaxKeep, 3600, INT_MAX, 3600, "<whowas:maxkeep>");

	ValidIP(DNSServer, "<dns:server>");
	ValidHost(ServerName, "<server:name>");
	if (!sid.empty() && !ServerInstance->IsSID(sid))
		throw CoreException(sid + " is not a valid server ID. A server ID must be 3 characters long, with the first character a digit and the next two characters a digit or letter.");

	std::string defbind = options->getString("defaultbind");
	if (assign(defbind) == "ipv4")
	{
		WildcardIPv6 = false;
	}
	else if (assign(defbind) == "ipv6")
	{
		WildcardIPv6 = true;
	}
	else
	{
		WildcardIPv6 = true;
		int socktest = socket(AF_INET6, SOCK_STREAM, 0);
		if (socktest < 0)
			WildcardIPv6 = false;
		else
			close(socktest);
	}
	ConfigTagList tags = ConfTags("uline");
	for(ConfigIter i = tags.first; i != tags.second; ++i)
	{
		ConfigTag* tag = i->second;
		std::string server;
		if (!tag->readString("server", server))
			throw CoreException("<uline> tag missing server at " + tag->getTagLocation());
		ulines[assign(server)] = tag->getBool("silent");
	}

	tags = ConfTags("banlist");
	for(ConfigIter i = tags.first; i != tags.second; ++i)
	{
		ConfigTag* tag = i->second;
		std::string chan;
		if (!tag->readString("chan", chan))
			throw CoreException("<banlist> tag missing chan at " + tag->getTagLocation());
		maxbans[chan] = tag->getInt("limit");
	}

	ReadXLine(this, "badip", "ipmask", ServerInstance->XLines->GetFactory("Z"));
	ReadXLine(this, "badnick", "nick", ServerInstance->XLines->GetFactory("Q"));
	ReadXLine(this, "badhost", "host", ServerInstance->XLines->GetFactory("K"));
	ReadXLine(this, "exception", "host", ServerInstance->XLines->GetFactory("E"));

	memset(HideModeLists, 0, sizeof(HideModeLists));
	for (const unsigned char* p = (const unsigned char*)ConfValue("security")->getString("hidemodes").c_str(); *p; ++p)
		HideModeLists[*p] = true;

	std::string v = security->getString("announceinvites");

	if (v == "ops")
		AnnounceInvites = ServerConfig::INVITE_ANNOUNCE_OPS;
	else if (v == "all")
		AnnounceInvites = ServerConfig::INVITE_ANNOUNCE_ALL;
	else if (v == "dynamic")
		AnnounceInvites = ServerConfig::INVITE_ANNOUNCE_DYNAMIC;
	else
		AnnounceInvites = ServerConfig::INVITE_ANNOUNCE_NONE;

	v = security->getString("operspywhois");
	if (v == "splitmsg")
		OperSpyWhois = SPYWHOIS_SPLITMSG;
	else if (v == "on" || v == "yes")
		OperSpyWhois = SPYWHOIS_NEWLINE;
	else
		OperSpyWhois = SPYWHOIS_NONE;

	Limits.Finalise();
}

// WARNING: it is not safe to use most of the codebase in this function, as it
// will run in the config reader thread
void ServerConfig::Read()
{
	/* Load and parse the config file, if there are any errors then explode */

	ParseStack stack(this);
	try
	{
		valid = stack.ParseFile(ServerInstance->ConfigFileName, 0);
	}
	catch (CoreException& err)
	{
		valid = false;
		errstr << err.GetReason();
	}
	if (valid)
	{
		DNSServer = ConfValue("dns")->getString("server");
		FindDNS(DNSServer);
	}
}

void ServerConfig::Apply(ServerConfig* old, const std::string &useruid)
{
	valid = true;

	/* The stuff in here may throw CoreException, be sure we're in a position to catch it. */
	try
	{
		for (int Index = 0; Index * sizeof(Deprecated) < sizeof(ChangedConfig); Index++)
		{
			std::string dummy;
			if (ConfValue(ChangedConfig[Index].tag)->readString(ChangedConfig[Index].value, dummy, true))
				errstr << "Your configuration contains a deprecated value: <"
					<< ChangedConfig[Index].tag << ":" << ChangedConfig[Index].value << "> - " << ChangedConfig[Index].reason
					<< " (at " << ConfValue(ChangedConfig[Index].tag)->getTagLocation() << ")\n";
		}

		Fill();

		// Handle special items
		CrossCheckOperClassType();
		CrossCheckConnectBlocks(old);
	}
	catch (CoreException &ce)
	{
		errstr << ce.GetReason();
	}

	// write once here, to try it out and make sure its ok
	ServerInstance->WritePID(this->PID);

	// Check errors before dealing with failed binds, since continuing on failed bind is wanted in some circumstances.
	valid = errstr.str().empty();

	/*
	 * These values can only be set on boot. Keep their old values. Do it before we send messages so we actually have a servername.
	 */
	if (old)
	{
		this->ServerName = old->ServerName;
		this->sid = old->sid;
		this->cmdline = old->cmdline;

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

	if (!valid)
		ServerInstance->Logs->Log("CONFIG",DEFAULT, "There were errors in your configuration file:");

	while (errstr.good())
	{
		std::string line;
		getline(errstr, line, '\n');
		if (line.empty())
			continue;
		// On startup, print out to console (still attached at this point)
		if (!old)
			printf("%s\n", line.c_str());
		// If a user is rehashing, tell them directly
		if (user)
			user->SendText(":%s NOTICE %s :*** %s", ServerInstance->Config->ServerName.c_str(), user->nick.c_str(), line.c_str());
		// Also tell opers
		ServerInstance->SNO->WriteGlobalSno('a', line);
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

	if (user)
		user->SendText(":%s NOTICE %s :*** Successfully rehashed server.",
			ServerInstance->Config->ServerName.c_str(), user->nick.c_str());
	ServerInstance->SNO->WriteGlobalSno('a', "*** Successfully rehashed server.");
}

void ServerConfig::ApplyModules(User* user)
{
	Module* whowas = ServerInstance->Modules->Find("cmd_whowas.so");
	if (whowas)
		WhowasRequest(NULL, whowas, WhowasRequest::WHOWAS_PRUNE).Send();

	const std::vector<std::string> v = ServerInstance->Modules->GetAllModuleNames(0);
	std::vector<std::string> added_modules;
	std::set<std::string> removed_modules(v.begin(), v.end());

	ConfigTagList tags = ConfTags("module");
	for(ConfigIter i = tags.first; i != tags.second; ++i)
	{
		ConfigTag* tag = i->second;
		std::string name;
		if (tag->readString("name", name))
		{
			// if this module is already loaded, the erase will succeed, so we need do nothing
			// otherwise, we need to add the module (which will be done later)
			if (removed_modules.erase(name) == 0)
				added_modules.push_back(name);
		}
	}

	if (ConfValue("options")->getBool("allowhalfop") && removed_modules.erase("m_halfop.so") == 0)
		added_modules.push_back("m_halfop.so");

	for (std::set<std::string>::iterator removing = removed_modules.begin(); removing != removed_modules.end(); removing++)
	{
		// Don't remove cmd_*.so, just remove m_*.so
		if (removing->c_str()[0] == 'c')
			continue;
		Module* m = ServerInstance->Modules->Find(*removing);
		if (m && ServerInstance->Modules->Unload(m))
		{
			ServerInstance->SNO->WriteGlobalSno('a', "*** REHASH UNLOADED MODULE: %s",removing->c_str());

			if (user)
				user->WriteNumeric(RPL_UNLOADEDMODULE, "%s %s :Module %s successfully unloaded.",user->nick.c_str(), removing->c_str(), removing->c_str());
			else
				ServerInstance->SNO->WriteGlobalSno('a', "Module %s successfully unloaded.", removing->c_str());
		}
		else
		{
			if (user)
				user->WriteNumeric(ERR_CANTUNLOADMODULE, "%s %s :Failed to unload module %s: %s",user->nick.c_str(), removing->c_str(), removing->c_str(), ServerInstance->Modules->LastError().c_str());
			else
				 ServerInstance->SNO->WriteGlobalSno('a', "Failed to unload module %s: %s", removing->c_str(), ServerInstance->Modules->LastError().c_str());
		}
	}

	for (std::vector<std::string>::iterator adding = added_modules.begin(); adding != added_modules.end(); adding++)
	{
		if (ServerInstance->Modules->Load(adding->c_str()))
		{
			ServerInstance->SNO->WriteGlobalSno('a', "*** REHASH LOADED MODULE: %s",adding->c_str());
			if (user)
				user->WriteNumeric(RPL_LOADEDMODULE, "%s %s :Module %s successfully loaded.",user->nick.c_str(), adding->c_str(), adding->c_str());
			else
				ServerInstance->SNO->WriteGlobalSno('a', "Module %s successfully loaded.", adding->c_str());
		}
		else
		{
			if (user)
				user->WriteNumeric(ERR_CANTLOADMODULE, "%s %s :Failed to load module %s: %s",user->nick.c_str(), adding->c_str(), adding->c_str(), ServerInstance->Modules->LastError().c_str());
			else
				ServerInstance->SNO->WriteGlobalSno('a', "Failed to load module %s: %s", adding->c_str(), ServerInstance->Modules->LastError().c_str());
		}
	}
}

bool ServerConfig::StartsWithWindowsDriveLetter(const std::string &path)
{
	return (path.length() > 2 && isalpha(path[0]) && path[1] == ':');
}

ConfigTag* ServerConfig::ConfValue(const std::string &tag)
{
	ConfigTagList found = config_data.equal_range(tag);
	if (found.first == found.second)
		return NULL;
	ConfigTag* rv = found.first->second;
	found.first++;
	if (found.first != found.second)
		ServerInstance->Logs->Log("CONFIG",DEFAULT, "Multiple <" + tag + "> tags found; only first will be used "
			"(first at " + rv->getTagLocation() + "; second at " + found.first->second->getTagLocation() + ")");
	return rv;
}

ConfigTagList ServerConfig::ConfTags(const std::string& tag)
{
	return config_data.equal_range(tag);
}

bool ServerConfig::FileExists(const char* file)
{
	struct stat sb;
	if (stat(file, &sb) == -1)
		return false;

	if ((sb.st_mode & S_IFDIR) > 0)
		return false;

	FILE *input = fopen(file, "r");
	if (input == NULL)
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

std::string ServerConfig::GetSID()
{
	return sid;
}

void ConfigReaderThread::Run()
{
	Config->Read();
	done = true;
}

void ConfigReaderThread::Finish()
{
	ServerConfig* old = ServerInstance->Config;
	ServerInstance->Logs->Log("CONFIG",DEBUG,"Switching to new configuration...");
	ServerInstance->Config = this->Config;
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
		Config->ApplyDisabled();
		User* user = ServerInstance->FindNick(TheUserUID);
		FOREACH_MOD(I_OnRehash, OnRehash(user));
		ServerInstance->BuildISupport();

		ServerInstance->Logs->CloseLogs();
		ServerInstance->Logs->OpenFileLogs();

		if (Config->RawLog && !old->RawLog)
			ServerInstance->Users->ServerNoticeAll("*** Raw I/O logging is enabled on this server. All messages, passwords, and commands are being recorded.");

		Config = old;
	}
	else
	{
		// whoops, abort!
		ServerInstance->Config = old;
	}
}
