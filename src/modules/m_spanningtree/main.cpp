/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2010 InspIRCd Development Team
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

#include "cachetimer.h"
#include "resolvers.h"
#include "main.h"
#include "utils.h"
#include "treeserver.h"
#include "link.h"
#include "treesocket.h"
#include "commands.h"
#include "protocolinterface.h"

ModuleSpanningTree::ModuleSpanningTree()
{
	Utils = new SpanningTreeUtilities(this);
	commands = new SpanningTreeCommands(this);
	RefreshTimer = NULL;
}

SpanningTreeCommands::SpanningTreeCommands(ModuleSpanningTree* module)
	: rconnect(module, module->Utils), rsquit(module, module->Utils), autoconnect(module, module->Utils),
	svsjoin(module), svspart(module), svsnick(module), metadata(module),
	uid(module), opertype(module), fjoin(module), fmode(module), ftopic(module),
	fhost(module), fident(module), fname(module)
{
}

void ModuleSpanningTree::init()
{
	ServerInstance->Modules->AddService(commands->rconnect);
	ServerInstance->Modules->AddService(commands->rsquit);
	ServerInstance->Modules->AddService(commands->autoconnect);
	ServerInstance->Modules->AddService(commands->svsjoin);
	ServerInstance->Modules->AddService(commands->svspart);
	ServerInstance->Modules->AddService(commands->svsnick);
	ServerInstance->Modules->AddService(commands->metadata);
	ServerInstance->Modules->AddService(commands->uid);
	ServerInstance->Modules->AddService(commands->opertype);
	ServerInstance->Modules->AddService(commands->fjoin);
	ServerInstance->Modules->AddService(commands->fmode);
	ServerInstance->Modules->AddService(commands->ftopic);
	ServerInstance->Modules->AddService(commands->fhost);
	ServerInstance->Modules->AddService(commands->fident);
	ServerInstance->Modules->AddService(commands->fname);
	RefreshTimer = new CacheRefreshTimer(Utils);
	ServerInstance->Timers->AddTimer(RefreshTimer);

	Implementation eventlist[] =
	{
		I_OnPreCommand, I_OnGetServerDescription, I_OnUserInvite, I_OnPostTopicChange,
		I_OnWallops, I_OnUserNotice, I_OnUserMessage, I_OnBackgroundTimer, I_OnUserJoin,
		I_OnChangeHost, I_OnChangeName, I_OnChangeIdent, I_OnUserPart, I_OnUnloadModule,
		I_OnUserQuit, I_OnUserPostNick, I_OnUserKick, I_OnRemoteKill,
		I_OnOper, I_OnAddLine, I_OnDelLine, I_OnLoadModule, I_OnStats,
		I_OnSetAway, I_OnPostCommand, I_OnUserConnect, I_OnAcceptConnection
	};
	ServerInstance->Modules->Attach(eventlist, this, sizeof(eventlist)/sizeof(Implementation));

	delete ServerInstance->PI;
	ServerInstance->PI = new SpanningTreeProtocolInterface(this, Utils);
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
			(Utils->FlatLinks && (!IS_OPER(user))) ? ServerInstance->Config->ServerName.c_str() : Parent.c_str(),
			(Utils->FlatLinks && (!IS_OPER(user))) ? 0 : hops,
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
	return ((days_up ? (ConvToStr(days_up) + "d") : std::string(""))
			+ (hours_up ? (ConvToStr(hours_up) + "h") : std::string(""))
			+ (mins_up ? (ConvToStr(mins_up) + "m") : std::string(""))
			+ ConvToStr(secs) + "s");
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

	if (InspIRCd::Match(ServerInstance->Config->ServerName, x->Name))
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
		TreeSocket* newsocket = new TreeSocket(Utils, x, y, x->IPAddr);
		if (newsocket->GetFd() > -1)
		{
			Utils->Connections.push_back(newsocket);
		}
		else
		{
			ServerInstance->SNO->WriteToSnoMask('l', "CONNECT: Error connecting \002%s\002: %s.",
				x->Name.c_str(), newsocket->getError().c_str());
			ServerInstance->GlobalCulls.AddItem(newsocket);
		}
	}
	else
	{
		try
		{
			bool cached;
			ServernameResolver* snr = new ServernameResolver(Utils, x->IPAddr, x, cached, start_type, y);
			ServerInstance->AddResolver(snr, cached);
		}
		catch (ModuleException& e)
		{
			ServerInstance->SNO->WriteToSnoMask('l', "CONNECT: Error connecting \002%s\002: %s.",x->Name.c_str(), e.GetReason());
			ConnectServer(y, false);
		}
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

ModResult ModuleSpanningTree::HandleConnect(const std::vector<std::string>& parameters, User* user)
{
	for (std::vector<reference<Link> >::iterator i = Utils->LinkBlocks.begin(); i < Utils->LinkBlocks.end(); i++)
	{
		Link* x = *i;
		if (InspIRCd::Match(x->Name.c_str(),parameters[0]))
		{
			if (InspIRCd::Match(ServerInstance->Config->ServerName, x->Name))
			{
				user->SendServerNotice("*** CONNECT: Server \002%s\002 is ME, not connecting.",x->Name.c_str());
				return MOD_RES_DENY;
			}

			TreeServer* CheckDupe = Utils->FindServer(x->Name.c_str());
			if (!CheckDupe)
			{
				user->SendServerNotice("*** CONNECT: Connecting to server: \002%s\002 (%s:%d)",x->Name.c_str(),(x->HiddenFromStats ? "<hidden>" : x->IPAddr.c_str()),x->Port);
				ConnectServer(x);
				return MOD_RES_DENY;
			}
			else
			{
				user->SendServerNotice("*** CONNECT: Server \002%s\002 already exists on the network and is connected via \002%s\002",x->Name.c_str(),CheckDupe->GetParent()->GetName().c_str());
				return MOD_RES_DENY;
			}
		}
	}
	user->SendServerNotice("*** CONNECT: No server matching \002%s\002 could be found in the config file.",parameters[0].c_str());
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
		if (!IS_LOCAL(d) && IS_LOCAL(user))
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
				TreeSocketSet list;
				Utils->GetListOfServersForChannel(c,list,status,exempt_list);
				for (TreeSocketSet::iterator i = list.begin(); i != list.end(); i++)
				{
					(**i).WriteLine(":"+std::string(user->uuid)+" NOTICE "+cname+" :"+text);
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
		if (!IS_LOCAL(d) && (IS_LOCAL(user)))
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
				TreeSocketSet list;
				Utils->GetListOfServersForChannel(c,list,status,exempt_list);
				for (TreeSocketSet::iterator i = list.begin(); i != list.end(); i++)
				{
					(**i).WriteLine(":"+std::string(user->uuid)+" PRIVMSG "+cname+" :"+text);
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
	long ts = curtime * 1000 + (ServerInstance->Time_ns() / 1000000);

	for(server_hash::iterator i = Utils->sidlist.begin(); i != Utils->sidlist.end(); i++)
	{
		TreeServer* srv = i->second;
		if (!srv->Socket || !srv->Socket->getError().empty())
			// no need to check ourselves or any errored sockets
			continue;

		// warning deadline is the time the last ping was sent, plus the maximum lag
		time_t deadline = (srv->NextPing - Utils->PingFreq) + Utils->PingWarnTime;
		if (Utils->PingWarnTime && !srv->LastPingWasGood && curtime >= deadline && !srv->Warned)
		{
			// oops, we're a bit laggy today
			ServerInstance->SNO->WriteToSnoMask('l', "Server \002%s\002 has not responded to PING for %d seconds, high latency.",
				srv->GetName().c_str(), Utils->PingWarnTime);
			srv->Warned = true;
		}

		// send out a new ping if we don't have one pending, and if it's time
		if (srv->LastPingWasGood && curtime >= srv->NextPing)
		{
			// it's now time to ping
			srv->LastPingMsec = ts;
			srv->LastPingWasGood = false;
			srv->NextPing = curtime + Utils->PingFreq;
			if (srv->Parent == Utils->TreeRoot)
			{
				// this is a direct neighbor; update the socket ping information too
				srv->Socket->LastPingWasGood = false;
				srv->Socket->NextPing = srv->NextPing;
			}
			srv->Socket->WriteLine(std::string(":") + ServerInstance->Config->GetSID() + " PING " +
				ServerInstance->Config->GetSID() + " " + srv->GetID());
		}
	}

	// check for sockets that are timed out (ping has been pending for too long)
	for(size_t i = 0; i < Utils->Connections.size(); i++)
	{
		TreeSocket* sock = Utils->Connections[i];
		if (curtime > sock->NextPing && !sock->LastPingWasGood)
			sock->SendError("Ping timeout");
	}

	// start the autoconnects
	for (std::vector<reference<Autoconnect> >::iterator i = Utils->AutoconnectBlocks.begin(); i < Utils->AutoconnectBlocks.end(); ++i)
	{
		Autoconnect* x = *i;
		if (x->Enabled && curtime >= x->NextConnectTime)
		{
			x->NextConnectTime = curtime + x->Period;
			ConnectServer(x, true);
		}
	}
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
	params.push_back(":"+std::string(user->fullname));
	Utils->DoOneToMany(ServerInstance->Config->GetSID(), "UID", params);

	if (IS_OPER(user))
	{
		params.clear();
		params.push_back(user->oper->name);
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
		if (created)
		{
			irc::modestacker fmodes;
			memb->chan->ChanModes(fmodes, MODELIST_FULL);
			std::string modes = fmodes.popModeLine(FORMAT_NETWORK);
			if (modes.empty())
				params.push_back("+");
			else if (!fmodes.empty())
				params.push_back("*");
			else
				params.push_back(modes);
		}
		else
		{
			params.push_back("*");
		}
		params.push_back(memb->modes+","+std::string(memb->user->uuid));
		Utils->DoOneToMany(ServerInstance->Config->GetSID(),"FJOIN",params);
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
		SourceServer->UserCount--;
}

void ModuleSpanningTree::OnUserPostNick(User* user, const std::string &oldnick)
{
	if (IS_LOCAL(user))
	{
		parameterlist params;
		params.push_back(user->nick);

		/** IMPORTANT: We don't update the TS if the oldnick is just a case change of the newnick!
		 */
		if (irc::string(user->nick.c_str()) != irc::string(oldnick))
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

void ModuleSpanningTree::OnRemoteKill(User* source, User* dest, const std::string &reason, const std::string &operreason)
{
	if (!IS_LOCAL(source))
		return; // Only start routing if we're origin.

	ServerInstance->OperQuit.set(dest, operreason);
	parameterlist params;
	params.push_back(":"+operreason);
	Utils->DoOneToMany(dest->uuid,"OPERQUIT",params);
	params.clear();
	params.push_back(dest->uuid);
	params.push_back(":"+reason);
	Utils->DoOneToMany(source->uuid,"KILL",params);
}

void ModuleSpanningTree::ReadConfig(ConfigReadStatus&)
{
	// Re-read config stuff
	Utils->ReadConfiguration();
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

void ModuleSpanningTree::RedoConfig(Module* mod)
{
}

// note: the protocol does not allow direct umode +o except
// via NICK with 8 params. sending OPERTYPE infers +o modechange
// locally.
void ModuleSpanningTree::OnOper(User* user, const std::string &opertype)
{
	if (user->registered != REG_ALL || !IS_LOCAL(user))
		return;
	parameterlist params;
	params.push_back(opertype);
	Utils->DoOneToMany(user->uuid,"OPERTYPE",params);
}

void ModuleSpanningTree::OnAddLine(User* user, XLine *x)
{
	if (!x->IsBurstable() || loopCall)
		return;

	char data[MAXBUF];
	snprintf(data,MAXBUF,"%s %s %s %lu %lu :%s", x->type.c_str(), x->Displayable(),
	ServerInstance->Config->ServerName.c_str(), (unsigned long)x->set_time, (unsigned long)x->duration, x->reason.c_str());
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

CullResult ModuleSpanningTree::cull()
{
	Utils->cull();
	if (RefreshTimer)
		ServerInstance->Timers->DelTimer(RefreshTimer);
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
