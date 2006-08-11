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

/* Some misc defines */
#define ERROR -1
#define MAXCOMMAND 32

/* Crucial defines */
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

/* This define is used in place of strcmp when we 
 * want to check if a char* string contains only one
 * letter. Pretty fast, its just two compares and an
 * addition.
 */
#define IS_SINGLE(x,y) ( (*x == y) && (*(x+1) == 0) )

#define DELETE(x) {if (x) { delete x; x = NULL; }}

template<typename T> inline std::string ConvToStr(const T &in)
{
	std::stringstream tmp;
	if (!(tmp << in)) return std::string();
	return tmp.str();
}

class serverstats : public classbase
{
  public:
	unsigned long statsAccept;
	unsigned long statsRefused;
	unsigned long statsUnknown;
	unsigned long statsCollisions;
	unsigned long statsDns;
	unsigned long statsDnsGood;
	unsigned long statsDnsBad;
	unsigned long statsConnects;
	double statsSent;
	double statsRecv;
	unsigned long BoundPortCount;

	serverstats()
	{
		statsAccept = statsRefused = statsUnknown = 0;
		statsCollisions = statsDns = statsDnsGood = 0;
		statsDnsBad = statsConnects = 0;
		statsSent = statsRecv = 0.0;
		BoundPortCount = 0;
	}
};

class XLineManager;

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

	bool HasPort(int port, char* addr);
	bool BindSocket(int sockfd, insp_sockaddr client, insp_sockaddr server, int port, char* addr);

	void AddServerName(const std::string &servername);
	const char* FindServerNamePtr(const std::string &servername);
	bool FindServerName(const std::string &servername);

	std::string GetServerDescription(const char* servername);

	void WriteOpers(const char* text, ...);
	void WriteOpers(const std::string &text);
	
	userrec* FindNick(const std::string &nick);
	userrec* FindNick(const char* nick);

	chanrec* FindChan(const std::string &chan);
	chanrec* FindChan(const char* chan);

	void LoadAllModules();
	void CheckDie();
	void CheckRoot();
	void OpenLog(char** argv, int argc);

	bool UserToPseudo(userrec* user, const std::string &message);
	bool PseudoToUser(userrec* alive, userrec* zombie, const std::string &message);

	void ServerNoticeAll(char* text, ...);
	void ServerPrivmsgAll(char* text, ...);
	void WriteMode(const char* modes, int flags, const char* text, ...);

	bool IsChannel(const char *chname);

	static void Rehash(int status);
	static void Exit(int status);

	int UserCount();
	int RegisteredUserCount();
	int InvisibleUserCount();
	int OperCount();
	int UnregisteredUserCount();
	long ChannelCount();
	long LocalUserCount();

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

	const std::string& GetModuleName(Module* m);

	bool IsNick(const char* n);
	bool IsIdent(const char* n);

        userrec* FindDescriptor(int socket);

        bool AddMode(ModeHandler* mh, const unsigned char modechar);

        bool AddModeWatcher(ModeWatcher* mw);

        bool DelModeWatcher(ModeWatcher* mw);

        bool AddResolver(Resolver* r);

        void AddCommand(command_t *f);

        void SendMode(const char **parameters, int pcnt, userrec *user);

        bool MatchText(const std::string &sliteral, const std::string &spattern);

        bool CallCommandHandler(const std::string &commandname, const char** parameters, int pcnt, userrec* user);

        bool IsValidModuleCommand(const std::string &commandname, int pcnt, userrec* user);

        void AddGLine(long duration, const std::string &source, const std::string &reason, const std::string &hostmask);

        void AddQLine(long duration, const std::string &source, const std::string &reason, const std::string &nickname);

        void AddZLine(long duration, const std::string &source, const std::string &reason, const std::string &ipaddr);

        void AddKLine(long duration, const std::string &source, const std::string &reason, const std::string &hostmask);

        void AddELine(long duration, const std::string &source, const std::string &reason, const std::string &hostmask);

        bool DelGLine(const std::string &hostmask);

        bool DelQLine(const std::string &nickname);

        bool DelZLine(const std::string &ipaddr);

        bool DelKLine(const std::string &hostmask);

        bool DelELine(const std::string &hostmask);

        bool IsValidMask(const std::string &mask);

        void AddSocket(InspSocket* sock);

        void RemoveSocket(InspSocket* sock);

        void DelSocket(InspSocket* sock);

        void RehashServer();

        chanrec* GetChannelIndex(long index);

        void DumpText(userrec* User, const std::string &LinePrefix, stringstream &TextStream);

	bool NickMatchesEveryone(const std::string &nick, userrec* user);
	bool IPMatchesEveryone(const std::string &ip, userrec* user);
	bool HostMatchesEveryone(const std::string &mask, userrec* user);
	long Duration(const char* str);
	int OperPassCompare(const char* data,const char* input);
	bool ULine(const char* server);

	std::string GetRevision();
	std::string GetVersionString();
	void WritePID(const std::string &filename);
	char* ModuleError();
	bool LoadModule(const char* filename);
	bool UnloadModule(const char* filename);
	InspIRCd(int argc, char** argv);
	void DoOneIteration(bool process_module_sockets);
	void Log(int level, const char* text, ...);
	void Log(int level, const std::string &text);
	int Run();
};

#endif
