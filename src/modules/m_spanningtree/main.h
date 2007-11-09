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

#ifndef __ST_MAIN__
#define __ST_MAIN__

#include "inspircd.h"
#include <stdarg.h>

/** If you make a change which breaks the protocol, increment this.
 * If you  completely change the protocol, completely change the number.
 *
 * IMPORTANT: If you make changes, document your changes here, without fail:
 * http://www.inspircd.org/wiki/List_of_protocol_changes_between_versions
 *
 * Failure to document your protocol changes will result in a painfully
 * painful death by pain. You have been warned.
 */
const long ProtocolVersion = 1200;

/** Forward declarations
 */
class cmd_rconnect;
class cmd_rsquit;
class SpanningTreeUtilities;
class TimeSyncTimer;
class CacheRefreshTimer;
class TreeServer;
class Link;

/** This is the main class for the spanningtree module
 */
class ModuleSpanningTree : public Module
{
	int line;
	int NumServers;
	unsigned int max_local;
	unsigned int max_global;
	cmd_rconnect* command_rconnect;
	cmd_rsquit* command_rsquit;
	SpanningTreeUtilities* Utils;

 public:
	/** Timer for clock syncs
	 */
	TimeSyncTimer *SyncTimer;

	CacheRefreshTimer *RefreshTimer;

	/** Constructor
	 */
	ModuleSpanningTree(InspIRCd* Me);

	/** Shows /LINKS
	 */
	void ShowLinks(TreeServer* Current, User* user, int hops);

	/** Counts local servers
	 */
	int CountLocalServs();

	/** Counts local and remote servers
	 */
	int CountServs();

	/** Handle LINKS command
	 */
	void HandleLinks(const char** parameters, int pcnt, User* user);

	/** Handle LUSERS command
	 */
	void HandleLusers(const char** parameters, int pcnt, User* user);

	/** Show MAP output to a user (recursive)
	 */
	void ShowMap(TreeServer* Current, User* user, int depth, char matrix[128][128], float &totusers, float &totservers);

	/** Handle remote MOTD
	 */
	int HandleMotd(const char** parameters, int pcnt, User* user);

	/** Handle remote ADMIN
	 */
	int HandleAdmin(const char** parameters, int pcnt, User* user);

	/** Handle remote STATS
	 */
	int HandleStats(const char** parameters, int pcnt, User* user);

	/** Handle MAP command
	 */
	void HandleMap(const char** parameters, int pcnt, User* user);

	/** Handle SQUIT
	 */
	int HandleSquit(const char** parameters, int pcnt, User* user);

	/** Handle TIME
	 */
	int HandleTime(const char** parameters, int pcnt, User* user);

	/** Handle remote WHOIS
	 */
	int HandleRemoteWhois(const char** parameters, int pcnt, User* user);

	/** Handle remote MODULES
	 */
	int HandleModules(const char** parameters, int pcnt, User* user);

	/** Ping all local servers
	 */
	void DoPingChecks(time_t curtime);

	/** Connect a server locally
	 */
	void ConnectServer(Link* x);

	/** Check if any servers are due to be autoconnected
	 */
	void AutoConnectServers(time_t curtime);

	/** Handle remote VERSON
	 */
	int HandleVersion(const char** parameters, int pcnt, User* user);

	/** Handle CONNECT
	 */
	int HandleConnect(const char** parameters, int pcnt, User* user);

	/** Send out time sync to all servers
	 */
	void BroadcastTimeSync();

	/** Attempt to send a message to a user
	 */
	void RemoteMessage(User* user, const char* format, ...);

	/** Returns oper-specific MAP information
	 */
	const std::string MapOperInfo(TreeServer* Current);

	/** Display a time as a human readable string
	 */
	std::string TimeToStr(time_t secs);

	/**
	 ** *** MODULE EVENTS ***
	 **/

	virtual int OnPreCommand(const std::string &command, const char** parameters, int pcnt, User *user, bool validated, const std::string &original_line);
	virtual void OnPostCommand(const std::string &command, const char** parameters, int pcnt, User *user, CmdResult result, const std::string &original_line);
	virtual void OnGetServerDescription(const std::string &servername,std::string &description);
	virtual void OnUserInvite(User* source,User* dest,Channel* channel);
	virtual void OnPostLocalTopicChange(User* user, Channel* chan, const std::string &topic);
	virtual void OnWallops(User* user, const std::string &text);
	virtual void OnUserNotice(User* user, void* dest, int target_type, const std::string &text, char status, const CUList &exempt_list);
	virtual void OnUserMessage(User* user, void* dest, int target_type, const std::string &text, char status, const CUList &exempt_list);
	virtual void OnBackgroundTimer(time_t curtime);
	virtual void OnUserJoin(User* user, Channel* channel, bool sync, bool &silent);
	virtual void OnChangeHost(User* user, const std::string &newhost);
	virtual void OnChangeName(User* user, const std::string &gecos);
	virtual void OnUserPart(User* user, Channel* channel, const std::string &partmessage, bool &silent);
	virtual void OnUserConnect(User* user);
	virtual void OnUserQuit(User* user, const std::string &reason, const std::string &oper_message);
	virtual void OnUserPostNick(User* user, const std::string &oldnick);
	virtual void OnUserKick(User* source, User* user, Channel* chan, const std::string &reason, bool &silent);
	virtual void OnRemoteKill(User* source, User* dest, const std::string &reason, const std::string &operreason);
	virtual void OnRehash(User* user, const std::string &parameter);
	virtual void OnOper(User* user, const std::string &opertype);
	void OnLine(User* source, const std::string &host, bool adding, char linetype, long duration, const std::string &reason);
	virtual void OnAddLine(XLine* line, User* user);
	virtual void OnDelLine(XLine* line, User* user);
	virtual void OnMode(User* user, void* dest, int target_type, const std::string &text);
	virtual int OnStats(char statschar, User* user, string_list &results);
	virtual void OnSetAway(User* user);
	virtual void OnCancelAway(User* user);
	virtual void ProtoSendMode(void* opaque, int target_type, void* target, const std::string &modeline);
	virtual void ProtoSendMetaData(void* opaque, int target_type, void* target, const std::string &extname, const std::string &extdata);
	virtual void OnEvent(Event* event);
	virtual ~ModuleSpanningTree();
	virtual Version GetVersion();
	void Prioritize();
};

#endif
