/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2007 InspIRCd Development Team
 * See: http://www.inspircd.org/wiki/index.php/Credits
 *
 * This program is free but copyrighted software; see
 *	    the file COPYING for details.
 *
 * ---------------------------------------------------
 */

/* $ModDesc: Provides a spanning tree server link protocol */

#include "inspircd.h"
#include "commands/cmd_whois.h"
#include "commands/cmd_stats.h"
#include "socket.h"
#include "wildcard.h"
#include "xline.h"
#include "transport.h"

#include "m_spanningtree/timesynctimer.h"
#include "m_spanningtree/resolvers.h"
#include "m_spanningtree/main.h"
#include "m_spanningtree/utils.h"
#include "m_spanningtree/treeserver.h"
#include "m_spanningtree/link.h"
#include "m_spanningtree/treesocket.h"
#include "m_spanningtree/rconnect.h"
#include "m_spanningtree/rsquit.h"

/* $ModDep: m_spanningtree/timesynctimer.h m_spanningtree/resolvers.h m_spanningtree/main.h m_spanningtree/utils.h m_spanningtree/treeserver.h m_spanningtree/link.h m_spanningtree/treesocket.h m_spanningtree/rconnect.h m_spanningtree/rsquit.h */

ModuleSpanningTree::ModuleSpanningTree(InspIRCd* Me)
	: Module(Me), max_local(0), max_global(0)
{
	ServerInstance->Modules->UseInterface("BufferedSocketHook");
	Utils = new SpanningTreeUtilities(Me, this);
	command_rconnect = new cmd_rconnect(ServerInstance, this, Utils);
	ServerInstance->AddCommand(command_rconnect);
	command_rsquit = new cmd_rsquit(ServerInstance, this, Utils);
	ServerInstance->AddCommand(command_rsquit);
	if (Utils->EnableTimeSync)
	{
		SyncTimer = new TimeSyncTimer(ServerInstance, this);
		ServerInstance->Timers->AddTimer(SyncTimer);
	}
	else
		SyncTimer = NULL;

	RefreshTimer = new CacheRefreshTimer(ServerInstance, Utils);
	ServerInstance->Timers->AddTimer(RefreshTimer);

	Implementation eventlist[] =
	{
		I_OnPreCommand, I_OnGetServerDescription, I_OnUserInvite, I_OnPostLocalTopicChange,
		I_OnWallops, I_OnUserNotice, I_OnUserMessage, I_OnBackgroundTimer,
		I_OnUserJoin, I_OnChangeHost, I_OnChangeName, I_OnUserPart, I_OnUserConnect,
		I_OnUserQuit, I_OnUserPostNick, I_OnUserKick, I_OnRemoteKill, I_OnRehash,
		I_OnOper, I_OnAddLine, I_OnDelLine, I_ProtoSendMode, I_OnMode,
		I_OnStats, I_ProtoSendMetaData, I_OnEvent, I_OnSetAway, I_OnCancelAway, I_OnPostCommand
	};
	ServerInstance->Modules->Attach(eventlist, this, 29);
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
			if (*user->oper)
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

	user->WriteServ("364 %s %s %s :%d %s",	user->nick,Current->GetName().c_str(),
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

void ModuleSpanningTree::HandleLinks(const char** parameters, int pcnt, User* user)
{
	ShowLinks(Utils->TreeRoot,user,0);
	user->WriteServ("365 %s * :End of /LINKS list.",user->nick);
	return;
}

void ModuleSpanningTree::HandleLusers(const char** parameters, int pcnt, User* user)
{
	unsigned int n_users = ServerInstance->UserCount();

	/* Only update these when someone wants to see them, more efficient */
	if ((unsigned int)ServerInstance->LocalUserCount() > max_local)
		max_local = ServerInstance->LocalUserCount();
	if (n_users > max_global)
		max_global = n_users;

	unsigned int ulined_count = 0;
	unsigned int ulined_local_count = 0;

	/* If ulined are hidden and we're not an oper, count the number of ulined servers hidden,
	 * locally and globally (locally means directly connected to us)
	 */
	if ((Utils->HideULines) && (!*user->oper))
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
	user->WriteServ("251 %s :There are %d users and %d invisible on %d servers",user->nick,
			n_users-ServerInstance->ModeCount('i'),
			ServerInstance->ModeCount('i'),
			ulined_count ? this->CountServs() - ulined_count : this->CountServs());

	if (ServerInstance->OperCount())
		user->WriteServ("252 %s %d :operator(s) online",user->nick,ServerInstance->OperCount());

	if (ServerInstance->UnregisteredUserCount())
		user->WriteServ("253 %s %d :unknown connections",user->nick,ServerInstance->UnregisteredUserCount());
	
	if (ServerInstance->ChannelCount())
		user->WriteServ("254 %s %d :channels formed",user->nick,ServerInstance->ChannelCount());
	
	user->WriteServ("255 %s :I have %d clients and %d servers",user->nick,ServerInstance->LocalUserCount(),ulined_local_count ? this->CountLocalServs() - ulined_local_count : this->CountLocalServs());
	user->WriteServ("265 %s :Current Local Users: %d  Max: %d",user->nick,ServerInstance->LocalUserCount(),max_local);
	user->WriteServ("266 %s :Current Global Users: %d  Max: %d",user->nick,n_users,max_global);
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
	for (unsigned int j = 0; j < Utils->TreeRoot->ChildCount(); j++)
	{
		TreeServer* serv = Utils->TreeRoot->GetChild(j);
		TreeSocket* sock = serv->GetSocket();
		if (sock)
		{
			if (curtime >= serv->NextPingTime())
			{
				if (serv->AnsweredLastPing())
				{
					sock->WriteLine(std::string(":")+ServerInstance->Config->GetSID()+" PING "+serv->GetID());
					serv->SetNextPingTime(curtime + Utils->PingFreq);
					serv->LastPing = curtime;
					timeval t;
					gettimeofday(&t, NULL);
					long ts = (t.tv_sec * 1000) + (t.tv_usec / 1000);
					serv->LastPingMsec = ts;
					serv->Warned = false;
				}
				else
				{
					/* they didnt answer, boot them */
					sock->SendError("Ping timeout");
					sock->Squit(serv,"Ping timeout");
					ServerInstance->SE->DelFd(sock);
					sock->Close();
					return;
				}
			}
			else if ((Utils->PingWarnTime) && (!serv->Warned) && (curtime >= serv->NextPingTime() - (Utils->PingFreq - Utils->PingWarnTime)) && (!serv->AnsweredLastPing()))
			{
				/* The server hasnt responded, send a warning to opers */
				ServerInstance->SNO->WriteToSnoMask('l',"Server \002%s\002 has not responded to PING for %d seconds, high latency.", serv->GetName().c_str(), Utils->PingWarnTime);
				serv->Warned = true;
			}
		}
	}

	/* Cancel remote burst mode on any servers which still have it enabled due to latency/lack of data.
	 * This prevents lost REMOTECONNECT notices
	 */
	for (server_hash::iterator i = Utils->serverlist.begin(); i != Utils->serverlist.end(); i++)
		Utils->SetRemoteBursting(i->second, false);
}

void ModuleSpanningTree::ConnectServer(Link* x)
{
	bool ipvalid = true;
	QueryType start_type = DNS_QUERY_A;
#ifdef IPV6
	start_type = DNS_QUERY_AAAA;
	if (strchr(x->IPAddr.c_str(),':'))
	{
		in6_addr n;
		if (inet_pton(AF_INET6, x->IPAddr.c_str(), &n) < 1)
			ipvalid = false;
	}
	else
#endif
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
		TreeSocket* newsocket = new TreeSocket(Utils, ServerInstance, x->IPAddr,x->Port,false,x->Timeout ? x->Timeout : 10,x->Name.c_str(), x->Bind, x->Hook.empty() ? NULL : Utils->hooks[x->Hook.c_str()]);
		if (newsocket->GetFd() > -1)
		{
			/* Handled automatically on success */
		}
		else
		{
			RemoteMessage(NULL, "CONNECT: Error connecting \002%s\002: %s.",x->Name.c_str(),strerror(errno));
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
			RemoteMessage(NULL, "CONNECT: Error connecting \002%s\002: %s.",x->Name.c_str(), e.GetReason());
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

int ModuleSpanningTree::HandleVersion(const char** parameters, int pcnt, User* user)
{
	// we've already checked if pcnt > 0, so this is safe
	TreeServer* found = Utils->FindServerMask(parameters[0]);
	if (found)
	{
		std::string Version = found->GetVersion();
		user->WriteServ("351 %s :%s",user->nick,Version.c_str());
		if (found == Utils->TreeRoot)
		{
			ServerInstance->Config->Send005(user);
		}
	}
	else
	{
		user->WriteServ("402 %s %s :No such server",user->nick,parameters[0]);
	}
	return 1;
}

/* This method will attempt to get a link message out to as many people as is required.
 * If a user is provided, and that user is local, then the user is sent the message using
 * WriteServ (they are the local initiator of that message). If the user is remote, they are
 * sent that message remotely via PUSH.
 * If the user is NULL, then the notice is sent locally via WriteToSnoMask with snomask 'l',
 * and remotely via SNONOTICE with mask 'l'.
 */
void ModuleSpanningTree::RemoteMessage(User* user, const char* format, ...)
{
	/* This could cause an infinite loop, because DoOneToMany() will, on error,
	 * call TreeSocket::OnError(), which in turn will call this function to
	 * notify everyone of the error. So, drop any messages that are generated
	 * during the sending of another message. -Special */
	static bool SendingRemoteMessage = false;
	if (SendingRemoteMessage)
		return;
	SendingRemoteMessage = true;

	std::deque<std::string> params;
	char text[MAXBUF];
	va_list argsPtr;

	va_start(argsPtr, format);
	vsnprintf(text, MAXBUF, format, argsPtr);
	va_end(argsPtr);

	if (!user)
	{
		/* No user, target it generically at everyone */
		ServerInstance->SNO->WriteToSnoMask('l', "%s", text);
		params.push_back("l");
		params.push_back(std::string(":") + text);
		Utils->DoOneToMany(ServerInstance->Config->GetSID(), "SNONOTICE", params);
	}
	else
	{
		if (IS_LOCAL(user))
			user->WriteServ("NOTICE %s :%s", user->nick, text);
		else
		{
			params.push_back(user->uuid);
			params.push_back(std::string("::") + ServerInstance->Config->ServerName + " NOTICE " + user->nick + " :*** From " +
					ServerInstance->Config->ServerName+ ": " + text);
			Utils->DoOneToMany(ServerInstance->Config->GetSID(), "PUSH", params);
		}
	}
	
	SendingRemoteMessage = false;
}
	
int ModuleSpanningTree::HandleConnect(const char** parameters, int pcnt, User* user)
{
	for (std::vector<Link>::iterator x = Utils->LinkBlocks.begin(); x < Utils->LinkBlocks.end(); x++)
	{
		if (ServerInstance->MatchText(x->Name.c_str(),parameters[0]))
		{
			TreeServer* CheckDupe = Utils->FindServer(x->Name.c_str());
			if (!CheckDupe)
			{
				RemoteMessage(user, "*** CONNECT: Connecting to server: \002%s\002 (%s:%d)",x->Name.c_str(),(x->HiddenFromStats ? "<hidden>" : x->IPAddr.c_str()),x->Port);
				ConnectServer(&(*x));
				return 1;
			}
			else
			{
				RemoteMessage(user, "*** CONNECT: Server \002%s\002 already exists on the network and is connected via \002%s\002",x->Name.c_str(),CheckDupe->GetParent()->GetName().c_str());
				return 1;
			}
		}
	}
	RemoteMessage(user, "*** CONNECT: No server matching \002%s\002 could be found in the config file.",parameters[0]);
	return 1;
}

void ModuleSpanningTree::BroadcastTimeSync()
{
	if (Utils->MasterTime)
	{
		std::deque<std::string> params;
		params.push_back(ConvToStr(ServerInstance->Time(false)));
		params.push_back("FORCE");
		Utils->DoOneToMany(ServerInstance->Config->GetSID(), "TIMESET", params);
	}
}

void ModuleSpanningTree::OnGetServerDescription(const std::string &servername,std::string &description)
{
	TreeServer* s = Utils->FindServer(servername);
	if (s)
	{
		description = s->GetDesc();
	}
}

void ModuleSpanningTree::OnUserInvite(User* source,User* dest,Channel* channel)
{
	if (IS_LOCAL(source))
	{
		std::deque<std::string> params;
		params.push_back(dest->uuid);
		params.push_back(channel->name);
		Utils->DoOneToMany(source->uuid,"INVITE",params);
	}
}

void ModuleSpanningTree::OnPostLocalTopicChange(User* user, Channel* chan, const std::string &topic)
{
	std::deque<std::string> params;
	params.push_back(chan->name);
	params.push_back(":"+topic);
	Utils->DoOneToMany(user->uuid,"TOPIC",params);
}

void ModuleSpanningTree::OnWallops(User* user, const std::string &text)
{
	if (IS_LOCAL(user))
	{
		std::deque<std::string> params;
		params.push_back(":"+text);
		Utils->DoOneToMany(user->uuid,"WALLOPS",params);
	}
}

void ModuleSpanningTree::OnUserNotice(User* user, void* dest, int target_type, const std::string &text, char status, const CUList &exempt_list)
{
	if (target_type == TYPE_USER)
	{
		User* d = (User*)dest;
		if ((d->GetFd() < 0) && (IS_LOCAL(user)))
		{
			std::deque<std::string> params;
			params.clear();
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
			std::deque<std::string> par;
			par.push_back(target);
			par.push_back(":"+text);
			Utils->DoOneToMany(user->uuid,"NOTICE",par);
		}
	}
}

void ModuleSpanningTree::OnUserMessage(User* user, void* dest, int target_type, const std::string &text, char status, const CUList &exempt_list)
{
	if (target_type == TYPE_USER)
	{
		// route private messages which are targetted at clients only to the server
		// which needs to receive them
		User* d = (User*)dest;
		if ((d->GetFd() < 0) && (IS_LOCAL(user)))
		{
			std::deque<std::string> params;
			params.clear();
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
			std::deque<std::string> par;
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
}

void ModuleSpanningTree::OnUserJoin(User* user, Channel* channel, bool sync, bool &silent)
{
	// Only do this for local users
	if (IS_LOCAL(user))
	{
		if (channel->GetUserCounter() == 1)
		{
			std::deque<std::string> params;
			// set up their permissions and the channel TS with FJOIN.
			// All users are FJOINed now, because a module may specify
			// new joining permissions for the user.
			params.push_back(channel->name);
			params.push_back(ConvToStr(channel->age));
			params.push_back(std::string(channel->GetAllPrefixChars(user))+","+std::string(user->uuid));
			Utils->DoOneToMany(ServerInstance->Config->GetSID(),"FJOIN",params);
			/* First user in, sync the modes for the channel */
			params.pop_back();
			params.push_back(std::string("+") + channel->ChanModes(true));
			Utils->DoOneToMany(ServerInstance->Config->GetSID(),"FMODE",params);
		}
		else
		{
			std::deque<std::string> params;
			params.push_back(channel->name);
			params.push_back(ConvToStr(channel->age));
			Utils->DoOneToMany(user->uuid,"JOIN",params);
		}
	}
}

void ModuleSpanningTree::OnChangeHost(User* user, const std::string &newhost)
{
	// only occurs for local clients
	if (user->registered != REG_ALL)
		return;
	std::deque<std::string> params;
	params.push_back(newhost);
	Utils->DoOneToMany(user->uuid,"FHOST",params);
}

void ModuleSpanningTree::OnChangeName(User* user, const std::string &gecos)
{
	// only occurs for local clients
	if (user->registered != REG_ALL)
		return;
	std::deque<std::string> params;
	params.push_back(gecos);
	Utils->DoOneToMany(user->uuid,"FNAME",params);
}

void ModuleSpanningTree::OnUserPart(User* user, Channel* channel, const std::string &partmessage, bool &silent)
{
	if (IS_LOCAL(user))
	{
		std::deque<std::string> params;
		params.push_back(channel->name);
		if (!partmessage.empty())
			params.push_back(":"+partmessage);
		Utils->DoOneToMany(user->uuid,"PART",params);
	}
}

void ModuleSpanningTree::OnUserConnect(User* user)
{
	if (IS_LOCAL(user))
	{
		std::deque<std::string> params;
		params.push_back(user->uuid);
		params.push_back(ConvToStr(user->age));
		params.push_back(user->nick);
		params.push_back(user->host);
		params.push_back(user->dhost);
		params.push_back(user->ident);
		params.push_back("+"+std::string(user->FormatModes()));
		params.push_back(user->GetIPString());
		params.push_back(ConvToStr(user->signon));
		params.push_back(":"+std::string(user->fullname));
		Utils->DoOneToMany(ServerInstance->Config->GetSID(), "UID", params);
		// User is Local, change needs to be reflected!
		TreeServer* SourceServer = Utils->FindServer(user->server);
		if (SourceServer)
		{
			SourceServer->AddUserCount();
		}
	}
}

void ModuleSpanningTree::OnUserQuit(User* user, const std::string &reason, const std::string &oper_message)
{
	if ((IS_LOCAL(user)) && (user->registered == REG_ALL))
	{
		std::deque<std::string> params;

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
		SourceServer->DelUserCount();
	}
}

void ModuleSpanningTree::OnUserPostNick(User* user, const std::string &oldnick)
{
	if (IS_LOCAL(user))
	{
		std::deque<std::string> params;
		params.push_back(user->nick);

		/** IMPORTANT: We don't update the TS if the oldnick is just a case change of the newnick!
		 */
		if (irc::string(user->nick) != assign(oldnick))
			user->age = ServerInstance->Time(true);

		params.push_back(ConvToStr(user->age));
		Utils->DoOneToMany(user->uuid,"NICK",params);
	}
}

void ModuleSpanningTree::OnUserKick(User* source, User* user, Channel* chan, const std::string &reason, bool &silent)
{
	if ((source) && (IS_LOCAL(source)))
	{
		std::deque<std::string> params;
		params.push_back(chan->name);
		params.push_back(user->uuid);
		params.push_back(":"+reason);
		Utils->DoOneToMany(source->uuid,"KICK",params);
	}
	else if (!source)
	{
		std::deque<std::string> params;
		params.push_back(chan->name);
		params.push_back(user->uuid);
		params.push_back(":"+reason);
		Utils->DoOneToMany(ServerInstance->Config->GetSID(),"KICK",params);
	}
}

void ModuleSpanningTree::OnRemoteKill(User* source, User* dest, const std::string &reason, const std::string &operreason)
{
	std::deque<std::string> params;
	params.push_back(":"+reason);
	Utils->DoOneToMany(dest->uuid,"OPERQUIT",params);
	params.clear();
	params.push_back(dest->uuid);
	params.push_back(":"+reason);
	dest->SetOperQuit(operreason);
	Utils->DoOneToMany(source->uuid,"KILL",params);
}

void ModuleSpanningTree::OnRehash(User* user, const std::string &parameter)
{
	if (!parameter.empty())
	{
		std::deque<std::string> params;
		params.push_back(parameter);
		Utils->DoOneToMany(user ? user->nick : ServerInstance->Config->GetSID(), "REHASH", params);
		// check for self
		if (ServerInstance->MatchText(ServerInstance->Config->ServerName,parameter))
		{
			ServerInstance->WriteOpers("*** Remote rehash initiated locally by \002%s\002", user ? user->nick : ServerInstance->Config->ServerName);
			ServerInstance->RehashServer();
		}
	}
	Utils->ReadConfiguration(false);
	InitializeDisabledCommands(ServerInstance->Config->DisabledCommands, ServerInstance);
}

// note: the protocol does not allow direct umode +o except
// via NICK with 8 params. sending OPERTYPE infers +o modechange
// locally.
void ModuleSpanningTree::OnOper(User* user, const std::string &opertype)
{
	if (IS_LOCAL(user))
	{
		std::deque<std::string> params;
		params.push_back(opertype);
		Utils->DoOneToMany(user->uuid,"OPERTYPE",params);
	}
}

void ModuleSpanningTree::OnAddLine(XLine* line, User* user)
{
	if (line->type == "K")
		return;

	char data[MAXBUF];
	snprintf(data,MAXBUF,"%s %s %s %lu %lu :%s", line->type.c_str(), line->Displayable(), ServerInstance->Config->ServerName, line->set_time,
			line->duration, line->reason);
	std::deque<std::string> params;
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

void ModuleSpanningTree::OnDelLine(XLine* line, User* user)
{
	if (line->type == "K")
		return;

	char data[MAXBUF];
	snprintf(data,MAXBUF,"%s %s", line->type.c_str(), line->Displayable());
	std::deque<std::string> params;
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

void ModuleSpanningTree::OnMode(User* user, void* dest, int target_type, const std::string &text)
{
	if ((IS_LOCAL(user)) && (user->registered == REG_ALL))
	{
		std::deque<std::string> params;
		std::string command;
		std::string output_text;

		ServerInstance->Parser->TranslateUIDs(TR_SPACENICKLIST, text, output_text);

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

void ModuleSpanningTree::OnSetAway(User* user)
{
	if (IS_LOCAL(user))
	{
		std::deque<std::string> params;
		params.push_back(":"+std::string(user->awaymsg));
		Utils->DoOneToMany(user->uuid,"AWAY",params);
	}
}

void ModuleSpanningTree::OnCancelAway(User* user)
{
	if (IS_LOCAL(user))
	{
		std::deque<std::string> params;
		params.clear();
		Utils->DoOneToMany(user->uuid,"AWAY",params);
	}
}

void ModuleSpanningTree::ProtoSendMode(void* opaque, int target_type, void* target, const std::string &modeline)
{
	TreeSocket* s = (TreeSocket*)opaque;
	std::string output_text;

	ServerInstance->Parser->TranslateUIDs(TR_SPACENICKLIST, modeline, output_text);

	if (target)
	{
		if (target_type == TYPE_USER)
		{
			User* u = (User*)target;
			s->WriteLine(std::string(":")+ServerInstance->Config->GetSID()+" FMODE "+u->uuid+" "+ConvToStr(u->age)+" "+output_text);
		}
		else
		{
			Channel* c = (Channel*)target;
			s->WriteLine(std::string(":")+ServerInstance->Config->GetSID()+" FMODE "+c->name+" "+ConvToStr(c->age)+" "+output_text);
		}
	}
}

void ModuleSpanningTree::ProtoSendMetaData(void* opaque, int target_type, void* target, const std::string &extname, const std::string &extdata)
{
	TreeSocket* s = (TreeSocket*)opaque;
	if (target)
	{
		if (target_type == TYPE_USER)
		{
			User* u = (User*)target;
			s->WriteLine(std::string(":")+ServerInstance->Config->GetSID()+" METADATA "+u->uuid+" "+extname+" :"+extdata);
		}
		else if (target_type == TYPE_CHANNEL)
		{
			Channel* c = (Channel*)target;
			s->WriteLine(std::string(":")+ServerInstance->Config->GetSID()+" METADATA "+c->name+" "+extname+" :"+extdata);
		}
	}
	if (target_type == TYPE_OTHER)
	{
		s->WriteLine(std::string(":")+ServerInstance->Config->GetSID()+" METADATA * "+extname+" :"+extdata);
	}
}

void ModuleSpanningTree::OnEvent(Event* event)
{
	std::deque<std::string>* params = (std::deque<std::string>*)event->GetData();
	if (event->GetEventID() == "send_metadata")
	{
		if (params->size() < 3)
			return;
		(*params)[2] = ":" + (*params)[2];
		Utils->DoOneToMany(ServerInstance->Config->GetSID(),"METADATA",*params);
	}
	else if (event->GetEventID() == "send_topic")
	{
		if (params->size() < 2)
			return;
		(*params)[1] = ":" + (*params)[1];
		params->insert(params->begin() + 1,ServerInstance->Config->ServerName);
		params->insert(params->begin() + 1,ConvToStr(ServerInstance->Time(true)));
		Utils->DoOneToMany(ServerInstance->Config->GetSID(),"FTOPIC",*params);
	}
	else if (event->GetEventID() == "send_mode")
	{
		if (params->size() < 2)
			return;
		// Insert the TS value of the object, either User or Channel
		time_t ourTS = 0;
		std::string output_text;

		/* Warning: in-place translation is only safe for type TR_NICK */
		for (size_t n = 0; n < params->size(); n++)
			ServerInstance->Parser->TranslateUIDs(TR_NICK, (*params)[n], (*params)[n]);

		User* a = ServerInstance->FindNick((*params)[0]);
		if (a)
		{
			ourTS = a->age;
			Utils->DoOneToMany(ServerInstance->Config->GetSID(),"MODE",*params);
			return;
		}
		else
		{
			Channel* a = ServerInstance->FindChan((*params)[0]);
			if (a)
			{
				ourTS = a->age;
				params->insert(params->begin() + 1,ConvToStr(ourTS));
				Utils->DoOneToMany(ServerInstance->Config->GetSID(),"FMODE",*params);
			}
		}
	}
	else if (event->GetEventID() == "send_mode_explicit")
	{
		if (params->size() < 2)
			return;
		std::string output_text;

		/* Warning: in-place translation is only safe for type TR_NICK */
		for (size_t n = 0; n < params->size(); n++)
			ServerInstance->Parser->TranslateUIDs(TR_NICK, (*params)[n], (*params)[n]);

		Utils->DoOneToMany(ServerInstance->Config->GetSID(),"MODE",*params);
	}
	else if (event->GetEventID() == "send_opers")
	{
		if (params->size() < 1)
			return;
		(*params)[0] = ":" + (*params)[0];
		Utils->DoOneToMany(ServerInstance->Config->GetSID(),"OPERNOTICE",*params);
	}
	else if (event->GetEventID() == "send_modeset")
	{
		if (params->size() < 2)
			return;
		(*params)[1] = ":" + (*params)[1];
		Utils->DoOneToMany(ServerInstance->Config->GetSID(),"MODENOTICE",*params);
	}
	else if (event->GetEventID() == "send_snoset")
	{
		if (params->size() < 2)
			return;
		(*params)[1] = ":" + (*params)[1];
		Utils->DoOneToMany(ServerInstance->Config->GetSID(),"SNONOTICE",*params);
	}
	else if (event->GetEventID() == "send_push")
	{
		if (params->size() < 2)
			return;
			
		User *a = ServerInstance->FindNick((*params)[0]);
			
		if (!a)
			return;

		(*params)[0] = a->uuid;
		(*params)[1] = ":" + (*params)[1];
		Utils->DoOneToOne(ServerInstance->Config->GetSID(), "PUSH", *params, a->server);
	}
}

ModuleSpanningTree::~ModuleSpanningTree()
{
	/* This will also free the listeners */
	delete Utils;
	if (SyncTimer)
		ServerInstance->Timers->DelTimer(SyncTimer);

	ServerInstance->Timers->DelTimer(RefreshTimer);

	ServerInstance->Modules->DoneWithInterface("BufferedSocketHook");
}

Version ModuleSpanningTree::GetVersion()
{
	return Version(1,1,0,2,VF_VENDOR,API_VERSION);
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
	ServerInstance->Modules->SetPriority(this, PRIO_LAST);
}

MODULE_INIT(ModuleSpanningTree)
