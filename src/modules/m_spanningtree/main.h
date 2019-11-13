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
#include "event.h"
#include "modules/dns.h"
#include "modules/ssl.h"
#include "modules/stats.h"
#include "modules/ctctags.h"
#include "modules/server.h"
#include "servercommand.h"
#include "commands.h"
#include "protocolinterface.h"

/** An enumeration of all known protocol versions.
 *
 * If you introduce new protocol versions please document them here:
 * https://docs.inspircd.org/spanningtree/changes
 */
enum ProtocolVersion
{
	/** The linking protocol version introduced in InspIRCd v2.0. */
	PROTO_INSPIRCD_20 = 1202,

	/** The linking protocol version introduced in InspIRCd v2.1 alpha 0. */
	PROTO_INSPIRCD_21_A0 = 1203,

	/** The linking protocol version introduced in InspIRCd v2.1 beta 2. */
	PROTO_INSPIRCD_21_B2 = 1204,

	/** The linking protocol version introduced in InspIRCd v3.0. */
	PROTO_INSPIRCD_30 = 1205,

	/** The oldest version of the protocol that we support. */
	PROTO_OLDEST = PROTO_INSPIRCD_20,

	/** The newest version of the protocol that we support. */
	PROTO_NEWEST = PROTO_INSPIRCD_30
};

/** Forward declarations
 */
class SpanningTreeUtilities;
class CacheRefreshTimer;
class TreeServer;
class Link;
class Autoconnect;

/** This is the main class for the spanningtree module
 */
class ModuleSpanningTree
	: public Module
	, public Away::EventListener
	, public Stats::EventListener
	, public CTCTags::EventListener
{
	/** Client to server commands, registered in the core
	 */
	CommandRConnect rconnect;
	CommandRSQuit rsquit;
	CommandMap map;

	/** Server to server only commands, not registered in the core
	 */
	SpanningTreeCommands commands;

	/** Next membership id assigned when a local user joins a channel
	 */
	Membership::Id currmembid;

	/** The specialized ProtocolInterface that is assigned to ServerInstance->PI on load
	 */
	SpanningTreeProtocolInterface protocolinterface;

	/** Event provider for our broadcast events. */
	Events::ModuleEventProvider broadcasteventprov;

	/** Event provider for our link events. */
	Events::ModuleEventProvider linkeventprov;

	/** Event provider for our message events. */
	Events::ModuleEventProvider messageeventprov;

	/** Event provider for our sync events. */
	Events::ModuleEventProvider synceventprov;

	/** API for accessing user SSL certificates. */
	UserCertificateAPI sslapi;

 public:
	dynamic_reference<DNS::Manager> DNS;

	/** Event provider for message tags. */
	Events::ModuleEventProvider tagevprov;

	ServerCommandManager CmdManager;

	/** Set to true if inside a spanningtree call, to prevent sending
	 * xlines and other things back to their source
	 */
	bool loopCall;

	/** Constructor
	 */
	ModuleSpanningTree();
	void init() override;

	/** Shows /LINKS
	 */
	void ShowLinks(TreeServer* Current, User* user, int hops);

	/** Handle LINKS command
	 */
	void HandleLinks(const CommandBase::Params& parameters, User* user);

	/** Handle SQUIT
	 */
	ModResult HandleSquit(const CommandBase::Params& parameters, User* user);

	/** Handle remote WHOIS
	 */
	ModResult HandleRemoteWhois(const CommandBase::Params& parameters, User* user);

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
	ModResult HandleVersion(const CommandBase::Params& parameters, User* user);

	/** Handle CONNECT
	 */
	ModResult HandleConnect(const CommandBase::Params& parameters, User* user);

	/** Retrieves the event provider for broadcast events. */
	const Events::ModuleEventProvider& GetBroadcastEventProvider() const { return broadcasteventprov; }

	/** Retrieves the event provider for link events. */
	const Events::ModuleEventProvider& GetLinkEventProvider() const { return linkeventprov; }

	/** Retrieves the event provider for message events. */
	const Events::ModuleEventProvider& GetMessageEventProvider() const { return messageeventprov; }

	/** Retrieves the event provider for sync events. */
	const Events::ModuleEventProvider& GetSyncEventProvider() const { return synceventprov; }

	/**
	 ** *** MODULE EVENTS ***
	 **/

	ModResult OnPreCommand(std::string& command, CommandBase::Params& parameters, LocalUser* user, bool validated) override;
	void OnPostCommand(Command*, const CommandBase::Params& parameters, LocalUser* user, CmdResult result, bool loop) override;
	void OnUserConnect(LocalUser* source) override;
	void OnUserInvite(User* source, User* dest, Channel* channel, time_t timeout, unsigned int notifyrank, CUList& notifyexcepts) override;
	ModResult OnPreTopicChange(User* user, Channel* chan, const std::string& topic) override;
	void OnPostTopicChange(User* user, Channel* chan, const std::string &topic) override;
	void OnUserPostMessage(User* user, const MessageTarget& target, const MessageDetails& details) override;
	void OnUserPostTagMessage(User* user, const MessageTarget& target, const CTCTags::TagMessageDetails& details) override;
	void OnBackgroundTimer(time_t curtime) override;
	void OnUserJoin(Membership* memb, bool sync, bool created, CUList& excepts) override;
	void OnChangeHost(User* user, const std::string &newhost) override;
	void OnChangeRealName(User* user, const std::string& real) override;
	void OnChangeIdent(User* user, const std::string &ident) override;
	void OnUserPart(Membership* memb, std::string &partmessage, CUList& excepts) override;
	void OnUserQuit(User* user, const std::string &reason, const std::string &oper_message) override;
	void OnUserPostNick(User* user, const std::string &oldnick) override;
	void OnUserKick(User* source, Membership* memb, const std::string &reason, CUList& excepts) override;
	void OnPreRehash(User* user, const std::string &parameter) override;
	void ReadConfig(ConfigStatus& status) override;
	void OnOper(User* user, const std::string &opertype) override;
	void OnAddLine(User *u, XLine *x) override;
	void OnDelLine(User *u, XLine *x) override;
	ModResult OnStats(Stats::Context& stats) override;
	void OnUserAway(User* user) override;
	void OnUserBack(User* user) override;
	void OnLoadModule(Module* mod) override;
	void OnUnloadModule(Module* mod) override;
	ModResult OnAcceptConnection(int newsock, ListenSocket* from, irc::sockets::sockaddrs* client, irc::sockets::sockaddrs* server) override;
	void OnMode(User* source, User* u, Channel* c, const Modes::ChangeList& modes, ModeParser::ModeProcessFlag processflags) override;
	void OnShutdown(const std::string& reason) override;
	CullResult cull() override;
	~ModuleSpanningTree();
	Version GetVersion() override;
	void Prioritize() override;
};
