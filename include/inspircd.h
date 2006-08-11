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

/* This define is used in place of strcmp when we 
 * want to check if a char* string contains only one
 * letter. Pretty fast, its just two compares and an
 * addition.
 */
#define IS_SINGLE(x,y) ( (*x == y) && (*(x+1) == 0) )

#define DELETE(x) { InspIRCd::Log(DEBUG,"%s:%d: delete()",__FILE__,__LINE__); if (x) { delete x; x = NULL; } else InspIRCd::Log(DEBUG,"Attempt to delete NULL pointer!"); }

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


class InspIRCd : public classbase
{
 private:
	char MODERR[MAXBUF];
	bool expire_run;
	servernamelist servernames;
 
	void EraseFactory(int j);
	void EraseModule(int j);
	void BuildISupport();
	void MoveTo(std::string modulename,int slot);
	void Start();
	void SetSignals(bool SEGVHandler);
	bool DaemonSeed();
	void MakeLowerMap();
	void MoveToLast(std::string modulename);
	void MoveToFirst(std::string modulename);
	void MoveAfter(std::string modulename, std::string after);
	void MoveBefore(std::string modulename, std::string before);

	void ProcessUser(userrec* cu);
	void DoSocketTimeouts(time_t TIME);
	void DoBackgroundUserStuff(time_t TIME);

	bool AllModulesReportReady(userrec* user);

	int ModCount;
	char LogFileName[MAXBUF];

	featurelist Features;

	time_t TIME;
	time_t OLDTIME;

	char ReadBuffer[65535];

 public:
	time_t startup_time;
	ModeParser* ModeGrok;
	CommandParser* Parser;
	SocketEngine* SE;
	serverstats* stats;
	ServerConfig* Config;
	std::vector<InspSocket*> module_sockets;
	InspSocket* socket_ref[MAX_DESCRIPTORS];	/* XXX: This should probably be made private, with inline accessors */
	userrec* fd_ref_table[MAX_DESCRIPTORS];		/* XXX: Ditto */
	user_hash clientlist;
	chan_hash chanlist;
	std::vector<userrec*> local_users;
	std::vector<userrec*> all_opers;
	irc::whowas::whowas_users whowas;
	DNS* Res;
	TimerManager* Timers;
	command_table cmdlist;

	ModuleList modules;
	FactoryList factory;

	time_t Time();

	int GetModuleCount();

	Module* FindModule(const std::string &name);

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

	static void Error(int status);
	static void Rehash(int status);
	static void Exit(int status);

	int usercnt();
	int registered_usercount();
	int usercount_invisible();
	int usercount_opers();
	int usercount_unknown();
	long chancount();
	long local_count();

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

        bool IsUlined(const std::string &server);

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

        long CalcDuration(const std::string &duration);

        bool IsValidMask(const std::string &mask);

        void AddSocket(InspSocket* sock);

        void RemoveSocket(InspSocket* sock);

        void DelSocket(InspSocket* sock);

        void RehashServer();

        chanrec* GetChannelIndex(long index);

        void DumpText(userrec* User, const std::string &LinePrefix, stringstream &TextStream);

	std::string GetRevision();
	std::string GetVersionString();
	void WritePID(const std::string &filename);
	char* ModuleError();
	bool LoadModule(const char* filename);
	bool UnloadModule(const char* filename);
	InspIRCd(int argc, char** argv);
	void DoOneIteration(bool process_module_sockets);
	static void Log(int level, const char* text, ...);
	static void Log(int level, const std::string &text);
	int Run();
};

#endif
