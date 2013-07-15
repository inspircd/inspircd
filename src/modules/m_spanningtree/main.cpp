/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2007-2009 Craig Edwards <craigedwards@brainbox.cc>
 *   Copyright (C) 2007-2008 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2008 Thomas Stagner <aquanight@inspircd.org>
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
#include "protocolinterface.h"

ModuleSpanningTree::ModuleSpanningTree()
	: commands(NULL), DNS(this, "DNS")
{
}

SpanningTreeCommands::SpanningTreeCommands(ModuleSpanningTree* module)
	: rconnect(module), rsquit(module),
	svsjoin(module), svspart(module), svsnick(module), metadata(module),
	uid(module), opertype(module), fjoin(module), ijoin(module), resync(module),
	fmode(module), ftopic(module), fhost(module), fident(module), fname(module),
	away(module), addline(module), delline(module), encap(module), idle(module),
	nick(module), ping(module), pong(module), push(module), save(module),
	server(module), squit(module), snonotice(module), version(module),
	burst(module), endburst(module)
{
}

void ModuleSpanningTree::init()
{
	ServerInstance->SNO->EnableSnomask('l', "LINK");

	Utils = new SpanningTreeUtilities(this);
	Utils->TreeRoot = new TreeServer;
	commands = new SpanningTreeCommands(this);
	ServerInstance->Modules->AddService(commands->rconnect);
	ServerInstance->Modules->AddService(commands->rsquit);

	delete ServerInstance->PI;
	ServerInstance->PI = new SpanningTreeProtocolInterface(Utils);
	loopCall = false;

	// update our local user count
	Utils->TreeRoot->UserCount = ServerInstance->Users->local_users.size();
}

void ModuleSpanningTree::ShowLinks(TreeServer* Current, User* user, int hops)
{
	std::string Parent = Utils->TreeRoot->GetName();
	if (Current->GetParent())
	{
		Parent = Current->GetParent()->GetName();
	}
	for (unsigned int q = 0; q < Current->ChildCount(); q++)
	{
		if ((Current->GetChild(q)->Hidden) || ((Utils->HideULines) && (ServerInstance->ULine(Current->GetChild(q)->GetName()))))
		{
			if (user->IsOper())
			{
				 ShowLinks(Current->GetChild(q),user,hops+1);
			}
		}
		else
		{
			ShowLinks(Current->GetChild(q),user,hops+1);
		}
	}
	/* Don't display the line if its a uline, hide ulines is on, and the user isnt an oper */
	if ((Utils->HideULines) && (ServerInstance->ULine(Current->GetName())) && (!user->IsOper()))
		return;
	/* Or if the server is hidden and they're not an oper */
	else if ((Current->Hidden) && (!user->IsOper()))
		return;

	user->WriteNumeric(364, "%s %s %s :%d %s",	user->nick.c_str(), Current->GetName().c_str(),
			(Utils->FlatLinks && (!user->IsOper())) ? ServerInstance->Config->ServerName.c_str() : Parent.c_str(),
			(Utils->FlatLinks && (!user->IsOper())) ? 0 : hops,
			Current->GetDesc().c_str());
}

int ModuleSpanningTree::CountServs()
{
	return Utils->serverlist.size();
}

void ModuleSpanningTree::HandleLinks(const std::vector<std::string>& parameters, User* user)
{
	ShowLinks(Utils->TreeRoot,user,0);
	user->WriteNumeric(365, "%s * :End of /LINKS list.",user->nick.c_str());
	return;
}

std::string ModuleSpanningTree::TimeToStr(time_t secs)
{
	time_t mins_up = secs / 60;
	time_t hours_up = mins_up / 60;
	time_t days_up = hours_up / 24;
	secs = secs % 60;
	mins_up = mins_up % 60;
	hours_up = hours_up % 24;
	return ((days_up ? (ConvToStr(days_up) + "d") : "")
			+ (hours_up ? (ConvToStr(hours_up) + "h") : "")
			+ (mins_up ? (ConvToStr(mins_up) + "m") : "")
			+ ConvToStr(secs) + "s");
}

void ModuleSpanningTree::DoPingChecks(time_t curtime)
{
	/*
	 * Cancel remote burst mode on any servers which still have it enabled due to latency/lack of data.
	 * This prevents lost REMOTECONNECT notices
	 */
	long ts = ServerInstance->Time() * 1000 + (ServerInstance->Time_ns() / 1000000);

restart:
	for (server_hash::iterator i = Utils->serverlist.begin(); i != Utils->serverlist.end(); i++)
	{
		TreeServer *s = i->second;

		if (s->GetSocket() && s->GetSocket()->GetLinkState() == DYING)
		{
			s->GetSocket()->Close();
			goto restart;
		}

		// Fix for bug #792, do not ping servers that are not connected yet!
		// Remote servers have Socket == NULL and local connected servers have
		// Socket->LinkState == CONNECTED
		if (s->GetSocket() && s->GetSocket()->GetLinkState() != CONNECTED)
			continue;

		// Now do PING checks on all servers
		TreeServer *mts = Utils->BestRouteTo(s->GetID());

		if (mts)
		{
			// Only ping if this server needs one
			if (curtime >= s->NextPingTime())
			{
				// And if they answered the last
				if (s->AnsweredLastPing())
				{
					// They did, send a ping to them
					s->SetNextPingTime(curtime + Utils->PingFreq);
					TreeSocket *tsock = mts->GetSocket();

					// ... if we can find a proper route to them
					if (tsock)
					{
						tsock->WriteLine(":" + ServerInstance->Config->GetSID() + " PING " + s->GetID());
						s->LastPingMsec = ts;
					}
				}
				else
				{
					// They didn't answer the last ping, if they are locally connected, get rid of them.
					TreeSocket *sock = s->GetSocket();
					if (sock)
					{
						sock->SendError("Ping timeout");
						sock->Close();
						goto restart;
					}
				}
			}

			// If warn on ping enabled and not warned and the difference is sufficient and they didn't answer the last ping...
			if ((Utils->PingWarnTime) && (!s->Warned) && (curtime >= s->NextPingTime() - (Utils->PingFreq - Utils->PingWarnTime)) && (!s->AnsweredLastPing()))
			{
				/* The server hasnt responded, send a warning to opers */
				ServerInstance->SNO->WriteToSnoMask('l',"Server \002%s\002 has not responded to PING for %d seconds, high latency.", s->GetName().c_str(), Utils->PingWarnTime);
				s->Warned = true;
			}
		}
	}
}

void ModuleSpanningTree::ConnectServer(Autoconnect* a, bool on_timer)
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
		Link* x = Utils->FindLink(a->servers[a->position]);
		if (x)
		{
			ServerInstance->SNO->WriteToSnoMask('l', "AUTOCONNECT: Auto-connecting server \002%s\002", x->Name.c_str());
			ConnectServer(x, a);
			return;
		}
		a->position++;
	}
	// Autoconnect chain has been fully iterated; start at the beginning on the
	// next AutoConnectServers run
	a->position = -1;
}

void ModuleSpanningTree::ConnectServer(Link* x, Autoconnect* y)
{
	bool ipvalid = true;

	if (InspIRCd::Match(ServerInstance->Config->ServerName, assign(x->Name)))
	{
		ServerInstance->SNO->WriteToSnoMask('l', "CONNECT: Not connecting to myself.");
		return;
	}

	DNS::QueryType start_type = DNS::QUERY_AAAA;
	if (strchr(x->IPAddr.c_str(),':'))
	{
		in6_addr n;
		if (inet_pton(AF_INET6, x->IPAddr.c_str(), &n) < 1)
			ipvalid = false;
	}
	else
	{
		in_addr n;
		if (inet_aton(x->IPAddr.c_str(),&n) < 1)
			ipvalid = false;
	}

	/* Do we already have an IP? If so, no need to resolve it. */
	if (ipvalid)
	{
		/* Gave a hook, but it wasnt one we know */
		TreeSocket* newsocket = new TreeSocket(x, y, x->IPAddr);
		if (newsocket->GetFd() > -1)
		{
			/* Handled automatically on success */
		}
		else
		{
			ServerInstance->SNO->WriteToSnoMask('l', "CONNECT: Error connecting \002%s\002: %s.",
				x->Name.c_str(), newsocket->getError().c_str());
			ServerInstance->GlobalCulls.AddItem(newsocket);
		}
	}
	else if (!DNS)
	{
		ServerInstance->SNO->WriteToSnoMask('l', "CONNECT: Error connecting \002%s\002: Hostname given and m_dns.so is not loaded, unable to resolve.", x->Name.c_str());
	}
	else
	{
		ServernameResolver* snr = new ServernameResolver(*DNS, x->IPAddr, x, start_type, y);
		try
		{
			DNS->Process(snr);
		}
		catch (DNS::Exception& e)
		{
			delete snr;
			ServerInstance->SNO->WriteToSnoMask('l', "CONNECT: Error connecting \002%s\002: %s.",x->Name.c_str(), e.GetReason());
			ConnectServer(y, false);
		}
	}
}

void ModuleSpanningTree::AutoConnectServers(time_t curtime)
{
	for (std::vector<reference<Autoconnect> >::iterator i = Utils->AutoconnectBlocks.begin(); i < Utils->AutoconnectBlocks.end(); ++i)
	{
		Autoconnect* x = *i;
		if (curtime >= x->NextConnectTime)
		{
			x->NextConnectTime = curtime + x->Period;
			ConnectServer(x, true);
		}
	}
}

void ModuleSpanningTree::DoConnectTimeout(time_t curtime)
{
	std::map<TreeSocket*, std::pair<std::string, int> >::iterator i = Utils->timeoutlist.begin();
	while (i != Utils->timeoutlist.end())
	{
		TreeSocket* s = i->first;
		std::pair<std::string, int> p = i->second;
		std::map<TreeSocket*, std::pair<std::string, int> >::iterator me = i;
		i++;
		if (s->GetLinkState() == DYING)
		{
			Utils->timeoutlist.erase(me);
			s->Close();
		}
		else if (curtime > s->age + p.second)
		{
			ServerInstance->SNO->WriteToSnoMask('l',"CONNECT: Error connecting \002%s\002 (timeout of %d seconds)",p.first.c_str(),p.second);
			Utils->timeoutlist.erase(me);
			s->Close();
		}
	}
}

ModResult ModuleSpanningTree::HandleVersion(const std::vector<std::string>& parameters, User* user)
{
	// we've already checked if pcnt > 0, so this is safe
	TreeServer* found = Utils->FindServerMask(parameters[0]);
	if (found)
	{
		if (found == Utils->TreeRoot)
		{
			// Pass to default VERSION handler.
			return MOD_RES_PASSTHRU;
		}
		std::string Version = found->GetVersion();
		user->WriteNumeric(351, "%s :%s",user->nick.c_str(),Version.c_str());
	}
	else
	{
		user->WriteNumeric(402, "%s %s :No such server",user->nick.c_str(),parameters[0].c_str());
	}
	return MOD_RES_DENY;
}

/* This method will attempt to get a message to a remote user.
 */
void ModuleSpanningTree::RemoteMessage(User* user, const char* format, ...)
{
	std::string text;
	VAFORMAT(text, format, format);

	if (IS_LOCAL(user))
		user->WriteNotice(text);
	else
		ServerInstance->PI->SendUserNotice(user, text);
}

ModResult ModuleSpanningTree::HandleConnect(const std::vector<std::string>& parameters, User* user)
{
	for (std::vector<reference<Link> >::iterator i = Utils->LinkBlocks.begin(); i < Utils->LinkBlocks.end(); i++)
	{
		Link* x = *i;
		if (InspIRCd::Match(x->Name.c_str(),parameters[0]))
		{
			if (InspIRCd::Match(ServerInstance->Config->ServerName, assign(x->Name)))
			{
				RemoteMessage(user, "*** CONNECT: Server \002%s\002 is ME, not connecting.",x->Name.c_str());
				return MOD_RES_DENY;
			}

			TreeServer* CheckDupe = Utils->FindServer(x->Name.c_str());
			if (!CheckDupe)
			{
				RemoteMessage(user, "*** CONNECT: Connecting to server: \002%s\002 (%s:%d)",x->Name.c_str(),(x->HiddenFromStats ? "<hidden>" : x->IPAddr.c_str()),x->Port);
				ConnectServer(x);
				return MOD_RES_DENY;
			}
			else
			{
				RemoteMessage(user, "*** CONNECT: Server \002%s\002 already exists on the network and is connected via \002%s\002", x->Name.c_str(), CheckDupe->GetParent()->GetName().c_str());
				return MOD_RES_DENY;
			}
		}
	}
	RemoteMessage(user, "*** CONNECT: No server matching \002%s\002 could be found in the config file.",parameters[0].c_str());
	return MOD_RES_DENY;
}

void ModuleSpanningTree::OnGetServerDescription(const std::string &servername,std::string &description)
{
	TreeServer* s = Utils->FindServer(servername);
	if (s)
	{
		description = s->GetDesc();
	}
}

void ModuleSpanningTree::OnUserInvite(User* source,User* dest,Channel* channel, time_t expiry)
{
	if (IS_LOCAL(source))
	{
		parameterlist params;
		params.push_back(dest->uuid);
		params.push_back(channel->name);
		params.push_back(ConvToStr(expiry));
		Utils->DoOneToMany(source->uuid,"INVITE",params);
	}
}

void ModuleSpanningTree::OnPostTopicChange(User* user, Channel* chan, const std::string &topic)
{
	// Drop remote events on the floor.
	if (!IS_LOCAL(user))
		return;

	parameterlist params;
	params.push_back(chan->name);
	params.push_back(":"+topic);
	Utils->DoOneToMany(user->uuid,"TOPIC",params);
}

void ModuleSpanningTree::LocalMessage(User* user, void* dest, int target_type, const std::string &text, char status, const CUList &exempt_list, const char* message_type)
{
	/* Server or remote origin, dest should always be non-null */
	if ((!user) || (!IS_LOCAL(user)) || (!dest))
		return;

	if (target_type == TYPE_USER)
	{
		User* d = (User*) dest;
		if (!IS_LOCAL(d))
		{
			parameterlist params;
			params.push_back(d->uuid);
			params.push_back(":"+text);
			Utils->DoOneToOne(user->uuid, message_type, params, d->server);
		}
	}
	else if (target_type == TYPE_CHANNEL)
	{
		Utils->SendChannelMessage(user->uuid, (Channel*)dest, text, status, exempt_list, message_type);
	}
	else if (target_type == TYPE_SERVER)
	{
		char* target = (char*) dest;
		parameterlist par;
		par.push_back(target);
		par.push_back(":"+text);
		Utils->DoOneToMany(user->uuid, message_type, par);
	}
}

void ModuleSpanningTree::OnUserMessage(User* user, void* dest, int target_type, const std::string& text, char status, const CUList& exempt_list, MessageType msgtype)
{
	LocalMessage(user, dest, target_type, text, status, exempt_list, (msgtype == MSG_PRIVMSG ? "PRIVMSG" : "NOTICE"));
}

void ModuleSpanningTree::OnBackgroundTimer(time_t curtime)
{
	AutoConnectServers(curtime);
	DoPingChecks(curtime);
	DoConnectTimeout(curtime);
}

void ModuleSpanningTree::OnUserConnect(LocalUser* user)
{
	if (user->quitting)
		return;

	parameterlist params;
	params.push_back(user->uuid);
	params.push_back(ConvToStr(user->age));
	params.push_back(user->nick);
	params.push_back(user->host);
	params.push_back(user->dhost);
	params.push_back(user->ident);
	params.push_back(user->GetIPString());
	params.push_back(ConvToStr(user->signon));
	params.push_back("+"+std::string(user->FormatModes(true)));
	params.push_back(":"+user->fullname);
	Utils->DoOneToMany(ServerInstance->Config->GetSID(), "UID", params);

	if (user->IsOper())
	{
		params.clear();
		params.push_back(":");
		params[0].append(user->oper->name);
		Utils->DoOneToMany(user->uuid,"OPERTYPE",params);
	}

	for(Extensible::ExtensibleStore::const_iterator i = user->GetExtList().begin(); i != user->GetExtList().end(); i++)
	{
		ExtensionItem* item = i->first;
		std::string value = item->serialize(FORMAT_NETWORK, user, i->second);
		if (!value.empty())
			ServerInstance->PI->SendMetaData(user, item->name, value);
	}

	Utils->TreeRoot->UserCount++;
}

void ModuleSpanningTree::OnUserJoin(Membership* memb, bool sync, bool created_by_local, CUList& excepts)
{
	// Only do this for local users
	if (IS_LOCAL(memb->user))
	{
		parameterlist params;
		params.push_back(memb->chan->name);
		if (created_by_local)
		{
			params.push_back(ConvToStr(memb->chan->age));
			params.push_back(std::string("+") + memb->chan->ChanModes(true));
			params.push_back(memb->modes+","+memb->user->uuid);
			Utils->DoOneToMany(ServerInstance->Config->GetSID(),"FJOIN",params);
		}
		else
		{
			if (!memb->modes.empty())
			{
				params.push_back(ConvToStr(memb->chan->age));
				params.push_back(memb->modes);
			}
			Utils->DoOneToMany(memb->user->uuid, "IJOIN", params);
		}
	}
}

void ModuleSpanningTree::OnChangeHost(User* user, const std::string &newhost)
{
	if (user->registered != REG_ALL || !IS_LOCAL(user))
		return;

	parameterlist params;
	params.push_back(newhost);
	Utils->DoOneToMany(user->uuid,"FHOST",params);
}

void ModuleSpanningTree::OnChangeName(User* user, const std::string &gecos)
{
	if (user->registered != REG_ALL || !IS_LOCAL(user))
		return;

	parameterlist params;
	params.push_back(gecos);
	Utils->DoOneToMany(user->uuid,"FNAME",params);
}

void ModuleSpanningTree::OnChangeIdent(User* user, const std::string &ident)
{
	if ((user->registered != REG_ALL) || (!IS_LOCAL(user)))
		return;

	parameterlist params;
	params.push_back(ident);
	Utils->DoOneToMany(user->uuid,"FIDENT",params);
}

void ModuleSpanningTree::OnUserPart(Membership* memb, std::string &partmessage, CUList& excepts)
{
	if (IS_LOCAL(memb->user))
	{
		parameterlist params;
		params.push_back(memb->chan->name);
		if (!partmessage.empty())
			params.push_back(":"+partmessage);
		Utils->DoOneToMany(memb->user->uuid,"PART",params);
	}
}

void ModuleSpanningTree::OnUserQuit(User* user, const std::string &reason, const std::string &oper_message)
{
	if ((IS_LOCAL(user)) && (user->registered == REG_ALL))
	{
		parameterlist params;

		if (oper_message != reason)
			ServerInstance->PI->SendMetaData(user, "operquit", oper_message);

		params.push_back(":"+reason);
		Utils->DoOneToMany(user->uuid,"QUIT",params);
	}

	// Regardless, We need to modify the user Counts..
	TreeServer* SourceServer = Utils->FindServer(user->server);
	if (SourceServer)
	{
		SourceServer->UserCount--;
	}
}

void ModuleSpanningTree::OnUserPostNick(User* user, const std::string &oldnick)
{
	if (IS_LOCAL(user))
	{
		parameterlist params;
		params.push_back(user->nick);

		/** IMPORTANT: We don't update the TS if the oldnick is just a case change of the newnick!
		 */
		if (irc::string(user->nick.c_str()) != assign(oldnick))
			user->age = ServerInstance->Time();

		params.push_back(ConvToStr(user->age));
		Utils->DoOneToMany(user->uuid,"NICK",params);
	}
	else if (!loopCall && user->nick == user->uuid)
	{
		parameterlist params;
		params.push_back(user->uuid);
		params.push_back(ConvToStr(user->age));
		Utils->DoOneToMany(ServerInstance->Config->GetSID(),"SAVE",params);
	}
}

void ModuleSpanningTree::OnUserKick(User* source, Membership* memb, const std::string &reason, CUList& excepts)
{
	parameterlist params;
	params.push_back(memb->chan->name);
	params.push_back(memb->user->uuid);
	params.push_back(":"+reason);
	if (IS_LOCAL(source))
	{
		Utils->DoOneToMany(source->uuid,"KICK",params);
	}
	else if (source == ServerInstance->FakeClient)
	{
		Utils->DoOneToMany(ServerInstance->Config->GetSID(),"KICK",params);
	}
}

void ModuleSpanningTree::OnPreRehash(User* user, const std::string &parameter)
{
	if (loopCall)
		return; // Don't generate a REHASH here if we're in the middle of processing a message that generated this one

	ServerInstance->Logs->Log(MODNAME, LOG_DEBUG, "OnPreRehash called with param %s", parameter.c_str());

	// Send out to other servers
	if (!parameter.empty() && parameter[0] != '-')
	{
		parameterlist params;
		params.push_back(parameter);
		Utils->DoOneToAllButSender(user ? user->uuid : ServerInstance->Config->GetSID(), "REHASH", params, user ? user->server : ServerInstance->Config->ServerName);
	}
}

void ModuleSpanningTree::OnRehash(User* user)
{
	// Re-read config stuff
	try
	{
		Utils->ReadConfiguration();
	}
	catch (ModuleException& e)
	{
		// Refresh the IP cache anyway, so servers read before the error will be allowed to connect
		Utils->RefreshIPCache();
		// Always warn local opers with snomask +l, also warn globally (snomask +L) if the rehash was issued by a remote user
		std::string msg = "Error in configuration: ";
		msg.append(e.GetReason());
		ServerInstance->SNO->WriteToSnoMask('l', msg);
		if (user && !IS_LOCAL(user))
			ServerInstance->PI->SendSNONotice("L", msg);
	}
}

void ModuleSpanningTree::OnLoadModule(Module* mod)
{
	std::string data;
	data.push_back('+');
	data.append(mod->ModuleSourceFile);
	Version v = mod->GetVersion();
	if (!v.link_data.empty())
	{
		data.push_back('=');
		data.append(v.link_data);
	}
	ServerInstance->PI->SendMetaData(NULL, "modules", data);
}

void ModuleSpanningTree::OnUnloadModule(Module* mod)
{
	ServerInstance->PI->SendMetaData(NULL, "modules", "-" + mod->ModuleSourceFile);

	unsigned int items = Utils->TreeRoot->ChildCount();
	for(unsigned int x = 0; x < items; x++)
	{
		TreeServer* srv = Utils->TreeRoot->GetChild(x);
		TreeSocket* sock = srv->GetSocket();
		if (sock && sock->GetIOHook() && sock->GetIOHook()->creator == mod)
		{
			sock->SendError("SSL module unloaded");
			sock->Close();
		}
	}
}

// note: the protocol does not allow direct umode +o except
// via NICK with 8 params. sending OPERTYPE infers +o modechange
// locally.
void ModuleSpanningTree::OnOper(User* user, const std::string &opertype)
{
	if (user->registered != REG_ALL || !IS_LOCAL(user))
		return;
	parameterlist params;
	params.push_back(":");
	params[0].append(opertype);
	Utils->DoOneToMany(user->uuid,"OPERTYPE",params);
}

void ModuleSpanningTree::OnAddLine(User* user, XLine *x)
{
	if (!x->IsBurstable() || loopCall)
		return;

	parameterlist params;
	params.push_back(x->type);
	params.push_back(x->Displayable());
	params.push_back(ServerInstance->Config->ServerName);
	params.push_back(ConvToStr(x->set_time));
	params.push_back(ConvToStr(x->duration));
	params.push_back(":" + x->reason);

	if (!user)
	{
		/* Server-set lines */
		Utils->DoOneToMany(ServerInstance->Config->GetSID(), "ADDLINE", params);
	}
	else if (IS_LOCAL(user))
	{
		/* User-set lines */
		Utils->DoOneToMany(user->uuid, "ADDLINE", params);
	}
}

void ModuleSpanningTree::OnDelLine(User* user, XLine *x)
{
	if (!x->IsBurstable() || loopCall)
		return;

	parameterlist params;
	params.push_back(x->type);
	params.push_back(x->Displayable());

	if (!user)
	{
		/* Server-unset lines */
		Utils->DoOneToMany(ServerInstance->Config->GetSID(), "DELLINE", params);
	}
	else if (IS_LOCAL(user))
	{
		/* User-unset lines */
		Utils->DoOneToMany(user->uuid, "DELLINE", params);
	}
}

ModResult ModuleSpanningTree::OnSetAway(User* user, const std::string &awaymsg)
{
	if (IS_LOCAL(user))
	{
		parameterlist params;
		if (!awaymsg.empty())
		{
			params.push_back(ConvToStr(user->awaytime));
			params.push_back(":" + awaymsg);
		}
		Utils->DoOneToMany(user->uuid, "AWAY", params);
	}

	return MOD_RES_PASSTHRU;
}

void ModuleSpanningTree::ProtoSendMode(void* opaque, TargetTypeFlags target_type, void* target, const parameterlist &modeline, const std::vector<TranslateType> &translate)
{
	TreeSocket* s = (TreeSocket*)opaque;
	std::string output_text = CommandParser::TranslateUIDs(translate, modeline);

	if (target)
	{
		if (target_type == TYPE_USER)
		{
			User* u = (User*)target;
			s->WriteLine(":"+ServerInstance->Config->GetSID()+" MODE "+u->uuid+" "+output_text);
		}
		else if (target_type == TYPE_CHANNEL)
		{
			Channel* c = (Channel*)target;
			s->WriteLine(":"+ServerInstance->Config->GetSID()+" FMODE "+c->name+" "+ConvToStr(c->age)+" "+output_text);
		}
	}
}

void ModuleSpanningTree::ProtoSendMetaData(void* opaque, Extensible* target, const std::string &extname, const std::string &extdata)
{
	TreeSocket* s = static_cast<TreeSocket*>(opaque);
	User* u = dynamic_cast<User*>(target);
	Channel* c = dynamic_cast<Channel*>(target);
	if (u)
		s->WriteLine(":"+ServerInstance->Config->GetSID()+" METADATA "+u->uuid+" "+extname+" :"+extdata);
	else if (c)
		s->WriteLine(":"+ServerInstance->Config->GetSID()+" METADATA "+c->name+" "+ConvToStr(c->age)+" "+extname+" :"+extdata);
	else if (!target)
		s->WriteLine(":"+ServerInstance->Config->GetSID()+" METADATA * "+extname+" :"+extdata);
}

CullResult ModuleSpanningTree::cull()
{
	if (Utils)
		Utils->cull();
	return this->Module::cull();
}

ModuleSpanningTree::~ModuleSpanningTree()
{
	delete ServerInstance->PI;
	ServerInstance->PI = new ProtocolInterface;

	/* This will also free the listeners */
	delete Utils;

	delete commands;
}

Version ModuleSpanningTree::GetVersion()
{
	return Version("Allows servers to be linked", VF_VENDOR);
}

/* It is IMPORTANT that m_spanningtree is the last module in the chain
 * so that any activity it sees is FINAL, e.g. we arent going to send out
 * a NICK message before m_cloaking has finished putting the +x on the user,
 * etc etc.
 * Therefore, we return PRIORITY_LAST to make sure we end up at the END of
 * the module call queue.
 */
void ModuleSpanningTree::Prioritize()
{
	ServerInstance->Modules->SetPriority(this, PRIORITY_LAST);
}

MODULE_INIT(ModuleSpanningTree)
