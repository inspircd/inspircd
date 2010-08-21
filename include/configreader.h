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

#ifndef INSPIRCD_CONFIGREADER
#define INSPIRCD_CONFIGREADER

#include <sstream>
#include <string>
#include <vector>
#include <map>

/** Structure representing a single <tag> in config */
class CoreExport ConfigTag : public refcountbase
{
	std::vector<KeyVal> items;
 public:
	const std::string tag;
	const std::string src_name;
	const int src_line;

	/** Get the value of an option, using def if it does not exist */
	std::string getString(const std::string& key, const std::string& def = "");
	/** Get the value of an option, using def if it does not exist */
	long getInt(const std::string& key, long def = 0);
	/** Get the value of an option, using def if it does not exist */
	double getFloat(const std::string& key, double def = 0);
	/** Get the value of an option, using def if it does not exist */
	bool getBool(const std::string& key, bool def = false);

	/** Get the value of an option
	 * @param key The option to get
	 * @param value The location to store the value (unmodified if does not exist)
	 * @param allow_newline Allow newlines in the option (normally replaced with spaces)
	 * @return true if the option exists
	 */
	bool readString(const std::string& key, std::string& value, bool allow_newline = false);

	std::string getTagLocation();

	inline const std::vector<KeyVal>& getItems() const { return items; }

	/** Create a new ConfigTag, giving access to the private KeyVal item list */
	static ConfigTag* create(const std::string& Tag, const std::string& file, int line,
		std::vector<KeyVal>*&items);
 private:
	ConfigTag(const std::string& Tag, const std::string& file, int line);
};

/** Defines the server's length limits on various length-limited
 * items such as topics, nicknames, channel names etc.
 */
class ServerLimits
{
 public:
	/** Maximum nickname length */
	size_t NickMax;
	/** Maximum channel length */
	size_t ChanMax;
	/** Maximum number of modes per line */
	size_t MaxModes;
	/** Maximum length of ident, not including ~ etc */
	size_t IdentMax;
	/** Maximum length of a quit message */
	size_t MaxQuit;
	/** Maximum topic length */
	size_t MaxTopic;
	/** Maximum kick message length */
	size_t MaxKick;
	/** Maximum GECOS (real name) length */
	size_t MaxGecos;
	/** Maximum away message length */
	size_t MaxAway;

	/** Creating the class initialises it to the defaults
	 * as in 1.1's ./configure script. Reading other values
	 * from the config will change these values.
	 */
	ServerLimits() : NickMax(31), ChanMax(64), MaxModes(20), IdentMax(12), MaxQuit(255), MaxTopic(307), MaxKick(255), MaxGecos(128), MaxAway(200)
	{
	}
};

struct CommandLineConf
{
	/** If this value is true, the owner of the
	 * server specified -nofork on the command
	 * line, causing the daemon to stay in the
	 * foreground.
	 */
	bool nofork;

	/** If this value if true then all log
	 * messages will be output, regardless of
	 * the level given in the config file.
	 * This is set with the -debug commandline
	 * option.
	 */
	bool forcedebug;

	/** If this is true then log output will be
	 * written to the logfile. This is the default.
	 * If you put -nolog on the commandline then
	 * the logfile will not be written.
	 * This is meant to be used in conjunction with
	 * -debug for debugging without filling up the
	 * hard disk.
	 */
	bool writelog;

	/** True if we have been told to run the testsuite from the commandline,
	 * rather than entering the mainloop.
	 */
	bool TestSuite;

	/** Saved argc from startup
	 */
	int argc;

	/** Saved argv from startup
	 */
	char** argv;

	std::string startup_log;
};

class CoreExport OperInfo : public refcountbase
{
 public:
	std::set<std::string> AllowedOperCommands;
	std::set<std::string> AllowedPrivs;

	/** Configuration blocks defined for this oper. This includes <oper>,
	 * <type>, and <class> blocks.
	 */
	std::vector<reference<ConfigTag> > config_blocks;
	/** Name of the oper type; i.e. the one shown in WHOIS */
	std::string name;

	/** Get a configuration item, searching in the oper, type, and class blocks (in that order) */
	std::string getConfig(const std::string& key);
	void init();

	/** Construct an OperInfo object from a <type> or <oper> block */
	OperInfo(ConfigTag* tag);
	/** Construct a dummy OperInfo for a remote user */
	OperInfo(const std::string& Name) : name(Name) {}

	inline const char* NameStr()
	{
		return irc::Spacify(name.c_str());
	}
};

enum RehashReason {
	REHASH_BOOT,
	REHASH_LOAD,
	REHASH_NEWCONF
};

class CoreExport ConfigReadStatus : public interfacebase
{
 public:
	/** Reason for the rehash */
	const RehashReason reason;
	/** Current error list for this rehash */
	std::stringstream errors;
	/** Is this failure fatal? Will block module load or boot, and cautions about this on rehash. If
	 * set prior to switching ServerInstance->Config, will also block rehash.
	 */
	bool fatal;
	ConfigReadStatus(RehashReason Reason) : reason(Reason), fatal(false) {}
	/** Raw error reporting */
	void ReportError(const std::string& msg, bool fatal = true);
	/** Simple error reporting */
	void ReportError(ConfigTag* where, const char* why, bool fatal = true);
	/** Get a single tag, warn on duplicates; return NULL if not found */
	ConfigTag* GetTag(const std::string& key);
	/** Convenience wrapper on ServerInstance->Config->ConfTags */
	ConfigTagList GetTags(const std::string& key);
};

/** This class holds the bulk of the runtime configuration for the ircd.
 * It allows for reading new config values, accessing configuration files,
 * and storage of the configuration data needed to run the ircd, such as
 * the servername, connect classes, /ADMIN data, MOTDs and filenames etc.
 */
class CoreExport ServerConfig
{
  private:
	void CrossCheckOperClassType();
	void CrossCheckConnectBlocks(ServerConfig* current);

 public:
	ServerConfig(RehashReason);

	ConfigReadStatus status;

	inline ConfigTag* GetTag(const std::string& tag) { return status.GetTag(tag); }
	inline ConfigTag* ConfValue(const std::string& tag) { return status.GetTag(tag); }

	ConfigTagList GetTags(const std::string& tag);
	inline ConfigTagList ConfTags(const std::string& tag) { return GetTags(tag); }

	/** Bind to IPv6 by default */
	bool WildcardIPv6;

	/** Used to indicate who we announce invites to on a channel */
	enum InviteAnnounceState { INVITE_ANNOUNCE_NONE, INVITE_ANNOUNCE_ALL, INVITE_ANNOUNCE_OPS, INVITE_ANNOUNCE_DYNAMIC };
	enum OperSpyWhoisState { SPYWHOIS_NONE, SPYWHOIS_NEWLINE, SPYWHOIS_SPLITMSG };

  	/** This holds all the information in the config file,
	 * it's indexed by tag name to a vector of key/values.
	 */
	ConfigDataHash config_data;

	/** This holds all extra files that have been read in the configuration
	 * (for example, MOTD and RULES files are stored here)
	 */
	ConfigFileCache Files;

	/** Length limits, see definition of ServerLimits class
	 */
	ServerLimits Limits;

	/** Configuration parsed from the command line.
	 */
	CommandLineConf cmdline;

	/** Clones CIDR range for ipv4 (0-32)
	 * Defaults to 32 (checks clones on all IPs seperately)
	 */
	int c_ipv4_range;

	/** Clones CIDR range for ipv6 (0-128)
	 * Defaults to 128 (checks on all IPs seperately)
	 */
	int c_ipv6_range;

	/** Max number of WhoWas entries per user.
	 */
	int WhoWasGroupSize;

	/** Max number of cumulative user-entries in WhoWas.
	 *  When max reached and added to, push out oldest entry FIFO style.
	 */
	int WhoWasMaxGroups;

	/** Max seconds a user is kept in WhoWas before being pruned.
	 */
	int WhoWasMaxKeep;

	/** Holds the server name of the local server
	 * as defined by the administrator.
	 */
	std::string ServerName;

	/** Notice to give to users when they are Xlined
	 */
	std::string MoronBanner;

	/* Holds the network name the local server
	 * belongs to. This is an arbitary field defined
	 * by the administrator.
	 */
	std::string Network;

	/** Holds the description of the local server
	 * as defined by the administrator.
	 */
	std::string ServerDesc;

	/** Holds the admin's name, for output in
	 * the /ADMIN command.
	 */
	std::string AdminName;

	/** Holds the email address of the admin,
	 * for output in the /ADMIN command.
	 */
	std::string AdminEmail;

	/** Holds the admin's nickname, for output
	 * in the /ADMIN command
	 */
	std::string AdminNick;

	/** The admin-configured /DIE password
	 */
	std::string diepass;

	/** The admin-configured /RESTART password
	 */
	std::string restartpass;

	/** The hash method for *BOTH* the die and restart passwords.
	 */
	std::string powerhash;

	/** The quit prefix in use, or an empty string
	 */
	std::string PrefixQuit;

	/** The quit suffix in use, or an empty string
	 */
	std::string SuffixQuit;

	/** The fixed quit message in use, or an empty string
	 */
	std::string FixedQuit;

	/** The part prefix in use, or an empty string
	 */
	std::string PrefixPart;

	/** The part suffix in use, or an empty string
	 */
	std::string SuffixPart;

	/** The fixed part message in use, or an empty string
	 */
	std::string FixedPart;

	/** The DNS server to use for DNS queries
	 */
	std::string DNSServer;

	/** Pretend disabled commands don't exist.
	 */
	bool DisabledDontExist;

	/** The full path to the modules directory.
	 * This is either set at compile time, or
	 * overridden in the configuration file via
	 * the <path> tag.
	 */
	std::string ModPath;

	/** If set to true, then all opers on this server are
	 * shown with a generic 'is an IRC operator' line rather
	 * than the oper type. Oper types are still used internally.
	 */
	bool GenericOper;

	/** If this value is true, banned users (+b, not extbans) will not be able to change nick
	 * if banned on any channel, nor to message them.
	 */
	bool RestrictBannedUsers;

	/** If this is set to true, then mode lists (e.g
	 * MODE #chan b) are hidden from unprivileged
	 * users.
	 */
	bool HideModeLists[256];

	/** The number of seconds the DNS subsystem
	 * will wait before timing out any request.
	 */
	int dns_timeout;

	/** The size of the read() buffer in the user
	 * handling code, used to read data into a user's
	 * recvQ.
	 */
	int NetBufferSize;

	/** The value to be used for listen() backlogs
	 * as default.
	 */
	int MaxConn;

	/** The soft limit value assigned to the irc server.
	 * The IRC server will not allow more than this
	 * number of local users.
	 */
	unsigned int SoftLimit;

	/** Maximum number of targets for a multi target command
	 * such as PRIVMSG or KICK
	 */
	unsigned int MaxTargets;

	/** True if we're going to hide netsplits as *.net *.split for non-opers
	 */
	bool HideSplits;

	/** True if we're going to hide ban reasons for non-opers (e.g. G-Lines,
	 * K-Lines, Z-Lines)
	 */
	bool HideBans;

	/** Announce invites to the channel with a server notice
	 */
	InviteAnnounceState AnnounceInvites;

	/** If this is enabled then operators will
	 * see invisible (+i) channels in /whois.
	 */
	OperSpyWhoisState OperSpyWhois;

	/** True if raw I/O is being logged */
	bool RawLog;

	/** Set to a non-empty string to obfuscate the server name of users in WHOIS
	 */
	std::string HideWhoisServer;

	/** Set to a non empty string to obfuscate nicknames prepended to a KILL.
	 */
	std::string HideKillsServer;

	/** The full pathname and filename of the PID
	 * file as defined in the configuration.
	 */
	std::string PID;

	/** The connect classes in use by the IRC server.
	 */
	ClassVector Classes;

	/** The 005 tokens of this server (ISUPPORT)
	 * populated/repopulated upon loading or unloading
	 * modules.
	 */
	std::string data005;

	/** isupport strings
	 */
	std::vector<std::string> isupport;

	/** STATS characters in this list are available
	 * only to operators.
	 */
	std::string UserStats;

	/** Default channel modes
	 */
	std::string DefaultModes;

	/** Custom version string, which if defined can replace the system info in VERSION.
	 */
	std::string CustomVersion;

	/** List of u-lined servers
	 */
	std::map<irc::string, bool> ulines;

	/** Max banlist sizes for channels (the std::string is a glob)
	 */
	std::map<std::string, int> maxbans;

	/** If set to true, no user DNS lookups are to be performed
	 */
	bool NoUserDns;

	/** If set to true, provide syntax hints for unknown commands
	 */
	bool SyntaxHints;

	/** If set to true, users appear to quit then rejoin when their hosts change.
	 * This keeps clients synchronized properly.
	 */
	bool CycleHosts;

	/** If set to true, the CycleHosts mode change will be sourced from the user,
	 * rather than the server
	 */
	bool CycleHostsFromUser;

	/** If set to true, prefixed channel NOTICEs and PRIVMSGs will have the prefix
	 *  added to the outgoing text for undernet style msg prefixing.
	 */
	bool UndernetMsgPrefix;

	/** If set to true, the full nick!user@host will be shown in the TOPIC command
	 * for who set the topic last. If false, only the nick is shown.
	 */
	bool FullHostInTopic;

	/** If this value is true, invites will bypass more than just +i
	 */
	bool InvBypassModes;

	/** If this value is true, modes can have their letters removed (enables +Z)
	 */
	bool NameOnlyModes;

	/** Oper block and type index.
	 * For anonymous oper blocks (type only), prefix with a space.
	 */
	OperIndex oper_blocks;

	ConfigTagIndex oper_classes;

	/** TS6-like server ID.
	 * NOTE: 000...999 are usable for InspIRCd servers. This
	 * makes code simpler. 0AA, 1BB etc with letters are reserved
	 * for services use.
	 */
	std::string sid;

	/** Get server ID as string with required leading zeroes
	 */
	std::string GetSID();

	/** Update the 005 vector
	 */
	void Update005();

	/** Send the 005 numerics (ISUPPORT) to a user
	 */
	void Send005(User* user);

	/** Read the entire configuration into memory
	 * and initialize this class. All other methods
	 * should be used only by the core.
	 */
	void Read();

	/** Apply configuration changes from the old configuration.
	 */
	void Apply(ServerConfig* old, const std::string &useruid);
	void ApplyModules(User* user);

	void Fill();

	/* Returns true if the given string starts with a windows drive letter
	 */
	bool StartsWithWindowsDriveLetter(const std::string &path);

	void ApplyDisabled();

	/** Clean a filename, stripping the directories (and drives) from string.
	 * @param name Directory to tidy
	 * @return The cleaned filename
	 */
	static const char* CleanFilename(const char* name);

	/** Check if a file exists.
	 * @param file The full path to a file
	 * @return True if the file exists and is readable.
	 */
	static bool FileExists(const char* file);
};

/** The background thread for config reading, so that reading from executable includes
 * does not block.
 */
class CoreExport ConfigReaderThread : public Thread
{
	ServerConfig* Config;
	volatile bool done;
 public:
	const std::string TheUserUID;
	ConfigReaderThread(const std::string &useruid)
		: Config(new ServerConfig(REHASH_NEWCONF)), done(false), TheUserUID(useruid)
	{
	}

	virtual ~ConfigReaderThread()
	{
		delete Config;
	}

	void Run();
	/** Run in the main thread to apply the configuration */
	void Finish();
	bool IsDone() { return done; }
};

#endif
