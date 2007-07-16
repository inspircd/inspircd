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
#include "globals.h"
#include "modules.h"
#include "socketengine.h"
#include "socket.h"

/* Required forward definitions */
class ServerConfig;
class InspIRCd;
class InspSocket;

/** Types of data in the core config
 */
enum ConfigDataType
{
	DT_NOTHING       = 0,		/* No data */
	DT_INTEGER       = 1,		/* Integer */
	DT_CHARPTR       = 2,		/* Char pointer */
	DT_BOOLEAN       = 3,		/* Boolean */
	DT_ALLOW_NEWLINE = 128		/* New line characters allowed */
};

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
	/** Initialize with a char pointer */
	ValueItem(char* value);
	/** Change value to a char pointer */
	void Set(char* value);
	/** Change value to a const char pointer */
	void Set(const char* val);
	/** Change value to an int */
	void Set(int value);
	/** Get value as an int */
	int GetInteger();
	/** Get value as a string */
	char* GetString();
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
	/** Contained item */
	T val;
 public:

	/** Initialize with nothing */
	ValueContainer()
	{
		val = NULL;
	}

	/** Initialize with a value of type T */
	ValueContainer(T Val)
	{
		val = Val;
	}

	/** Change value to type T of size s */
	void Set(T newval, size_t s)
	{
		memcpy(val, newval, s);
	}
};

/** A specialization of ValueContainer to hold a pointer to a bool
 */
typedef ValueContainer<bool*> ValueContainerBool;

/** A specialization of ValueContainer to hold a pointer to
 * an unsigned int
 */
typedef ValueContainer<unsigned int*> ValueContainerUInt;

/** A specialization of ValueContainer to hold a pointer to
 * a char array.
 */
typedef ValueContainer<char*> ValueContainerChar;

/** A specialization of ValueContainer to hold a pointer to
 * an int
 */
typedef ValueContainer<int*> ValueContainerInt;

/** A set of ValueItems used by multi-value validator functions
 */
typedef std::deque<ValueItem> ValueList;

/** A callback for validating a single value
 */
typedef bool (*Validator)(ServerConfig* conf, const char*, const char*, ValueItem&);
/** A callback for validating multiple value entries
 */
typedef bool (*MultiValidator)(ServerConfig* conf, const char*, char**, ValueList&, int*);
/** A callback indicating the end of a group of entries
 */
typedef bool (*MultiNotify)(ServerConfig* conf, const char*);

/** Holds a core configuration item and its callbacks
 */
struct InitialConfig
{
	/** Tag name */
	char* tag;
	/** Value name */
	char* value;
	/** Default, if not defined */
	char* default_value;
	/** Value containers */
	ValueContainerBase* val;
	/** Data types */
	ConfigDataType datatype;
	/** Validation function */
	Validator validation_function;
};

/** Holds a core configuration item and its callbacks
 * where there may be more than one item
 */
struct MultiConfig
{
	/** Tag name */
	const char*	tag;
	/** One or more items within tag */
	char*		items[13];
	/** One or more defaults for items within tags */
	char*		items_default[13];
	/** One or more data types */
	int		datatype[13];
	/** Initialization function */
	MultiNotify	init_function;
	/** Validation function */
	MultiValidator	validation_function;
	/** Completion function */
	MultiNotify	finish_function;
};

/** A set of oper types
 */
typedef std::map<irc::string,char*> opertype_t;

/** A Set of oper classes
 */
typedef std::map<irc::string,char*> operclass_t;


/** This class holds the bulk of the runtime configuration for the ircd.
 * It allows for reading new config values, accessing configuration files,
 * and storage of the configuration data needed to run the ircd, such as
 * the servername, connect classes, /ADMIN data, MOTDs and filenames etc.
 */
class CoreExport ServerConfig : public Extensible
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

	/** This private method processes one line of
	 * configutation, appending errors to errorstream
	 * and setting error if an error has occured.
	 */
	bool ParseLine(ConfigDataHash &target, std::string &line, long linenumber, std::ostringstream &errorstream);
  
	/** Process an include directive
	 */
	bool DoInclude(ConfigDataHash &target, const std::string &file, std::ostringstream &errorstream);

	/** Check that there is only one of each configuration item
	 */
	bool CheckOnce(char* tag, bool bail, userrec* user);
  
  public:

	InspIRCd* GetInstance();
	  
  	/** This holds all the information in the config file,
	 * it's indexed by tag name to a vector of key/values.
	 */
	ConfigDataHash config_data;

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

	/** The last string found within a <die> tag, or
	 * an empty string.
	 */
	char DieValue[MAXBUF];

	/** The DNS server to use for DNS queries
	 */
	char DNSServer[MAXBUF];

	/** This variable contains a space-seperated list
	 * of commands which are disabled by the
	 * administrator of the server for non-opers.
	 */
	char DisabledCommands[MAXBUF];

	/** The full path to the modules directory.
	 * This is either set at compile time, or
	 * overridden in the configuration file via
	 * the <options> tag.
	 */
	char ModPath[1024];

	/** The full pathname to the executable, as
	 * given in argv[0] when the program starts.
	 */
	char MyExecutable[1024];

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

	/** The loglevel in use by the IRC server
	 */
	int LogLevel;

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
	bool AnnounceInvites;

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
	char PID[1024];

	/** The connect classes in use by the IRC server.
	 */
	ClassVector Classes;

	/** A list of module names (names only, no paths)
	 * which are currently loaded by the server.
	 */
	std::vector<std::string> module_names;

	/** A list of the classes for listening client ports
	 */
	std::vector<ListenSocket*> ports;

	/** Boolean sets of which modules implement which functions
	 */
	char implement_lists[255][255];

	/** Global implementation list
	 */
	char global_implementation[255];

	/** A list of ports claimed by IO Modules
	 */
	std::map<int,Module*> IOHookModule;

	std::map<InspSocket*, Module*> SocketIOHookModule;

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

	/** Construct a new ServerConfig
	 */
	ServerConfig(InspIRCd* Instance);

	/** Clears the include stack in preperation for a Read() call.
	 */
	void ClearStack();

	/** Update the 005 vector
	 */
	void Update005();

	/** Send the 005 numerics (ISUPPORT) to a user
	 */
	void Send005(userrec* user);

	/** Read the entire configuration into memory
	 * and initialize this class. All other methods
	 * should be used only by the core.
	 */
	void Read(bool bail, userrec* user);

	/** Read a file into a file_cache object
	 */
	bool ReadFile(file_cache &F, const char* fname);

	/** Report a configuration error given in errormessage.
	 * @param bail If this is set to true, the error is sent to the console, and the program exits
	 * @param user If this is set to a non-null value, and bail is false, the errors are spooled to
	 * this user as SNOTICEs.
	 * If the parameter is NULL, the messages are spooled to all users via WriteOpers as SNOTICEs.
	 */
	void ReportConfigError(const std::string &errormessage, bool bail, userrec* user);

	/** Load 'filename' into 'target', with the new config parser everything is parsed into
	 * tag/key/value at load-time rather than at read-value time.
	 */
	bool LoadConf(ConfigDataHash &target, const char* filename, std::ostringstream &errorstream);

	/** Load 'filename' into 'target', with the new config parser everything is parsed into
	 * tag/key/value at load-time rather than at read-value time.
	 */
	bool LoadConf(ConfigDataHash &target, const std::string &filename, std::ostringstream &errorstream);
	
	/* Both these return true if the value existed or false otherwise */
	
	/** Writes 'length' chars into 'result' as a string
	 */
	bool ConfValue(ConfigDataHash &target, const char* tag, const char* var, int index, char* result, int length, bool allow_linefeeds = false);
	/** Writes 'length' chars into 'result' as a string
	 */
	bool ConfValue(ConfigDataHash &target, const char* tag, const char* var, const char* default_value, int index, char* result, int length, bool allow_linefeeds = false);

	/** Writes 'length' chars into 'result' as a string
	 */
	bool ConfValue(ConfigDataHash &target, const std::string &tag, const std::string &var, int index, std::string &result, bool allow_linefeeds = false);
	/** Writes 'length' chars into 'result' as a string
	 */
	bool ConfValue(ConfigDataHash &target, const std::string &tag, const std::string &var, const std::string &default_value, int index, std::string &result, bool allow_linefeeds = false);
	
	/** Tries to convert the value to an integer and write it to 'result'
	 */
	bool ConfValueInteger(ConfigDataHash &target, const char* tag, const char* var, int index, int &result);
	/** Tries to convert the value to an integer and write it to 'result'
	 */
	bool ConfValueInteger(ConfigDataHash &target, const char* tag, const char* var, const char* default_value, int index, int &result);
	/** Tries to convert the value to an integer and write it to 'result'
	 */
	bool ConfValueInteger(ConfigDataHash &target, const std::string &tag, const std::string &var, int index, int &result);
	/** Tries to convert the value to an integer and write it to 'result'
	 */
	bool ConfValueInteger(ConfigDataHash &target, const std::string &tag, const std::string &var, const std::string &default_value, int index, int &result);
	
	/** Returns true if the value exists and has a true value, false otherwise
	 */
	bool ConfValueBool(ConfigDataHash &target, const char* tag, const char* var, int index);
	/** Returns true if the value exists and has a true value, false otherwise
	 */
	bool ConfValueBool(ConfigDataHash &target, const char* tag, const char* var, const char* default_value, int index);
	/** Returns true if the value exists and has a true value, false otherwise
	 */
	bool ConfValueBool(ConfigDataHash &target, const std::string &tag, const std::string &var, int index);
	/** Returns true if the value exists and has a true value, false otherwise
	 */
	bool ConfValueBool(ConfigDataHash &target, const std::string &tag, const std::string &var, const std::string &default_value, int index);
	
	/** Returns the number of occurences of tag in the config file
	 */
	int ConfValueEnum(ConfigDataHash &target, const char* tag);
	/** Returns the number of occurences of tag in the config file
	 */
	int ConfValueEnum(ConfigDataHash &target, const std::string &tag);
	
	/** Returns the numbers of vars inside the index'th 'tag in the config file
	 */
	int ConfVarEnum(ConfigDataHash &target, const char* tag, int index);
	/** Returns the numbers of vars inside the index'th 'tag in the config file
	 */
	int ConfVarEnum(ConfigDataHash &target, const std::string &tag, int index);
	
	/** Get a pointer to the module which has hooked the given port.
	 * @parameter port Port number
	 * @return Returns a pointer to the hooking module, or NULL
	 */
	Module* GetIOHook(int port);

	/** Hook a module to a client port, so that it can receive notifications
	 * of low-level port activity.
	 * @param port The port number
	 * @param Module the module to hook to the port
	 * @return True if the hook was successful.
	 */
	bool AddIOHook(int port, Module* iomod);

	/** Delete a module hook from a client port.
	 * @param port The port to detatch from
	 * @return True if successful
	 */
	bool DelIOHook(int port);
	
	/** Get a pointer to the module which has hooked the given InspSocket class.
	 * @parameter port Port number
	 * @return Returns a pointer to the hooking module, or NULL
	 */
	Module* GetIOHook(InspSocket* is);

	/** Hook a module to an InspSocket class, so that it can receive notifications
	 * of low-level socket activity.
	 * @param iomod The module to hook to the socket
	 * @param is The InspSocket to attach to
	 * @return True if the hook was successful.
	 */
	bool AddIOHook(Module* iomod, InspSocket* is);

	/** Delete a module hook from an InspSocket.
	 * @param is The InspSocket to detatch from.
	 * @return True if the unhook was successful
	 */
	bool DelIOHook(InspSocket* is);

	/** Returns the fully qualified path to the inspircd directory
	 * @return The full program directory
	 */
	std::string GetFullProgDir();

	/** Returns true if a directory is valid (within the modules directory).
	 * @param dirandfile The directory and filename to check
	 * @return True if the directory is valid
	 */
	static bool DirValid(const char* dirandfile);

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
	
};

/** Initialize the disabled commands list
 */
CoreExport bool InitializeDisabledCommands(const char* data, InspIRCd* ServerInstance);

/** Initialize the oper types
 */
bool InitTypes(ServerConfig* conf, const char* tag);

/** Initialize the oper classes
 */
bool InitClasses(ServerConfig* conf, const char* tag);

/** Initialize an oper type 
 */
bool DoType(ServerConfig* conf, const char* tag, char** entries, ValueList &values, int* types);

/** Initialize an oper class
 */
bool DoClass(ServerConfig* conf, const char* tag, char** entries, ValueList &values, int* types);

/** Finish initializing the oper types and classes
 */
bool DoneClassesAndTypes(ServerConfig* conf, const char* tag);

#endif

