/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd is copyright (C) 2002-2006 ChatSpike-Dev.
 *		       E-mail:
 *		<brain@chatspike.net>
 *	   	  <Craig@chatspike.net>
 *     
 * Written by Craig Edwards, Craig McLure, and others.
 * This program is free but copyrighted software; see
 *	    the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#ifndef __INSPIRCD_H__
#define __INSPIRCD_H__

#include <time.h>
#include <string>
#include <sstream>
#include "inspircd_config.h"
#include "users.h"
#include "channels.h"
#include "socket.h"
#include "mode.h"
#include "helperfuncs.h"
#include "socketengine.h"
#include "command_parse.h"

/** Returned by some functions to indicate failure,
 * and the exit code of the program if it terminates.
 */
#define ERROR -1

/** Crucial defines
 */
#define ETIREDGERBILS EAGAIN

/** Debug levels for use with InspIRCd::Log()
 */
enum DebugLevel
{
	DEBUG		=	10,
	VERBOSE		=	20,
	DEFAULT		=	30,
	SPARSE		=	40,
	NONE		=	50,
};

/**
 * This define is used in place of strcmp when we 
 * want to check if a char* string contains only one
 * letter. Pretty fast, its just two compares and an
 * addition.
 */
#define IS_SINGLE(x,y) ( (*x == y) && (*(x+1) == 0) )

/** Delete a pointer, and NULL its value
 */
#define DELETE(x) {if (x) { delete x; x = NULL; }}

/** Template function to convert any input type to std::string
 */
template<typename T> inline std::string ConvToStr(const T &in)
{
	std::stringstream tmp;
	if (!(tmp << in)) return std::string();
	return tmp.str();
}

/** This class contains various STATS counters
 * It is used by the InspIRCd class, which internally
 * has an instance of it.
 */
class serverstats : public classbase
{
  public:
	/** Number of accepted connections
	 */
	unsigned long statsAccept;
	/** Number of failed accepts
	 */
	unsigned long statsRefused;
	/** Number of unknown commands seen
	 */
	unsigned long statsUnknown;
	/** Number of nickname collisions handled
	 */
	unsigned long statsCollisions;
	/** Number of DNS queries sent out
	 */
	unsigned long statsDns;
	/** Number of good DNS replies received
	 * NOTE: This may not tally to the number sent out,
	 * due to timeouts and other latency issues.
	 */
	unsigned long statsDnsGood;
	/** Number of bad (negative) DNS replies received
	 * NOTE: This may not tally to the number sent out,
	 * due to timeouts and other latency issues.
	 */
	unsigned long statsDnsBad;
	/** Number of inbound connections seen
	 */
	unsigned long statsConnects;
	/** Total bytes of data transmitted
	 */
	double statsSent;
	/** Total bytes of data received
	 */
	double statsRecv;
	/** Number of bound listening ports
	 */
	unsigned long BoundPortCount;

	/** The constructor initializes all the counts to zero
	 */
	serverstats()
		: statsAccept(0), statsRefused(0), statsUnknown(0), statsCollisions(0), statsDns(0),
		statsDnsGood(0), statsDnsBad(0), statsConnects(0), statsSent(0.0), statsRecv(0.0),
		BoundPortCount(0)
	{
	}
};

class XLineManager;

/** The main singleton class of the irc server.
 * This class contains instances of all the other classes
 * in this software, with the exception of the base class,
 * classbase. Amongst other things, it contains a ModeParser,
 * a DNS object, a CommandParser object, and a list of active
 * Module objects, and facilities for Module objects to
 * interact with the core system it implements. You should
 * NEVER attempt to instantiate a class of type InspIRCd
 * yourself. If you do, this is equivalent to spawning a second
 * IRC server, and could have catastrophic consequences for the
 * program in terms of ram usage (basically, you could create
 * an obese forkbomb built from recursively spawning irc servers!)
 */
class InspIRCd : public classbase
{
 private:
	/** Holds a string describing the last module error to occur
	 */
	char MODERR[MAXBUF];

	/** This is an internal flag used by the mainloop
	 */
	bool expire_run;

	/** List of server names we've seen.
	 */
	servernamelist servernames;
 
	/** Remove a ModuleFactory pointer
	 */
	void EraseFactory(int j);

	/** Remove a Module pointer
	 */
	void EraseModule(int j);

	/** Build the ISUPPORT string by triggering all modules On005Numeric events
	 */
	void BuildISupport();

	/** Move a given module to a specific slot in the list
	 */
	void MoveTo(std::string modulename,int slot);

	/** Display the startup banner
	 */
	void Start();

	/** Set up the signal handlers
	 */
	void SetSignals(bool SEGVHandler);

	/** Daemonize the ircd and close standard input/output streams
	 */
	bool DaemonSeed();

	/** Build the upper/lowercase comparison table
	 */
	void MakeLowerMap();

	/** Moves the given module to the last slot in the list
	 */
	void MoveToLast(std::string modulename);

	/** Moves the given module to the first slot in the list
	 */
	void MoveToFirst(std::string modulename);

	/** Moves one module to be placed after another in the list
	 */
	void MoveAfter(std::string modulename, std::string after);

	/** Moves one module to be placed before another in the list
	 */
	void MoveBefore(std::string modulename, std::string before);

	/** Process a user whos socket has been flagged as active
	 */
	void ProcessUser(userrec* cu);

	/** Iterate the list of InspSocket objects, removing ones which have timed out
	 */
	void DoSocketTimeouts(time_t TIME);

	/** Perform background user events such as PING checks
	 */
	void DoBackgroundUserStuff(time_t TIME);

	/** Returns true when all modules have done pre-registration checks on a user
	 */
	bool AllModulesReportReady(userrec* user);

	/** Total number of modules loaded into the ircd, minus one
	 */
	int ModCount;

	/** Logfile pathname specified on the commandline, or empty string
	 */
	char LogFileName[MAXBUF];

	/** The feature names published by various modules
	 */
	featurelist Features;

	/** The current time, updated in the mainloop
	 */
	time_t TIME;

	/** The time that was recorded last time around the mainloop
	 */
	time_t OLDTIME;

	/** A 64k buffer used to read client lines into
	 */
	char ReadBuffer[65535];

	/** Number of seconds in a minute
	 */
	const long duration_m;

	/** Number of seconds in an hour
	 */
	const long duration_h;

	/** Number of seconds in a day
	 */
	const long duration_d;

	/** Number of seconds in a week
	 */
	const long duration_w;

	/** Number of seconds in a year
	 */
	const long duration_y;

 public:
	/** Time this ircd was booted
	 */
	time_t startup_time;

	/** Mode handler, handles mode setting and removal
	 */
	ModeParser* Modes;

	/** Command parser, handles client to server commands
	 */
	CommandParser* Parser;

	/** Socket engine, handles socket activity events
	 */
	SocketEngine* SE;

	/** Stats class, holds miscellaneous stats counters
	 */
	serverstats* stats;

	/**  Server Config class, holds configuration file data
	 */
	ServerConfig* Config;

	/** Module sockets list, holds the active set of InspSocket classes
	 */
	std::vector<InspSocket*> module_sockets;

	/** Socket reference table, provides fast lookup of fd to InspSocket*
	 */
	InspSocket* socket_ref[MAX_DESCRIPTORS];

	/** user reference table, provides fast lookup of fd to userrec*
	 */
	userrec* fd_ref_table[MAX_DESCRIPTORS];

	/** Client list, a hash_map containing all clients, local and remote
	 */
	user_hash clientlist;

	/** Channel list, a hash_map containing all channels
	 */
	chan_hash chanlist;

	/** Local client list, a vector containing only local clients
	 */
	std::vector<userrec*> local_users;

	/** Oper list, a vector containing all local and remote opered users
	 */
	std::vector<userrec*> all_opers;

	/** Whowas container, contains a map of vectors of users tracked by WHOWAS
	 */
	irc::whowas::whowas_users whowas;

	/** DNS class, provides resolver facilities to the core and modules
	 */
	DNS* Res;

	/** Timer manager class, triggers InspTimer timer events
	 */
	TimerManager* Timers;

	/** Command list, a hash_map of command names to command_t*
	 */
	command_table cmdlist;

	/** X-Line manager. Handles G/K/Q/E line setting, removal and matching
	 */
	XLineManager* XLines;

	/** A list of Module* module classes
	 * Note that this list is always exactly 255 in size.
	 * The actual number of loaded modules is available from GetModuleCount()
	 */
	ModuleList modules;

	/** A list of ModuleFactory* module factories
	 * Note that this list is always exactly 255 in size.
	 * The actual number of loaded modules is available from GetModuleCount()
	 */
	FactoryList factory;

	/** Get the current time
	 * Because this only calls time() once every time around the mainloop,
	 * it is much faster than calling time() directly.
	 */
	time_t Time();

	/** Get the total number of currently loaded modules
	 */
	int GetModuleCount();

	/** Find a module by name, and return a Module* to it.
	 * This is preferred over iterating the module lists yourself.
	 * @param name The module name to look up
	 */
	Module* FindModule(const std::string &name);

	/** Bind all ports specified in the configuration file.
	 * @param bail True if the function should bail back to the shell on failure
	 */
	int BindPorts(bool bail);

	/** Returns true if this server has the given port bound to the given address
	 */
	bool HasPort(int port, char* addr);

	/** Binds a socket on an already open file descriptor
	 */
	bool BindSocket(int sockfd, insp_sockaddr client, insp_sockaddr server, int port, char* addr);

	/** Adds a server name to the list of servers we've seen
	 */
	void AddServerName(const std::string &servername);

	/** Finds a cached char* pointer of a server name,
	 * This is used to optimize userrec by storing only the pointer to the name
	 */
	const char* FindServerNamePtr(const std::string &servername);

	/** Returns true if we've seen the given server name before
	 */
	bool FindServerName(const std::string &servername);

	/** Gets the GECOS (description) field of the given server.
	 * If the servername is not that of the local server, the name
	 * is passed to handling modules which will attempt to determine
	 * the GECOS that bleongs to the given servername.
	 */
	std::string GetServerDescription(const char* servername);

	/** Write text to all opers connected to this server
	 */
	void WriteOpers(const char* text, ...);

	/** Write text to all opers connected to this server
	 */
	void WriteOpers(const std::string &text);
	
	/** Find a nickname in the nick hash
	 */
	userrec* FindNick(const std::string &nick);

	/** Find a nickname in the nick hash
	 */
	userrec* FindNick(const char* nick);

	/** Find a channel in the channels hash
	 */
	chanrec* FindChan(const std::string &chan);

	/** Find a channel in the channels hash
	 */
	chanrec* FindChan(const char* chan);

	/** Called by the constructor to load all modules from the config file.
	 */
	void LoadAllModules();

	/** Check for a 'die' tag in the config file, and abort if found
	 */
	void CheckDie();

	/** Check we aren't running as root, and exit if we are
	 */
	void CheckRoot();

	/** Determine the right path for, and open, the logfile
	 */
	void OpenLog(char** argv, int argc);

	/** Convert a user to a pseudoclient, disconnecting the real user
	 */
	bool UserToPseudo(userrec* user, const std::string &message);

	/** Convert a pseudoclient to a real user, discarding the pseudoclient
	 */
	bool PseudoToUser(userrec* alive, userrec* zombie, const std::string &message);

	/** Send a server notice to all local users
	 */
	void ServerNoticeAll(char* text, ...);

	/** Send a server message (PRIVMSG) to all local users
	 */
	void ServerPrivmsgAll(char* text, ...);

	/** Send text to all users with a specific set of modes
	 */
	void WriteMode(const char* modes, int flags, const char* text, ...);

	/** Return true if a channel name is valid
	 */
	bool IsChannel(const char *chname);

	/** Rehash the local server
	 */
	static void Rehash(int status);

	/** Causes the server to exit immediately
	 */
	static void Exit(int status);

	/** Return a count of users, unknown and known connections
	 */
	int UserCount();

	/** Return a count of fully registered connections only
	 */
	int RegisteredUserCount();

	/** Return a count of invisible (umode +i) users only
	 */
	int InvisibleUserCount();

	/** Return a count of opered (umode +o) users only
	 */
	int OperCount();

	/** Return a count of unregistered (before NICK/USER) users only
	 */
	int UnregisteredUserCount();

	/** Return a count of channels on the network
	 */
	long ChannelCount();

	/** Return a count of local users on this server only
	 */
	long LocalUserCount();

	/** Send an error notice to all local users, opered and unopered
	 */
	void SendError(const char *s);

	/** For use with Module::Prioritize().
	 * When the return value of this function is returned from
	 * Module::Prioritize(), this specifies that the module wishes
	 * to be ordered exactly BEFORE 'modulename'. For more information
	 * please see Module::Prioritize().
	 * @param modulename The module your module wants to be before in the call list
	 * @returns a priority ID which the core uses to relocate the module in the list
	 */
	long PriorityBefore(const std::string &modulename);

	/** For use with Module::Prioritize().
	 * When the return value of this function is returned from
	 * Module::Prioritize(), this specifies that the module wishes
	 * to be ordered exactly AFTER 'modulename'. For more information please
	 * see Module::Prioritize().
	 * @param modulename The module your module wants to be after in the call list
	 * @returns a priority ID which the core uses to relocate the module in the list
	 */
	long PriorityAfter(const std::string &modulename);

	/** Publish a 'feature'.
	 * There are two ways for a module to find another module it depends on.
	 * Either by name, using InspIRCd::FindModule, or by feature, using this
	 * function. A feature is an arbitary string which identifies something this
	 * module can do. For example, if your module provides SSL support, but other
	 * modules provide SSL support too, all the modules supporting SSL should
	 * publish an identical 'SSL' feature. This way, any module requiring use
	 * of SSL functions can just look up the 'SSL' feature using FindFeature,
	 * then use the module pointer they are given.
	 * @param FeatureName The case sensitive feature name to make available
	 * @param Mod a pointer to your module class
	 * @returns True on success, false if the feature is already published by
	 * another module.
	 */
	bool PublishFeature(const std::string &FeatureName, Module* Mod);

	/** Unpublish a 'feature'.
	 * When your module exits, it must call this method for every feature it
	 * is providing so that the feature table is cleaned up.
	 * @param FeatureName the feature to remove
	 */
	bool UnpublishFeature(const std::string &FeatureName);

	/** Find a 'feature'.
	 * There are two ways for a module to find another module it depends on.
	 * Either by name, using InspIRCd::FindModule, or by feature, using the
	 * InspIRCd::PublishFeature method. A feature is an arbitary string which
	 * identifies something this module can do. For example, if your module
	 * provides SSL support, but other modules provide SSL support too, all
	 * the modules supporting SSL should publish an identical 'SSL' feature.
	 * To find a module capable of providing the feature you want, simply
	 * call this method with the feature name you are looking for.
	 * @param FeatureName The feature name you wish to obtain the module for
	 * @returns A pointer to a valid module class on success, NULL on failure.
	 */
	Module* FindFeature(const std::string &FeatureName);

	/** Given a pointer to a Module, return its filename
	 */
	const std::string& GetModuleName(Module* m);

	/** Return true if a nickname is valid
	 */
	bool IsNick(const char* n);

	/** Return true if an ident is valid
	 */
	bool IsIdent(const char* n);

	/** Find a username by their file descriptor.
	 * It is preferred to use this over directly accessing the fd_ref_table array.
	 */
        userrec* FindDescriptor(int socket);

	/** Add a new mode to this server's mode parser
	 */
        bool AddMode(ModeHandler* mh, const unsigned char modechar);

	/** Add a new mode watcher to this server's mode parser
	 */
        bool AddModeWatcher(ModeWatcher* mw);

	/** Delete a mode watcher from this server's mode parser
	 */
        bool DelModeWatcher(ModeWatcher* mw);

	/** Add a dns Resolver class to this server's active set
	 */
        bool AddResolver(Resolver* r);

	/** Add a command to this server's command parser
	 */
        void AddCommand(command_t *f);

	/** Send a modechange.
	 * The parameters provided are identical to that sent to the
	 * handler for class cmd_mode.
	 */
        void SendMode(const char **parameters, int pcnt, userrec *user);

	/** Match two strings using pattern matching.
	 * This operates identically to the global function match(),
	 * except for that it takes std::string arguments rather than
	 * const char* ones.
	 */
        bool MatchText(const std::string &sliteral, const std::string &spattern);

	/** Call the handler for a given command.
	 * @return True if the command handler was called successfully
	 */
        bool CallCommandHandler(const std::string &commandname, const char** parameters, int pcnt, userrec* user);

	/** Return true if the command is a module-implemented command and the given parameters are valid for it
	 */
        bool IsValidModuleCommand(const std::string &commandname, int pcnt, userrec* user);

	/** Add a gline and apply it
	 */
        void AddGLine(long duration, const std::string &source, const std::string &reason, const std::string &hostmask);

	/** Add a qline and apply it
	 */
        void AddQLine(long duration, const std::string &source, const std::string &reason, const std::string &nickname);

	/** Add a zline and apply it
	 */
        void AddZLine(long duration, const std::string &source, const std::string &reason, const std::string &ipaddr);

	/** Add a kline and apply it
	 */
        void AddKLine(long duration, const std::string &source, const std::string &reason, const std::string &hostmask);

	/** Add an eline
	 */
        void AddELine(long duration, const std::string &source, const std::string &reason, const std::string &hostmask);

	/** Delete a gline
	 */
        bool DelGLine(const std::string &hostmask);

	/** Delete a qline
	 */
        bool DelQLine(const std::string &nickname);

	/** Delete a zline
	 */
        bool DelZLine(const std::string &ipaddr);

	/** Delete a kline
	 */
        bool DelKLine(const std::string &hostmask);

	/** Delete an eline
	 */
        bool DelELine(const std::string &hostmask);

	/** Return true if the given parameter is a valid nick!user@host mask
	 */
        bool IsValidMask(const std::string &mask);

	/** Add an InspSocket class to the active set
	 */
        void AddSocket(InspSocket* sock);

	/** Remove an InspSocket class from the active set at next time around the loop
	 */
        void RemoveSocket(InspSocket* sock);

	/** Delete a socket immediately without waiting for the next iteration of the mainloop
	 */
        void DelSocket(InspSocket* sock);

	/** Rehash the local server
	 */
        void RehashServer();

	/** Return the channel whos index number matches that provided
	 */
        chanrec* GetChannelIndex(long index);

	/** Dump text to a user target, splitting it appropriately to fit
	 */
        void DumpText(userrec* User, const std::string &LinePrefix, stringstream &TextStream);

	/** Check if the given nickmask matches too many users, send errors to the given user
	 */
	bool NickMatchesEveryone(const std::string &nick, userrec* user);

	/** Check if the given IP mask matches too many users, send errors to the given user
	 */
	bool IPMatchesEveryone(const std::string &ip, userrec* user);

	/** Check if the given hostmask matches too many users, send errors to the given user
	 */
	bool HostMatchesEveryone(const std::string &mask, userrec* user);

	/** Calculate a duration in seconds from a string in the form 1y2w3d4h6m5s
	 */
	long Duration(const char* str);

	/** Attempt to compare an oper password to a string from the config file.
	 * This will be passed to handling modules which will compare the data
	 * against possible hashed equivalents in the input string.
	 */
	int OperPassCompare(const char* data,const char* input);

	/** Check if a given server is a uline.
	 * An empty string returns true, this is by design.
	 */
	bool ULine(const char* server);

	/** Returns the subversion revision ID of this ircd
	 */
	std::string GetRevision();

	/** Returns the full version string of this ircd
	 */
	std::string GetVersionString();

	/** Attempt to write the process id to a given file
	 */
	void WritePID(const std::string &filename);

	/** Returns text describing the last module error
	 */
	char* ModuleError();

	/** Load a given module file
	 */
	bool LoadModule(const char* filename);

	/** Unload a given module file
	 */
	bool UnloadModule(const char* filename);

	/** This constructor initialises all the subsystems and reads the config file.
	 */
	InspIRCd(int argc, char** argv);

	/** Do one iteration of the mainloop
	 */
	void DoOneIteration(bool process_module_sockets);

	/** Output a log message to the ircd.log file
	 */
	void Log(int level, const char* text, ...);

	/** Output a log message to the ircd.log file
	 */
	void Log(int level, const std::string &text);

	/** Begin execution of the server.
	 * NOTE: this function NEVER returns. Internally,
	 * after performing some initialisation routines,
	 * it will repeatedly call DoOneIteration in a loop.
	 */
	int Run();
};

#endif

