/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2019 Matt Schatz <genius3000@g3k.solutions>
 *   Copyright (C) 2013-2016 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2013-2014, 2016-2022 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2013 Daniel Vassdal <shutter@canternet.org>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2012 Justin Crawford <Justasic@Gmail.com>
 *   Copyright (C) 2012 DjSlash <djslash@djslash.org>
 *   Copyright (C) 2012 ChrisTX <xpipe@hotmail.de>
 *   Copyright (C) 2009-2011 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2008 Thomas Stagner <aquanight@inspircd.org>
 *   Copyright (C) 2007-2010 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2006-2008 Craig Edwards <brain@inspircd.org>
 *   Copyright (C) 2006 Oliver Lupton <om@inspircd.org>
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


#include <filesystem>
#ifndef _WIN32
# include <unistd.h>
#endif

#include "inspircd.h"
#include "configparser.h"
#include "utility/string.h"

ServerConfig::ReadResult::ReadResult(const std::string& c, const std::string& e)
	: contents(c)
	, error(e)
{
}

ServerConfig::ServerLimits::ServerLimits(const std::shared_ptr<ConfigTag>& tag)
	: MaxLine(tag->getNum<size_t>("maxline", 512, 512))
	, MaxNick(tag->getNum<size_t>("maxnick", 30, 1, MaxLine))
	, MaxChannel(tag->getNum<size_t>("maxchan", 60, 1, MaxLine))
	, MaxModes(tag->getNum<size_t>("maxmodes", 20, 1))
	, MaxUser(tag->getNum<size_t>("maxuser", tag->getNum<size_t>("maxident", 10, 1, MaxLine), 1, MaxLine))
	, MaxQuit(tag->getNum<size_t>("maxquit", 300, 0, MaxLine))
	, MaxTopic(tag->getNum<size_t>("maxtopic", 330, 1, MaxLine))
	, MaxKick(tag->getNum<size_t>("maxkick", 300, 1, MaxLine))
	, MaxReal(tag->getNum<size_t>("maxreal", 130, 1, MaxLine))
	, MaxAway(tag->getNum<size_t>("maxaway", 200, 1, MaxLine))
	, MaxHost(tag->getNum<size_t>("maxhost", 64, 1, MaxLine))
{
}

ServerConfig::ServerPaths::ServerPaths(const std::shared_ptr<ConfigTag>& tag)
	: Config(tag->getString("configdir", INSPIRCD_CONFIG_PATH, 1))
	, Data(tag->getString("datadir", INSPIRCD_DATA_PATH, 1))
	, Log(tag->getString("logdir", INSPIRCD_LOG_PATH, 1))
	, Module(tag->getString("moduledir", INSPIRCD_MODULE_PATH, 1))
	, Runtime(tag->getString("runtimedir", INSPIRCD_RUNTIME_PATH, 1))
{
}

std::string ServerConfig::ServerPaths::ExpandPath(const std::string& base, const std::string& fragment)
{
	// The fragment is an absolute path, don't modify it.
	if (std::filesystem::path(fragment).is_absolute())
		return fragment;

	// The fragment is relative to a home directory, expand that.
	if (!fragment.compare(0, 2, "~/", 2))
	{
		const char* homedir = getenv("HOME");
		if (homedir && *homedir)
			return std::string(homedir) + '/' + fragment.substr(2);
	}

	return base + '/' + fragment;
}

ServerConfig::ServerConfig()
	: EmptyTag(std::make_shared<ConfigTag>("empty", FilePosition("<auto>", 0, 0)))
	, Limits(EmptyTag)
	, Paths(EmptyTag)
{
}

ServerConfig::ReadResult ServerConfig::ReadFile(const std::string& file, time_t mincache)
{
	auto contents = filecontents.find(file);
	if (contents != filecontents.end())
	{
		if (!mincache || contents->second.second >= mincache)
			return ReadResult(contents->second.first, {});
		filecontents.erase(contents);
	}

	bool executable = false;
	std::string name = file;
	std::string path = file;

	// If the caller specified a short name (e.g. <file motd="motd.txt">) then look it up.
	auto source = filesources.find(file);
	if (source != filesources.end())
	{
		name = source->first;
		path = source->second.first;
		executable = source->second.second;
	}

	// Try to open the file and error out if it fails.
	auto fh = ParseStack::DoOpenFile(path, executable);
	if (!fh)
		return ReadResult({}, strerror(errno));

	std::stringstream datastream;
	char databuf[4096];
	while (fgets(databuf, sizeof(databuf), fh.get()))
	{
		size_t len = strlen(databuf);
		if (len)
			datastream.write(databuf, len);
	}

	filecontents[name] = { datastream.str(), ServerInstance->Time() };
	return ReadResult(filecontents[name].first, {});
}

void ServerConfig::CrossCheckOperBlocks()
{
	std::unordered_map<std::string, std::shared_ptr<ConfigTag>> operclass;
	for (const auto& [_, tag] : ConfTags("class"))
	{
		const std::string name = tag->getString("name");
		if (name.empty())
			throw CoreException("<class:name> missing from tag at " + tag->source.str());

		if (!operclass.emplace(name, tag).second)
			throw CoreException("Duplicate class block with name " + name + " at " + tag->source.str());
	}

	for (const auto& [_, tag] : ConfTags("type"))
	{
		const std::string name = tag->getString("name");
		if (name.empty())
			throw CoreException("<type:name> is missing from tag at " + tag->source.str());

		auto type = std::make_shared<OperType>(name, nullptr);

		// Copy the settings from the oper class.
		irc::spacesepstream classlist(tag->getString("classes"));
		for (std::string classname; classlist.GetToken(classname); )
		{
			auto klass = operclass.find(classname);
			if (klass == operclass.end())
				throw CoreException("Oper type " + name + " has missing class " + classname + " at " + tag->source.str());

			// Apply the settings from the class.
			type->Configure(klass->second, false);
		}

		// Once the classes have been applied we can apply this.
		type->Configure(tag, true);

		if (!OperTypes.emplace(name, type).second)
			throw CoreException("Duplicate type block with name " + name + " at " + tag->source.str());
	}

	for (const auto& [_, tag] : ConfTags("oper"))
	{
		const auto name = tag->getString("name");
		if (name.empty())
			throw CoreException("<oper:name> missing from tag at " + tag->source.str());

		const auto typestr = tag->getString("type");
		if (typestr.empty())
			throw CoreException("<oper:type> missing from tag at " + tag->source.str());

		if (tag->getString("password").empty() && !tag->getBool("nopassword"))
			throw CoreException("<oper:password> missing from tag at " + tag->source.str());

		const auto type = OperTypes.find(typestr);
		if (type == OperTypes.end())
			throw CoreException("Oper block " + name + " has missing type " + typestr + " at " + tag->source.str());

		auto account = std::make_shared<OperAccount>(name, type->second, tag);
		if (!OperAccounts.emplace(name, account).second)
			throw CoreException("Duplicate oper block with name " + name + " at " + tag->source.str());
	}
}

void ServerConfig::CrossCheckConnectBlocks(ServerConfig* current)
{
	typedef std::map<std::pair<std::string, ConnectClass::Type>, std::shared_ptr<ConnectClass>> ClassMap;
	ClassMap oldBlocksByMask;
	if (current)
	{
		for (const auto& c : current->Classes)
		{
			switch (c->type)
			{
				case ConnectClass::ALLOW:
				case ConnectClass::DENY:
					oldBlocksByMask[std::make_pair(insp::join(c->GetHosts()), c->type)] = c;
					break;

				case ConnectClass::NAMED:
					oldBlocksByMask[std::make_pair(c->GetName(), c->type)] = c;
					break;
			}
		}
	}

	size_t blk_count = config_data.count("connect");
	if (blk_count == 0)
	{
		// No connect blocks found; make a trivial default block
		auto tag = std::make_shared<ConfigTag>("connect", FilePosition("<auto>", 0, 0));
		tag->GetItems()["allow"] = "*";
		config_data.emplace("connect", tag);
		blk_count = 1;
	}

	Classes.resize(blk_count);
	std::map<std::string, size_t> names;

	bool try_again = true;
	for(size_t tries = 0; try_again; tries++)
	{
		try_again = false;
		size_t i = 0;
		for (const auto& [_, tag] : ConfTags("connect"))
		{
			if (Classes[i])
			{
				i++;
				continue;
			}

			std::shared_ptr<ConnectClass> parent;
			std::string parentName = tag->getString("parent");
			if (!parentName.empty())
			{
				std::map<std::string, size_t>::const_iterator parentIter = names.find(parentName);
				if (parentIter == names.end())
				{
					try_again = true;
					// couldn't find parent this time. If it's the last time, we'll never find it.
					if (tries >= blk_count)
						throw CoreException("Could not find parent connect class \"" + parentName + "\" for connect block at " + tag->source.str());

					i++;
					continue;
				}
				parent = Classes[parentIter->second];
			}

			std::string name = tag->getString("name");
			std::string mask;
			ConnectClass::Type type;

			if (tag->readString("allow", mask, false) && !mask.empty())
				type = ConnectClass::ALLOW;
			else if (tag->readString("deny", mask, false) && !mask.empty())
				type = ConnectClass::DENY;
			else if (!name.empty())
				type = ConnectClass::NAMED;
			else
				throw CoreException("Connect class must have allow, deny, or name specified at " + tag->source.str());

			if (name.empty())
				name = "unnamed-" + ConvToStr(i);

			if (names.find(name) != names.end())
				throw CoreException("Two connect classes with name \"" + name + "\" defined!");
			names[name] = i;

			std::vector<std::string> masks;
			irc::spacesepstream maskstream(mask);
			for (std::string maskentry; maskstream.GetToken(maskentry); )
				masks.push_back(maskentry);

			auto me = parent
				? std::make_shared<ConnectClass>(tag, type, masks, parent)
				: std::make_shared<ConnectClass>(tag, type, masks);

			me->Configure(name, tag);

			ClassMap::iterator oldMask = oldBlocksByMask.find(std::make_pair(mask, me->type));
			if (oldMask != oldBlocksByMask.end())
			{
				std::shared_ptr<ConnectClass> old = oldMask->second;
				oldBlocksByMask.erase(oldMask);
				old->Update(me);
				me = old;
			}
			Classes[i] = me;
			i++;
		}
	}
}

namespace
{
	// Attempts to find something to use as a default server hostname.
	std::string GetServerHost()
	{
		char hostname[256];
		if (gethostname(hostname, sizeof(hostname)) == 0)
		{
			std::string name(hostname);
			if (name.find('.') == std::string::npos)
				name.append(".local");

			if (name.length() <= ServerInstance->Config->Limits.MaxHost && InspIRCd::IsFQDN(name))
				return name;
		}
		return "irc.example.com";
	}

	// Checks whether the system can create IPv6 sockets.
	bool CanCreateIPv6Socket()
	{
		int fd = socket(AF_INET6, SOCK_STREAM, 0);
		if (fd < 0)
			return false;

		SocketEngine::Close(fd);
		return true;
	}
}

void ServerConfig::Fill()
{
	// Read the <server> config.
	const auto& server = ConfValue("server");
	if (ServerId.empty())
	{
		ServerName = server->getString("name", GetServerHost(), InspIRCd::IsFQDN);

		ServerId = server->getString("id");
		if (!ServerId.empty() && !InspIRCd::IsSID(ServerId))
			throw CoreException(ServerId + " is not a valid server ID. A server ID must be 3 characters long, with the first character a digit and the next two characters a digit or letter.");
	}
	else
	{
		if (server->getString("name", ServerName, 1) != ServerName)
			throw CoreException("You must restart to change the server name!");

		if (server->getString("id", ServerId, 1) != ServerId)
			throw CoreException("You must restart to change the server id!");
	}
	ServerDesc = server->getString("description", ServerName, 1);
	Network = server->getString("network", ServerName, 1);


	// Read the <options> config.
	const auto& options = ConfValue("options");
	DefaultModes = options->getString("defaultmodes", "not");
	MaskInList = options->getBool("maskinlist");
	MaskInTopic = options->getBool("maskintopic", options->getBool("hostintopic"));
	NoSnoticeStack = options->getBool("nosnoticestack");
	SyntaxHints = options->getBool("syntaxhints");
	XLineMessage = options->getString("xlinemessage", "You're banned!", 1);
	XLineQuit = options->getString("xlinequit", "%fulltype%: %reason%", 1);
	RestrictBannedUsers = options->getEnum("restrictbannedusers", ServerConfig::BUT_RESTRICT_NOTIFY, {
		{ "no",     ServerConfig::BUT_NORMAL          },
		{ "silent", ServerConfig::BUT_RESTRICT_SILENT },
		{ "yes",    ServerConfig::BUT_RESTRICT_NOTIFY },
	});
	WildcardIPv6 = options->getEnum("defaultbind", CanCreateIPv6Socket(), {
		{ "auto", CanCreateIPv6Socket() },
		{ "ipv4", false                 },
		{ "ipv6", true                  },
	});

	// Read the <performance> config.
	const auto& performance = ConfValue("performance");
	MaxConn = performance->getNum<int>("somaxconn", SOMAXCONN, 1);
	NetBufferSize = performance->getNum<size_t>("netbuffersize", 10240, 1024, 65534);
	SoftLimit = performance->getNum<size_t>("softlimit", (SocketEngine::GetMaxFds() > 0 ? SocketEngine::GetMaxFds() : SIZE_MAX), 10);
	TimeSkipWarn = performance->getDuration("timeskipwarn", 2, 0, 30);

	// Read the <security> config.
	const auto& security = ConfValue("security");
	CustomVersion = security->getString("customversion");
	HideServer = security->getString("hideserver", {}, InspIRCd::IsFQDN);
	MaxTargets = security->getNum<size_t>("maxtargets", 5, 1, 50);
	XLineQuitPublic = security->getString("publicxlinequit", security->getBool("hidebans") ? "%fulltype%" : "");

	// Read the <cidr> config.
	const auto& cidr = ConfValue("cidr");
	IPv4Range = cidr->getNum<unsigned char>("ipv4clone", 32, 1, 32);
	IPv6Range = cidr->getNum<unsigned char>("ipv6clone", 128, 1, 128);

	// Read any left over config tags.
	Limits = ServerLimits(ConfValue("limits"));
	Paths = ServerPaths(ConfValue("path"));
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
	catch (const CoreException& err)
	{
		valid = false;
		errstr << err.GetReason() << std::endl;
	}
}

void ServerConfig::Apply(ServerConfig* old, const std::string& useruid)
{
	valid = true;
	if (old)
	{
		/*
		 * These values can only be set on boot. Keep their old values. Do it before we send messages so we actually have a servername.
		 */
		this->CaseMapping = old->CaseMapping;
		this->CommandLine = old->CommandLine;
		this->ServerId = old->ServerId;
		this->ServerName = old->ServerName;
	}

	/* The stuff in here may throw CoreException, be sure we're in a position to catch it. */
	try
	{
		// Ensure the user has actually edited ther config.
		auto dietags = ConfTags("die");
		if (!dietags.empty())
		{
			errstr << "Your configuration has not been edited correctly!" << std::endl;
			for (const auto& [_, tag] : dietags)
			{
				const std::string reason = tag->getString("reason", "You left a <die> tag in your config", 1);
				errstr << reason <<  " (at " << tag->source.str() << ")" << std::endl;
			}
		}

		// Reject the broken configs that outdated tutorials keep pushing.
		if (!ConfValue("power")->getString("pause").empty())
		{
			errstr << "You appear to be using a config file from an ancient outdated tutorial!" << std::endl
				<< "This will almost certainly not work. You should instead create a config" << std::endl
				<< "file using the examples shipped with InspIRCd or by referring to the" << std::endl
				<< "docs available at " INSPIRCD_DOCS "configuration." << std::endl;
		}

		Fill();

		// Handle special items
		CrossCheckOperBlocks();
		CrossCheckConnectBlocks(old);
	}
	catch (const CoreException& ce)
	{
		errstr << ce.GetReason() << std::endl;
	}

	// Check errors before dealing with failed binds, since continuing on failed bind is wanted in some circumstances.
	valid = errstr.str().empty();

	auto binds = ConfTags("bind");
	if (binds.empty())
		errstr << "Possible configuration error: you have not defined any <bind> blocks." << std::endl
			<< "You will need to do this if you want clients to be able to connect!" << std::endl;

	if (old && valid)
	{
		// On first run, ports are bound later on
		FailedPortList pl;
		ServerInstance->BindPorts(pl);
		if (!pl.empty())
		{
			errstr << "Warning! Some of your listener" << (pl.size() == 1 ? "s" : "") << " failed to bind:" << std::endl;
			for (const auto& fp : pl)
			{
				if (fp.sa.family() != AF_UNSPEC)
					errstr << "  " << fp.sa.str() << ": ";

				errstr << fp.error << std::endl << "  " << "Created from <bind> tag at " << fp.tag->source.str() << std::endl;
			}
		}
	}

	auto* user = ServerInstance->Users.FindUUID(useruid);

	if (!valid)
	{
		ServerInstance->Logs.Normal("CONFIG", "There were errors in your configuration file:");
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
			fmt::println(line);

		// If a user is rehashing, tell them directly
		if (user)
			user->WriteRemoteNotice(INSP_FORMAT("*** {}", line));
		// Also tell opers
		ServerInstance->SNO.WriteGlobalSno('r', line);
	}

	errstr.clear();
	errstr.str(std::string());

	/* No old configuration -> initial boot, nothing more to do here */
	if (!old)
	{
		if (!valid)
		{
			ServerInstance->Exit(EXIT_FAILURE);
		}

		return;
	}

	// If there were errors processing configuration, don't touch modules.
	if (!valid)
		return;

	ApplyModules(user);

	if (user)
		user->WriteRemoteNotice("*** Successfully rehashed server.");
	ServerInstance->SNO.WriteGlobalSno('r', "*** Successfully rehashed server.");
}

void ServerConfig::ApplyModules(User* user) const
{
	std::vector<std::string> added_modules;
	ModuleManager::ModuleMap removed_modules = ServerInstance->Modules.GetModules();

	for (const auto& module : GetModules())
	{
		const std::string name = ModuleManager::ExpandModName(module);

		// if this module is already loaded, the erase will succeed, so we need do nothing
		// otherwise, we need to add the module (which will be done later)
		if (removed_modules.erase(name) == 0)
			added_modules.push_back(name);
	}

	for (const auto& [modname, mod] : removed_modules)
	{
		// Don't remove core_*, just remove m_*
		if (InspIRCd::Match(modname, "core_*", ascii_case_insensitive_map))
			continue;

		if (ServerInstance->Modules.Unload(mod))
		{
			const std::string message = INSP_FORMAT("The {} module was unloaded.", modname);
			if (user)
				user->WriteNumeric(RPL_UNLOADEDMODULE, modname, message);

			ServerInstance->SNO.WriteGlobalSno('r', message);
		}
		else
		{
			const std::string message = INSP_FORMAT("Failed to unload the {} module: {}", modname, ServerInstance->Modules.LastError());
			if (user)
				user->WriteNumeric(ERR_CANTUNLOADMODULE, modname, message);

			ServerInstance->SNO.WriteGlobalSno('r', message);
		}
	}

	for (const auto& modname : added_modules)
	{
		// Skip modules which are already loaded.
		if (ServerInstance->Modules.Find(modname))
			continue;

		if (ServerInstance->Modules.Load(modname))
		{
			const std::string message = INSP_FORMAT("The {} module was loaded.", modname);
			if (user)
				user->WriteNumeric(RPL_LOADEDMODULE, modname, message);

			ServerInstance->SNO.WriteGlobalSno('r', message);
		}
		else
		{
			const std::string message = INSP_FORMAT("Failed to load the {} module: {}", modname, ServerInstance->Modules.LastError());
			if (user)
				user->WriteNumeric(ERR_CANTLOADMODULE, modname, message);

			ServerInstance->SNO.WriteGlobalSno('r', message);
		}
	}
}

const std::shared_ptr<ConfigTag>& ServerConfig::ConfValue(const std::string& tag, const std::shared_ptr<ConfigTag>& def) const
{
	auto tags = insp::equal_range(config_data, tag);
	if (tags.empty())
		return def ? def : EmptyTag;

	if (tags.count() > 1)
	{
		ServerInstance->Logs.Warning("CONFIG", "Multiple ({}) <{}> tags found; only the first will be used (first at {}, last at {})",
			tags.count(), tag, tags.begin()->second->source.str(), std::prev(tags.end())->second->source.str());
	}
	return tags.begin()->second;
}

ServerConfig::TagList ServerConfig::ConfTags(const std::string& tag, std::optional<TagList> def) const
{
	auto range = insp::equal_range(config_data, tag);
	return range.empty() && def ? *def : range;
}

std::string ServerConfig::Escape(const std::string& str)
{
	std::stringstream escaped;
	for (const auto chr : str)
	{
		switch (chr)
		{
			case '"':
				escaped << "&quot;";
				break;

			case '&':
				escaped << "&amp;";
				break;

			default:
				escaped << chr;
				break;
		}
	}
	return escaped.str();
}

std::vector<std::string> ServerConfig::GetModules() const
{
	auto tags = ConfTags("module");
	std::vector<std::string> modules;
	modules.reserve(tags.count());
	for (const auto& [_, tag] : tags)
	{
		const std::string shortname = ModuleManager::ShrinkModName(tag->getString("name"));
		if (shortname.empty())
		{
			ServerInstance->Logs.Warning("CONFIG", "Malformed <module> tag at " + tag->source.str() + "; skipping ...");
			continue;
		}

		// Rewrite the old names of renamed modules.
		if (insp::equalsci(shortname, "cgiirc"))
			modules.push_back("gateway");
		else if (insp::equalsci(shortname, "cloaking"))
		{
			modules.push_back("cloak");
			modules.push_back("cloak_md5");
		}
		else if (insp::equalsci(shortname, "gecosban"))
			modules.push_back("realnameban");
		else if (insp::equalsci(shortname, "helpop"))
		{
			modules.push_back("help");
			modules.push_back("helpmode");
		}
		else if (insp::equalsci(shortname, "mlock"))
			modules.push_back("services");
		else if (insp::equalsci(shortname, "namesx"))
			modules.push_back("multiprefix");
		else if (insp::equalsci(shortname, "regex_pcre2"))
			modules.push_back("regex_pcre");
		else if (insp::equalsci(shortname, "sha256"))
			modules.push_back("sha2");
		else if (insp::equalsci(shortname, "services_account"))
		{
			modules.push_back("account");
			modules.push_back("services");
		}
		else if (insp::equalsci(shortname, "servprotect"))
			modules.push_back("services");
		else if (insp::equalsci(shortname, "svshold"))
			modules.push_back("services");
		else if (insp::equalsci(shortname, "topiclock"))
			modules.push_back("services");
		else
		{
			// No need to rewrite this module name.
			modules.push_back(shortname);
		}
	}
	return modules;
}

void ConfigReaderThread::OnStart()
{
	Config->Read();
	done = true;
}

void ConfigReaderThread::OnStop()
{
	ServerConfig* old = ServerInstance->Config;
	ServerInstance->Logs.Normal("CONFIG", "Switching to new configuration...");
	ServerInstance->Config = this->Config;
	Config->Apply(old, UUID);

	if (Config->valid)
	{
		/*
		 * Apply the changed configuration from the rehash.
		 *
		 * XXX: The order of these is IMPORTANT, do not reorder them without testing
		 * thoroughly!!!
		 */
		ServerInstance->Users.RehashCloneCounts();

		auto* user = ServerInstance->Users.FindUUID(UUID);
		ConfigStatus status(user);

		for (const auto& [modname, mod] : ServerInstance->Modules.GetModules())
		{
			try
			{
				ServerInstance->Logs.Debug("MODULE", "Rehashing " + modname);
				mod->ReadConfig(status);
			}
			catch (const CoreException& modex)
			{
				ServerInstance->Logs.Critical("MODULE", "Unable to read the configuration for {}: {}",
					mod->ModuleFile, modex.what());
				if (user)
					user->WriteNotice(modname + ": " + modex.GetReason());
			}
		}

		// The description of this server may have changed - update it for WHOIS etc.
		ServerInstance->FakeClient->server->description = Config->ServerDesc;

		try
		{
			ServerInstance->Logs.CloseLogs();
			ServerInstance->Logs.OpenLogs(true);
		}
		catch (const CoreException& ex)
		{
			ServerInstance->Logs.Critical("LOG", "Cannot open log files: " + ex.GetReason());
			if (user)
				user->WriteNotice("Cannot open log files: " + ex.GetReason());
		}

		if (Config->RawLog && !old->RawLog)
		{
			for (auto* luser : ServerInstance->Users.GetLocalUsers())
				Log::NotifyRawIO(luser, MessageType::PRIVMSG);
		}

		Config = old;
	}
	else
	{
		// whoops, abort!
		ServerInstance->Config = old;
	}
}
