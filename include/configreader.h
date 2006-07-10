/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd is copyright (C) 2002-2006 ChatSpike-Dev.
 *                       E-mail:
 *                <brain@chatspike.net>
 *                <Craig@chatspike.net>
 *                <omster@gmail.com>
 *     
 * Written by Craig Edwards, Craig McLure, and others.
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
#include "inspircd.h"
#include "globals.h"
#include "modules.h"

typedef bool (*Validator)(const char*, const char*, void*);
typedef bool (*MultiValidator)(const char*, char**, void**, int*);
typedef bool (*MultiNotify)(const char*);

enum ConfigDataType { DT_NOTHING, DT_INTEGER, DT_CHARPTR, DT_BOOLEAN };

class InitialConfig : public classbase
{
 public:
	char* tag;
	char* value;
	void* val;
	ConfigDataType datatype;
	Validator validation_function;
};

class MultiConfig : public classbase
{
 public:
	const char* tag;
	char* items[12];
	int datatype[12];
	MultiNotify	init_function;
	MultiValidator	validation_function;
	MultiNotify	finish_function;
};

/** This class holds the bulk of the runtime configuration for the ircd.
 * It allows for reading new config values, accessing configuration files,
 * and storage of the configuration data needed to run the ircd, such as
 * the servername, connect classes, /ADMIN data, MOTDs and filenames etc.
 */
class ServerConfig : public Extensible
{
  private:
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
  
	bool DoInclude(ConfigDataHash &target, const std::string &file, std::ostringstream &errorstream);

	/** Check that there is only one of each configuration item
	 */
	bool CheckOnce(char* tag, bool bail, userrec* user);
  
  public:
	  
  	/** This holds all the information in the config file,
	 * it's indexed by tag name to a vector of key/values.
	 */
	ConfigDataHash config_data;

	/** Holds the server name of the local server
	 * as defined by the administrator.
	 */
	char ServerName[MAXBUF];
	
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

	/** The temporary directory where modules are copied
	 */
	char TempDir[1024];

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

	/** If this is enabled then operators will
	 * see invisible (+i) channels in /whois.
	 */
	bool OperSpyWhois;

	/** Set to a non-empty string to obfuscate the server name of users in WHOIS
	 */
	char HideWhoisServer[MAXBUF];

	/** A list of IP addresses the server is listening
	 * on.
	 */
	char addrs[MAXBUF][255];

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

	/** A list of ports which the server is listening on
	 */
	int ports[255];

	/** Boolean sets of which modules implement which functions
	 */
	char implement_lists[255][255];

	/** Global implementation list
	 */
	char global_implementation[255];

	/** A list of ports claimed by IO Modules
	 */
	std::map<int,Module*> IOHookModule;

	/** The 005 tokens of this server (ISUPPORT)
	 * populated/repopulated upon loading or unloading
	 * modules.
	 */
	std::string data005;

	/** STATS characters in this list are available
	 * only to operators.
	 */
	char OperOnlyStats[MAXBUF];
	
	/** The path and filename of the ircd.log file
	 */
	std::string logpath;

	/** Custom version string, which if defined can replace the system info in VERSION.
	 */
	char CustomVersion[MAXBUF];

	/** List of u-lined servers
	 */
	std::vector<irc::string> ulines;

	/** Max banlist sizes for channels (the std::string is a glob)
	 */
	std::map<std::string,int> maxbans;

	ServerConfig();

	/** Clears the include stack in preperation for
	 * a Read() call.
	 */
	void ClearStack();

	/** Read the entire configuration into memory
	 * and initialize this class. All other methods
	 * should be used only by the core.
	 */
	void Read(bool bail, userrec* user);

	/** Load 'filename' into 'target', with the new config parser everything is parsed into
	 * tag/key/value at load-time rather than at read-value time.
	 */
	bool LoadConf(ConfigDataHash &target, const char* filename, std::ostringstream &errorstream);
	bool LoadConf(ConfigDataHash &target, const std::string &filename, std::ostringstream &errorstream);
	
	/* Both these return true if the value existed or false otherwise */
	
	/* Writes 'length' chars into 'result' as a string */
	bool ConfValue(ConfigDataHash &target, const char* tag, const char* var, int index, char* result, int length);
	bool ConfValue(ConfigDataHash &target, const std::string &tag, const std::string &var, int index, std::string &result);
	
	/* Tries to convert the value to an integer and write it to 'result' */
	bool ConfValueInteger(ConfigDataHash &target, const char* tag, const char* var, int index, int &result);
	bool ConfValueInteger(ConfigDataHash &target, const std::string &tag, const std::string &var, int index, int &result);
	
	/* Returns true if the value exists and has a true value, false otherwise */
	bool ConfValueBool(ConfigDataHash &target, const char* tag, const char* var, int index);
	bool ConfValueBool(ConfigDataHash &target, const std::string &tag, const std::string &var, int index);
	
	/* Returns the number of occurences of tag in the config file */
	int ConfValueEnum(ConfigDataHash &target, const char* tag);
	int ConfValueEnum(ConfigDataHash &target, const std::string &tag);
	
	/* Returns the numbers of vars inside the index'th 'tag in the config file */
	int ConfVarEnum(ConfigDataHash &target, const char* tag, int index);
	int ConfVarEnum(ConfigDataHash &target, const std::string &tag, int index);
	
	Module* GetIOHook(int port);
	bool AddIOHook(int port, Module* iomod);
	bool DelIOHook(int port);
};

#endif
