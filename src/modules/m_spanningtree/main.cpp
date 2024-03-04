/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2021 Herman <GermanAizek@yandex.ru>
 *   Copyright (C) 2020 Matt Schatz <genius3000@g3k.solutions>
 *   Copyright (C) 2019 linuxdaemon <linuxdaemon.irc@gmail.com>
 *   Copyright (C) 2013, 2017-2023 Sadie Powell <sadie@witchery.services>
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
#include "clientprotocolmsg.h"
#include "iohook.h"
#include "socket.h"
#include "xline.h"

#include "commands.h"
#include "link.h"
#include "main.h"
#include "resolvers.h"
#include "translate.h"
#include "treeserver.h"
#include "treesocket.h"
#include "utils.h"

ModuleSpanningTree::ModuleSpanningTree()
	: Module(VF_VENDOR, "Allows linking multiple servers together as part of one network.")
	, Away::EventListener(this)
	, Stats::EventListener(this)
	, CTCTags::EventListener(this)
	, rconnect(this)
	, rsquit(this)
	, map(this)
	, commands(this)
	, broadcasteventprov(this, "event/server-broadcast")
	, linkeventprov(this, "event/server-link")
	, messageeventprov(this, "event/server-message")
	, synceventprov(this, "event/server-sync")
	, sslapi(this)
	, servertags(this)
	, DNS(this)
	, tagevprov(this)
{
}

SpanningTreeCommands::SpanningTreeCommands(ModuleSpanningTree* module)
	: metadata(module)
	, uid(module)
	, opertype(module)
	, fjoin(module)
	, ijoin(module)
	, resync(module)
	, fmode(module)
	, ftopic(module)
	, fhost(module)
	, frhost(module)
	, fident(module)
	, fname(module)
	, away(module)
	, addline(module)
	, delline(module)
	, encap(module)
	, idle(module)
	, nick(module)
	, ping(module)
	, pong(module)
	, save(module)
	, server(module)
	, squit(module)
	, snonotice(module)
	, endburst(module)
	, sinfo(module)
	, num(module)
	, lmode(module)
{
}

namespace
{
	void SetLocalUsersServer(Server* newserver)
	{
		// Does not change the server of quitting users because those are not in the list

		ServerInstance->FakeClient->server = newserver;
		for (auto* user : ServerInstance->Users.GetLocalUsers())
			user->server = newserver;
	}

	void ResetMembershipIds()
	{
		// Set all membership ids to 0
		for (const auto* user : ServerInstance->Users.GetLocalUsers())
		{
			for (auto* memb : user->chans)
				memb->id = 0;
		}
	}
}

void ModuleSpanningTree::init()
{
	ServerInstance->SNO.EnableSnomask('l', "LINK");

	ResetMembershipIds();

	Utils = new SpanningTreeUtilities(this);
	Utils->TreeRoot = new TreeServer;

	ServerInstance->PI = &protocolinterface;

	delete ServerInstance->FakeClient->server;
	SetLocalUsersServer(Utils->TreeRoot);
}

void ModuleSpanningTree::ShowLinks(TreeServer* Current, User* user, int hops)
{
	std::string Parent = Utils->TreeRoot->GetName();
	if (Current->GetParent())
	{
		Parent = Current->GetParent()->GetName();
	}

	for (auto* server : Current->GetChildren())
	{
		if ((server->Hidden) || ((Utils->HideServices) && (server->IsService())))
		{
			if (user->IsOper())
			{
				ShowLinks(server, user, hops+1);
			}
		}
		else
		{
			ShowLinks(server, user, hops+1);
		}
	}
	/* Don't display the line if its a service, hide services is on, and the user isn't an oper */
	if ((Utils->HideServices) && (Current->IsService()) && (!user->IsOper()))
		return;
	/* Or if the server is hidden and they're not an oper */
	else if ((Current->Hidden) && (!user->IsOper()))
		return;

	user->WriteNumeric(RPL_LINKS, Current->GetName(),
			(((Utils->FlatLinks) && (!user->IsOper())) ? ServerInstance->Config->GetServerName() : Parent),
			INSP_FORMAT("{} {}", (((Utils->FlatLinks) && (!user->IsOper())) ? 0 : hops), Current->GetDesc()));
}

void ModuleSpanningTree::HandleLinks(const CommandBase::Params& parameters, User* user)
{
	ShowLinks(Utils->TreeRoot, user, 0);
	user->WriteNumeric(RPL_ENDOFLINKS, '*', "End of /LINKS list.");
}

void ModuleSpanningTree::ConnectServer(const std::shared_ptr<Autoconnect>& a, bool on_timer)
{
	if (!a)
		return;
	for(unsigned int j=0; j < a->servers.size(); j++)
	{
		if (Utils->FindServer(a->servers[j]))
		{
			// found something in this block. Should the server fail,
			// we want to start at the start of the list, not in the
			// middle where we left off
			a->position = -1;
			return;
		}
	}
	if (on_timer && a->position >= 0)
		return;
	if (!on_timer && a->position < 0)
		return;

	a->position++;
	while (a->position < (int)a->servers.size())
	{
		std::shared_ptr<Link> x = Utils->FindLink(a->servers[a->position]);
		if (x)
		{
			ServerInstance->SNO.WriteToSnoMask('l', "AUTOCONNECT: Auto-connecting server \002{}\002", x->Name);
			ConnectServer(x, a);
			return;
		}
		a->position++;
	}
	// Autoconnect chain has been fully iterated; start at the beginning on the
	// next AutoConnectServers run
	a->position = -1;
}

void ModuleSpanningTree::ConnectServer(const std::shared_ptr<Link>& x, const std::shared_ptr<Autoconnect>& y)
{
	if (InspIRCd::Match(ServerInstance->Config->ServerName, x->Name, ascii_case_insensitive_map))
	{
		ServerInstance->SNO.WriteToSnoMask('l', "CONNECT: Not connecting to myself.");
		return;
	}

	irc::sockets::sockaddrs sa;
	if (x->IPAddr.find('/') != std::string::npos)
	{
		if (!irc::sockets::isunix(x->IPAddr) || !sa.from_unix(x->IPAddr))
		{
			// We don't use the family() != AF_UNSPEC check below for UNIX sockets as
			// that results in a DNS lookup.
			ServerInstance->SNO.WriteToSnoMask('l', "CONNECT: Error connecting \002{}\002: {} is not a UNIX socket!",
				x->Name, x->IPAddr);
			return;
		}
	}
	else
	{
		// If this fails then the IP sa will be AF_UNSPEC.
		sa.from_ip_port(x->IPAddr, x->Port);
	}

	/* Do we already have an IP? If so, no need to resolve it. */
	if (sa.family() != AF_UNSPEC)
	{
		// Create a TreeServer object that will start connecting immediately in the background
		auto* newsocket = new TreeSocket(x, y, sa);
		if (!newsocket->HasFd())
		{
			ServerInstance->SNO.WriteToSnoMask('l', "CONNECT: Error connecting \002{}\002: {}.",
				x->Name, newsocket->GetError());
			ServerInstance->GlobalCulls.AddItem(newsocket);
		}
	}
	else if (!DNS)
	{
		ServerInstance->SNO.WriteToSnoMask('l', "CONNECT: Error connecting \002{}\002: Hostname given and core_dns is not loaded, unable to resolve.", x->Name);
	}
	else
	{
		// Guess start_type from bindip aftype
		DNS::QueryType start_type = DNS::QUERY_AAAA;
		irc::sockets::sockaddrs bind;
		if (!x->Bind.empty() && bind.from_ip(x->Bind))
		{
			if (bind.family() == AF_INET)
				start_type = DNS::QUERY_A;
		}

		auto* snr = new ServerNameResolver(*DNS, x->IPAddr, x, start_type, y);
		try
		{
			DNS->Process(snr);
		}
		catch (const DNS::Exception& e)
		{
			delete snr;
			ServerInstance->SNO.WriteToSnoMask('l', "CONNECT: Error connecting \002{}\002: {}.", x->Name, e.GetReason());
			ConnectServer(y, false);
		}
	}
}

void ModuleSpanningTree::AutoConnectServers(time_t curtime)
{
	for (const auto& x : Utils->AutoconnectBlocks)
	{
		if (curtime >= x->NextConnectTime)
		{
			x->NextConnectTime = curtime + x->Period;
			ConnectServer(x, true);
		}
	}
}

void ModuleSpanningTree::DoConnectTimeout(time_t curtime)
{
	SpanningTreeUtilities::TimeoutList::iterator i = Utils->timeoutlist.begin();
	while (i != Utils->timeoutlist.end())
	{
		TreeSocket* s = i->first;
		std::pair<std::string, unsigned int> p = i->second;
		SpanningTreeUtilities::TimeoutList::iterator me = i;
		i++;
		if (s->GetLinkState() == DYING)
		{
			Utils->timeoutlist.erase(me);
			s->Close();
		}
		else if (curtime > s->age + (time_t)p.second)
		{
			ServerInstance->SNO.WriteToSnoMask('l', "CONNECT: Error connecting \002{}\002 (timeout of {} seconds)", p.first, p.second);
			Utils->timeoutlist.erase(me);
			s->Close();
		}
	}
}

ModResult ModuleSpanningTree::HandleVersion(const CommandBase::Params& parameters, User* user)
{
	// We've already confirmed that !parameters.empty(), so this is safe
	TreeServer* found = Utils->FindServerMask(parameters[0]);
	if (found)
	{
		if (found == Utils->TreeRoot)
		{
			// Pass to default VERSION handler.
			return MOD_RES_PASSTHRU;
		}

		Numeric::Numeric numeric(RPL_VERSION);
		if (user->IsOper())
		{
			numeric.push(found->rawversion + ".");
			numeric.push(found->GetName());
			numeric.push("[" + found->GetId() + "] " + found->customversion);
		}
		else
		{
			numeric.push(found->rawbranch + ".");
			numeric.push(found->GetPublicName());
			numeric.push(found->customversion);
		}
		user->WriteNumeric(numeric);
	}
	else
	{
		user->WriteNumeric(ERR_NOSUCHSERVER, parameters[0], "No such server");
	}
	return MOD_RES_DENY;
}

ModResult ModuleSpanningTree::HandleConnect(const CommandBase::Params& parameters, User* user)
{
	for (const auto& x : Utils->LinkBlocks)
	{
		if (InspIRCd::Match(x->Name, parameters[0], ascii_case_insensitive_map))
		{
			if (InspIRCd::Match(ServerInstance->Config->ServerName, x->Name, ascii_case_insensitive_map))
			{
				user->WriteRemoteNotice(INSP_FORMAT("*** CONNECT: Server \002{}\002 is ME, not connecting.", x->Name));
				return MOD_RES_DENY;
			}

			TreeServer* CheckDupe = Utils->FindServer(x->Name);
			if (!CheckDupe)
			{
				user->WriteRemoteNotice(INSP_FORMAT("*** CONNECT: Connecting to server: \002{}\002 ({}:{})", x->Name, (x->HiddenFromStats ? "<hidden>" : x->IPAddr), x->Port));
				ConnectServer(x);
				return MOD_RES_DENY;
			}
			else
			{
				user->WriteRemoteNotice(INSP_FORMAT("*** CONNECT: Server \002{}\002 already exists on the network and is connected via \002{}\002", x->Name, CheckDupe->GetParent()->GetName()));
				return MOD_RES_DENY;
			}
		}
	}
	user->WriteRemoteNotice(INSP_FORMAT("*** CONNECT: No server matching \002{}\002 could be found in the config file.", parameters[0]));
	return MOD_RES_DENY;
}

void ModuleSpanningTree::OnUserInvite(User* source, User* dest, Channel* channel, time_t expiry, ModeHandler::Rank notifyrank, CUList& notifyexcepts)
{
	if (IS_LOCAL(source))
	{
		CmdBuilder params(source, "INVITE");
		params.push(dest->uuid);
		params.push(channel->name);
		params.push_int(channel->age);
		params.push(ConvToStr(expiry));
		params.Broadcast();
	}
}

ModResult ModuleSpanningTree::OnPreTopicChange(User* user, Channel* chan, const std::string& topic)
{
	// XXX: Deny topic changes if the current topic set time is the current time or is in the future because
	// other servers will drop our FTOPIC. This restriction will be removed when the protocol is updated.
	if ((chan->topicset >= ServerInstance->Time()) && (Utils->serverlist.size() > 1))
	{
		user->WriteNumeric(ERR_UNAVAILRESOURCE, chan->name, "Retry topic change later");
		return MOD_RES_DENY;
	}
	return MOD_RES_PASSTHRU;
}

void ModuleSpanningTree::OnPostTopicChange(User* user, Channel* chan, const std::string& topic)
{
	// Drop remote events on the floor.
	if (!IS_LOCAL(user))
		return;

	CommandFTopic::Builder(user, chan).Broadcast();
}

void ModuleSpanningTree::OnUserPostMessage(User* user, const MessageTarget& target, const MessageDetails& details)
{
	if (!IS_LOCAL(user))
		return;

	const char* message_type = (details.type == MessageType::PRIVMSG ? "PRIVMSG" : "NOTICE");
	switch (target.type)
	{
		case MessageTarget::TYPE_USER:
		{
			User* d = target.Get<User>();
			if (!IS_LOCAL(d))
			{
				CmdBuilder params(user, message_type);
				params.push_tags(details.tags_out);
				params.push(d->uuid);
				params.push_last(details.text);
				params.Unicast(d);
			}
			break;
		}
		case MessageTarget::TYPE_CHANNEL:
		{
			Utils->SendChannelMessage(user, target.Get<Channel>(), details.text, target.status, details.tags_out, details.exemptions, message_type);
			break;
		}
		case MessageTarget::TYPE_SERVER:
		{
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

void ModuleSpanningTree::OnUserPostTagMessage(User* user, const MessageTarget& target, const CTCTags::TagMessageDetails& details)
{
	if (!IS_LOCAL(user))
		return;

	switch (target.type)
	{
		case MessageTarget::TYPE_USER:
		{
			User* d = target.Get<User>();
			if (!IS_LOCAL(d))
			{
				CmdBuilder params(user, "TAGMSG");
				params.push_tags(details.tags_out);
				params.push(d->uuid);
				params.Unicast(d);
			}
			break;
		}
		case MessageTarget::TYPE_CHANNEL:
		{
			Utils->SendChannelMessage(user, target.Get<Channel>(), "", target.status, details.tags_out, details.exemptions, "TAGMSG");
			break;
		}
		case MessageTarget::TYPE_SERVER:
		{
			const std::string* serverglob = target.Get<std::string>();
			CmdBuilder par(user, "TAGMSG");
			par.push_tags(details.tags_out);
			par.push(std::string("$") + *serverglob);
			par.Broadcast();
			break;
		}
	}
}

void ModuleSpanningTree::OnBackgroundTimer(time_t curtime)
{
	AutoConnectServers(curtime);
	DoConnectTimeout(curtime);
}

void ModuleSpanningTree::OnUserConnect(LocalUser* user)
{
	if (user->quitting)
		return;

	// Create the lazy ssl_cert metadata for this user if not already created.
	if (sslapi)
		sslapi->GetCertificate(user);

	CommandUID::Builder uid(user, true);
	CommandUID::Builder olduid(user, false);

	// NOTE: we can't do CommandUID::Builder(user).Broadcast() whilst
	// we still support the 1205 protocol.
	for (const auto* server : Utils->TreeRoot->GetChildren())
	{
		TreeSocket* socket = server->GetSocket();
		if (!socket)
			continue; // Should never happen?

		if (socket->proto_version >= PROTO_INSPIRCD_4)
			socket->WriteLine(uid);
		else
			socket->WriteLine(olduid);
	}

	if (user->IsOper())
		CommandOpertype::Builder(user, user->oper).Broadcast();

	if (user->IsAway())
		CommandAway::Builder(user).Broadcast();

	if (user->uniqueusername) // TODO: convert this to BooleanExtItem in v4.
		CommandMetadata::Builder(user, "uniqueusername", "1").Broadcast();

	for (const auto& [item, obj] : user->GetExtList())
	{
		const std::string value = item->ToNetwork(user, obj);
		if (!value.empty())
			ServerInstance->PI->SendMetadata(user, item->name, value);
	}

	Utils->TreeRoot->UserCount++;
}

void ModuleSpanningTree::OnUserJoin(Membership* memb, bool sync, bool created_by_local, CUList& excepts)
{
	// Only do this for local users
	if (!IS_LOCAL(memb->user))
		return;

	// Assign the current membership id to the new Membership and increase it
	memb->id = currmembid++;

	if (created_by_local)
	{
		CommandFJoin::Builder params(memb->chan);
		params.add(memb);
		params.finalize();
		params.Broadcast();
		SpanningTreeUtilities::SendListLimits(memb->chan, nullptr);
	}
	else
	{
		CmdBuilder params(memb->user, "IJOIN");
		params.push(memb->chan->name);
		params.push_int(memb->id);
		if (!memb->modes.empty())
		{
			params.push(ConvToStr(memb->chan->age));
			params.push(memb->GetAllPrefixModes());
		}
		params.Broadcast();
	}
}

void ModuleSpanningTree::OnChangeHost(User* user, const std::string& newhost)
{
	if (!user->IsFullyConnected() || !IS_LOCAL(user))
		return;

	CmdBuilder(user, "FHOST").push(newhost).push('*').Broadcast();
}

void ModuleSpanningTree::OnChangeRealHost(User* user, const std::string& newhost)
{
	if (!user->IsFullyConnected() || !IS_LOCAL(user))
		return;

	CmdBuilder(user, "FHOST").push('*').push(newhost).Broadcast();
}

void ModuleSpanningTree::OnChangeRealName(User* user, const std::string& real)
{
	if (!user->IsFullyConnected() || !IS_LOCAL(user))
		return;

	CmdBuilder(user, "FNAME").push_last(real).Broadcast();
}

void ModuleSpanningTree::OnChangeUser(User* user, const std::string& newuser)
{
	if (!user->IsFullyConnected() || !IS_LOCAL(user))
		return;

	CmdBuilder(user, "FIDENT").push(newuser).push("*").Broadcast();
}

void ModuleSpanningTree::OnChangeRealUser(User* user, const std::string& newuser)
{
	if (!user->IsFullyConnected() || !IS_LOCAL(user))
		return;

	CmdBuilder(user, "FIDENT").push("*").push(newuser).Broadcast();
}

void ModuleSpanningTree::OnUserPart(Membership* memb, std::string& partmessage, CUList& excepts)
{
	if (IS_LOCAL(memb->user))
	{
		CmdBuilder params(memb->user, "PART");
		params.push(memb->chan->name);
		if (!partmessage.empty())
			params.push_last(partmessage);
		params.Broadcast();
	}
}

void ModuleSpanningTree::OnUserQuit(User* user, const std::string& reason, const std::string& oper_message)
{
	if (IS_LOCAL(user))
	{
		if (oper_message != reason)
			ServerInstance->PI->SendMetadata(user, "operquit", oper_message);

		CmdBuilder(user, "QUIT").push_last(reason).Broadcast();
	}
	else
	{
		// Hide the message if one of the following is true:
		// - User is being quit due to a netsplit and quietbursts is on
		// - User is on a silent services server
		TreeServer* server = TreeServer::Get(user);
		bool hide = (((server->IsDead()) && (Utils->quiet_bursts)) || (server->IsSilentService()));
		if (!hide)
		{
			ServerInstance->SNO.WriteToSnoMask('Q', "Client exiting on server {}: {} ({}) [{}]", user->server->GetName(),
				user->GetRealMask(), user->GetAddress(), oper_message);
		}
	}

	// Regardless, update the UserCount
	TreeServer::Get(user)->UserCount--;
}

void ModuleSpanningTree::OnUserPostNick(User* user, const std::string& oldnick)
{
	if (IS_LOCAL(user))
	{
		// The nick TS is updated by the core, we don't do it
		CmdBuilder params(user, "NICK");
		params.push(user->nick);
		params.push(ConvToStr(user->nickchanged));
		params.Broadcast();
	}
	else if (!loopCall)
	{
		ServerInstance->Logs.Normal(MODNAME, "WARNING: Changed nick of remote user {} from {} to {} TS {} by ourselves!", user->uuid, oldnick, user->nick, user->nickchanged);
	}
}

void ModuleSpanningTree::OnUserKick(User* source, Membership* memb, const std::string& reason, CUList& excepts)
{
	if ((!IS_LOCAL(source)) && (source != ServerInstance->FakeClient))
		return;

	CmdBuilder params(source, "KICK");
	params.push(memb->chan->name);
	params.push(memb->user->uuid);
	// If a remote user is being kicked by us then send the membership id in the kick too
	if (!IS_LOCAL(memb->user))
		params.push_int(memb->id);
	params.push_last(reason);
	params.Broadcast();
}

void ModuleSpanningTree::OnPreRehash(User* user, const std::string& parameter)
{
	ServerInstance->Logs.Debug(MODNAME, "OnPreRehash called with param {}", parameter);

	// Send out to other servers
	if (!parameter.empty() && parameter[0] != '-')
	{
		CmdBuilder params(user ? user : ServerInstance->FakeClient, "REHASH");
		params.push(parameter);
		params.Forward(user ? TreeServer::Get(user)->GetRoute() : nullptr);
	}
}

void ModuleSpanningTree::ReadConfig(ConfigStatus& status)
{
	// Did this rehash change the description of this server?
	const std::string& newdesc = ServerInstance->Config->ServerDesc;
	if (newdesc != Utils->TreeRoot->GetDesc())
	{
		// Broadcast a SINFO desc message to let the network know about the new description. This is the description
		// string that is sent in the SERVER message initially and shown for example in WHOIS.
		// We don't need to update the field itself in the Server object - the core does that.
		CommandSInfo::Builder(Utils->TreeRoot, "desc", newdesc).Broadcast();
	}

	// Re-read config stuff
	try
	{
		Utils->ReadConfiguration();
	}
	catch (const ModuleException& e)
	{
		// Refresh the IP cache anyway, so servers read before the error will be allowed to connect
		Utils->RefreshIPCache();
		// Always warn local opers with snomask +l, also warn globally (snomask +L) if the rehash was issued by a remote user
		std::string msg = "Error in configuration: ";
		msg.append(e.GetReason());
		ServerInstance->SNO.WriteToSnoMask('l', msg);
		if (status.srcuser && !IS_LOCAL(status.srcuser))
			ServerInstance->PI->SendSNONotice('L', msg);
	}
}

namespace
{
	void BroadcastModuleState(Module* mod, bool loading)
	{
		std::stringstream buffer;
		buffer << (loading ? '+' : '-') << ModuleManager::ShrinkModName(mod->ModuleFile);

		std::stringstream compatbuffer;
		compatbuffer << (loading ? '+' : '-') << mod->ModuleFile;

		if (loading)
		{
			const std::string linkstring = SpanningTreeUtilities::BuildLinkString(PROTO_INSPIRCD_4, mod);
			if (!linkstring.empty())
				buffer << '=' << linkstring;

			const std::string compatlinkstring = SpanningTreeUtilities::BuildLinkString(PROTO_INSPIRCD_3, mod);
			if (!compatlinkstring.empty())
				compatbuffer << '=' << compatlinkstring;
		}

		for (const auto* child : Utils->TreeRoot->GetChildren())
		{
			if (!child->GetSocket())
				continue; // Should never happen?

			if (child->GetSocket()->proto_version <= PROTO_INSPIRCD_3)
				CommandMetadata::Builder("modules", buffer.str()).Forward(child);
			else
				CommandMetadata::Builder("modules", compatbuffer.str()).Forward(child);
		}
	}
}

void ModuleSpanningTree::OnLoadModule(Module* mod)
{
	BroadcastModuleState(mod, true);
}

void ModuleSpanningTree::OnUnloadModule(Module* mod)
{
	if (!Utils)
		return;

	BroadcastModuleState(mod, false);
	if (mod == this)
	{
		// We are being unloaded, inform modules about all servers splitting which cannot be done later when the servers are actually disconnected
		for (const auto& [_, server] : Utils->serverlist)
		{
			if (!server->IsRoot())
				linkeventprov.Call(&ServerProtocol::LinkEventListener::OnServerSplit, server, false);
		}
		return;
	}

	// Some other module is being unloaded. If it provides an IOHook we use, we must close that server connection now.

restart:
	// Close all connections which use an IO hook provided by this module
	for (const auto* child : Utils->TreeRoot->GetChildren())
	{
		TreeSocket* sock = child->GetSocket();
		if (sock->GetModHook(mod))
		{
			sock->SendError("TLS module unloaded");
			sock->Close();
			// XXX: The list we're iterating is modified by TreeServer::SQuit() which is called by Close()
			goto restart;
		}
	}

	for (const auto& [sock, _] : Utils->timeoutlist)
	{
		if (sock->GetModHook(mod))
			sock->Close();
	}
}

void ModuleSpanningTree::OnOperLogin(User* user, const std::shared_ptr<OperAccount>& oper, bool automatic)
{
	if (!user->IsFullyConnected() || !IS_LOCAL(user))
		return;

	// Note: The protocol does not allow direct umode +o;
	// sending OPERTYPE infers +o modechange locally.
	CommandOpertype::Builder(user, oper, automatic).Broadcast();
}

void ModuleSpanningTree::OnAddLine(User* user, XLine* x)
{
	if (!x->IsBurstable() || loopCall || (user && !IS_LOCAL(user)))
		return;

	if (!user)
		user = ServerInstance->FakeClient;

	CommandAddLine::Builder(x, user).Broadcast();
}

void ModuleSpanningTree::OnDelLine(User* user, XLine* x)
{
	if (!x->IsBurstable() || loopCall || (user && !IS_LOCAL(user)))
		return;

	if (!user)
		user = ServerInstance->FakeClient;

	CmdBuilder params(user, "DELLINE");
	params.push(x->type);
	params.push(x->Displayable());
	params.Broadcast();
}

void ModuleSpanningTree::OnUserAway(User* user, const std::optional<AwayState>& prevstate)
{
	if (IS_LOCAL(user) && user->IsFullyConnected())
		CommandAway::Builder(user).Broadcast();
}

void ModuleSpanningTree::OnUserBack(User* user, const std::optional<AwayState>& prevstate)
{
	OnUserAway(user, prevstate);
}

void ModuleSpanningTree::OnMode(User* source, User* u, Channel* c, const Modes::ChangeList& modes, ModeParser::ModeProcessFlag processflags)
{
	if (processflags & ModeParser::MODE_LOCALONLY)
		return;

	if (u)
	{
		if (!u->IsFullyConnected())
			return;

		CmdBuilder params(source, "MODE");
		params.push(u->uuid);
		params.push(ClientProtocol::Messages::Mode::ToModeLetters(modes));
		params.push_raw(Translate::ModeChangeListToParams(modes.getlist()));
		params.Broadcast();
	}
	else
	{
		CmdBuilder params(source, "FMODE");
		params.push(c->name);
		params.push_int(c->age);
		params.push(ClientProtocol::Messages::Mode::ToModeLetters(modes));
		params.push_raw(Translate::ModeChangeListToParams(modes.getlist()));
		params.Broadcast();
	}
}

void ModuleSpanningTree::OnShutdown(const std::string& reason)
{
	const TreeServer::ChildServers& children = Utils->TreeRoot->GetChildren();
	while (!children.empty())
		children.front()->SQuit(reason, true);
}

void ModuleSpanningTree::OnDecodeMetadata(Extensible* target, const std::string& extname, const std::string& extdata)
{
	// HACK: this should use automatically synced user metadata in v4.
	if (target && target->extype == ExtensionType::USER && irc::equals(extname, "uniqueusername"))
		static_cast<User*>(target)->uniqueusername = (extdata != "0");
}

Cullable::Result ModuleSpanningTree::Cull()
{
	if (Utils)
		Utils->Cull();
	return Module::Cull();
}

ModuleSpanningTree::~ModuleSpanningTree()
{
	ServerInstance->PI = &ServerInstance->DefaultProtocolInterface;

	auto* newsrv = new Server(ServerInstance->Config->ServerId, ServerInstance->Config->ServerName, ServerInstance->Config->ServerDesc);
	SetLocalUsersServer(newsrv);

	delete Utils;
}

/* It is IMPORTANT that m_spanningtree is the last module in the chain
 * so that any activity it sees is FINAL, e.g. we arent going to send out
 * a NICK message before the cloak module has finished putting the +x on the user,
 * etc etc.
 * Therefore, we set our priority to PRIORITY_LAST to make sure we end up at the END of
 * the module call queue.
 */
void ModuleSpanningTree::Prioritize()
{
	ServerInstance->Modules.SetPriority(this, PRIORITY_LAST);
	ServerInstance->Modules.SetPriority(this, I_OnPreTopicChange, PRIORITY_FIRST);
}

MODULE_INIT(ModuleSpanningTree)
