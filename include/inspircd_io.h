/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  Inspire is copyright (C) 2002-2005 ChatSpike-Dev.
 *                       E-mail:
 *                <brain@chatspike.net>
 *           	  <Craig@chatspike.net>
 *     
 * Written by Craig Edwards, Craig McLure, and others.
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#ifndef __INSPIRCD_IO_H__
#define __INSPIRCD_IO_H__

#include <sstream>
#include <string>
#include <vector>
#include "inspircd.h"
#include "globals.h"

/** Flags for use with log()
 */
#define DEBUG 10
#define VERBOSE 20
#define DEFAULT 30
#define SPARSE 40
#define NONE 50

/** This class holds the bulk of the runtime configuration for the ircd.
 * It allows for reading new config values, accessing configuration files,
 * and storage of the configuration data needed to run the ircd, such as
 * the servername, connect classes, /ADMIN data, MOTDs and filenames etc.
 */
class ServerConfig : public classbase
{
  private:
	/** This variable holds the names of all
	 * files included from the main one. This
	 * is used to make sure that no files are
	 * recursively included.
	 */
	std::vector<std::string> include_stack;

	/** Used by the config file subsystem to
	 * safely read a C-style string without
	 * dependency upon any certain style of
	 * linefeed, e.g. it can read both windows
	 * and UNIX style linefeeds transparently.
	 */
	int fgets_safe(char* buffer, size_t maxsize, FILE* &file);

	/** This private method processes one line of
	 * configutation, appending errors to errorstream
	 * and setting error if an error has occured.
	 */
	std::string ConfProcess(char* buffer, long linenumber, std::stringstream* errorstream, bool &error, std::string filename);

  public:

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

	/** If this value is true, the owner of the
	 * server has chosen to unlimit the coredump
	 * size to as large a value as his account
	 * settings will allow. This is often used
	 * when debugging.
	 */
        bool unlimitcore;

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

	/** The parsed configuration file as a stringstream.
	 * You should pass this to any configuration methods
	 * of this class, and not access it directly. It is
	 * recommended that modules use ConfigReader instead
	 * which provides a simpler abstraction of configuration
	 * files.
	 */
	std::stringstream config_f;

	/** The connect classes in use by the IRC server.
	 */
	ClassVector Classes;

	/** A list of module names (names only, no paths)
	 * which are currently loaded by the server.
	 */
	std::vector<std::string> module_names;

	/** A list of ports which the server is listening on
	 */
	int ports[MAXSOCKS];

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

	bool LoadConf(const char* filename, std::stringstream *target, std::stringstream* errorstream);
	int ConfValue(char* tag, char* var, int index, char *result, std::stringstream *config);
	int ReadConf(std::stringstream *config_f,const char* tag, const char* var, int index, char *result);
	int ConfValueEnum(char* tag,std::stringstream *config);
	int EnumConf(std::stringstream *config_f,const char* tag);
	int EnumValues(std::stringstream *config, const char* tag, int index);
};


void Exit (int); 
void Start (void); 
int DaemonSeed (void); 
bool FileExists (const char* file);
int OpenTCPSocket (void); 
int BindSocket (int sockfd, struct sockaddr_in client, struct sockaddr_in server, int port, char* addr);
void WritePID(std::string filename);
int BindPorts();

#endif
