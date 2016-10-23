/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2007-2009 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2006-2009 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2006-2008 Craig Edwards <craigedwards@brainbox.cc>
 *   Copyright (C) 2008 Thomas Stagner <aquanight@inspircd.org>
 *   Copyright (C) 2006 Oliver Lupton <oliverlupton@gmail.com>
 *
 * This file is part of InspIRCd.  InspIRCd is free software: you can
 * redistribute it and/or modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation, version 2.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */


#include "inspircd.h"
#include "xline.h"
#include "listmode.h"
#include "exitcodes.h"
#include "configparser.h"
#include <iostream>

ServerLimits::ServerLimits(ConfigTag* tag)
	: NickMax(tag->getInt("maxnick", 32))
	, ChanMax(tag->getInt("maxchan", 64))
	, MaxModes(tag->getInt("maxmodes", 20))
	, IdentMax(tag->getInt("maxident", 11))
	, MaxQuit(tag->getInt("maxquit", 255))
	, MaxTopic(tag->getInt("maxtopic", 307))
	, MaxKick(tag->getInt("maxkick", 255))
	, MaxGecos(tag->getInt("maxgecos", 128))
	, MaxAway(tag->getInt("maxaway", 200))
	, MaxLine(tag->getInt("maxline", 512))
	, MaxHost(tag->getInt("maxhost", 64))
{
}

static ConfigTag* CreateEmptyTag()
{
	std::vector<KeyVal>* items;
	return ConfigTag::create("empty", "<auto>", 0, items);
}

ServerConfig::ServerConfig()
	: EmptyTag(CreateEmptyTag())
	, Limits(EmptyTag)
	, NoSnoticeStack(false)
{
	RawLog = HideBans = HideSplits = false;
	WildcardIPv6 = true;
	dns_timeout = 5;
	MaxTargets = 20;
	NetBufferSize = 10240;
	MaxConn = SOMAXCONN;
	MaxChans = 20;
	OperMaxChans = 30;
	c_ipv4_range = 32;
	c_ipv6_range = 128;
}

ServerConfig::~ServerConfig()
{
	delete EmptyTag;
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

bool ServerConfig::ApplyDisabledCommands(const std::string& data)
{
	std::stringstream dcmds(data);
	std::string thiscmd;

	/* Enable everything first */
	const CommandParser::CommandMap& commands = ServerInstance->Parser.GetCommands();
	for (CommandParser::CommandMap::const_iterator x = commands.begin(); x != commands.end(); ++x)
		x->second->Disable(false);

	/* Now disable all the ones which the user wants disabled */
	while (dcmds >> thiscmd)
	{
		Command* handler = ServerInstance->Parser.GetHandler(thiscmd);
		if (handler)
			handler->Disable(true);
	}
	return true;
}

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

typedef std::map<std::string, ConfigTag*> LocalIndex;
void ServerConfig::CrossCheckOperClassType()
{
	LocalIndex operclass;
	ConfigTagList tags = ConfTags("class");
	for(ConfigIter i = tags.first; i != tags.second; ++i)
	{
		ConfigTag* tag = i->second;
		std::string name = tag->getString("name");
		if (name.empty())
			throw CoreException("<class:name> missing from tag at " + tag->getTagLocation());
		if (operclass.find(name) != operclass.end())
			throw CoreException("Duplicate class block with name " + name + " at " + tag->getTagLocation());
		operclass[name] = tag;
	}
	tags = ConfTags("type");
	for(ConfigIter i = tags.first; i != tags.second; ++i)
	{
		ConfigTag* tag = i->second;
		std::string name = tag->getString("name");
		if (name.empty())
			throw CoreException("<type:name> is missing from tag at " + tag->getTagLocation());
		if (OperTypes.find(name) != OperTypes.end())
			throw CoreException("Duplicate type block with name " + name + " at " + tag->getTagLocation());

		OperInfo* ifo = new OperInfo;
		OperTypes[name] = ifo;
		ifo->name = name;
		ifo->type_block = tag;

		std::string classname;
		irc::spacesepstream str(tag->getString("classes"));
		while (str.GetToken(classname))
		{
			LocalIndex::iterator cls = operclass.find(classname);
			if (cls == operclass.end())
				throw CoreException("Oper type " + name + " has missing class " + classname);
			ifo->class_blocks.push_back(cls->second);
		}
	}

	tags = ConfTags("oper");
	for(ConfigIter i = tags.first; i != tags.second; ++i)
	{
		ConfigTag* tag = i->second;

		std::string name = tag->getString("name");
		if (name.empty())
			throw CoreException("<oper:name> missing from tag at " + tag->getTagLocation());

		std::string type = tag->getString("type");
		OperIndex::iterator tblk = OperTypes.find(type);
		if (tblk == OperTypes.end())
			throw CoreException("Oper block " + name + " has missing type " + type);
		if (oper_blocks.find(name) != oper_blocks.end())
			throw CoreException("Duplicate oper block with name " + name + " at " + tag->getTagLocation());

		OperInfo* ifo = new OperInfo;
		ifo->name = type;
		ifo->oper_block = tag;
		ifo->type_block = tblk->second->type_block;
		ifo->class_blocks.assign(tblk->second->class_blocks.begin(), tblk->second->class_blocks.end());
		oper_blocks[name] = ifo;
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
			if (c->name.compare(0, 8, "unnamed-", 8))
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

			std::string name = tag->getString("name");
			std::string mask, typeMask;
			char type;

			if (tag->readString("allow", mask, false))
			{
				type = CC_ALLOW;
				typeMask = 'a' + mask;
			}
			else if (tag->readString("deny", mask, false))
			{
				type = CC_DENY;
				typeMask = 'd' + mask;
			}
			else if (!name.empty())
			{
				type = CC_NAMED;
				mask = name;
				typeMask = 'n' + mask;
			}
			else
			{
				throw CoreException("Connect class must have allow, deny, or name specified at " + tag->getTagLocation());
			}

			if (name.empty())
			{
				name = "unnamed-" + ConvToStr(i);
			}
			else
			{
				typeMask = 'n' + name;
			}

			if (names.find(name) != names.end())
				throw CoreException("Two connect classes with name \"" + name + "\" defined!");
			names[name] = i;

			ConnectClass* me = parent ?
				new ConnectClass(tag, type, mask, *parent) :
				new ConnectClass(tag, type, mask);

			me->name = name;

			me->registration_timeout = tag->getInt("timeout", me->registration_timeout);
			me->pingtime = tag->getInt("pingfreq", me->pingtime);
			std::string sendq;
			if (tag->readString("sendq", sendq))
			{
				// attempt to guess a good hard/soft sendq from a single value
				long value = atol(sendq.c_str());
				if (value > 16384)
					me->softsendqmax = value / 16;
				else
					me->softsendqmax = value;
				me->hardsendqmax = value * 8;
			}
			me->softsendqmax = tag->getInt("softsendq", me->softsendqmax);
			me->hardsendqmax = tag->getInt("hardsendq", me->hardsendqmax);
			me->recvqmax = tag->getInt("recvq", me->recvqmax);
			me->penaltythreshold = tag->getInt("threshold", me->penaltythreshold);
			me->commandrate = tag->getInt("commandrate", me->commandrate);
			me->fakelag = tag->getBool("fakelag", me->fakelag);
			me->maxlocal = tag->getInt("localmax", me->maxlocal);
			me->maxglobal = tag->getInt("globalmax", me->maxglobal);
			me->maxchans = tag->getInt("maxchans", me->maxchans);
			me->maxconnwarn = tag->getBool("maxconnwarn", me->maxconnwarn);
			me->limit = tag->getInt("limit", me->limit);
			me->resolvehostnames = tag->getBool("resolvehostnames", me->resolvehostnames);

			std::string ports = tag->getString("port");
			if (!ports.empty())
			{
				irc::portparser portrange(ports, false);
				while (int port = portrange.GetToken())
					me->ports.insert(port);
			}

			ClassMap::iterator oldMask = oldBlocksByMask.find(typeMask);
			if (oldMask != oldBlocksByMask.end())
			{
				ConnectClass* old = oldMask->second;
				oldBlocksByMask.erase(oldMask);
				old->Update(me);
				delete me;
				me = old;
			}
			Classes[i] = me;
		}
	}
}

/** Represents a deprecated configuration tag.
 */
struct DeprecatedConfig
{
	/** Tag name. */
	std::string tag;

	/** Attribute key. */
	std::string key;

	/** Attribute value. */
	std::string value;

	/** Reason for deprecation. */
	std::string reason;
};

static const DeprecatedConfig ChangedConfig[] = {
	{ "bind",        "transport",   "",                 "has been moved to <bind:ssl> as of 2.0" },
	{ "die",         "value",       "",                 "you need to reread your config" },
	{ "gnutls",      "starttls",    "",                 "has been replaced with m_starttls as of 3.0" },
	{ "link",        "autoconnect", "",                 "2.0+ does not use this attribute - define <autoconnect> tags instead" },
	{ "link",        "transport",   "",                 "has been moved to <link:ssl> as of 2.0" },
	{ "module",      "name",        "m_chanprotect.so", "has been replaced with m_customprefix as of 3.0" },
	{ "module",      "name",        "m_halfop.so",      "has been replaced with m_customprefix as of 3.0" },
	{ "options",     "cyclehosts",  "",                 "has been replaced with m_hostcycle as of 3.0" },
	{ "performance", "nouserdns",   "",                 "has been moved to <connect:resolvehostnames> as of 3.0" }
};

void ServerConfig::Fill()
{
	ConfigTag* options = ConfValue("options");
	ConfigTag* security = ConfValue("security");
	if (sid.empty())
	{
		ServerName = ConfValue("server")->getString("name", "irc.example.com");
		ValidHost(ServerName, "<server:name>");

		sid = ConfValue("server")->getString("id");
		if (!sid.empty() && !InspIRCd::IsSID(sid))
			throw CoreException(sid + " is not a valid server ID. A server ID must be 3 characters long, with the first character a digit and the next two characters a digit or letter.");
	}
	else
	{
		if (ServerName != ConfValue("server")->getString("name"))
			throw CoreException("You must restart to change the server name");

		std::string nsid = ConfValue("server")->getString("id");
		if (!nsid.empty() && nsid != sid)
			throw CoreException("You must restart to change the server id");
	}
	SoftLimit = ConfValue("performance")->getInt("softlimit", (SocketEngine::GetMaxFds() > 0 ? SocketEngine::GetMaxFds() : LONG_MAX), 10);
	CCOnConnect = ConfValue("performance")->getBool("clonesonconnect", true);
	MaxConn = ConfValue("performance")->getInt("somaxconn", SOMAXCONN);
	XLineMessage = options->getString("xlinemessage", options->getString("moronbanner", "You're banned!"));
	ServerDesc = ConfValue("server")->getString("description", "Configure Me");
	Network = ConfValue("server")->getString("network", "Network");
	NetBufferSize = ConfValue("performance")->getInt("netbuffersize", 10240, 1024, 65534);
	dns_timeout = ConfValue("dns")->getInt("timeout", 5);
	DisabledCommands = ConfValue("disabled")->getString("commands", "");
	DisabledDontExist = ConfValue("disabled")->getBool("fakenonexistant");
	UserStats = security->getString("userstats");
	CustomVersion = security->getString("customversion");
	HideSplits = security->getBool("hidesplits");
	HideBans = security->getBool("hidebans");
	HideWhoisServer = security->getString("hidewhois");
	HideKillsServer = security->getString("hidekills");
	HideULineKills = security->getBool("hideulinekills");
	RestrictBannedUsers = security->getBool("restrictbannedusers", true);
	GenericOper = security->getBool("genericoper");
	SyntaxHints = options->getBool("syntaxhints");
	CycleHostsFromUser = options->getBool("cyclehostsfromuser");
	FullHostInTopic = options->getBool("hostintopic");
	MaxTargets = security->getInt("maxtargets", 20, 1, 31);
	DefaultModes = options->getString("defaultmodes", "not");
	PID = ConfValue("pid")->getString("file");
	MaxChans = ConfValue("channels")->getInt("users", 20);
	OperMaxChans = ConfValue("channels")->getInt("opers");
	c_ipv4_range = ConfValue("cidr")->getInt("ipv4clone", 32);
	c_ipv6_range = ConfValue("cidr")->getInt("ipv6clone", 128);
	Limits = ServerLimits(ConfValue("limits"));
	Paths.Config = ConfValue("path")->getString("configdir", INSPIRCD_CONFIG_PATH);
	Paths.Data = ConfValue("path")->getString("datadir", INSPIRCD_DATA_PATH);
	Paths.Log = ConfValue("path")->getString("logdir", INSPIRCD_LOG_PATH);
	Paths.Module = ConfValue("path")->getString("moduledir", INSPIRCD_MODULE_PATH);
	NoSnoticeStack = options->getBool("nosnoticestack", false);

	if (Network.find(' ') != std::string::npos)
		throw CoreException(Network + " is not a valid network name. A network name must not contain spaces.");

	std::string defbind = options->getString("defaultbind");
	if (stdalgo::string::equalsci(defbind, "ipv4"))
	{
		WildcardIPv6 = false;
	}
	else if (stdalgo::string::equalsci(defbind, "ipv6"))
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
			SocketEngine::Close(socktest);
	}

	ReadXLine(this, "badip", "ipmask", ServerInstance->XLines->GetFactory("Z"));
	ReadXLine(this, "badnick", "nick", ServerInstance->XLines->GetFactory("Q"));
	ReadXLine(this, "badhost", "host", ServerInstance->XLines->GetFactory("K"));
	ReadXLine(this, "exception", "host", ServerInstance->XLines->GetFactory("E"));

	memset(DisabledUModes, 0, sizeof(DisabledUModes));
	std::string modes = ConfValue("disabled")->getString("usermodes");
	for (std::string::const_iterator p = modes.begin(); p != modes.end(); ++p)
	{
		// Complain when the character is not a-z or A-Z
		if ((*p < 'A') || (*p > 'z') || ((*p < 'a') && (*p > 'Z')))
			throw CoreException("Invalid usermode " + std::string(1, *p) + " was found.");
		DisabledUModes[*p - 'A'] = 1;
	}

	memset(DisabledCModes, 0, sizeof(DisabledCModes));
	modes = ConfValue("disabled")->getString("chanmodes");
	for (std::string::const_iterator p = modes.begin(); p != modes.end(); ++p)
	{
		if ((*p < 'A') || (*p > 'z') || ((*p < 'a') && (*p > 'Z')))
			throw CoreException("Invalid chanmode " + std::string(1, *p) + " was found.");
		DisabledCModes[*p - 'A'] = 1;
	}

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
		OperSpyWhois = SPYWHOIS_SINGLEMSG;
	else
		OperSpyWhois = SPYWHOIS_NONE;
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
		errstr << err.GetReason() << std::endl;
	}
}

void ServerConfig::Apply(ServerConfig* old, const std::string &useruid)
{
	valid = true;
	if (old)
	{
		/*
		 * These values can only be set on boot. Keep their old values. Do it before we send messages so we actually have a servername.
		 */
		this->ServerName = old->ServerName;
		this->sid = old->sid;
		this->cmdline = old->cmdline;
	}

	/* The stuff in here may throw CoreException, be sure we're in a position to catch it. */
	try
	{
		for (int index = 0; index * sizeof(DeprecatedConfig) < sizeof(ChangedConfig); index++)
		{
			std::string value;
			ConfigTagList tags = ConfTags(ChangedConfig[index].tag);
			for(ConfigIter i = tags.first; i != tags.second; ++i)
			{
				if (i->second->readString(ChangedConfig[index].key, value, true)
					&& (ChangedConfig[index].value.empty() || value == ChangedConfig[index].value))
				{
					errstr << "Your configuration contains a deprecated value: <"  << ChangedConfig[index].tag;
					if (ChangedConfig[index].value.empty())
					{
						errstr << ':' << ChangedConfig[index].key;
					}
					else
					{
						errstr << ' ' << ChangedConfig[index].key << "=\"" << ChangedConfig[index].value << "\"";
					}
					errstr << "> - " << ChangedConfig[index].reason << " (at " << i->second->getTagLocation() << ")" << std::endl;
				}
			}
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

	// Check errors before dealing with failed binds, since continuing on failed bind is wanted in some circumstances.
	valid = errstr.str().empty();

	// write once here, to try it out and make sure its ok
	if (valid)
		ServerInstance->WritePID(this->PID, !old);

	ConfigTagList binds = ConfTags("bind");
	if (binds.first == binds.second)
		 errstr << "Possible configuration error: you have not defined any <bind> blocks." << std::endl
			 << "You will need to do this if you want clients to be able to connect!" << std::endl;

	if (old && valid)
	{
		// On first run, ports are bound later on
		FailedPortList pl;
		ServerInstance->BindPorts(pl);
		if (pl.size())
		{
			errstr << "Not all your client ports could be bound." << std::endl
				<< "The following port(s) failed to bind:" << std::endl;

			int j = 1;
			for (FailedPortList::iterator i = pl.begin(); i != pl.end(); i++, j++)
			{
				errstr << j << ".\tAddress: " << (i->first.empty() ? "<all>" : i->first.c_str()) << "\tReason: "
					<< i->second << std::endl;
			}
		}
	}

	User* user = useruid.empty() ? NULL : ServerInstance->FindNick(useruid);

	if (!valid)
	{
		ServerInstance->Logs->Log("CONFIG", LOG_DEFAULT, "There were errors in your configuration file:");
		Classes.clear();
	}

	while (errstr.good())
	{
		std::string line;
		getline(errstr, line, '\n');
		if (line.empty())
			continue;
		// On startup, print out to console (still attached at this point)
		if (!old)
			std::cout << line << std::endl;
		// If a user is rehashing, tell them directly
		if (user)
			user->WriteRemoteNotice(InspIRCd::Format("*** %s", line.c_str()));
		// Also tell opers
		ServerInstance->SNO->WriteGlobalSno('a', line);
	}

	errstr.clear();
	errstr.str(std::string());

	// Re-parse our MOTD and RULES files for colors -- Justasic
	for (ClassVector::const_iterator it = this->Classes.begin(), it_end = this->Classes.end(); it != it_end; ++it)
	{
		ConfigTag *tag = (*it)->config;
		// Make sure our connection class allows motd colors
		if(!tag->getBool("allowmotdcolors"))
			continue;

		ConfigFileCache::iterator file = this->Files.find(tag->getString("motd", "motd"));
		if (file != this->Files.end())
			InspIRCd::ProcessColors(file->second);
	}

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
		user->WriteRemoteNotice("*** Successfully rehashed server.");
	ServerInstance->SNO->WriteGlobalSno('a', "*** Successfully rehashed server.");
}

void ServerConfig::ApplyModules(User* user)
{
	std::vector<std::string> added_modules;
	ModuleManager::ModuleMap removed_modules = ServerInstance->Modules->GetModules();

	ConfigTagList tags = ConfTags("module");
	for(ConfigIter i = tags.first; i != tags.second; ++i)
	{
		ConfigTag* tag = i->second;
		std::string name;
		if (tag->readString("name", name))
		{
			name = ModuleManager::ExpandModName(name);
			// if this module is already loaded, the erase will succeed, so we need do nothing
			// otherwise, we need to add the module (which will be done later)
			if (removed_modules.erase(name) == 0)
				added_modules.push_back(name);
		}
	}

	for (ModuleManager::ModuleMap::iterator i = removed_modules.begin(); i != removed_modules.end(); ++i)
	{
		const std::string& modname = i->first;
		// Don't remove core_*.so, just remove m_*.so
		if (modname.c_str()[0] == 'c')
			continue;
		if (ServerInstance->Modules->Unload(i->second))
		{
			ServerInstance->SNO->WriteGlobalSno('a', "*** REHASH UNLOADED MODULE: %s", modname.c_str());

			if (user)
				user->WriteNumeric(RPL_UNLOADEDMODULE, modname, InspIRCd::Format("Module %s successfully unloaded.", modname.c_str()));
			else
				ServerInstance->SNO->WriteGlobalSno('a', "Module %s successfully unloaded.", modname.c_str());
		}
		else
		{
			if (user)
				user->WriteNumeric(ERR_CANTUNLOADMODULE, modname, InspIRCd::Format("Failed to unload module %s: %s", modname.c_str(), ServerInstance->Modules->LastError().c_str()));
			else
				ServerInstance->SNO->WriteGlobalSno('a', "Failed to unload module %s: %s", modname.c_str(), ServerInstance->Modules->LastError().c_str());
		}
	}

	for (std::vector<std::string>::iterator adding = added_modules.begin(); adding != added_modules.end(); adding++)
	{
		if (ServerInstance->Modules->Load(*adding))
		{
			ServerInstance->SNO->WriteGlobalSno('a', "*** REHASH LOADED MODULE: %s",adding->c_str());
			if (user)
				user->WriteNumeric(RPL_LOADEDMODULE, *adding, InspIRCd::Format("Module %s successfully loaded.", adding->c_str()));
			else
				ServerInstance->SNO->WriteGlobalSno('a', "Module %s successfully loaded.", adding->c_str());
		}
		else
		{
			if (user)
				user->WriteNumeric(ERR_CANTLOADMODULE, *adding, InspIRCd::Format("Failed to load module %s: %s", adding->c_str(), ServerInstance->Modules->LastError().c_str()));
			else
				ServerInstance->SNO->WriteGlobalSno('a', "Failed to load module %s: %s", adding->c_str(), ServerInstance->Modules->LastError().c_str());
		}
	}
}

ConfigTag* ServerConfig::ConfValue(const std::string &tag)
{
	ConfigTagList found = config_data.equal_range(tag);
	if (found.first == found.second)
		return EmptyTag;
	ConfigTag* rv = found.first->second;
	found.first++;
	if (found.first != found.second)
		ServerInstance->Logs->Log("CONFIG", LOG_DEFAULT, "Multiple <" + tag + "> tags found; only first will be used "
			"(first at " + rv->getTagLocation() + "; second at " + found.first->second->getTagLocation() + ")");
	return rv;
}

ConfigTagList ServerConfig::ConfTags(const std::string& tag)
{
	return config_data.equal_range(tag);
}

std::string ServerConfig::Escape(const std::string& str, bool xml)
{
	std::string escaped;
	for (std::string::const_iterator it = str.begin(); it != str.end(); ++it)
	{
		switch (*it)
		{
			case '"':
				escaped += xml ? "&quot;" : "\"";
				break;
			case '&':
				escaped += xml ? "&amp;" : "&";
				break;
			case '\\':
				escaped += xml ? "\\" : "\\\\";
				break;
			default:
				escaped += *it;
				break;
		}
	}
	return escaped;
}

void ConfigReaderThread::Run()
{
	Config->Read();
	done = true;
}

void ConfigReaderThread::Finish()
{
	ServerConfig* old = ServerInstance->Config;
	ServerInstance->Logs->Log("CONFIG", LOG_DEBUG, "Switching to new configuration...");
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
		ServerInstance->Users.RehashCloneCounts();
		ServerInstance->XLines->CheckELines();
		ServerInstance->XLines->ApplyLines();
		ChanModeReference ban(NULL, "ban");
		static_cast<ListModeBase*>(*ban)->DoRehash();
		Config->ApplyDisabledCommands(Config->DisabledCommands);
		User* user = ServerInstance->FindNick(TheUserUID);

		ConfigStatus status(user);
		const ModuleManager::ModuleMap& mods = ServerInstance->Modules->GetModules();
		for (ModuleManager::ModuleMap::const_iterator i = mods.begin(); i != mods.end(); ++i)
			i->second->ReadConfig(status);

		// The description of this server may have changed - update it for WHOIS etc.
		ServerInstance->FakeClient->server->description = Config->ServerDesc;

		ServerInstance->ISupport.Build();

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
