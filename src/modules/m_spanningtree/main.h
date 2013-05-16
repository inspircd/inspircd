/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2008 Thomas Stagner <aquanight@inspircd.org>
 *   Copyright (C) 2007-2008 Craig Edwards <craigedwards@brainbox.cc>
 *   Copyright (C) 2007 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *
 * This file is part of InspIRCd.  InspIRCd is free software: you can
 * redistribute it and/or modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation, version 2.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */


#pragma once

#include "inspircd.h"

#include "modules/dns.h"
/** If you make a change which breaks the protocol, increment this.
 * If you  completely change the protocol, completely change the number.
 *
 * IMPORTANT: If you make changes, document your changes here, without fail:
 * http://wiki.inspircd.org/List_of_protocol_changes_between_versions
 *
 * Failure to document your protocol changes will result in a painfully
 * painful death by pain. You have been warned.
 */
const long ProtocolVersion = 1205;
const long MinCompatProtocol = 1202;

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
	void LocalMessage(User* user, void* dest, int target_type, const std::string &text, char status, const CUList &exempt_list, const char* message_type);

 public:
	dynamic_reference<DNS::Manager> DNS;

	SpanningTreeUtilities* Utils;

	/** Set to true if inside a spanningtree call, to prevent sending
	 * xlines and other things back to their source
	 */
	bool loopCall;

	/** Constructor
	 */
	ModuleSpanningTree();
	void init() CXX11_OVERRIDE;

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

	ModResult OnPreCommand(std::string &command, std::vector<std::string>& parameters, LocalUser *user, bool validated, const std::string &original_line) CXX11_OVERRIDE;
	void OnPostCommand(const std::string &command, const std::vector<std::string>& parameters, LocalUser *user, CmdResult result, const std::string &original_line) CXX11_OVERRIDE;
	void OnGetServerDescription(const std::string &servername,std::string &description) CXX11_OVERRIDE;
	void OnUserConnect(LocalUser* source) CXX11_OVERRIDE;
	void OnUserInvite(User* source,User* dest,Channel* channel, time_t) CXX11_OVERRIDE;
	void OnPostTopicChange(User* user, Channel* chan, const std::string &topic) CXX11_OVERRIDE;
	void OnWallops(User* user, const std::string &text) CXX11_OVERRIDE;
	void OnUserMessage(User* user, void* dest, int target_type, const std::string& text, char status, const CUList& exempt_list, MessageType msgtype) CXX11_OVERRIDE;
	void OnBackgroundTimer(time_t curtime) CXX11_OVERRIDE;
	void OnUserJoin(Membership* memb, bool sync, bool created, CUList& excepts) CXX11_OVERRIDE;
	void OnChangeHost(User* user, const std::string &newhost) CXX11_OVERRIDE;
	void OnChangeName(User* user, const std::string &gecos) CXX11_OVERRIDE;
	void OnChangeIdent(User* user, const std::string &ident) CXX11_OVERRIDE;
	void OnUserPart(Membership* memb, std::string &partmessage, CUList& excepts) CXX11_OVERRIDE;
	void OnUserQuit(User* user, const std::string &reason, const std::string &oper_message) CXX11_OVERRIDE;
	void OnUserPostNick(User* user, const std::string &oldnick) CXX11_OVERRIDE;
	void OnUserKick(User* source, Membership* memb, const std::string &reason, CUList& excepts) CXX11_OVERRIDE;
	void OnRemoteKill(User* source, User* dest, const std::string &reason, const std::string &operreason) CXX11_OVERRIDE;
	void OnPreRehash(User* user, const std::string &parameter) CXX11_OVERRIDE;
	void OnRehash(User* user) CXX11_OVERRIDE;
	void OnOper(User* user, const std::string &opertype) CXX11_OVERRIDE;
	void OnLine(User* source, const std::string &host, bool adding, char linetype, long duration, const std::string &reason);
	void OnAddLine(User *u, XLine *x) CXX11_OVERRIDE;
	void OnDelLine(User *u, XLine *x) CXX11_OVERRIDE;
	void OnMode(User* user, void* dest, int target_type, const std::vector<std::string> &text, const std::vector<TranslateType> &translate) CXX11_OVERRIDE;
	ModResult OnStats(char statschar, User* user, string_list &results) CXX11_OVERRIDE;
	ModResult OnSetAway(User* user, const std::string &awaymsg) CXX11_OVERRIDE;
	void ProtoSendMode(void* opaque, TargetTypeFlags target_type, void* target, const std::vector<std::string> &modeline, const std::vector<TranslateType> &translate);
	void ProtoSendMetaData(void* opaque, Extensible* target, const std::string &extname, const std::string &extdata);
	void OnLoadModule(Module* mod) CXX11_OVERRIDE;
	void OnUnloadModule(Module* mod) CXX11_OVERRIDE;
	ModResult OnAcceptConnection(int newsock, ListenSocket* from, irc::sockets::sockaddrs* client, irc::sockets::sockaddrs* server) CXX11_OVERRIDE;
	CullResult cull();
	~ModuleSpanningTree();
	Version GetVersion() CXX11_OVERRIDE;
	void Prioritize();
};
