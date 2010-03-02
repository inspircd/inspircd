/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2010 InspIRCd Development Team
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
const long ProtocolVersion = 1203;
const long MinCompatProtocol = 1201;

/** Forward declarations
 */
class SpanningTreeCommands;
class SpanningTreeUtilities;
class CacheRefreshTimer;
class TreeServer;
class Link;
class Autoconnect;

/** This is the main class for the spanningtree module
 */
class ModuleSpanningTree : public Module
{
	SpanningTreeCommands* commands;
	void RedoConfig(Module* mod);

 public:
	SpanningTreeUtilities* Utils;

	CacheRefreshTimer *RefreshTimer;
	/** Set to true if inside a spanningtree call, to prevent sending
	 * xlines and other things back to their source
	 */
	bool loopCall;

	/** Constructor
	 */
	ModuleSpanningTree();
	void init();

	/** Shows /LINKS
	 */
	void ShowLinks(TreeServer* Current, User* user, int hops);

	/** Counts local and remote servers
	 */
	int CountServs();

	/** Handle LINKS command
	 */
	void HandleLinks(const std::vector<std::string>& parameters, User* user);

	/** Show MAP output to a user (recursive)
	 */
	void ShowMap(TreeServer* Current, User* user, int depth, int &line, char* names, int &maxnamew, char* stats);

	/** Handle MAP command
	 */
	bool HandleMap(const std::vector<std::string>& parameters, User* user);

	/** Handle SQUIT
	 */
	ModResult HandleSquit(const std::vector<std::string>& parameters, User* user);

	/** Handle remote WHOIS
	 */
	ModResult HandleRemoteWhois(const std::vector<std::string>& parameters, User* user);

	/** Ping all local servers
	 */
	void DoPingChecks(time_t curtime);

	/** Connect a server locally
	 */
	void ConnectServer(Link* x, Autoconnect* y = NULL);

	/** Connect the next autoconnect server
	 */
	void ConnectServer(Autoconnect* y, bool on_timer);

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

	ModResult OnPreCommand(std::string &command, std::vector<std::string>& parameters, LocalUser *user, bool validated, const std::string &original_line);
	void OnPostCommand(const std::string &command, const std::vector<std::string>& parameters, LocalUser *user, CmdResult result, const std::string &original_line);
	void OnGetServerDescription(const std::string &servername,std::string &description);
	void OnUserConnect(LocalUser* source);
	void OnUserInvite(User* source,User* dest,Channel* channel, time_t);
	void OnPostTopicChange(User* user, Channel* chan, const std::string &topic);
	void OnWallops(User* user, const std::string &text);
	void OnUserNotice(User* user, void* dest, int target_type, const std::string &text, char status, const CUList &exempt_list);
	void OnUserMessage(User* user, void* dest, int target_type, const std::string &text, char status, const CUList &exempt_list);
	void OnBackgroundTimer(time_t curtime);
	void OnUserJoin(Membership* memb, bool sync, bool created, CUList& excepts);
	void OnChangeHost(User* user, const std::string &newhost);
	void OnChangeName(User* user, const std::string &gecos);
	void OnChangeIdent(User* user, const std::string &ident);
	void OnUserPart(Membership* memb, std::string &partmessage, CUList& excepts);
	void OnUserQuit(User* user, const std::string &reason, const std::string &oper_message);
	void OnUserPostNick(User* user, const std::string &oldnick);
	void OnUserKick(User* source, Membership* memb, const std::string &reason, CUList& excepts);
	void OnRemoteKill(User* source, User* dest, const std::string &reason, const std::string &operreason);
	void OnRehash(User* user);
	void OnOper(User* user, const std::string &opertype);
	void OnLine(User* source, const std::string &host, bool adding, char linetype, long duration, const std::string &reason);
	void OnAddLine(User *u, XLine *x);
	void OnDelLine(User *u, XLine *x);
	ModResult OnStats(char statschar, User* user, string_list &results);
	ModResult OnSetAway(User* user, const std::string &awaymsg);
	void OnLoadModule(Module* mod);
	void OnUnloadModule(Module* mod);
	StreamSocket* OnAcceptConnection(int newsock, ListenSocket* from, irc::sockets::sockaddrs* client, irc::sockets::sockaddrs* server);
	CullResult cull();
	~ModuleSpanningTree();
	Version GetVersion();
	void Prioritize();
};

#endif
