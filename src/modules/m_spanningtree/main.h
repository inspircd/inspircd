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

#ifndef __ST_MAIN__
#define __ST_MAIN__

#include "inspircd.h"
#include <stdarg.h>

/** If you make a change which breaks the protocol, increment this.
 * If you  completely change the protocol, completely change the number.
 *
 * IMPORTANT: If you make changes, document your changes here, without fail:
 * http://wiki.inspircd.org/List_of_protocol_changes_between_versions
 *
 * Failure to document your protocol changes will result in a painfully
 * painful death by pain. You have been warned.
 */
const long ProtocolVersion = 1201;
const long MinCompatProtocol = 1201;

/** Forward declarations
 */
class CommandRConnect;
class CommandRSQuit;
class SpanningTreeUtilities;
class CacheRefreshTimer;
class TreeServer;
class Link;

/** This is the main class for the spanningtree module
 */
class ModuleSpanningTree : public Module
{
	unsigned int max_local;
	unsigned int max_global;
	CommandRConnect* command_rconnect;
	CommandRSQuit* command_rsquit;
	SpanningTreeUtilities* Utils;

	void RedoConfig(Module* mod, const std::string &name);

 public:
	CacheRefreshTimer *RefreshTimer;
	/** Set to true if inside a spanningtree call, to prevent sending
	 * xlines and other things back to their source
	 */
	bool loopCall;

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
	void HandleLinks(const std::vector<std::string>& parameters, User* user);

	/** Handle LUSERS command
	 */
	void HandleLusers(const std::vector<std::string>& parameters, User* user);

	/** Show MAP output to a user (recursive)
	 */
	void ShowMap(TreeServer* Current, User* user, int depth, int &line, char* names, int &maxnamew, char* stats);

	/** Handle remote MOTD
	 */
	ModResult HandleMotd(const std::vector<std::string>& parameters, User* user);

	/** Handle remote ADMIN
	 */
	ModResult HandleAdmin(const std::vector<std::string>& parameters, User* user);

	/** Handle remote STATS
	 */
	ModResult HandleStats(const std::vector<std::string>& parameters, User* user);

	/** Handle MAP command
	 */
	bool HandleMap(const std::vector<std::string>& parameters, User* user);

	/** Handle SQUIT
	 */
	ModResult HandleSquit(const std::vector<std::string>& parameters, User* user);

	/** Handle TIME
	 */
	ModResult HandleTime(const std::vector<std::string>& parameters, User* user);

	/** Handle remote WHOIS
	 */
	ModResult HandleRemoteWhois(const std::vector<std::string>& parameters, User* user);

	/** Handle remote MODULES
	 */
	ModResult HandleModules(const std::vector<std::string>& parameters, User* user);

	/** Ping all local servers
	 */
	void DoPingChecks(time_t curtime);

	/** Connect a server locally
	 */
	void ConnectServer(Link* x);

	/** Check if any servers are due to be autoconnected
	 */
	void AutoConnectServers(time_t curtime);

	/** Check if any connecting servers should timeout
	 */
	void DoConnectTimeout(time_t curtime);

	/** Handle remote VERSON
	 */
	ModResult HandleVersion(const std::vector<std::string>& parameters, User* user);

	/** Handle CONNECT
	 */
	ModResult HandleConnect(const std::vector<std::string>& parameters, User* user);

	/** Attempt to send a message to a user
	 */
	void RemoteMessage(User* user, const char* format, ...) CUSTOM_PRINTF(3, 4);

	/** Returns oper-specific MAP information
	 */
	const std::string MapOperInfo(TreeServer* Current);

	/** Display a time as a human readable string
	 */
	std::string TimeToStr(time_t secs);

	/**
	 ** *** MODULE EVENTS ***
	 **/

	virtual ModResult OnPreCommand(std::string &command, std::vector<std::string>& parameters, User *user, bool validated, const std::string &original_line);
	virtual void OnPostCommand(const std::string &command, const std::vector<std::string>& parameters, User *user, CmdResult result, const std::string &original_line);
	virtual void OnGetServerDescription(const std::string &servername,std::string &description);
	virtual void OnUserConnect(User* source);
	virtual void OnUserInvite(User* source,User* dest,Channel* channel, time_t);
	virtual void OnPostLocalTopicChange(User* user, Channel* chan, const std::string &topic);
	virtual void OnWallops(User* user, const std::string &text);
	virtual void OnUserNotice(User* user, void* dest, int target_type, const std::string &text, char status, const CUList &exempt_list);
	virtual void OnUserMessage(User* user, void* dest, int target_type, const std::string &text, char status, const CUList &exempt_list);
	virtual void OnBackgroundTimer(time_t curtime);
	virtual void OnUserJoin(User* user, Channel* channel, bool sync, bool &silent, bool created);
	virtual ModResult OnChangeLocalUserHost(User* user, const std::string &newhost);
	virtual void OnChangeName(User* user, const std::string &gecos);
	virtual void OnChangeIdent(User* user, const std::string &ident);
	virtual void OnUserPart(User* user, Channel* channel, std::string &partmessage, bool &silent);
	virtual void OnUserQuit(User* user, const std::string &reason, const std::string &oper_message);
	virtual void OnUserPostNick(User* user, const std::string &oldnick);
	virtual void OnUserKick(User* source, User* user, Channel* chan, const std::string &reason, bool &silent);
	virtual void OnRemoteKill(User* source, User* dest, const std::string &reason, const std::string &operreason);
	virtual void OnPreRehash(User* user, const std::string &parameter);
	virtual void OnRehash(User* user);
	virtual void OnOper(User* user, const std::string &opertype);
	void OnLine(User* source, const std::string &host, bool adding, char linetype, long duration, const std::string &reason);
	virtual void OnAddLine(User *u, XLine *x);
	virtual void OnDelLine(User *u, XLine *x);
	virtual void OnMode(User* user, void* dest, int target_type, const std::vector<std::string> &text, const std::vector<TranslateType> &translate);
	virtual ModResult OnStats(char statschar, User* user, string_list &results);
	virtual ModResult OnSetAway(User* user, const std::string &awaymsg);
	virtual void ProtoSendMode(void* opaque, TargetTypeFlags target_type, void* target, const std::vector<std::string> &modeline, const std::vector<TranslateType> &translate);
	virtual void ProtoSendMetaData(void* opaque, Extensible* target, const std::string &extname, const std::string &extdata);
	virtual std::string ProtoTranslate(Extensible* item);
	virtual void OnEvent(Event* event);
	virtual void OnLoadModule(Module* mod,const std::string &name);
	virtual void OnUnloadModule(Module* mod,const std::string &name);
	virtual ~ModuleSpanningTree();
	virtual Version GetVersion();
	void Prioritize();
};

#endif
