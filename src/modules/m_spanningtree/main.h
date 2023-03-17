/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2019 linuxdaemon <linuxdaemon.irc@gmail.com>
 *   Copyright (C) 2013-2016, 2018 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2013, 2017-2021 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2013 Adam <Adam@anope.org>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2009 Uli Schlachter <psychon@inspircd.org>
 *   Copyright (C) 2007-2008 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2007, 2009 Craig Edwards <brain@inspircd.org>
 *   Copyright (C) 2007 Robin Burchell <robin+git@viroteck.net>
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
#include "tags.h"

/** An enumeration of all known protocol versions.
 *
 * If you introduce new protocol versions please document them here:
 * https://docs.inspircd.org/spanningtree/changes
 */
enum ProtocolVersion {
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
    , public CTCTags::EventListener {
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

    /** Tag for marking services pseudoclients. */
    ServiceTag servicetag;

  public:
    dynamic_reference<DNS::Manager> DNS;

    /** Event provider for message tags. */
    ClientProtocol::MessageTagEvent tagevprov;

    ServerCommandManager CmdManager;

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

    /** Handle remote VERSION
     */
    ModResult HandleVersion(const CommandBase::Params& parameters, User* user);

    /** Handle CONNECT
     */
    ModResult HandleConnect(const CommandBase::Params& parameters, User* user);

    /** Retrieves the event provider for broadcast events. */
    const Events::ModuleEventProvider& GetBroadcastEventProvider() const {
        return broadcasteventprov;
    }

    /** Retrieves the event provider for link events. */
    const Events::ModuleEventProvider& GetLinkEventProvider() const {
        return linkeventprov;
    }

    /** Retrieves the event provider for message events. */
    const Events::ModuleEventProvider& GetMessageEventProvider() const {
        return messageeventprov;
    }

    /** Retrieves the event provider for sync events. */
    const Events::ModuleEventProvider& GetSyncEventProvider() const {
        return synceventprov;
    }

    /**
     ** *** MODULE EVENTS ***
     **/

    ModResult OnPreCommand(std::string& command, CommandBase::Params& parameters,
                           LocalUser* user, bool validated) CXX11_OVERRIDE;
    void OnPostCommand(Command*, const CommandBase::Params& parameters,
                       LocalUser* user, CmdResult result, bool loop) CXX11_OVERRIDE;
    void OnUserConnect(LocalUser* source) CXX11_OVERRIDE;
    void OnUserInvite(User* source, User* dest, Channel* channel, time_t timeout,
                      unsigned int notifyrank, CUList& notifyexcepts) CXX11_OVERRIDE;
    ModResult OnPreTopicChange(User* user, Channel* chan,
                               const std::string& topic) CXX11_OVERRIDE;
    void OnPostTopicChange(User* user, Channel* chan,
                           const std::string &topic) CXX11_OVERRIDE;
    void OnUserPostMessage(User* user, const MessageTarget& target,
                           const MessageDetails& details) CXX11_OVERRIDE;
    void OnUserPostTagMessage(User* user, const MessageTarget& target,
                              const CTCTags::TagMessageDetails& details) CXX11_OVERRIDE;
    void OnBackgroundTimer(time_t curtime) CXX11_OVERRIDE;
    void OnUserJoin(Membership* memb, bool sync, bool created,
                    CUList& excepts) CXX11_OVERRIDE;
    void OnChangeHost(User* user, const std::string &newhost) CXX11_OVERRIDE;
    void OnChangeRealName(User* user, const std::string& real) CXX11_OVERRIDE;
    void OnChangeIdent(User* user, const std::string &ident) CXX11_OVERRIDE;
    void OnUserPart(Membership* memb, std::string &partmessage,
                    CUList& excepts) CXX11_OVERRIDE;
    void OnUserQuit(User* user, const std::string &reason,
                    const std::string &oper_message) CXX11_OVERRIDE;
    void OnUserPostNick(User* user, const std::string &oldnick) CXX11_OVERRIDE;
    void OnUserKick(User* source, Membership* memb, const std::string &reason,
                    CUList& excepts) CXX11_OVERRIDE;
    void OnPreRehash(User* user, const std::string &parameter) CXX11_OVERRIDE;
    void ReadConfig(ConfigStatus& status) CXX11_OVERRIDE;
    void OnOper(User* user, const std::string &opertype) CXX11_OVERRIDE;
    void OnAddLine(User *u, XLine *x) CXX11_OVERRIDE;
    void OnDelLine(User *u, XLine *x) CXX11_OVERRIDE;
    ModResult OnStats(Stats::Context& stats) CXX11_OVERRIDE;
    void OnUserAway(User* user) CXX11_OVERRIDE;
    void OnUserBack(User* user) CXX11_OVERRIDE;
    void OnLoadModule(Module* mod) CXX11_OVERRIDE;
    void OnUnloadModule(Module* mod) CXX11_OVERRIDE;
    ModResult OnAcceptConnection(int newsock, ListenSocket* from,
                                 irc::sockets::sockaddrs* client,
                                 irc::sockets::sockaddrs* server) CXX11_OVERRIDE;
    void OnMode(User* source, User* u, Channel* c, const Modes::ChangeList& modes,
                ModeParser::ModeProcessFlag processflags) CXX11_OVERRIDE;
    void OnShutdown(const std::string& reason) CXX11_OVERRIDE;
    void OnDecodeMetaData(Extensible* target, const std::string& extname,
                          const std::string& extdata) CXX11_OVERRIDE;
    CullResult cull() CXX11_OVERRIDE;
    ~ModuleSpanningTree();
    Version GetVersion() CXX11_OVERRIDE;
    void Prioritize() CXX11_OVERRIDE;
};
