/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2009 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *	    the file COPYING for details.
 *
 * ---------------------------------------------------
 */

/* $ModDesc: Provides a spanning tree server link protocol */

#include "inspircd.h"
#include "socket.h"
#include "xline.h"
#include "../transport.h"

#include "cachetimer.h"
#include "resolvers.h"
#include "main.h"
#include "utils.h"
#include "treeserver.h"
#include "link.h"
#include "treesocket.h"
#include "rconnect.h"
#include "rsquit.h"
#include "protocolinterface.h"

/* $ModDep: m_spanningtree/cachetimer.h m_spanningtree/resolvers.h m_spanningtree/main.h m_spanningtree/utils.h m_spanningtree/treeserver.h m_spanningtree/link.h m_spanningtree/treesocket.h m_spanningtree/rconnect.h m_spanningtree/rsquit.h m_spanningtree/protocolinterface.h */

ModuleSpanningTree::ModuleSpanningTree(InspIRCd* Me)
	: Module(Me), max_local(0), max_global(0)
{
	ServerInstance->Modules->UseInterface("BufferedSocketHook");
	Utils = new SpanningTreeUtilities(ServerInstance, this);
	command_rconnect = new CommandRConnect(this, Utils);
	ServerInstance->AddCommand(command_rconnect);
	command_rsquit = new CommandRSQuit(this, Utils);
	ServerInstance->AddCommand(command_rsquit);
	RefreshTimer = new CacheRefreshTimer(ServerInstance, Utils);
	ServerInstance->Timers->AddTimer(RefreshTimer);

	Implementation eventlist[] =
	{
		I_OnPreCommand, I_OnGetServerDescription, I_OnUserInvite, I_OnPostTopicChange,
		I_OnWallops, I_OnUserNotice, I_OnUserMessage, I_OnBackgroundTimer, I_OnUserJoin,
		I_OnChangeLocalUserHost, I_OnChangeName, I_OnChangeIdent, I_OnUserPart, I_OnUnloadModule,
		I_OnUserQuit, I_OnUserPostNick, I_OnUserKick, I_OnRemoteKill, I_OnRehash, I_OnPreRehash,
		I_OnOper, I_OnAddLine, I_OnDelLine, I_OnMode, I_OnLoadModule, I_OnStats, I_OnEvent,
		I_OnSetAway, I_OnPostCommand, I_OnUserConnect
	};
	ServerInstance->Modules->Attach(eventlist, this, sizeof(eventlist)/sizeof(Implementation));

	delete ServerInstance->PI;
	ServerInstance->PI = new SpanningTreeProtocolInterface(this, Utils, ServerInstance);
	loopCall = false;

	// update our local user count
	Utils->TreeRoot->SetUserCount(ServerInstance->Users->local_users.size());
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
		if ((Current->GetChild(q)->Hidden) || ((Utils->HideULines) && (ServerInstance->ULine(Current->GetChild(q)->GetName().c_str()))))
		{
			if (IS_OPER(user))
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
	if ((Utils->HideULines) && (ServerInstance->ULine(Current->GetName().c_str())) && (!IS_OPER(user)))
		return;
	/* Or if the server is hidden and they're not an oper */
	else if ((Current->Hidden) && (!IS_OPER(user)))
		return;

	user->WriteNumeric(364, "%s %s %s :%d %s",	user->nick.c_str(),Current->GetName().c_str(),
			(Utils->FlatLinks && (!IS_OPER(user))) ? ServerInstance->Config->ServerName : Parent.c_str(),
			(Utils->FlatLinks && (!IS_OPER(user))) ? 0 : hops,
			Current->GetDesc().c_str());
}

int ModuleSpanningTree::CountLocalServs()
{
	return Utils->TreeRoot->ChildCount();
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

void ModuleSpanningTree::HandleLusers(const std::vector<std::string>& parameters, User* user)
{
	unsigned int n_users = ServerInstance->Users->UserCount();

	/* Only update these when someone wants to see them, more efficient */
	if ((unsigned int)ServerInstance->Users->LocalUserCount() > max_local)
		max_local = ServerInstance->Users->LocalUserCount();
	if (n_users > max_global)
		max_global = n_users;

	unsigned int ulined_count = 0;
	unsigned int ulined_local_count = 0;

	/* If ulined are hidden and we're not an oper, count the number of ulined servers hidden,
	 * locally and globally (locally means directly connected to us)
	 */
	if ((Utils->HideULines) && (!IS_OPER(user)))
	{
		for (server_hash::iterator q = Utils->serverlist.begin(); q != Utils->serverlist.end(); q++)
		{
			if (ServerInstance->ULine(q->second->GetName().c_str()))
			{
				ulined_count++;
				if (q->second->GetParent() == Utils->TreeRoot)
					ulined_local_count++;
			}
		}
	}
	user->WriteNumeric(251, "%s :There are %d users and %d invisible on %d servers",user->nick.c_str(),
			n_users-ServerInstance->Users->ModeCount('i'),
			ServerInstance->Users->ModeCount('i'),
			ulined_count ? this->CountServs() - ulined_count : this->CountServs());

	if (ServerInstance->Users->OperCount())
		user->WriteNumeric(252, "%s %d :operator(s) online",user->nick.c_str(),ServerInstance->Users->OperCount());

	if (ServerInstance->Users->UnregisteredUserCount())
		user->WriteNumeric(253, "%s %d :unknown connections",user->nick.c_str(),ServerInstance->Users->UnregisteredUserCount());

	if (ServerInstance->ChannelCount())
		user->WriteNumeric(254, "%s %ld :channels formed",user->nick.c_str(),ServerInstance->ChannelCount());

	user->WriteNumeric(255, "%s :I have %d clients and %d servers",user->nick.c_str(),ServerInstance->Users->LocalUserCount(),ulined_local_count ? this->CountLocalServs() - ulined_local_count : this->CountLocalServs());
	user->WriteNumeric(265, "%s :Current Local Users: %d  Max: %d",user->nick.c_str(),ServerInstance->Users->LocalUserCount(),max_local);
	user->WriteNumeric(266, "%s :Current Global Users: %d  Max: %d",user->nick.c_str(),n_users,max_global);
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
	return ((days_up ? (ConvToStr(days_up) + "d") : std::string(""))
			+ (hours_up ? (ConvToStr(hours_up) + "h") : std::string(""))
			+ (mins_up ? (ConvToStr(mins_up) + "m") : std::string(""))
			+ ConvToStr(secs) + "s");
}

void ModuleSpanningTree::DoPingChecks(time_t curtime)
{
	/*
	 * Cancel remote burst mode on any servers which still have it enabled due to latency/lack of data.
	 * This prevents lost REMOTECONNECT notices
	 */
	timeval t;
	gettimeofday(&t, NULL);
	long ts = (t.tv_sec * 1000) + (t.tv_usec / 1000);

	for (server_hash::iterator i = Utils->serverlist.begin(); i != Utils->serverlist.end(); i++)
	{
		TreeServer *s = i->second;

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
						tsock->WriteLine(std::string(":") + ServerInstance->Config->GetSID() + " PING " +
								ServerInstance->Config->GetSID() + " " + s->GetID());
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
						sock->Squit(s,"Ping timeout");
						ServerInstance->SE->DelFd(sock);
						sock->Close();
						return;
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

void ModuleSpanningTree::ConnectServer(Link* x)
{
	bool ipvalid = true;

	if (InspIRCd::Match(ServerInstance->Config->ServerName, assign(x->Name)))
	{
		ServerInstance->SNO->WriteToSnoMask('l', "CONNECT: Not connecting to myself.");
		return;
	}

	QueryType start_type = DNS_QUERY_A;
	start_type = DNS_QUERY_AAAA;
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
		if ((!x->Hook.empty()) && (Utils->hooks.find(x->Hook.c_str()) == Utils->hooks.end()))
			return;
		TreeSocket* newsocket = new TreeSocket(Utils, ServerInstance, x->IPAddr,x->Port, x->Timeout ? x->Timeout : 10,x->Name.c_str(), x->Bind, x->Hook.empty() ? NULL : Utils->hooks[x->Hook.c_str()]);
		if (newsocket->GetFd() > -1)
		{
			/* Handled automatically on success */
		}
		else
		{
			ServerInstance->SNO->WriteToSnoMask('l', "CONNECT: Error connecting \002%s\002: %s.",x->Name.c_str(),strerror(errno));
			if (ServerInstance->SocketCull.find(newsocket) == ServerInstance->SocketCull.end())
				ServerInstance->SocketCull[newsocket] = newsocket;
			Utils->DoFailOver(x);
		}
	}
	else
	{
		try
		{
			bool cached;
			ServernameResolver* snr = new ServernameResolver((Module*)this, Utils, ServerInstance,x->IPAddr, *x, cached, start_type);
			ServerInstance->AddResolver(snr, cached);
		}
		catch (ModuleException& e)
		{
			ServerInstance->SNO->WriteToSnoMask('l', "CONNECT: Error connecting \002%s\002: %s.",x->Name.c_str(), e.GetReason());
			Utils->DoFailOver(x);
		}
	}
}

void ModuleSpanningTree::AutoConnectServers(time_t curtime)
{
	for (std::vector<Link>::iterator x = Utils->LinkBlocks.begin(); x < Utils->LinkBlocks.end(); x++)
	{
		if ((x->AutoConnect) && (curtime >= x->NextConnectTime))
		{
			x->NextConnectTime = curtime + x->AutoConnect;
			TreeServer* CheckDupe = Utils->FindServer(x->Name.c_str());
			if (x->FailOver.length())
			{
				TreeServer* CheckFailOver = Utils->FindServer(x->FailOver.c_str());
				if (CheckFailOver)
				{
					/* The failover for this server is currently a member of the network.
					 * The failover probably succeeded, where the main link did not.
					 * Don't try the main link until the failover is gone again.
					 */
					continue;
				}
			}
			if (!CheckDupe)
			{
				// an autoconnected server is not connected. Check if its time to connect it
				ServerInstance->SNO->WriteToSnoMask('l',"AUTOCONNECT: Auto-connecting server \002%s\002 (%lu seconds until next attempt)",x->Name.c_str(),x->AutoConnect);
				this->ConnectServer(&(*x));
			}
		}
	}
}

void ModuleSpanningTree::DoConnectTimeout(time_t curtime)
{
	std::vector<Link*> failovers;
	for (std::map<TreeSocket*, std::pair<std::string, int> >::iterator i = Utils->timeoutlist.begin(); i != Utils->timeoutlist.end(); i++)
	{
		TreeSocket* s = i->first;
		std::pair<std::string, int> p = i->second;
		if (curtime > s->age + p.second)
		{
			ServerInstance->SNO->WriteToSnoMask('l',"CONNECT: Error connecting \002%s\002 (timeout of %d seconds)",p.first.c_str(),p.second);
			ServerInstance->SE->DelFd(s);
			s->Close();
			Link* MyLink = Utils->FindLink(p.first);
			if (MyLink)
				failovers.push_back(MyLink);
		}
	}
	/* Trigger failover for each timed out socket */
	for (std::vector<Link*>::const_iterator n = failovers.begin(); n != failovers.end(); ++n)
	{
		Utils->DoFailOver(*n);
	}
}

ModResult ModuleSpanningTree::HandleVersion(const std::vector<std::string>& parameters, User* user)
{
	// we've already checked if pcnt > 0, so this is safe
	TreeServer* found = Utils->FindServerMask(parameters[0]);
	if (found)
	{
		std::string Version = found->GetVersion();
		user->WriteNumeric(351, "%s :%s",user->nick.c_str(),Version.c_str());
		if (found == Utils->TreeRoot)
		{
			ServerInstance->Config->Send005(user);
		}
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
	char text[MAXBUF];
	va_list argsPtr;

	va_start(argsPtr, format);
	vsnprintf(text, MAXBUF, format, argsPtr);
	va_end(argsPtr);

	if (IS_LOCAL(user))
		user->WriteServ("NOTICE %s :%s", user->nick.c_str(), text);
	else
		ServerInstance->PI->SendUserNotice(user, text);
}

ModResult ModuleSpanningTree::HandleConnect(const std::vector<std::string>& parameters, User* user)
{
	for (std::vector<Link>::iterator x = Utils->LinkBlocks.begin(); x < Utils->LinkBlocks.end(); x++)
	{
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
				ConnectServer(&(*x));
				return MOD_RES_DENY;
			}
			else
			{
				RemoteMessage(user, "*** CONNECT: Server \002%s\002 already exists on the network and is connected via \002%s\002",x->Name.c_str(),CheckDupe->GetParent()->GetName().c_str());
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

void ModuleSpanningTree::OnWallops(User* user, const std::string &text)
{
	if (IS_LOCAL(user))
	{
		parameterlist params;
		params.push_back(":"+text);
		Utils->DoOneToMany(user->uuid,"WALLOPS",params);
	}
}

void ModuleSpanningTree::OnUserNotice(User* user, void* dest, int target_type, const std::string &text, char status, const CUList &exempt_list)
{
	/* Server origin */
	if (user == NULL)
		return;

	if (target_type == TYPE_USER)
	{
		User* d = (User*)dest;
		if ((d->GetFd() < 0) && (IS_LOCAL(user)))
		{
			parameterlist params;
			params.push_back(d->uuid);
			params.push_back(":"+text);
			Utils->DoOneToOne(user->uuid,"NOTICE",params,d->server);
		}
	}
	else if (target_type == TYPE_CHANNEL)
	{
		if (IS_LOCAL(user))
		{
			Channel *c = (Channel*)dest;
			if (c)
			{
				std::string cname = c->name;
				if (status)
					cname = status + cname;
				TreeServerList list;
				Utils->GetListOfServersForChannel(c,list,status,exempt_list);
				for (TreeServerList::iterator i = list.begin(); i != list.end(); i++)
				{
					TreeSocket* Sock = i->second->GetSocket();
					if (Sock)
						Sock->WriteLine(":"+std::string(user->uuid)+" NOTICE "+cname+" :"+text);
				}
			}
		}
	}
	else if (target_type == TYPE_SERVER)
	{
		if (IS_LOCAL(user))
		{
			char* target = (char*)dest;
			parameterlist par;
			par.push_back(target);
			par.push_back(":"+text);
			Utils->DoOneToMany(user->uuid,"NOTICE",par);
		}
	}
}

void ModuleSpanningTree::OnUserMessage(User* user, void* dest, int target_type, const std::string &text, char status, const CUList &exempt_list)
{
	/* Server origin */
	if (user == NULL)
		return;

	if (target_type == TYPE_USER)
	{
		// route private messages which are targetted at clients only to the server
		// which needs to receive them
		User* d = (User*)dest;
		if ((d->GetFd() < 0) && (IS_LOCAL(user)))
		{
			parameterlist params;
			params.push_back(d->uuid);
			params.push_back(":"+text);
			Utils->DoOneToOne(user->uuid,"PRIVMSG",params,d->server);
		}
	}
	else if (target_type == TYPE_CHANNEL)
	{
		if (IS_LOCAL(user))
		{
			Channel *c = (Channel*)dest;
			if (c)
			{
				std::string cname = c->name;
				if (status)
					cname = status + cname;
				TreeServerList list;
				Utils->GetListOfServersForChannel(c,list,status,exempt_list);
				for (TreeServerList::iterator i = list.begin(); i != list.end(); i++)
				{
					TreeSocket* Sock = i->second->GetSocket();
					if (Sock)
						Sock->WriteLine(":"+std::string(user->uuid)+" PRIVMSG "+cname+" :"+text);
				}
			}
		}
	}
	else if (target_type == TYPE_SERVER)
	{
		if (IS_LOCAL(user))
		{
			char* target = (char*)dest;
			parameterlist par;
			par.push_back(target);
			par.push_back(":"+text);
			Utils->DoOneToMany(user->uuid,"PRIVMSG",par);
		}
	}
}

void ModuleSpanningTree::OnBackgroundTimer(time_t curtime)
{
	AutoConnectServers(curtime);
	DoPingChecks(curtime);
	DoConnectTimeout(curtime);
}

void ModuleSpanningTree::OnUserConnect(User* user)
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
	params.push_back(":"+std::string(user->fullname));
	Utils->DoOneToMany(ServerInstance->Config->GetSID(), "UID", params);

	Utils->TreeRoot->SetUserCount(1); // increment by 1
}

void ModuleSpanningTree::OnUserJoin(Membership* memb, bool sync, bool created, CUList& excepts)
{
	// Only do this for local users
	if (IS_LOCAL(memb->user))
	{
		parameterlist params;
		// set up their permissions and the channel TS with FJOIN.
		// All users are FJOINed now, because a module may specify
		// new joining permissions for the user.
		params.push_back(memb->chan->name);
		params.push_back(ConvToStr(memb->chan->age));
		params.push_back(std::string("+") + memb->chan->ChanModes(true));
		params.push_back(memb->modes+","+std::string(memb->user->uuid));
		Utils->DoOneToMany(ServerInstance->Config->GetSID(),"FJOIN",params);
	}
}

ModResult ModuleSpanningTree::OnChangeLocalUserHost(User* user, const std::string &newhost)
{
	if (user->registered != REG_ALL)
		return MOD_RES_PASSTHRU;

	parameterlist params;
	params.push_back(newhost);
	Utils->DoOneToMany(user->uuid,"FHOST",params);
	return MOD_RES_PASSTHRU;
}

void ModuleSpanningTree::OnChangeName(User* user, const std::string &gecos)
{
	// only occurs for local clients
	if (user->registered != REG_ALL)
		return;

	parameterlist params;
	params.push_back(gecos);
	Utils->DoOneToMany(user->uuid,"FNAME",params);
}

void ModuleSpanningTree::OnChangeIdent(User* user, const std::string &ident)
{
	// only occurs for local clients
	if (user->registered != REG_ALL)
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
		{
			params.push_back(":"+oper_message);
			Utils->DoOneToMany(user->uuid,"OPERQUIT",params);
		}
		params.clear();
		params.push_back(":"+reason);
		Utils->DoOneToMany(user->uuid,"QUIT",params);
	}

	// Regardless, We need to modify the user Counts..
	TreeServer* SourceServer = Utils->FindServer(user->server);
	if (SourceServer)
	{
		SourceServer->SetUserCount(-1); // decrement by 1
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
	else if (IS_FAKE(source) && source != Utils->ServerUser)
	{
		Utils->DoOneToMany(ServerInstance->Config->GetSID(),"KICK",params);
	}
}

void ModuleSpanningTree::OnRemoteKill(User* source, User* dest, const std::string &reason, const std::string &operreason)
{
	if (!IS_LOCAL(source))
		return; // Only start routing if we're origin.

	User::OperQuit.set(dest, operreason);
	parameterlist params;
	params.push_back(":"+operreason);
	Utils->DoOneToMany(dest->uuid,"OPERQUIT",params);
	params.clear();
	params.push_back(dest->uuid);
	params.push_back(":"+reason);
	Utils->DoOneToMany(source->uuid,"KILL",params);
}

void ModuleSpanningTree::OnPreRehash(User* user, const std::string &parameter)
{
	ServerInstance->Logs->Log("remoterehash", DEBUG, "called with param %s", parameter.c_str());

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
	Utils->ReadConfiguration(true);
}

void ModuleSpanningTree::OnLoadModule(Module* mod, const std::string &name)
{
	this->RedoConfig(mod, name);
}

void ModuleSpanningTree::OnUnloadModule(Module* mod, const std::string &name)
{
	this->RedoConfig(mod, name);
}

void ModuleSpanningTree::RedoConfig(Module* mod, const std::string &name)
{
	/* If m_sha256.so is loaded (we use this for HMAC) or any module implementing a BufferedSocket interface is loaded,
	 * then we need to re-read our config again taking this into account.
	 */
	modulelist* ml = ServerInstance->Modules->FindInterface("BufferedSocketHook");
	bool IsBufferSocketModule = false;

	/* Did we find any modules? */
	if (ml && std::find(ml->begin(), ml->end(), mod) != ml->end())
		IsBufferSocketModule = true;

	if (name == "m_sha256.so" || IsBufferSocketModule)
	{
		Utils->ReadConfiguration(true);
	}
}

// note: the protocol does not allow direct umode +o except
// via NICK with 8 params. sending OPERTYPE infers +o modechange
// locally.
void ModuleSpanningTree::OnOper(User* user, const std::string &opertype)
{
	if (IS_LOCAL(user))
	{
		parameterlist params;
		params.push_back(opertype);
		Utils->DoOneToMany(user->uuid,"OPERTYPE",params);
	}
}

void ModuleSpanningTree::OnAddLine(User* user, XLine *x)
{
	if (!x->IsBurstable() || loopCall)
		return;

	char data[MAXBUF];
	snprintf(data,MAXBUF,"%s %s %s %lu %lu :%s", x->type.c_str(), x->Displayable(),
	ServerInstance->Config->ServerName, (unsigned long)x->set_time, (unsigned long)x->duration, x->reason.c_str());
	parameterlist params;
	params.push_back(data);

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
	if (x->type == "K")
		return;

	char data[MAXBUF];
	snprintf(data,MAXBUF,"%s %s", x->type.c_str(), x->Displayable());
	parameterlist params;
	params.push_back(data);

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

void ModuleSpanningTree::OnMode(User* user, void* dest, int target_type, const parameterlist &text, const std::vector<TranslateType> &translate)
{
	if ((IS_LOCAL(user)) && (user->registered == REG_ALL))
	{
		parameterlist params;
		std::string command;
		std::string output_text;

		ServerInstance->Parser->TranslateUIDs(translate, text, output_text);

		if (target_type == TYPE_USER)
		{
			User* u = (User*)dest;
			params.push_back(u->uuid);
			params.push_back(output_text);
			command = "MODE";
		}
		else
		{
			Channel* c = (Channel*)dest;
			params.push_back(c->name);
			params.push_back(ConvToStr(c->age));
			params.push_back(output_text);
			command = "FMODE";
		}

		Utils->DoOneToMany(user->uuid, command, params);
	}
}

ModResult ModuleSpanningTree::OnSetAway(User* user, const std::string &awaymsg)
{
	if (IS_LOCAL(user))
	{
		if (awaymsg.empty())
		{
			parameterlist params;
			Utils->DoOneToMany(user->uuid,"AWAY",params);
		}
		else
		{
			parameterlist params;
			params.push_back(ConvToStr(user->awaytime));
			params.push_back(":" + awaymsg);
			Utils->DoOneToMany(user->uuid,"AWAY",params);
		}
	}

	return MOD_RES_PASSTHRU;
}

void ModuleSpanningTree::ProtoSendMode(void* opaque, TargetTypeFlags target_type, void* target, const parameterlist &modeline, const std::vector<TranslateType> &translate)
{
	TreeSocket* s = (TreeSocket*)opaque;
	std::string output_text;

	ServerInstance->Parser->TranslateUIDs(translate, modeline, output_text);

	if (target)
	{
		if (target_type == TYPE_USER)
		{
			User* u = (User*)target;
			s->WriteLine(std::string(":")+ServerInstance->Config->GetSID()+" MODE "+u->uuid+" "+output_text);
		}
		else if (target_type == TYPE_CHANNEL)
		{
			Channel* c = (Channel*)target;
			s->WriteLine(std::string(":")+ServerInstance->Config->GetSID()+" FMODE "+c->name+" "+ConvToStr(c->age)+" "+output_text);
		}
	}
}

void ModuleSpanningTree::ProtoSendMetaData(void* opaque, Extensible* target, const std::string &extname, const std::string &extdata)
{
	TreeSocket* s = static_cast<TreeSocket*>(opaque);
	User* u = dynamic_cast<User*>(target);
	Channel* c = dynamic_cast<Channel*>(target);
	if (u)
		s->WriteLine(std::string(":")+ServerInstance->Config->GetSID()+" METADATA "+u->uuid+" "+extname+" :"+extdata);
	else if (c)
		s->WriteLine(std::string(":")+ServerInstance->Config->GetSID()+" METADATA "+c->name+" "+extname+" :"+extdata);
	else if (!target)
		s->WriteLine(std::string(":")+ServerInstance->Config->GetSID()+" METADATA * "+extname+" :"+extdata);
}

void ModuleSpanningTree::OnEvent(Event* event)
{
	if ((event->GetEventID() == "send_encap") || (event->GetEventID() == "send_metadata") || (event->GetEventID() == "send_topic") || (event->GetEventID() == "send_mode") || (event->GetEventID() == "send_mode_explicit") || (event->GetEventID() == "send_opers")
		|| (event->GetEventID() == "send_modeset") || (event->GetEventID() == "send_snoset") || (event->GetEventID() == "send_push"))
	{
		ServerInstance->Logs->Log("m_spanningtree", DEBUG, "WARNING: Deprecated use of old 1.1 style m_spanningtree event ignored, type '"+event->GetEventID()+"'!");
	}
}

ModuleSpanningTree::~ModuleSpanningTree()
{
	delete ServerInstance->PI;
	ServerInstance->PI = new ProtocolInterface(ServerInstance);

	/* This will also free the listeners */
	delete Utils;

	delete command_rconnect;
	delete command_rsquit;

	ServerInstance->Timers->DelTimer(RefreshTimer);

	ServerInstance->Modules->DoneWithInterface("BufferedSocketHook");
}

Version ModuleSpanningTree::GetVersion()
{
	return Version("$Id$", VF_VENDOR, API_VERSION);
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
