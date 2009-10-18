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

struct fpos
{
	std::string filename;
	int line;
	int col;
	fpos(const std::string& name, int l = 1, int c = 1) : filename(name), line(l), col(c) {}
	std::string str()
	{
		return filename + ":" + ConvToStr(line) + ":" + ConvToStr(col);
	}
};

enum ParseFlags
{
	FLAG_NO_EXEC = 1,
	FLAG_NO_INC = 2
};

struct ParseStack
{
	std::vector<std::string> reading;
	std::map<std::string, std::string> vars;
	ConfigDataHash& output;
	std::stringstream& errstr;

	ParseStack(ServerConfig* conf)
		: output(conf->config_data), errstr(conf->errstr)
	{
		vars["amp"] = "&";
		vars["quot"] = "\"";
		vars["newline"] = vars["nl"] = "\n";
	}
	bool ParseFile(const std::string& name, int flags);
	bool ParseExec(const std::string& name, int flags);
	void DoInclude(ConfigTag* includeTag, int flags);
};

struct Parser
{
	ParseStack& stack;
	const int flags;
	FILE* const file;
	fpos current;
	fpos last_tag;
	reference<ConfigTag> tag;
	int ungot;

	Parser(ParseStack& me, int myflags, FILE* conf, const std::string& name)
		: stack(me), flags(myflags), file(conf), current(name), last_tag(name), ungot(-1)
	{ }

	int next(bool eof_ok = false)
	{
		if (ungot != -1)
		{
			int ch = ungot;
			ungot = -1;
			return ch;
		}
		int ch = fgetc(file);
		if (ch == EOF && !eof_ok)
		{
			throw CoreException("Unexpected end-of-file");
		}
		else if (ch == '\n')
		{
			current.line++;
			current.col = 0;
		}
		else
		{
			current.col++;
		}
		return ch;
	}

	void unget(int ch)
	{
		if (ungot != -1)
			throw CoreException("INTERNAL ERROR: cannot unget twice");
		ungot = ch;
	}

	void comment()
	{
		while (1)
		{
			int ch = next();
			if (ch == '\n')
				return;
		}
	}

	void nextword(std::string& rv)
	{
		int ch = next();
		while (isspace(ch))
			ch = next();
		while (isalnum(ch) || ch == '_')
		{
			rv.push_back(ch);
			ch = next();
		}
		unget(ch);
	}

	bool kv()
	{
		std::string key;
		nextword(key);
		int ch = next();
		if (ch == '>' && key.empty())
		{
			return false;
		}
		else if (ch == '#' && key.empty())
		{
			comment();
			return true;
		}
		else if (ch != '=')
		{
			throw CoreException("Invalid character " + std::string(1, ch) + " in key (" + key + ")");
		}

		std::string value;
		ch = next();
		if (ch != '"')
		{
			throw CoreException("Invalid character in value of <" + tag->tag + ":" + key + ">");
		}
		while (1)
		{
			ch = next();
			if (ch == '&')
			{
				std::string varname;
				while (1)
				{
					ch = next();
					if (isalnum(ch))
						varname.push_back(ch);
					else if (ch == ';')
						break;
					else
					{
						stack.errstr << "Invalid XML entity name in value of <" + tag->tag + ":" + key + ">\n"
							<< "To include an ampersand or quote, use &amp; or &quot;\n";
						throw CoreException("Parse error");
					}
				}
				std::map<std::string, std::string>::iterator var = stack.vars.find(varname);
				if (var == stack.vars.end())
					throw CoreException("Undefined XML entity reference '&" + varname + ";'");
				value.append(var->second);
			}
			else if (ch == '"')
				break;
			value.push_back(ch);
		}
		tag->items.push_back(KeyVal(key, value));
		return true;
	}

	void dotag()
	{
		last_tag = current;
		std::string name;
		nextword(name);

		int spc = next();
		if (!isspace(spc))
			throw CoreException("Invalid character in tag name");

		if (name.empty())
			throw CoreException("Empty tag name");

		tag = new ConfigTag(name, current.filename, current.line);

		while (kv());

		if (name == "include")
		{
			stack.DoInclude(tag, flags);
		}
		else if (name == "define")
		{
			std::string varname = tag->getString("name");
			std::string value = tag->getString("value");
			if (varname.empty())
				throw CoreException("Variable definition must include variable name");
			stack.vars[varname] = value;
		}
		else
		{
			stack.output.insert(std::make_pair(name, tag));
		}
		// this is not a leak; reference<> takes care of the delete
		tag = NULL;
	}

	bool outer_parse()
	{
		try
		{
			while (1)
			{
				int ch = next(true);
				switch (ch)
				{
					case EOF:
						// this is the one place where an EOF is not an error
						return true;
					case '#':
						comment();
						break;
					case '<':
						dotag();
						break;
					case ' ':
					case '\r':
					case '\t':
					case '\n':
						break;
					case 0xFE:
					case 0xFF:
						stack.errstr << "Do not save your files as UTF-16; use ASCII!\n";
					default:
						throw CoreException("Syntax error - start of tag expected");
				}
			}
		}
		catch (CoreException& err)
		{
			stack.errstr << err.GetReason() << " at " << current.str();
			if (tag)
				stack.errstr << " (inside tag " << tag->tag << " at line " << tag->src_line << ")\n";
			else
				stack.errstr << " (last tag was on line " << last_tag.line << ")\n";
		}
		return false;
	}
};

void ParseStack::DoInclude(ConfigTag* tag, int flags)
{
	if (flags & FLAG_NO_INC)
		throw CoreException("Invalid <include> tag in file included with noinclude=\"yes\"");
	std::string name;
	if (tag->readString("file", name))
	{
		if (tag->getBool("noinclude", false))
			flags |= FLAG_NO_INC;
		if (tag->getBool("noexec", false))
			flags |= FLAG_NO_EXEC;
		if (!ParseFile(name, flags))
			throw CoreException("Included");
	}
	else if (tag->readString("executable", name))
	{
		if (flags & FLAG_NO_EXEC)
			throw CoreException("Invalid <include:executable> tag in file included with noexec=\"yes\"");
		if (tag->getBool("noinclude", false))
			flags |= FLAG_NO_INC;
		if (tag->getBool("noexec", true))
			flags |= FLAG_NO_EXEC;
		if (!ParseExec(name, flags))
			throw CoreException("Included");
	}
}

bool ParseStack::ParseFile(const std::string& name, int flags)
{
	ServerInstance->Logs->Log("CONFIG", DEBUG, "Reading file %s", name.c_str());
	for (unsigned int t = 0; t < reading.size(); t++)
	{
		if (std::string(name) == reading[t])
		{
			throw CoreException("File " + name + " is included recursively (looped inclusion)");
		}
	}

	/* It's not already included, add it to the list of files we've loaded */

	FILE* file = fopen(name.c_str(), "r");
	if (!file)
		throw CoreException("Could not read \"" + name + "\" for include");

	reading.push_back(name);
	Parser p(*this, flags, file, name);
	bool ok = p.outer_parse();
	reading.pop_back();
	return ok;
}

bool ParseStack::ParseExec(const std::string& name, int flags)
{
	ServerInstance->Logs->Log("CONFIG", DEBUG, "Reading executable %s", name.c_str());
	for (unsigned int t = 0; t < reading.size(); t++)
	{
		if (std::string(name) == reading[t])
		{
			throw CoreException("Executable " + name + " is included recursively (looped inclusion)");
		}
	}

	/* It's not already included, add it to the list of files we've loaded */

	FILE* file = popen(name.c_str(), "r");
	if (!file)
		throw CoreException("Could not open executable \"" + name + "\" for include");

	reading.push_back(name);
	Parser p(*this, flags, file, name);
	bool ok = p.outer_parse();
	reading.pop_back();
	return ok;
}

/////////////////////////////////////////////////////////////////////////////

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

static void ReqRead(ServerConfig* src, const std::string& tag, const std::string& key, std::string& dest)
{
	ConfigTag* t = src->ConfValue(tag);
	if (!t || !t->readString(key, dest))
		throw CoreException("You must specify a value for <" + tag + ":" + key + ">");
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


/* NOTE: Before anyone asks why we're not using inet_pton for this, it is because inet_pton and friends do not return so much detail,
 * even in strerror(errno). They just return 'yes' or 'no' to an address without such detail as to whats WRONG with the address.
 * Because ircd users arent as technical as they used to be (;)) we are going to give more of a useful error message.
 */
static void ValidIP(const std::string& ip, const std::string& key)
{
	const char* p = ip.c_str();
	int num_dots = 0;
	int num_seps = 0;
	int not_numbers = false;
	int not_hex = false;

	if (*p)
	{
		if (*p == '.')
			throw CoreException("The value of "+key+" is not an IP address");

		for (const char* ptr = p; *ptr; ++ptr)
		{
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
					throw CoreException("The value of "+key+" is not an IP address");
				case '.':
					num_dots++;
				break;
				case ':':
					num_seps++;
				break;
			}
		}

		if (num_dots > 3)
			throw CoreException("The value of "+key+" is an IPv4 address with too many fields!");

		if (num_seps > 8)
			throw CoreException("The value of "+key+" is an IPv6 address with too many fields!");

		if (num_seps == 0 && num_dots < 3)
			throw CoreException("The value of "+key+" looks to be a malformed IPv4 address");

		if (num_seps == 0 && num_dots == 3 && not_numbers)
			throw CoreException("The value of "+key+" contains non-numeric characters in an IPv4 address");

		if (num_seps != 0 && not_hex)
			throw CoreException("The value of "+key+" contains non-hexdecimal characters in an IPv6 address");

		if (num_seps != 0 && num_dots != 3 && num_dots != 0)
			throw CoreException("The value of "+key+" is a malformed IPv6 4in6 address");
	}
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

// Specialized validators

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
			ServerInstance->Logs->Log("CONFIG",DEFAULT,"<dns:server> set to '%s' as first resolver in /etc/resolv.conf.",server.c_str());
			return;
		}
	}

	ServerInstance->Logs->Log("CONFIG",DEFAULT,"/etc/resolv.conf contains no viable nameserver entries! Defaulting to nameserver '127.0.0.1'!");
	server = "127.0.0.1";
}
#endif

static void ReadXLine(ServerConfig* conf, const std::string& tag, const std::string& key, XLineFactory* make)
{
	for(int i=0;; ++i)
	{
		ConfigTag* ctag = conf->ConfValue(tag, i);
		if (!ctag)
			break;
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
	for (int i = 0;; ++i)
	{
		ConfigTag* tag = ConfValue("class", i);
		if (!tag)
			break;
		std::string name = tag->getString("name");
		if (name.empty())
			throw CoreException("<class:name> missing from tag at " + tag->getTagLocation());
		operclass[name] = tag;
	}
	for (int i = 0;; ++i)
	{
		ConfigTag* tag = ConfValue("type", i);
		if (!tag)
			break;

		std::string name = tag->getString("name");
		if (name.empty())
			throw CoreException("<type:name> is missing from tag at " + tag->getTagLocation());
		opertypes[name] = tag;

		std::string classname;
		irc::spacesepstream str(tag->getString("classes"));
		while (str.GetToken(classname))
		{
			if (operclass.find(classname) == operclass.end())
				throw CoreException("Oper type " + name + " has missing class " + classname);
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

	ClassMap newBlocksByMask;
	std::map<std::string, int> names;

	bool try_again = true;
	for(int tries=0; try_again; tries++)
	{
		try_again = false;
		for(unsigned int i=0;; i++)
		{
			ConfigTag* tag = ConfValue("connect", i);
			if (!tag)
				break;
			if (Classes.size() <= i)
				Classes.resize(i+1);
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
					if (tries == 50)
						throw CoreException("Could not find parent connect class \"" + parentName + "\" for connect block " + ConvToStr(i));
					continue;
				}
				parent = Classes[parentIter->second];
			}

			std::string name = tag->getString("name");
			if (!name.empty())
			{
				if (names.find(name) != names.end())
					throw CoreException("Two connect classes with name \"" + name + "\" defined!");
				names[name] = i;
			}

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
			else
			{
				throw CoreException("Connect class must have an allow or deny mask at " + tag->getTagLocation());
			}
			ClassMap::iterator dupMask = newBlocksByMask.find(typeMask);
			if (dupMask != newBlocksByMask.end())
				throw CoreException("Two connect classes cannot have the same mask (" + mask + ")");

			ConnectClass* me = parent ? 
				new ConnectClass(tag, type, mask, *parent) :
				new ConnectClass(tag, type, mask);

			if (!name.empty())
				me->name = name;

			tag->readString("password", me->pass);
			tag->readString("hash", me->hash);
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
			me->maxlocal = tag->getInt("localmax", me->maxlocal);
			me->maxglobal = tag->getInt("globalmax", me->maxglobal);
			me->port = tag->getInt("port", me->port);
			me->maxchans = tag->getInt("maxchans", me->maxchans);
			me->limit = tag->getInt("limit", me->limit);

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
	{"die",     "value",            "has always been deprecated"},
};

void ServerConfig::Fill()
{
	ReqRead(this, "server", "name", ServerName);
	ReqRead(this, "power", "diepass", diepass);
	ReqRead(this, "power", "restartpass", restartpass);

	ConfigTag* options = ConfValue("options");
	ConfigTag* security = ConfValue("security");
	powerhash = ConfValue("power")->getString("hash");
	DieDelay = ConfValue("power")->getInt("pause");
	PrefixQuit = options->getString("prefixquit");
	SuffixQuit = options->getString("suffixquit");
	FixedQuit = options->getString("fixedquit");
	PrefixPart = options->getString("prefixpart");
	SuffixPart = options->getString("suffixpart");
	FixedPart = options->getString("fixedpart");
	SoftLimit = ConfValue("performance")->getInt("softlimit", ServerInstance->SE->GetMaxFds());
	MaxConn = ConfValue("performance")->getInt("somaxconn", SOMAXCONN);
	MoronBanner = options->getString("moronbanner", "You're banned!");
	ServerDesc = ConfValue("server")->getString("description", "Configure Me");
	Network = ConfValue("server")->getString("network", "Network");
	sid = ConfValue("server")->getString("id", "");
	AdminName = ConfValue("admin")->getString("name", "");
	AdminEmail = ConfValue("admin")->getString("email", "null@example.com");
	AdminNick = ConfValue("admin")->getString("nick", "admin");
	ModPath = options->getString("moduledir", MOD_PATH);
	NetBufferSize = ConfValue("performance")->getInt("netbuffersize", 10240);
	MaxWhoResults = ConfValue("performance")->getInt("maxwho", 1024);
	dns_timeout = ConfValue("dns")->getInt("timeout", 5);
	DisabledCommands = ConfValue("disabled")->getString("commands", "");
	DisabledDontExist = ConfValue("disabled")->getBool("fakenonexistant");
	SetUser = security->getString("runasuser");
	SetGroup = security->getString("runasgroup");
	UserStats = security->getString("userstats");
	CustomVersion = security->getString("customversion");
	HideSplits = security->getBool("hidesplits");
	HideBans = security->getBool("hidebans");
	HideWhoisServer = security->getString("hidewhois");
	HideKillsServer = security->getString("hidekills");
	OperSpyWhois = security->getBool("operspywhois");
	RestrictBannedUsers = security->getBool("restrictbannedusers", true);
	GenericOper = security->getBool("genericoper");
	NoUserDns = ConfValue("performance")->getBool("nouserdns");
	SyntaxHints = options->getBool("syntaxhints");
	CycleHosts = options->getBool("cyclehosts");
	UndernetMsgPrefix = options->getBool("ircumsgprefix");
	FullHostInTopic = options->getBool("hostintopic");
	MaxTargets = security->getInt("maxtargets", 20);
	DefaultModes = options->getString("defaultmodes", "nt");
	PID = ConfValue("pid")->getString("file");
	WhoWasGroupSize = ConfValue("whowas")->getInt("groupsize");
	WhoWasMaxGroups = ConfValue("whowas")->getInt("maxgroups");
	WhoWasMaxKeep = ServerInstance->Duration(ConfValue("whowas")->getString("maxkeep"));
	DieValue = ConfValue("die")->getString("value");
	MaxChans = ConfValue("channels")->getInt("users");
	OperMaxChans = ConfValue("channels")->getInt("opers");
	c_ipv4_range = ConfValue("cidr")->getInt("ipv4clone");
	c_ipv6_range = ConfValue("cidr")->getInt("ipv6clone");
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

	range(SoftLimit, 10, ServerInstance->SE->GetMaxFds(), ServerInstance->SE->GetMaxFds(), "<performance:softlimit>");
	range(MaxConn, 0, SOMAXCONN, SOMAXCONN, "<performance:somaxconn>");
	range(MaxTargets, 1, 31, 20, "<security:maxtargets>");
	range(NetBufferSize, 1024, 65534, 10240, "<performance:netbuffersize>");
	range(MaxWhoResults, 1, 65535, 1024, "<performace:maxwho>");
	range(WhoWasGroupSize, 0, 10000, 10, "<whowas:groupsize>");
	range(WhoWasMaxGroups, 0, 1000000, 10240, "<whowas:maxgroups>");
	range(WhoWasMaxKeep, 3600, INT_MAX, 3600, "<whowas:maxkeep>");

	ValidIP(DNSServer, "<dns:server>");
	ValidHost(ServerName, "<server:name>");
	if (!sid.empty() && !ServerInstance->IsSID(sid))
		throw CoreException(sid + " is not a valid server ID. A server ID must be 3 characters long, with the first character a digit and the next two characters a digit or letter.");

	for (int i = 0;; ++i)
	{
		ConfigTag* tag = ConfValue("uline", i);
		if (!tag)
			break;
		std::string server;
		if (!tag->readString("server", server))
			throw CoreException("<uline> tag missing server at " + tag->getTagLocation());
		ulines[assign(server)] = tag->getBool("silent");
	}

	for(int i=0;; ++i)
	{
		ConfigTag* tag = ConfValue("banlist", i);
		if (!tag)
			break;
		std::string chan;
		if (!tag->readString("chan", chan))
			throw CoreException("<banlist> tag missing chan at " + tag->getTagLocation());
		maxbans[chan] = tag->getInt("limit");
	}

	ReadXLine(this, "badip", "ipmask", ServerInstance->XLines->GetFactory("Z"));
	ReadXLine(this, "badnick", "nick", ServerInstance->XLines->GetFactory("Q"));
	ReadXLine(this, "badhost", "host", ServerInstance->XLines->GetFactory("K"));
	ReadXLine(this, "exception", "host", ServerInstance->XLines->GetFactory("E"));

	memset(DisabledUModes, 0, sizeof(DisabledUModes));
	for (const unsigned char* p = (const unsigned char*)ConfValue("disabled")->getString("usermodes").c_str(); *p; ++p)
	{
		if (*p < 'A' || *p > ('A' + 64)) throw CoreException(std::string("Invalid usermode ")+(char)*p+" was found.");
		DisabledUModes[*p - 'A'] = 1;
	}

	memset(DisabledCModes, 0, sizeof(DisabledCModes));
	for (const unsigned char* p = (const unsigned char*)ConfValue("disabled")->getString("chanmodes").c_str(); *p; ++p)
	{
		if (*p < 'A' || *p > ('A' + 64)) throw CoreException(std::string("Invalid chanmode ")+(char)*p+" was found.");
		DisabledCModes[*p - 'A'] = 1;
	}

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

	Limits.Finalise();
}

/* These tags MUST occur and must ONLY occur once in the config file */
static const char* const Once[] = { "server", "admin", "files", "power", "options" };

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
		ReadFile(MOTD, ConfValue("files")->getString("motd"));
		ReadFile(RULES, ConfValue("files")->getString("rules"));
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
		/* Check we dont have more than one of singular tags, or any of them missing
		 */
		for (int Index = 0; Index * sizeof(*Once) < sizeof(Once); Index++)
		{
			std::string tag = Once[Index];
			if (!ConfValue(tag))
				throw CoreException("You have not defined a <"+tag+"> tag, this is required.");
			if (ConfValue(tag, 1))
			{
				errstr << "You have more than one <" << tag << "> tag.\n"
					<< "First occurrence at " << ConfValue(tag, 0)->getTagLocation()
					<< "; second occurrence at " << ConfValue(tag, 1)->getTagLocation() << std::endl;
			}
		}

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

		if (ConfValue("options")->getBool("allowhalfop"))
			ServerInstance->Modes->AddMode(new ModeChannelHalfOp);

		return;
	}

	// If there were errors processing configuration, don't touch modules.
	if (!valid)
		return;

	ApplyModules(user);

	if (user)
		user->WriteServ("NOTICE %s :*** Successfully rehashed server.", user->nick.c_str());
	ServerInstance->SNO->WriteGlobalSno('a', "*** Successfully rehashed server.");
}

void ServerConfig::ApplyModules(User* user)
{
	bool AllowHalfOp = ConfValue("options")->getBool("allowhalfop");
	ModeHandler* mh = ServerInstance->Modes->FindMode('h', MODETYPE_CHANNEL);
	if (AllowHalfOp && !mh) {
		ServerInstance->Logs->Log("CONFIG", DEFAULT, "Enabling halfop mode.");
		mh = new ModeChannelHalfOp;
		ServerInstance->Modes->AddMode(mh);
	} else if (!AllowHalfOp && mh) {
		ServerInstance->Logs->Log("CONFIG", DEFAULT, "Disabling halfop mode.");
		ServerInstance->Modes->DelMode(mh);
		delete mh;
	}

	Module* whowas = ServerInstance->Modules->Find("cmd_whowas.so");
	if (whowas)
		WhowasRequest(NULL, whowas, WhowasRequest::WHOWAS_PRUNE).Send();

	const std::vector<std::string> v = ServerInstance->Modules->GetAllModuleNames(0);
	std::vector<std::string> added_modules;
	std::set<std::string> removed_modules(v.begin(), v.end());

	for(int i=0; ; i++)
	{
		ConfigTag* tag = ConfValue("module", i);
		if (!tag)
			break;
		std::string name;
		if (tag->readString("name", name))
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

ConfigTag* ServerConfig::ConfValue(const std::string &tag, int offset)
{
	ConfigDataHash::size_type pos = offset;
	if (pos >= config_data.count(tag))
		return NULL;

	ConfigDataHash::iterator iter = config_data.find(tag);

	for(int i = 0; i < offset; i++)
		iter++;

	return iter->second;
}

bool ConfigTag::readString(const std::string& key, std::string& value, bool allow_lf)
{
	if (!this)
		return false;
	for(std::vector<KeyVal>::iterator j = items.begin(); j != items.end(); ++j)
	{
		if(j->first != key)
			continue;
		value = j->second;
 		if (!allow_lf && (value.find('\n') != std::string::npos))
		{
			ServerInstance->Logs->Log("CONFIG",DEFAULT, "Value of <" + tag + ":" + key + "> at " + getTagLocation() +
				" contains a linefeed, and linefeeds in this value are not permitted -- stripped to spaces.");
			for (std::string::iterator n = value.begin(); n != value.end(); n++)
				if (*n == '\n')
					*n = ' ';
		}
		return true;
	}
	return false;
}

std::string ConfigTag::getString(const std::string& key, const std::string& def)
{
	std::string res = def;
	readString(key, res);
	return res;
}

long ConfigTag::getInt(const std::string &key, long def)
{
	std::string result;
	if(!readString(key, result))
		return def;

	const char* res_cstr = result.c_str();
	char* res_tail = NULL;
	long res = strtol(res_cstr, &res_tail, 0);
	if (res_tail == res_cstr)
		return def;
	switch (toupper(*res_tail))
	{
		case 'K':
			res= res* 1024;
			break;
		case 'M':
			res= res* 1024 * 1024;
			break;
		case 'G':
			res= res* 1024 * 1024 * 1024;
			break;
	}
	return res;
}

double ConfigTag::getFloat(const std::string &key, double def)
{
	std::string result;
	if (!readString(key, result))
		return def;
	return strtod(result.c_str(), NULL);
}

bool ConfigTag::getBool(const std::string &key, bool def)
{
	std::string result;
	if(!readString(key, result))
		return def;

	return (result == "yes" || result == "true" || result == "1" || result == "on");
}

std::string ConfigTag::getTagLocation()
{
	return src_name + ":" + ConvToStr(src_line);
}

/** Read the contents of a file located by `fname' into a file_cache pointed at by `F'.
 */
bool ServerConfig::ReadFile(file_cache &F, const std::string& fname)
{
	if (fname.empty())
		return false;

	FILE* file = NULL;
	char linebuf[MAXBUF];

	F.clear();

	if (!FileExists(fname.c_str()))
		return false;
	file = fopen(fname.c_str(), "r");

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
