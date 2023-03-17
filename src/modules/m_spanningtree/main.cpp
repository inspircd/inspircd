/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2021 Herman <GermanAizek@yandex.ru>
 *   Copyright (C) 2020 Matt Schatz <genius3000@g3k.solutions>
 *   Copyright (C) 2019 linuxdaemon <linuxdaemon.irc@gmail.com>
 *   Copyright (C) 2013, 2017-2022 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2013, 2016 Adam <Adam@anope.org>
 *   Copyright (C) 2012-2016, 2018 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2007-2009 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2007-2009 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2005, 2007-2009 Craig Edwards <brain@inspircd.org>
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


#include "inspircd.h"
#include "socket.h"
#include "xline.h"
#include "iohook.h"

#include "resolvers.h"
#include "main.h"
#include "utils.h"
#include "treeserver.h"
#include "link.h"
#include "treesocket.h"
#include "commands.h"
#include "translate.h"

ModuleSpanningTree::ModuleSpanningTree()
    : Away::EventListener(this)
    , Stats::EventListener(this)
    , CTCTags::EventListener(this)
    , rconnect(this)
    , rsquit(this)
    , map(this)
    , commands(this)
    , currmembid(0)
    , broadcasteventprov(this, "event/server-broadcast")
    , linkeventprov(this, "event/server-link")
    , messageeventprov(this, "event/server-message")
    , synceventprov(this, "event/server-sync")
    , sslapi(this)
    , servicetag(this)
    , DNS(this, "DNS")
    , tagevprov(this)
    , loopCall(false) {
}

SpanningTreeCommands::SpanningTreeCommands(ModuleSpanningTree* module)
    : svsjoin(module), svspart(module), svsnick(module), metadata(module),
      uid(module), opertype(module), fjoin(module), ijoin(module), resync(module),
      fmode(module), ftopic(module), fhost(module), fident(module), fname(module),
      away(module), addline(module), delline(module), encap(module), idle(module),
      nick(module), ping(module), pong(module), save(module),
      server(module), squit(module), snonotice(module),
      endburst(module), sinfo(module), num(module) {
}

namespace {
void SetLocalUsersServer(Server* newserver) {
    // Does not change the server of quitting users because those are not in the list

    ServerInstance->FakeClient->server = newserver;
    const UserManager::LocalList& list = ServerInstance->Users.GetLocalUsers();
    for (UserManager::LocalList::const_iterator i = list.begin(); i != list.end();
            ++i) {
        (*i)->server = newserver;
    }
}

void ResetMembershipIds() {
    // Set all membership ids to 0
    const UserManager::LocalList& list = ServerInstance->Users.GetLocalUsers();
    for (UserManager::LocalList::iterator i = list.begin(); i != list.end(); ++i) {
        LocalUser* user = *i;
        for (User::ChanList::iterator j = user->chans.begin(); j != user->chans.end();
                ++j) {
            (*j)->id = 0;
        }
    }
}
}

void ModuleSpanningTree::init() {
    ServerInstance->SNO->EnableSnomask('l', "LINK");

    ResetMembershipIds();

    Utils = new SpanningTreeUtilities(this);
    Utils->TreeRoot = new TreeServer;

    ServerInstance->PI = &protocolinterface;

    delete ServerInstance->FakeClient->server;
    SetLocalUsersServer(Utils->TreeRoot);
}

void ModuleSpanningTree::ShowLinks(TreeServer* Current, User* user, int hops) {
    std::string Parent = Utils->TreeRoot->GetName();
    if (Current->GetParent()) {
        Parent = Current->GetParent()->GetName();
    }

    const TreeServer::ChildServers& children = Current->GetChildren();
    for (TreeServer::ChildServers::const_iterator i = children.begin();
            i != children.end(); ++i) {
        TreeServer* server = *i;
        if ((server->Hidden) || ((Utils->HideULines) && (server->IsULine()))) {
            if (user->IsOper()) {
                ShowLinks(server, user, hops+1);
            }
        } else {
            ShowLinks(server, user, hops+1);
        }
    }
    /* Don't display the line if its a uline, hide ulines is on, and the user isn't an oper */
    if ((Utils->HideULines) && (Current->IsULine()) && (!user->IsOper())) {
        return;
    }
    /* Or if the server is hidden and they're not an oper */
    else if ((Current->Hidden) && (!user->IsOper())) {
        return;
    }

    user->WriteNumeric(RPL_LINKS, Current->GetName(),
                       (((Utils->FlatLinks)
                         && (!user->IsOper())) ? ServerInstance->Config->GetServerName() : Parent),
                       InspIRCd::Format("%d %s", (((Utils->FlatLinks)
                                        && (!user->IsOper())) ? 0 : hops), Current->GetDesc().c_str()));
}

void ModuleSpanningTree::HandleLinks(const CommandBase::Params& parameters,
                                     User* user) {
    ShowLinks(Utils->TreeRoot,user,0);
    user->WriteNumeric(RPL_ENDOFLINKS, '*', "End of /LINKS list.");
}

void ModuleSpanningTree::ConnectServer(Autoconnect* a, bool on_timer) {
    if (!a) {
        return;
    }
    for(unsigned int j=0; j < a->servers.size(); j++) {
        if (Utils->FindServer(a->servers[j])) {
            // found something in this block. Should the server fail,
            // we want to start at the start of the list, not in the
            // middle where we left off
            a->position = -1;
            return;
        }
    }
    if (on_timer && a->position >= 0) {
        return;
    }
    if (!on_timer && a->position < 0) {
        return;
    }

    a->position++;
    while (a->position < (int)a->servers.size()) {
        Link* x = Utils->FindLink(a->servers[a->position]);
        if (x) {
            ServerInstance->SNO->WriteToSnoMask('l',
                                                "AUTOCONNECT: Auto-connecting server \002%s\002", x->Name.c_str());
            ConnectServer(x, a);
            return;
        }
        a->position++;
    }
    // Autoconnect chain has been fully iterated; start at the beginning on the
    // next AutoConnectServers run
    a->position = -1;
}

void ModuleSpanningTree::ConnectServer(Link* x, Autoconnect* y) {
    if (InspIRCd::Match(ServerInstance->Config->ServerName, x->Name,
                        ascii_case_insensitive_map)) {
        ServerInstance->SNO->WriteToSnoMask('l', "CONNECT: Not connecting to myself.");
        return;
    }

    irc::sockets::sockaddrs sa;
    if (x->IPAddr.find('/') != std::string::npos) {
        if (!irc::sockets::isunix(x->IPAddr) || !irc::sockets::untosa(x->IPAddr, sa)) {
            // We don't use the family() != AF_UNSPEC check below for UNIX sockets as
            // that results in a DNS lookup.
            ServerInstance->SNO->WriteToSnoMask('l',
                                                "CONNECT: Error connecting \002%s\002: %s is not a UNIX socket!",
                                                x->Name.c_str(), x->IPAddr.c_str());
            return;
        }
    } else {
        // If this fails then the IP sa will be AF_UNSPEC.
        irc::sockets::aptosa(x->IPAddr, x->Port, sa);
    }

    /* Do we already have an IP? If so, no need to resolve it. */
    if (sa.family() != AF_UNSPEC) {
        // Create a TreeServer object that will start connecting immediately in the background
        TreeSocket* newsocket = new TreeSocket(x, y, sa);
        if (!newsocket->HasFd()) {
            ServerInstance->SNO->WriteToSnoMask('l',
                                                "CONNECT: Error connecting \002%s\002: %s.",
                                                x->Name.c_str(), newsocket->getError().c_str());
            ServerInstance->GlobalCulls.AddItem(newsocket);
        }
    } else if (!DNS) {
        ServerInstance->SNO->WriteToSnoMask('l',
                                            "CONNECT: Error connecting \002%s\002: Hostname given and core_dns is not loaded, unable to resolve.",
                                            x->Name.c_str());
    } else {
        // Guess start_type from bindip aftype
        DNS::QueryType start_type = DNS::QUERY_AAAA;
        irc::sockets::sockaddrs bind;
        if ((!x->Bind.empty()) && (irc::sockets::aptosa(x->Bind, 0, bind))) {
            if (bind.family() == AF_INET) {
                start_type = DNS::QUERY_A;
            }
        }

        ServernameResolver* snr = new ServernameResolver(*DNS, x->IPAddr, x, start_type,
                y);
        try {
            DNS->Process(snr);
        } catch (DNS::Exception& e) {
            delete snr;
            ServerInstance->SNO->WriteToSnoMask('l',
                                                "CONNECT: Error connecting \002%s\002: %s.",x->Name.c_str(),
                                                e.GetReason().c_str());
            ConnectServer(y, false);
        }
    }
}

void ModuleSpanningTree::AutoConnectServers(time_t curtime) {
    for (std::vector<reference<Autoconnect> >::iterator i =
                Utils->AutoconnectBlocks.begin(); i < Utils->AutoconnectBlocks.end(); ++i) {
        Autoconnect* x = *i;
        if (curtime >= x->NextConnectTime) {
            x->NextConnectTime = curtime + x->Period;
            ConnectServer(x, true);
        }
    }
}

void ModuleSpanningTree::DoConnectTimeout(time_t curtime) {
    SpanningTreeUtilities::TimeoutList::iterator i = Utils->timeoutlist.begin();
    while (i != Utils->timeoutlist.end()) {
        TreeSocket* s = i->first;
        std::pair<std::string, unsigned int> p = i->second;
        SpanningTreeUtilities::TimeoutList::iterator me = i;
        i++;
        if (s->GetLinkState() == DYING) {
            Utils->timeoutlist.erase(me);
            s->Close();
        } else if (curtime > s->age + (time_t)p.second) {
            ServerInstance->SNO->WriteToSnoMask('l',
                                                "CONNECT: Error connecting \002%s\002 (timeout of %u seconds)",p.first.c_str(),
                                                p.second);
            Utils->timeoutlist.erase(me);
            s->Close();
        }
    }
}

ModResult ModuleSpanningTree::HandleVersion(const CommandBase::Params&
        parameters, User* user) {
    // We've already confirmed that !parameters.empty(), so this is safe
    TreeServer* found = Utils->FindServerMask(parameters[0]);
    if (found) {
        if (found == Utils->TreeRoot) {
            // Pass to default VERSION handler.
            return MOD_RES_PASSTHRU;
        }

        // If an oper wants to see the version then show the full version string instead of the normal,
        // but only if it is non-empty.
        // If it's empty it might be that the server is still syncing (full version hasn't arrived yet)
        // or the server is a 2.0 server and does not send a full version.
        bool showfull = ((user->IsOper()) && (!found->GetFullVersion().empty()));

        Numeric::Numeric numeric(RPL_VERSION);
        irc::tokenstream tokens(showfull ? found->GetFullVersion() :
                                found->GetVersion());
        for (std::string token; tokens.GetTrailing(token); ) {
            numeric.push(token);
        }
        user->WriteNumeric(numeric);
    } else {
        user->WriteNumeric(ERR_NOSUCHSERVER, parameters[0], "No such server");
    }
    return MOD_RES_DENY;
}

ModResult ModuleSpanningTree::HandleConnect(const CommandBase::Params&
        parameters, User* user) {
    for (std::vector<reference<Link> >::iterator i = Utils->LinkBlocks.begin();
            i < Utils->LinkBlocks.end(); i++) {
        Link* x = *i;
        if (InspIRCd::Match(x->Name, parameters[0], ascii_case_insensitive_map)) {
            if (InspIRCd::Match(ServerInstance->Config->ServerName, x->Name,
                                ascii_case_insensitive_map)) {
                user->WriteRemoteNotice(
                    InspIRCd::Format("*** CONNECT: Server \002%s\002 is ME, not connecting.",
                                     x->Name.c_str()));
                return MOD_RES_DENY;
            }

            TreeServer* CheckDupe = Utils->FindServer(x->Name);
            if (!CheckDupe) {
                user->WriteRemoteNotice(
                    InspIRCd::Format("*** CONNECT: Connecting to server: \002%s\002 (%s:%d)",
                                     x->Name.c_str(), (x->HiddenFromStats ? "<hidden>" : x->IPAddr.c_str()),
                                     x->Port));
                ConnectServer(x);
                return MOD_RES_DENY;
            } else {
                user->WriteRemoteNotice(
                    InspIRCd::Format("*** CONNECT: Server \002%s\002 already exists on the network and is connected via \002%s\002",
                                     x->Name.c_str(), CheckDupe->GetParent()->GetName().c_str()));
                return MOD_RES_DENY;
            }
        }
    }
    user->WriteRemoteNotice(
        InspIRCd::Format("*** CONNECT: No server matching \002%s\002 could be found in the config file.",
                         parameters[0].c_str()));
    return MOD_RES_DENY;
}

void ModuleSpanningTree::OnUserInvite(User* source, User* dest,
                                      Channel* channel, time_t expiry, unsigned int notifyrank,
                                      CUList& notifyexcepts) {
    if (IS_LOCAL(source)) {
        CmdBuilder params(source, "INVITE");
        params.push(dest->uuid);
        params.push(channel->name);
        params.push_int(channel->age);
        params.push(ConvToStr(expiry));
        params.Broadcast();
    }
}

ModResult ModuleSpanningTree::OnPreTopicChange(User* user, Channel* chan,
        const std::string& topic) {
    // XXX: Deny topic changes if the current topic set time is the current time or is in the future because
    // other servers will drop our FTOPIC. This restriction will be removed when the protocol is updated.
    if ((chan->topicset >= ServerInstance->Time())
            && (Utils->serverlist.size() > 1)) {
        user->WriteNumeric(ERR_UNAVAILRESOURCE, chan->name, "Retry topic change later");
        return MOD_RES_DENY;
    }
    return MOD_RES_PASSTHRU;
}

void ModuleSpanningTree::OnPostTopicChange(User* user, Channel* chan,
        const std::string &topic) {
    // Drop remote events on the floor.
    if (!IS_LOCAL(user)) {
        return;
    }

    CommandFTopic::Builder(user, chan).Broadcast();
}

void ModuleSpanningTree::OnUserPostMessage(User* user,
        const MessageTarget& target, const MessageDetails& details) {
    if (!IS_LOCAL(user)) {
        return;
    }

    const char* message_type = (details.type == MSG_PRIVMSG ? "PRIVMSG" : "NOTICE");
    switch (target.type) {
    case MessageTarget::TYPE_USER: {
        User* d = target.Get<User>();
        if (!IS_LOCAL(d)) {
            CmdBuilder params(user, message_type);
            params.push_tags(details.tags_out);
            params.push(d->uuid);
            params.push_last(details.text);
            params.Unicast(d);
        }
        break;
    }
    case MessageTarget::TYPE_CHANNEL: {
        Utils->SendChannelMessage(user, target.Get<Channel>(), details.text,
                                  target.status, details.tags_out, details.exemptions, message_type);
        break;
    }
    case MessageTarget::TYPE_SERVER: {
        const std::string* serverglob = target.Get<std::string>();
        CmdBuilder par(user, message_type);
        par.push_tags(details.tags_out);
        par.push(std::string("$") + *serverglob);
        par.push_last(details.text);
        par.Broadcast();
        break;
    }
    }
}

void ModuleSpanningTree::OnUserPostTagMessage(User* user,
        const MessageTarget& target, const CTCTags::TagMessageDetails& details) {
    if (!IS_LOCAL(user)) {
        return;
    }

    switch (target.type) {
    case MessageTarget::TYPE_USER: {
        User* d = target.Get<User>();
        if (!IS_LOCAL(d)) {
            CmdBuilder params(user, "TAGMSG");
            params.push_tags(details.tags_out);
            params.push(d->uuid);
            params.Unicast(d);
        }
        break;
    }
    case MessageTarget::TYPE_CHANNEL: {
        Utils->SendChannelMessage(user, target.Get<Channel>(), "", target.status,
                                  details.tags_out, details.exemptions, "TAGMSG");
        break;
    }
    case MessageTarget::TYPE_SERVER: {
        const std::string* serverglob = target.Get<std::string>();
        CmdBuilder par(user, "TAGMSG");
        par.push_tags(details.tags_out);
        par.push(std::string("$") + *serverglob);
        par.Broadcast();
        break;
    }
    }
}

void ModuleSpanningTree::OnBackgroundTimer(time_t curtime) {
    AutoConnectServers(curtime);
    DoConnectTimeout(curtime);
}

void ModuleSpanningTree::OnUserConnect(LocalUser* user) {
    if (user->quitting) {
        return;
    }

    // Create the lazy ssl_cert metadata for this user if not already created.
    if (sslapi) {
        sslapi->GetCertificate(user);
    }

    CommandUID::Builder(user).Broadcast();

    if (user->IsOper()) {
        CommandOpertype::Builder(user).Broadcast();
    }

    for(Extensible::ExtensibleStore::const_iterator i = user->GetExtList().begin();
            i != user->GetExtList().end(); i++) {
        ExtensionItem* item = i->first;
        std::string value = item->ToNetwork(user, i->second);
        if (!value.empty()) {
            ServerInstance->PI->SendMetaData(user, item->name, value);
        }
    }

    Utils->TreeRoot->UserCount++;
}

void ModuleSpanningTree::OnUserJoin(Membership* memb, bool sync,
                                    bool created_by_local, CUList& excepts) {
    // Only do this for local users
    if (!IS_LOCAL(memb->user)) {
        return;
    }

    // Assign the current membership id to the new Membership and increase it
    memb->id = currmembid++;

    if (created_by_local) {
        CommandFJoin::Builder params(memb->chan);
        params.add(memb);
        params.finalize();
        params.Broadcast();
        Utils->SendListLimits(memb->chan, NULL);
    } else {
        CmdBuilder params(memb->user, "IJOIN");
        params.push(memb->chan->name);
        params.push_int(memb->id);
        if (!memb->modes.empty()) {
            params.push(ConvToStr(memb->chan->age));
            params.push(memb->modes);
        }
        params.Broadcast();
    }
}

void ModuleSpanningTree::OnChangeHost(User* user, const std::string &newhost) {
    if (user->registered != REG_ALL || !IS_LOCAL(user)) {
        return;
    }

    CmdBuilder(user, "FHOST").push(newhost).Broadcast();
}

void ModuleSpanningTree::OnChangeRealName(User* user, const std::string& real) {
    if (user->registered != REG_ALL || !IS_LOCAL(user)) {
        return;
    }

    CmdBuilder(user, "FNAME").push_last(real).Broadcast();
}

void ModuleSpanningTree::OnChangeIdent(User* user, const std::string &ident) {
    if ((user->registered != REG_ALL) || (!IS_LOCAL(user))) {
        return;
    }

    CmdBuilder(user, "FIDENT").push(ident).Broadcast();
}

void ModuleSpanningTree::OnUserPart(Membership* memb, std::string &partmessage,
                                    CUList& excepts) {
    if (IS_LOCAL(memb->user)) {
        CmdBuilder params(memb->user, "PART");
        params.push(memb->chan->name);
        if (!partmessage.empty()) {
            params.push_last(partmessage);
        }
        params.Broadcast();
    }
}

void ModuleSpanningTree::OnUserQuit(User* user, const std::string &reason,
                                    const std::string &oper_message) {
    if (IS_LOCAL(user)) {
        if (oper_message != reason) {
            ServerInstance->PI->SendMetaData(user, "operquit", oper_message);
        }

        CmdBuilder(user, "QUIT").push_last(reason).Broadcast();
    } else {
        // Hide the message if one of the following is true:
        // - User is being quit due to a netsplit and quietbursts is on
        // - Server is a silent uline
        TreeServer* server = TreeServer::Get(user);
        bool hide = (((server->IsDead()) && (Utils->quiet_bursts))
                     || (server->IsSilentULine()));
        if (!hide) {
            ServerInstance->SNO->WriteToSnoMask('Q',
                                                "Client exiting on server %s: %s (%s) [%s]",
                                                user->server->GetName().c_str(), user->GetFullRealHost().c_str(),
                                                user->GetIPString().c_str(), oper_message.c_str());
        }
    }

    // Regardless, update the UserCount
    TreeServer::Get(user)->UserCount--;
}

void ModuleSpanningTree::OnUserPostNick(User* user,
                                        const std::string &oldnick) {
    if (IS_LOCAL(user)) {
        // The nick TS is updated by the core, we don't do it
        CmdBuilder params(user, "NICK");
        params.push(user->nick);
        params.push(ConvToStr(user->age));
        params.Broadcast();
    } else if (!loopCall) {
        ServerInstance->Logs->Log(MODNAME, LOG_DEFAULT,
                                  "WARNING: Changed nick of remote user %s from %s to %s TS %lu by ourselves!",
                                  user->uuid.c_str(), oldnick.c_str(), user->nick.c_str(),
                                  (unsigned long) user->age);
    }
}

void ModuleSpanningTree::OnUserKick(User* source, Membership* memb,
                                    const std::string &reason, CUList& excepts) {
    if ((!IS_LOCAL(source)) && (source != ServerInstance->FakeClient)) {
        return;
    }

    CmdBuilder params(source, "KICK");
    params.push(memb->chan->name);
    params.push(memb->user->uuid);
    // If a remote user is being kicked by us then send the membership id in the kick too
    if (!IS_LOCAL(memb->user)) {
        params.push_int(memb->id);
    }
    params.push_last(reason);
    params.Broadcast();
}

void ModuleSpanningTree::OnPreRehash(User* user, const std::string &parameter) {
    ServerInstance->Logs->Log(MODNAME, LOG_DEBUG,
                              "OnPreRehash called with param %s", parameter.c_str());

    // Send out to other servers
    if (!parameter.empty() && parameter[0] != '-') {
        CmdBuilder params(user ? user : ServerInstance->FakeClient, "REHASH");
        params.push(parameter);
        params.Forward(user ? TreeServer::Get(user)->GetRoute() : NULL);
    }
}

void ModuleSpanningTree::ReadConfig(ConfigStatus& status) {
    // Did this rehash change the description of this server?
    const std::string& newdesc = ServerInstance->Config->ServerDesc;
    if (newdesc != Utils->TreeRoot->GetDesc()) {
        // Broadcast a SINFO desc message to let the network know about the new description. This is the description
        // string that is sent in the SERVER message initially and shown for example in WHOIS.
        // We don't need to update the field itself in the Server object - the core does that.
        CommandSInfo::Builder(Utils->TreeRoot, "desc", newdesc).Broadcast();
    }

    // Re-read config stuff
    try {
        Utils->ReadConfiguration();
    } catch (ModuleException& e) {
        // Refresh the IP cache anyway, so servers read before the error will be allowed to connect
        Utils->RefreshIPCache();
        // Always warn local opers with snomask +l, also warn globally (snomask +L) if the rehash was issued by a remote user
        std::string msg = "Error in configuration: ";
        msg.append(e.GetReason());
        ServerInstance->SNO->WriteToSnoMask('l', msg);
        if (status.srcuser && !IS_LOCAL(status.srcuser)) {
            ServerInstance->PI->SendSNONotice('L', msg);
        }
    }
}

void ModuleSpanningTree::OnLoadModule(Module* mod) {
    std::string data;
    data.push_back('+');
    data.append(mod->ModuleSourceFile);
    Version v = mod->GetVersion();
    if (!v.link_data.empty()) {
        data.push_back('=');
        data.append(v.link_data);
    }
    ServerInstance->PI->SendMetaData("modules", data);
}

void ModuleSpanningTree::OnUnloadModule(Module* mod) {
    if (!Utils) {
        return;
    }
    ServerInstance->PI->SendMetaData("modules", "-" + mod->ModuleSourceFile);

    if (mod == this) {
        // We are being unloaded, inform modules about all servers splitting which cannot be done later when the servers are actually disconnected
        const server_hash& servers = Utils->serverlist;
        for (server_hash::const_iterator i = servers.begin(); i != servers.end(); ++i) {
            TreeServer* server = i->second;
            if (!server->IsRoot()) {
                FOREACH_MOD_CUSTOM(GetLinkEventProvider(), ServerProtocol::LinkEventListener,
                                   OnServerSplit, (server, false));
            }
        }
        return;
    }

    // Some other module is being unloaded. If it provides an IOHook we use, we must close that server connection now.

restart:
    // Close all connections which use an IO hook provided by this module
    const TreeServer::ChildServers& list = Utils->TreeRoot->GetChildren();
    for (TreeServer::ChildServers::const_iterator i = list.begin(); i != list.end();
            ++i) {
        TreeSocket* sock = (*i)->GetSocket();
        if (sock->GetModHook(mod)) {
            sock->SendError("SSL module unloaded");
            sock->Close();
            // XXX: The list we're iterating is modified by TreeServer::SQuit() which is called by Close()
            goto restart;
        }
    }

    for (SpanningTreeUtilities::TimeoutList::const_iterator i =
                Utils->timeoutlist.begin(); i != Utils->timeoutlist.end(); ++i) {
        TreeSocket* sock = i->first;
        if (sock->GetModHook(mod)) {
            sock->Close();
        }
    }
}

void ModuleSpanningTree::OnOper(User* user, const std::string &opertype) {
    if (user->registered != REG_ALL || !IS_LOCAL(user)) {
        return;
    }

    // Note: The protocol does not allow direct umode +o;
    // sending OPERTYPE infers +o modechange locally.
    CommandOpertype::Builder(user).Broadcast();
}

void ModuleSpanningTree::OnAddLine(User* user, XLine *x) {
    if (!x->IsBurstable() || loopCall || (user && !IS_LOCAL(user))) {
        return;
    }

    if (!user) {
        user = ServerInstance->FakeClient;
    }

    CommandAddLine::Builder(x, user).Broadcast();
}

void ModuleSpanningTree::OnDelLine(User* user, XLine *x) {
    if (!x->IsBurstable() || loopCall || (user && !IS_LOCAL(user))) {
        return;
    }

    if (!user) {
        user = ServerInstance->FakeClient;
    }

    CmdBuilder params(user, "DELLINE");
    params.push(x->type);
    params.push(x->Displayable());
    params.Broadcast();
}

void ModuleSpanningTree::OnUserAway(User* user) {
    if (IS_LOCAL(user)) {
        CommandAway::Builder(user).Broadcast();
    }
}

void ModuleSpanningTree::OnUserBack(User* user) {
    OnUserAway(user);
}

void ModuleSpanningTree::OnMode(User* source, User* u, Channel* c,
                                const Modes::ChangeList& modes, ModeParser::ModeProcessFlag processflags) {
    if (processflags & ModeParser::MODE_LOCALONLY) {
        return;
    }

    if (u) {
        if (u->registered != REG_ALL) {
            return;
        }

        CmdBuilder params(source, "MODE");
        params.push(u->uuid);
        params.push(ClientProtocol::Messages::Mode::ToModeLetters(modes));
        params.push_raw(Translate::ModeChangeListToParams(modes.getlist()));
        params.Broadcast();
    } else {
        CmdBuilder params(source, "FMODE");
        params.push(c->name);
        params.push_int(c->age);
        params.push(ClientProtocol::Messages::Mode::ToModeLetters(modes));
        params.push_raw(Translate::ModeChangeListToParams(modes.getlist()));
        params.Broadcast();
    }
}

void ModuleSpanningTree::OnShutdown(const std::string& reason) {
    const TreeServer::ChildServers& children = Utils->TreeRoot->GetChildren();
    while (!children.empty()) {
        children.front()->SQuit(reason, true);
    }
}

void ModuleSpanningTree::OnDecodeMetaData(Extensible* target,
        const std::string& extname, const std::string& extdata) {
    // HACK: this should use automatically synced user metadata in v4.
    User* dest = static_cast<User*>(target);
    if (dest && (extname == "uniqueusername")) {
        dest->uniqueusername = (extdata != "0");
    }
}

CullResult ModuleSpanningTree::cull() {
    if (Utils) {
        Utils->cull();
    }
    return this->Module::cull();
}

ModuleSpanningTree::~ModuleSpanningTree() {
    ServerInstance->PI = &ServerInstance->DefaultProtocolInterface;

    Server* newsrv = new Server(ServerInstance->Config->GetSID(),
                                ServerInstance->Config->ServerName, ServerInstance->Config->ServerDesc);
    SetLocalUsersServer(newsrv);

    delete Utils;
}

Version ModuleSpanningTree::GetVersion() {
    return Version("Allows linking multiple servers together as part of one network.",
                   VF_VENDOR);
}

/* It is IMPORTANT that m_spanningtree is the last module in the chain
 * so that any activity it sees is FINAL, e.g. we arent going to send out
 * a NICK message before m_cloaking has finished putting the +x on the user,
 * etc etc.
 * Therefore, we set our priority to PRIORITY_LAST to make sure we end up at the END of
 * the module call queue.
 */
void ModuleSpanningTree::Prioritize() {
    ServerInstance->Modules->SetPriority(this, PRIORITY_LAST);
    ServerInstance->Modules.SetPriority(this, I_OnPreTopicChange, PRIORITY_FIRST);
}

MODULE_INIT(ModuleSpanningTree)
