/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2009 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#ifndef INSPIRCD_CONFIGREADER
#define INSPIRCD_CONFIGREADER

/* handy defines */

/** Determines if a channel op is exempt from given mode m,
 * in config of server instance s.
 */
#define CHANOPS_EXEMPT(s, m) (s->Config->ExemptChanOps[(unsigned char)m])

#include <sstream>
#include <string>
#include <vector>
#include <map>
#include "inspircd.h"
#include "modules.h"
#include "socketengine.h"
#include "socket.h"

/* Required forward definitions */
class ServerConfig;
class ServerLimits;
class InspIRCd;
class BufferedSocket;

/** A set of oper types
 */
typedef std::map<irc::string,std::string> opertype_t;

/** Holds an oper class.
 */
struct operclass_data : public classbase
{
	/** Command list for the class
	 */
	std::string commandlist;

	/** Channel mode list for the class
	 */
	std::string cmodelist;

	/** User mode list for the class
	 */
	std::string umodelist;

	/** Priviledges given by this class
	 */
	std::string privs;
};

/** A Set of oper classes
 */
typedef std::map<irc::string, operclass_data> operclass_t;

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

	/** Finalises the settings by adding one. This allows for them to be used as-is
	 * without a 'value+1' when using the std::string assignment methods etc.
	 */
	void Finalise()
	{
		NickMax++;
		ChanMax++;
		IdentMax++;
		MaxQuit++;
		MaxTopic++;
		MaxKick++;
		MaxGecos++;
		MaxAway++;
	}
};

/** This class holds the bulk of the runtime configuration for the ircd.
 * It allows for reading new config values, accessing configuration files,
 * and storage of the configuration data needed to run the ircd, such as
 * the servername, connect classes, /ADMIN data, MOTDs and filenames etc.
 */
class CoreExport ServerConfig : public classbase
{
  private:
	/** Creator/owner pointer
	 */
	InspIRCd* ServerInstance;

	/** This variable holds the names of all
	 * files included from the main one. This
	 * is used to make sure that no files are
	 * recursively included.
	 */
	std::vector<std::string> include_stack;

	/* classes removed by this rehash */
	std::vector<ConnectClass*> removed_classes;

	/** This private method processes one line of
	 * configutation, appending errors to errorstream
	 * and setting error if an error has occured.
	 */
	bool ParseLine(const std::string &filename, std::string &line, long &linenumber);

	/** Check that there is only one of each configuration item
	 */
	bool CheckOnce(const char* tag);

	void CrossCheckOperClassType();
	void CrossCheckConnectBlocks(ServerConfig* current);

 public:
	/** Process an include executable directive
	 */
	bool DoPipe(const std::string &file);

	/** Process an include file directive
	 */
	bool DoInclude(const std::string &file);

	/** Error stream, contains error output from any failed configuration parsing.
	 */
	std::stringstream errstr;

	/** True if this configuration is valid enough to run with */
	bool valid;

	/** Set of included files. Do we use this any more?
	 */
	std::map<std::string, std::istream*> IncludedFiles;

	/** Used to indicate who we announce invites to on a channel */
	enum InviteAnnounceState { INVITE_ANNOUNCE_NONE, INVITE_ANNOUNCE_ALL, INVITE_ANNOUNCE_OPS, INVITE_ANNOUNCE_DYNAMIC };

	/** Returns the creator InspIRCd pointer
	 */
	InspIRCd* GetInstance();

	/** Not used any more as it is named, can probably be removed or renamed.
	 */
	int DoDownloads();

  	/** This holds all the information in the config file,
	 * it's indexed by tag name to a vector of key/values.
	 */
	ConfigDataHash config_data;

	/** Length limits, see definition of ServerLimits class
	 */
	ServerLimits Limits;

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

	/** Both for set(g|u)id.
	 */
	char SetUser[MAXBUF];
	char SetGroup[MAXBUF];

	/** Holds the server name of the local server
	 * as defined by the administrator.
	 */
	char ServerName[MAXBUF];

	/** Notice to give to users when they are Xlined
	 */
	char MoronBanner[MAXBUF];

	/* Holds the network name the local server
	 * belongs to. This is an arbitary field defined
	 * by the administrator.
	 */
	char Network[MAXBUF];

	/** Holds the description of the local server
	 * as defined by the administrator.
	 */
	char ServerDesc[MAXBUF];

	/** Holds the admin's name, for output in
	 * the /ADMIN command.
	 */
	char AdminName[MAXBUF];

	/** Holds the email address of the admin,
	 * for output in the /ADMIN command.
	 */
	char AdminEmail[MAXBUF];

	/** Holds the admin's nickname, for output
	 * in the /ADMIN command
	 */
	char AdminNick[MAXBUF];

	/** The admin-configured /DIE password
	 */
	char diepass[MAXBUF];

	/** The admin-configured /RESTART password
	 */
	char restartpass[MAXBUF];

	/** The hash method for *BOTH* the die and restart passwords.
	 */
	char powerhash[MAXBUF];

	/** The pathname and filename of the message of the
	 * day file, as defined by the administrator.
	 */
	char motd[MAXBUF];

	/** The pathname and filename of the rules file,
	 * as defined by the administrator.
	 */
	char rules[MAXBUF];

	/** The quit prefix in use, or an empty string
	 */
	char PrefixQuit[MAXBUF];

	/** The quit suffix in use, or an empty string
	 */
	char SuffixQuit[MAXBUF];

	/** The fixed quit message in use, or an empty string
	 */
	char FixedQuit[MAXBUF];

	/** The part prefix in use, or an empty string
	 */
	char PrefixPart[MAXBUF];

	/** The part suffix in use, or an empty string
	 */
	char SuffixPart[MAXBUF];

	/** The fixed part message in use, or an empty string
	 */
	char FixedPart[MAXBUF];

	/** The last string found within a <die> tag, or
	 * an empty string.
	 */
	char DieValue[MAXBUF];

	/** The DNS server to use for DNS queries
	 */
	char DNSServer[MAXBUF];

	/** Pretend disabled commands don't exist.
	 */
	bool DisabledDontExist;

	/** This variable contains a space-seperated list
	 * of commands which are disabled by the
	 * administrator of the server for non-opers.
	 */
	char DisabledCommands[MAXBUF];

	/** This variable identifies which usermodes have been diabled.
	 */

	char DisabledUModes[64];

	/** This variable identifies which chanmodes have been disabled.
	 */
	char DisabledCModes[64];

	/** The full path to the modules directory.
	 * This is either set at compile time, or
	 * overridden in the configuration file via
	 * the <options> tag.
	 */
	std::string ModPath;

	/** The full pathname to the executable, as
	 * given in argv[0] when the program starts.
	 */
	std::string MyExecutable;

	/** The file handle of the logfile. If this
	 * value is NULL, the log file is not open,
	 * probably due to a permissions error on
	 * startup (this should not happen in normal
	 * operation!).
	 */
	FILE *log_file;

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

	/** If set to true, then all opers on this server are
	 * shown with a generic 'is an IRC operator' line rather
	 * than the oper type. Oper types are still used internally.
	 */
	bool GenericOper;

	/** If this value is true, banned users (+b, not extbans) will not be able to change nick
	 * if banned on any channel, nor to message them.
	 */
	bool RestrictBannedUsers;

	/** If this value is true, halfops have been
	 * enabled in the configuration file.
	 */
	bool AllowHalfop;

	/** If this is set to true, then mode lists (e.g
	 * MODE #chan b) are hidden from unprivileged
	 * users.
	 */
	bool HideModeLists[256];

	/** If this is set to true, then channel operators
	 * are exempt from this channel mode. Used for +Sc etc.
	 */
	bool ExemptChanOps[256];

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

	/** The maximum number of /WHO results allowed
	 * in any single /WHO command.
	 */
	int MaxWhoResults;

	/** True if the DEBUG loglevel is selected.
	 */
	int debugging;

	/** How many seconds to wait before exiting
	 * the program when /DIE is correctly issued.
	 */
	int DieDelay;

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
	bool OperSpyWhois;

	/** Set to a non-empty string to obfuscate the server name of users in WHOIS
	 */
	char HideWhoisServer[MAXBUF];

	/** Set to a non empty string to obfuscate nicknames prepended to a KILL.
	 */
	char HideKillsServer[MAXBUF];

	/** The MOTD file, cached in a file_cache type.
	 */
	file_cache MOTD;

	/** The RULES file, cached in a file_cache type.
	 */
	file_cache RULES;

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
	char UserStats[MAXBUF];

	/** The path and filename of the ircd.log file
	 */
	std::string logpath;

	/** Default channel modes
	 */
	char DefaultModes[MAXBUF];

	/** Custom version string, which if defined can replace the system info in VERSION.
	 */
	char CustomVersion[MAXBUF];

	/** List of u-lined servers
	 */
	std::map<irc::string, bool> ulines;

	/** Max banlist sizes for channels (the std::string is a glob)
	 */
	std::map<std::string, int> maxbans;

	/** Directory where the inspircd binary resides
	 */
	std::string MyDir;

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

	/** If set to true, prefixed channel NOTICEs and PRIVMSGs will have the prefix
	 *  added to the outgoing text for undernet style msg prefixing.
	 */
	bool UndernetMsgPrefix;

	/** If set to true, the full nick!user@host will be shown in the TOPIC command
	 * for who set the topic last. If false, only the nick is shown.
	 */
	bool FullHostInTopic;

	/** All oper type definitions from the config file
	 */
	opertype_t opertypes;

	/** All oper class definitions from the config file
	 */
	operclass_t operclass;

	/** Saved argv from startup
	 */
	char** argv;

	/** Saved argc from startup
	 */
	int argc;

	/** Max channels per user
	 */
	unsigned int MaxChans;

	/** Oper max channels per user
	 */
	unsigned int OperMaxChans;

	/** TS6-like server ID.
	 * NOTE: 000...999 are usable for InspIRCd servers. This
	 * makes code simpler. 0AA, 1BB etc with letters are reserved
	 * for services use.
	 */
	char sid[MAXBUF];

	/** True if we have been told to run the testsuite from the commandline,
	 * rather than entering the mainloop.
	 */
	bool TestSuite;

	/** Construct a new ServerConfig
	 */
	ServerConfig(InspIRCd* Instance);

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

	/** Read a file into a file_cache object
	 */
	bool ReadFile(file_cache &F, const char* fname);

	/* Returns true if the given string starts with a windows drive letter
	 */
	bool StartsWithWindowsDriveLetter(const std::string &path);

	/** Load 'filename' into 'target', with the new config parser everything is parsed into
	 * tag/key/value at load-time rather than at read-value time.
	 */
	bool LoadConf(FILE* &conf, const char* filename);

	/** Load 'filename' into 'target', with the new config parser everything is parsed into
	 * tag/key/value at load-time rather than at read-value time.
	 */
	bool LoadConf(FILE* &conf, const std::string &filename);

	/** Writes 'length' chars into 'result' as a string
	 */
	bool ConfValue(const char* tag, const char* var, int index, char* result, int length, bool allow_linefeeds = false);

	/** Writes 'length' chars into 'result' as a string
	 */
	bool ConfValue(const char* tag, const char* var, const char* default_value, int index, char* result, int length, bool allow_linefeeds = false);

	/** Writes 'length' chars into 'result' as a string
	 */
	bool ConfValue(const std::string &tag, const std::string &var, int index, std::string &result, bool allow_linefeeds = false);

	/** Writes 'length' chars into 'result' as a string
	 */
	bool ConfValue(const std::string &tag, const std::string &var, const std::string &default_value, int index, std::string &result, bool allow_linefeeds = false);

	/** Tries to convert the value to an integer and write it to 'result'
	 */
	bool ConfValueInteger(const char* tag, const char* var, int index, int &result);

	/** Tries to convert the value to an integer and write it to 'result'
	 */
	bool ConfValueInteger(const char* tag, const char* var, const char* default_value, int index, int &result);

	/** Tries to convert the value to an integer and write it to 'result'
	 */
	bool ConfValueInteger(const std::string &tag, const std::string &var, int index, int &result);

	/** Tries to convert the value to an integer and write it to 'result'
	 */
	bool ConfValueInteger(const std::string &tag, const std::string &var, const std::string &default_value, int index, int &result);

	/** Returns true if the value exists and has a true value, false otherwise
	 */
	bool ConfValueBool(const char* tag, const char* var, int index);

	/** Returns true if the value exists and has a true value, false otherwise
	 */
	bool ConfValueBool(const char* tag, const char* var, const char* default_value, int index);

	/** Returns true if the value exists and has a true value, false otherwise
	 */
	bool ConfValueBool(const std::string &tag, const std::string &var, int index);

	/** Returns true if the value exists and has a true value, false otherwise
	 */
	bool ConfValueBool(const std::string &tag, const std::string &var, const std::string &default_value, int index);

	/** Returns the number of occurences of tag in the config file
	 */
	int ConfValueEnum(const char* tag);
	/** Returns the number of occurences of tag in the config file
	 */
	int ConfValueEnum(const std::string &tag);

	/** Returns the numbers of vars inside the index'th 'tag in the config file
	 */
	int ConfVarEnum(const char* tag, int index);
	/** Returns the numbers of vars inside the index'th 'tag in the config file
	 */
	int ConfVarEnum(const std::string &tag, int index);

	bool ApplyDisabledCommands(const char* data);

	/** Returns the fully qualified path to the inspircd directory
	 * @return The full program directory
	 */
	std::string GetFullProgDir();

	/** Clean a filename, stripping the directories (and drives) from string.
	 * @param name Directory to tidy
	 * @return The cleaned filename
	 */
	static char* CleanFilename(char* name);

	/** Check if a file exists.
	 * @param file The full path to a file
	 * @return True if the file exists and is readable.
	 */
	static bool FileExists(const char* file);

	/** If this value is true, invites will bypass more than just +i
	 */
	bool InvBypassModes;

};


/** Types of data in the core config
 */
enum ConfigDataType
{
	DT_NOTHING       = 0,		/* No data */
	DT_INTEGER       = 1,		/* Integer */
	DT_CHARPTR       = 2,		/* Char pointer */
	DT_BOOLEAN       = 3,		/* Boolean */
	DT_HOSTNAME	 = 4,		/* Hostname syntax */
	DT_NOSPACES	 = 5,		/* No spaces */
	DT_IPADDRESS	 = 6,		/* IP address (v4, v6) */
	DT_CHANNEL	 = 7,		/* Channel name */
	DT_LIMIT     = 8,       /* size_t */
	DT_ALLOW_WILD	 = 64,		/* Allow wildcards/CIDR in DT_IPADDRESS */
	DT_ALLOW_NEWLINE = 128		/* New line characters allowed in DT_CHARPTR */
};

/** The maximum number of values in a core configuration tag. Can be increased if needed.
 */
#define MAX_VALUES_PER_TAG 18

/** Holds a config value, either string, integer or boolean.
 * Callback functions receive one or more of these, either on
 * their own as a reference, or in a reference to a deque of them.
 * The callback function can then alter the values of the ValueItem
 * classes to validate the settings.
 */
class ValueItem
{
	/** Actual data */
	std::string v;
 public:
	/** Initialize with an int */
	ValueItem(int value);
	/** Initialize with a bool */
	ValueItem(bool value);
	/** Initialize with a string */
	ValueItem(const char* value) : v(value) { }
	/** Change value to a string */
	void Set(const std::string &val);
	/** Change value to an int */
	void Set(int value);
	/** Get value as an int */
	int GetInteger();
	/** Get value as a string */
	const char* GetString() const;
	/** Get value as a string */
	inline const std::string& GetValue() const { return v; }
	/** Get value as a bool */
	bool GetBool();
};

/** The base class of the container 'ValueContainer'
 * used internally by the core to hold core values.
 */
class ValueContainerBase
{
 public:
	/** Constructor */
	ValueContainerBase() { }
	/** Destructor */
	virtual ~ValueContainerBase() { }
};

/** ValueContainer is used to contain pointers to different
 * core values such as the server name, maximum number of
 * clients etc.
 * It is specialized to hold a data type, then pointed at
 * a value in the ServerConfig class. When the value has been
 * read and validated, the Set method is called to write the
 * value safely in a type-safe manner.
 */
template<typename T> class ValueContainer : public ValueContainerBase
{
	T ServerConfig::* const vptr;
 public:
	/** Initialize with a value of type T */
	ValueContainer(T ServerConfig::* const offset) : vptr(offset)
	{
	}

	/** Change value to type T of size s */
	void Set(ServerConfig* conf, const T& value)
	{
		conf->*vptr = value;
	}

	void Set(ServerConfig* conf, const ValueItem& item);
};

template<> void ValueContainer<char[MAXBUF]>::Set(ServerConfig* conf, ValueItem const& item);


class ValueContainerLimit : public ValueContainerBase
{
	size_t ServerLimits::* const vptr;
 public:
	/** Initialize with a value of type T */
	ValueContainerLimit(size_t ServerLimits::* const offset) : vptr(offset)
	{
	}

	/** Change value to type T of size s */
	void Set(ServerConfig* conf, const size_t& value)
	{
		conf->Limits.*vptr = value;
	}
};

/** A specialization of ValueContainer to hold a pointer to a bool
 */
typedef ValueContainer<bool> ValueContainerBool;

/** A specialization of ValueContainer to hold a pointer to
 * an unsigned int
 */
typedef ValueContainer<unsigned int> ValueContainerUInt;

/** A specialization of ValueContainer to hold a pointer to
 * a char array.
 */
typedef ValueContainer<char[MAXBUF]> ValueContainerChar;

/** A specialization of ValueContainer to hold a pointer to
 * a char array.
 */
typedef ValueContainer<std::string> ValueContainerString;

/** A specialization of ValueContainer to hold a pointer to
 * an int
 */
typedef ValueContainer<int> ValueContainerInt;

/** A set of ValueItems used by multi-value validator functions
 */
typedef std::deque<ValueItem> ValueList;

/** A callback for validating a single value
 */
typedef bool (*Validator)(ServerConfig* conf, const char*, const char*, ValueItem&);
/** A callback for validating multiple value entries
 */
typedef bool (*MultiValidator)(ServerConfig* conf, const char*, const char**, ValueList&, int*);
/** A callback indicating the end of a group of entries
 */
typedef bool (*MultiNotify)(ServerConfig* conf, const char*);

/** Holds a core configuration item and its callbacks
 */
struct InitialConfig
{
	/** Tag name */
	const char* tag;
	/** Value name */
	const char* value;
	/** Default, if not defined */
	const char* default_value;
	/** Value containers */
	ValueContainerBase* val;
	/** Data types */
	int datatype;
	/** Validation function */
	Validator validation_function;
};

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

/** Holds a core configuration item and its callbacks
 * where there may be more than one item
 */
struct MultiConfig
{
	/** Tag name */
	const char*	tag;
	/** One or more items within tag */
	const char*	items[MAX_VALUES_PER_TAG];
	/** One or more defaults for items within tags */
	const char* items_default[MAX_VALUES_PER_TAG];
	/** One or more data types */
	int		datatype[MAX_VALUES_PER_TAG];
	/** Validation function */
	MultiValidator	validation_function;
};

#endif
