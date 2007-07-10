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
#include "modules.h"

/** If you make a change which breaks the protocol, increment this.
 * If you  completely change the protocol, completely change the number.
 *
 * IMPORTANT: If you make changes, document your changes here, without fail:
 * http://www.inspircd.org/wiki/List_of_protocol_changes_between_versions
 *
 * Failure to document your protocol changes will result in a painfully
 * painful death by pain. You have been warned.
 */
const long ProtocolVersion = 1105;

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
	void ShowLinks(TreeServer* Current, userrec* user, int hops);

	/** Counts local servers
	 */
	int CountLocalServs();

	/** Counts local and remote servers
	 */
	int CountServs();

	/** Handle LINKS command
	 */
	void HandleLinks(const char** parameters, int pcnt, userrec* user);

	/** Handle LUSERS command
	 */
	void HandleLusers(const char** parameters, int pcnt, userrec* user);

	/** Show MAP output to a user (recursive)
	 */
	void ShowMap(TreeServer* Current, userrec* user, int depth, char matrix[128][128], float &totusers, float &totservers);

	/** Handle remote MOTD
	 */
	int HandleMotd(const char** parameters, int pcnt, userrec* user);

	/** Handle remote ADMIN
	 */
	int HandleAdmin(const char** parameters, int pcnt, userrec* user);

	/** Handle remote STATS
	 */
	int HandleStats(const char** parameters, int pcnt, userrec* user);

	/** Handle MAP command
	 */
	void HandleMap(const char** parameters, int pcnt, userrec* user);

	/** Handle SQUIT
	 */
	int HandleSquit(const char** parameters, int pcnt, userrec* user);

	/** Handle TIME
	 */
	int HandleTime(const char** parameters, int pcnt, userrec* user);

	/** Handle remote WHOIS
	 */
	int HandleRemoteWhois(const char** parameters, int pcnt, userrec* user);

	/** Handle remote MODULES
	 */
	int HandleModules(const char** parameters, int pcnt, userrec* user);

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
	int HandleVersion(const char** parameters, int pcnt, userrec* user);

	/** Handle CONNECT
	 */
	int HandleConnect(const char** parameters, int pcnt, userrec* user);

	/** Send out time sync to all servers
	 */
	void BroadcastTimeSync();

	/** Returns oper-specific MAP information
	 */
	const std::string MapOperInfo(TreeServer* Current);

	/** Display a time as a human readable string
	 */
	std::string TimeToStr(time_t secs);

	/**
	 ** *** MODULE EVENTS ***
	 **/

	virtual int OnPreCommand(const std::string &command, const char** parameters, int pcnt, userrec *user, bool validated, const std::string &original_line);
	virtual void OnPostCommand(const std::string &command, const char** parameters, int pcnt, userrec *user, CmdResult result, const std::string &original_line);
	virtual void OnGetServerDescription(const std::string &servername,std::string &description);
	virtual void OnUserInvite(userrec* source,userrec* dest,chanrec* channel);
	virtual void OnPostLocalTopicChange(userrec* user, chanrec* chan, const std::string &topic);
	virtual void OnWallops(userrec* user, const std::string &text);
	virtual void OnUserNotice(userrec* user, void* dest, int target_type, const std::string &text, char status, const CUList &exempt_list);
	virtual void OnUserMessage(userrec* user, void* dest, int target_type, const std::string &text, char status, const CUList &exempt_list);
	virtual void OnBackgroundTimer(time_t curtime);
	virtual void OnUserJoin(userrec* user, chanrec* channel, bool &silent);
	virtual void OnChangeHost(userrec* user, const std::string &newhost);
	virtual void OnChangeName(userrec* user, const std::string &gecos);
	virtual void OnUserPart(userrec* user, chanrec* channel, const std::string &partmessage, bool &silent);
	virtual void OnUserConnect(userrec* user);
	virtual void OnUserQuit(userrec* user, const std::string &reason, const std::string &oper_message);
	virtual void OnUserPostNick(userrec* user, const std::string &oldnick);
	virtual void OnUserKick(userrec* source, userrec* user, chanrec* chan, const std::string &reason, bool &silent);
	virtual void OnRemoteKill(userrec* source, userrec* dest, const std::string &reason, const std::string &operreason);
	virtual void OnRehash(userrec* user, const std::string &parameter);
	virtual void OnOper(userrec* user, const std::string &opertype);
	void OnLine(userrec* source, const std::string &host, bool adding, char linetype, long duration, const std::string &reason);
	virtual void OnAddGLine(long duration, userrec* source, const std::string &reason, const std::string &hostmask);
	virtual void OnAddZLine(long duration, userrec* source, const std::string &reason, const std::string &ipmask);
	virtual void OnAddQLine(long duration, userrec* source, const std::string &reason, const std::string &nickmask);
	virtual void OnAddELine(long duration, userrec* source, const std::string &reason, const std::string &hostmask);
	virtual void OnDelGLine(userrec* source, const std::string &hostmask);
	virtual void OnDelZLine(userrec* source, const std::string &ipmask);
	virtual void OnDelQLine(userrec* source, const std::string &nickmask);
	virtual void OnDelELine(userrec* source, const std::string &hostmask);
	virtual void OnMode(userrec* user, void* dest, int target_type, const std::string &text);
	virtual int OnStats(char statschar, userrec* user, string_list &results);
	virtual void OnSetAway(userrec* user);
	virtual void OnCancelAway(userrec* user);
	virtual void ProtoSendMode(void* opaque, int target_type, void* target, const std::string &modeline);
	virtual void ProtoSendMetaData(void* opaque, int target_type, void* target, const std::string &extname, const std::string &extdata);
	virtual void OnEvent(Event* event);
	virtual ~ModuleSpanningTree();
	virtual Version GetVersion();
	void Implements(char* List);
	Priority Prioritize();
};

#endif
