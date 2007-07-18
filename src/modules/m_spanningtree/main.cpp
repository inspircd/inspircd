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
#include "configreader.h"
#include "users.h"
#include "channels.h"
#include "modules.h"
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
	ServerInstance->UseInterface("InspSocketHook");
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
}

void ModuleSpanningTree::ShowLinks(TreeServer* Current, userrec* user, int hops)
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

void ModuleSpanningTree::HandleLinks(const char** parameters, int pcnt, userrec* user)
{
	ShowLinks(Utils->TreeRoot,user,0);
	user->WriteServ("365 %s * :End of /LINKS list.",user->nick);
	return;
}

void ModuleSpanningTree::HandleLusers(const char** parameters, int pcnt, userrec* user)
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
			n_users-ServerInstance->InvisibleUserCount(),
			ServerInstance->InvisibleUserCount(),
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

const std::string ModuleSpanningTree::MapOperInfo(TreeServer* Current)
{
	time_t secs_up = ServerInstance->Time() - Current->age;
	return (" [Up: " + TimeToStr(secs_up) + " Lag: "+ConvToStr(Current->rtt)+"s]");
}

// WARNING: NOT THREAD SAFE - DONT GET ANY SMART IDEAS.
void ModuleSpanningTree::ShowMap(TreeServer* Current, userrec* user, int depth, char matrix[128][128], float &totusers, float &totservers)
{
	if (line < 128)
	{
		for (int t = 0; t < depth; t++)
		{
			matrix[line][t] = ' ';
		}
		// For Aligning, we need to work out exactly how deep this thing is, and produce
		// a 'Spacer' String to compensate.
		char spacer[40];
		memset(spacer,' ',40);
		if ((40 - Current->GetName().length() - depth) > 1) {
			spacer[40 - Current->GetName().length() - depth] = '\0';
		}
		else
		{
			spacer[5] = '\0';
		}
		float percent;
		char text[128];
		/* Neat and tidy default values, as we're dealing with a matrix not a simple string */
		memset(text, 0, 128);

		if (ServerInstance->clientlist->size() == 0) {
			// If there are no users, WHO THE HELL DID THE /MAP?!?!?!
			percent = 0;
		}
		else
		{
			percent = ((float)Current->GetUserCount() / (float)ServerInstance->clientlist->size()) * 100;
		}
		const std::string operdata = IS_OPER(user) ? MapOperInfo(Current) : "";
		snprintf(text, 126, "%s %s%5d [%5.2f%%]%s", Current->GetName().c_str(), spacer, Current->GetUserCount(), percent, operdata.c_str());
		totusers += Current->GetUserCount();
		totservers++;
		strlcpy(&matrix[line][depth],text,126);
		line++;
		for (unsigned int q = 0; q < Current->ChildCount(); q++)
		{
			if ((Current->GetChild(q)->Hidden) || ((Utils->HideULines) && (ServerInstance->ULine(Current->GetChild(q)->GetName().c_str()))))
			{
				if (*user->oper)
				{
					ShowMap(Current->GetChild(q),user,(Utils->FlatLinks && (!*user->oper)) ? depth : depth+2,matrix,totusers,totservers);
				}
			}
			else
			{
				ShowMap(Current->GetChild(q),user,(Utils->FlatLinks && (!*user->oper)) ? depth : depth+2,matrix,totusers,totservers);
			}
		}
	}
}

int ModuleSpanningTree::HandleMotd(const char** parameters, int pcnt, userrec* user)
{
	if (pcnt > 0)
	{
		if (match(ServerInstance->Config->ServerName, parameters[0]))
			return 0;

		/* Remote MOTD, the server is within the 1st parameter */
		std::deque<std::string> params;
		params.push_back(parameters[0]);
		/* Send it out remotely, generate no reply yet */
		TreeServer* s = Utils->FindServerMask(parameters[0]);
		if (s)
		{
			params[0] = s->GetName();
			Utils->DoOneToOne(user->nick, "MOTD", params, s->GetName());
		}
		else
			user->WriteServ( "402 %s %s :No such server", user->nick, parameters[0]);
		return 1;
	}
	return 0;
}

int ModuleSpanningTree::HandleAdmin(const char** parameters, int pcnt, userrec* user)
{
	if (pcnt > 0)
	{
		if (match(ServerInstance->Config->ServerName, parameters[0]))
			return 0;

		/* Remote ADMIN, the server is within the 1st parameter */
		std::deque<std::string> params;
		params.push_back(parameters[0]);
		/* Send it out remotely, generate no reply yet */
		TreeServer* s = Utils->FindServerMask(parameters[0]);
		if (s)
		{
			params[0] = s->GetName();
			Utils->DoOneToOne(user->nick, "ADMIN", params, s->GetName());
		}
		else
			user->WriteServ( "402 %s %s :No such server", user->nick, parameters[0]);
		return 1;
	}
	return 0;
}

int ModuleSpanningTree::HandleModules(const char** parameters, int pcnt, userrec* user)
{
	if (pcnt > 0)
	{
		if (match(ServerInstance->Config->ServerName, parameters[0]))
			return 0;

		std::deque<std::string> params;
		params.push_back(parameters[0]);
		TreeServer* s = Utils->FindServerMask(parameters[0]);
		if (s)
		{
			params[0] = s->GetName();
			Utils->DoOneToOne(user->nick, "MODULES", params, s->GetName());
		}
		else
			user->WriteServ( "402 %s %s :No such server", user->nick, parameters[0]);
		return 1;
	}
	return 0;
}

int ModuleSpanningTree::HandleStats(const char** parameters, int pcnt, userrec* user)
{
	if (pcnt > 1)
	{
		if (match(ServerInstance->Config->ServerName, parameters[1]))
			return 0;

		/* Remote STATS, the server is within the 2nd parameter */
		std::deque<std::string> params;
		params.push_back(parameters[0]);
		params.push_back(parameters[1]);
		/* Send it out remotely, generate no reply yet */

		TreeServer* s = Utils->FindServerMask(parameters[1]);
		if (s)
		{
			params[1] = s->GetName();
			Utils->DoOneToOne(user->nick, "STATS", params, s->GetName());
		}
		else
		{
			user->WriteServ( "402 %s %s :No such server", user->nick, parameters[1]);
		}
		return 1;
	}
	return 0;
}

// Ok, prepare to be confused.
// After much mulling over how to approach this, it struck me that
// the 'usual' way of doing a /MAP isnt the best way. Instead of
// keeping track of a ton of ascii characters, and line by line
// under recursion working out where to place them using multiplications
// and divisons, we instead render the map onto a backplane of characters
// (a character matrix), then draw the branches as a series of "L" shapes
// from the nodes. This is not only friendlier on CPU it uses less stack.
void ModuleSpanningTree::HandleMap(const char** parameters, int pcnt, userrec* user)
{
	// This array represents a virtual screen which we will
	// "scratch" draw to, as the console device of an irc
	// client does not provide for a proper terminal.
	float totusers = 0;
	float totservers = 0;
	char matrix[128][128];
	for (unsigned int t = 0; t < 128; t++)
	{
		matrix[t][0] = '\0';
	}
	line = 0;
	// The only recursive bit is called here.
	ShowMap(Utils->TreeRoot,user,0,matrix,totusers,totservers);
	// Process each line one by one. The algorithm has a limit of
	// 128 servers (which is far more than a spanning tree should have
	// anyway, so we're ok). This limit can be raised simply by making
	// the character matrix deeper, 128 rows taking 10k of memory.
	for (int l = 1; l < line; l++)
	{
		// scan across the line looking for the start of the
		// servername (the recursive part of the algorithm has placed
		// the servers at indented positions depending on what they
		// are related to)
		int first_nonspace = 0;
		while (matrix[l][first_nonspace] == ' ')
		{
			first_nonspace++;
		}
		first_nonspace--;
		// Draw the `- (corner) section: this may be overwritten by
		// another L shape passing along the same vertical pane, becoming
		// a |- (branch) section instead.
		matrix[l][first_nonspace] = '-';
		matrix[l][first_nonspace-1] = '`';
		int l2 = l - 1;
		// Draw upwards until we hit the parent server, causing possibly
		// other corners (`-) to become branches (|-)
		while ((matrix[l2][first_nonspace-1] == ' ') || (matrix[l2][first_nonspace-1] == '`'))
		{
			matrix[l2][first_nonspace-1] = '|';
			l2--;
		}
	}
	// dump the whole lot to the user. This is the easy bit, honest.
	for (int t = 0; t < line; t++)
	{
		user->WriteServ("006 %s :%s",user->nick,&matrix[t][0]);
	}
	float avg_users = totusers / totservers;
	user->WriteServ("270 %s :%.0f server%s and %.0f user%s, average %.2f users per server",user->nick,totservers,(totservers > 1 ? "s" : ""),totusers,(totusers > 1 ? "s" : ""),avg_users);
	user->WriteServ("007 %s :End of /MAP",user->nick);
	return;
}

int ModuleSpanningTree::HandleSquit(const char** parameters, int pcnt, userrec* user)
{
	TreeServer* s = Utils->FindServerMask(parameters[0]);
	if (s)
	{
		if (s == Utils->TreeRoot)
		{
			user->WriteServ("NOTICE %s :*** SQUIT: Foolish mortal, you cannot make a server SQUIT itself! (%s matches local server name)",user->nick,parameters[0]);
			return 1;
		}
		TreeSocket* sock = s->GetSocket();
		if (sock)
		{
			ServerInstance->SNO->WriteToSnoMask('l',"SQUIT: Server \002%s\002 removed from network by %s",parameters[0],user->nick);
			sock->Squit(s,std::string("Server quit by ") + user->GetFullRealHost());
			ServerInstance->SE->DelFd(sock);
			sock->Close();
		}
		else
		{
			if (IS_LOCAL(user))
				user->WriteServ("NOTICE %s :*** WARNING: Using SQUIT to split remote servers is deprecated. Please use RSQUIT instead.",user->nick);
		}
	}
	else
	{
		 user->WriteServ("NOTICE %s :*** SQUIT: The server \002%s\002 does not exist on the network.",user->nick,parameters[0]);
	}
	return 1;
}

int ModuleSpanningTree::HandleTime(const char** parameters, int pcnt, userrec* user)
{
	if ((IS_LOCAL(user)) && (pcnt))
	{
		TreeServer* found = Utils->FindServerMask(parameters[0]);
		if (found)
		{
			// we dont' override for local server
			if (found == Utils->TreeRoot)
				return 0;
			
			std::deque<std::string> params;
			params.push_back(found->GetName());
			params.push_back(user->nick);
			Utils->DoOneToOne(ServerInstance->Config->ServerName,"TIME",params,found->GetName());
		}
		else
		{
			user->WriteServ("402 %s %s :No such server",user->nick,parameters[0]);
		}
	}
	return 1;
}

int ModuleSpanningTree::HandleRemoteWhois(const char** parameters, int pcnt, userrec* user)
{
	if ((IS_LOCAL(user)) && (pcnt > 1))
	{
		userrec* remote = ServerInstance->FindNick(parameters[1]);
		if ((remote) && (remote->GetFd() < 0))
		{
			std::deque<std::string> params;
			params.push_back(parameters[1]);
			Utils->DoOneToOne(user->nick,"IDLE",params,remote->server);
			return 1;
		}
		else if (!remote)
		{
			user->WriteServ("401 %s %s :No such nick/channel",user->nick, parameters[1]);
			user->WriteServ("318 %s %s :End of /WHOIS list.",user->nick, parameters[1]);
			return 1;
		}
	}
	return 0;
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
					sock->WriteLine(std::string(":")+ServerInstance->Config->ServerName+" PING "+serv->GetName());
					serv->SetNextPingTime(curtime + 60);
					serv->LastPing = curtime;
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
			else if ((Utils->PingWarnTime) && (!serv->Warned) && (curtime >= serv->NextPingTime() - (60 - Utils->PingWarnTime)) && (!serv->AnsweredLastPing()))
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
			ServerInstance->SNO->WriteToSnoMask('l',"CONNECT: Error connecting \002%s\002: %s.",x->Name.c_str(),strerror(errno));
			delete newsocket;
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
			ServerInstance->SNO->WriteToSnoMask('l',"CONNECT: Error connecting \002%s\002: %s.",x->Name.c_str(), e.GetReason());
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

int ModuleSpanningTree::HandleVersion(const char** parameters, int pcnt, userrec* user)
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
	
int ModuleSpanningTree::HandleConnect(const char** parameters, int pcnt, userrec* user)
{
	for (std::vector<Link>::iterator x = Utils->LinkBlocks.begin(); x < Utils->LinkBlocks.end(); x++)
	{
		if (ServerInstance->MatchText(x->Name.c_str(),parameters[0]))
		{
			TreeServer* CheckDupe = Utils->FindServer(x->Name.c_str());
			if (!CheckDupe)
			{
				user->WriteServ("NOTICE %s :*** CONNECT: Connecting to server: \002%s\002 (%s:%d)",user->nick,x->Name.c_str(),(x->HiddenFromStats ? "<hidden>" : x->IPAddr.c_str()),x->Port);
				ConnectServer(&(*x));
				return 1;
			}
			else
			{
				user->WriteServ("NOTICE %s :*** CONNECT: Server \002%s\002 already exists on the network and is connected via \002%s\002",user->nick,x->Name.c_str(),CheckDupe->GetParent()->GetName().c_str());
				return 1;
			}
		}
	}
	user->WriteServ("NOTICE %s :*** CONNECT: No server matching \002%s\002 could be found in the config file.",user->nick,parameters[0]);
	return 1;
}

void ModuleSpanningTree::BroadcastTimeSync()
{
	if (Utils->MasterTime)
	{
		std::deque<std::string> params;
		params.push_back(ConvToStr(ServerInstance->Time(false)));
		params.push_back("FORCE");
		Utils->DoOneToMany(Utils->TreeRoot->GetName(), "TIMESET", params);
	}
}

int ModuleSpanningTree::OnStats(char statschar, userrec* user, string_list &results)
{
	if ((statschar == 'c') || (statschar == 'n'))
	{
		for (unsigned int i = 0; i < Utils->LinkBlocks.size(); i++)
		{
			results.push_back(std::string(ServerInstance->Config->ServerName)+" 213 "+user->nick+" "+statschar+" *@"+(Utils->LinkBlocks[i].HiddenFromStats ? "<hidden>" : Utils->LinkBlocks[i].IPAddr)+" * "+Utils->LinkBlocks[i].Name.c_str()+" "+ConvToStr(Utils->LinkBlocks[i].Port)+" "+(Utils->LinkBlocks[i].Hook.empty() ? "plaintext" : Utils->LinkBlocks[i].Hook)+" "+(Utils->LinkBlocks[i].AutoConnect ? 'a' : '-')+'s');
			if (statschar == 'c')
				results.push_back(std::string(ServerInstance->Config->ServerName)+" 244 "+user->nick+" H * * "+Utils->LinkBlocks[i].Name.c_str());
		}
		results.push_back(std::string(ServerInstance->Config->ServerName)+" 219 "+user->nick+" "+statschar+" :End of /STATS report");
		ServerInstance->SNO->WriteToSnoMask('t',"%s '%c' requested by %s (%s@%s)",(!strcmp(user->server,ServerInstance->Config->ServerName) ? "Stats" : "Remote stats"),statschar,user->nick,user->ident,user->host);
		return 1;
	}

	if (statschar == 'p')
	{
		/* show all server ports, after showing client ports. -- w00t */

		for (unsigned int i = 0; i < Utils->Bindings.size(); i++)
		{
			std::string ip = Utils->Bindings[i]->IP;
			if (ip.empty())
				ip = "*";

			std::string transport("plaintext");
			if (Utils->Bindings[i]->GetHook())
				transport = InspSocketNameRequest(this, Utils->Bindings[i]->GetHook()).Send();

			results.push_back(ConvToStr(ServerInstance->Config->ServerName) + " 249 "+user->nick+" :" + ip + ":" + ConvToStr(Utils->Bindings[i]->port)+
				" (server, " + transport + ")");
		}
	}
	return 0;
}

int ModuleSpanningTree::OnPreCommand(const std::string &command, const char** parameters, int pcnt, userrec *user, bool validated, const std::string &original_line)
{
	/* If the command doesnt appear to be valid, we dont want to mess with it. */
	if (!validated)
		return 0;

	if (command == "CONNECT")
	{
		return this->HandleConnect(parameters,pcnt,user);
	}
	else if (command == "STATS")
	{
		return this->HandleStats(parameters,pcnt,user);
	}
	else if (command == "MOTD")
	{
		return this->HandleMotd(parameters,pcnt,user);
	}
	else if (command == "ADMIN")
	{
		return this->HandleAdmin(parameters,pcnt,user);
	}
	else if (command == "SQUIT")
	{
		return this->HandleSquit(parameters,pcnt,user);
	}
	else if (command == "MAP")
	{
		this->HandleMap(parameters,pcnt,user);
		return 1;
	}
	else if ((command == "TIME") && (pcnt > 0))
	{
		return this->HandleTime(parameters,pcnt,user);
	}
	else if (command == "LUSERS")
	{
		this->HandleLusers(parameters,pcnt,user);
		return 1;
	}
	else if (command == "LINKS")
	{
		this->HandleLinks(parameters,pcnt,user);
		return 1;
	}
	else if (command == "WHOIS")
	{
		if (pcnt > 1)
		{
			// remote whois
			return this->HandleRemoteWhois(parameters,pcnt,user);
		}
	}
	else if ((command == "VERSION") && (pcnt > 0))
	{
		this->HandleVersion(parameters,pcnt,user);
		return 1;
	}
	else if ((command == "MODULES") && (pcnt > 0))
	{
		return this->HandleModules(parameters,pcnt,user);
	}
	return 0;
}

void ModuleSpanningTree::OnPostCommand(const std::string &command, const char** parameters, int pcnt, userrec *user, CmdResult result, const std::string &original_line)
{
	if ((result == CMD_SUCCESS) && (ServerInstance->IsValidModuleCommand(command, pcnt, user)))
	{
		// this bit of code cleverly routes all module commands
		// to all remote severs *automatically* so that modules
		// can just handle commands locally, without having
		// to have any special provision in place for remote
		// commands and linking protocols.
		std::deque<std::string> params;
		params.clear();
		for (int j = 0; j < pcnt; j++)
		{
			if (strchr(parameters[j],' '))
			{
				params.push_back(":" + std::string(parameters[j]));
			}
			else
			{
				params.push_back(std::string(parameters[j]));
			}
		}
		Utils->DoOneToMany(user->nick,command,params);
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

void ModuleSpanningTree::OnUserInvite(userrec* source,userrec* dest,chanrec* channel)
{
	if (IS_LOCAL(source))
	{
		std::deque<std::string> params;
		params.push_back(dest->nick);
		params.push_back(channel->name);
		Utils->DoOneToMany(source->nick,"INVITE",params);
	}
}

void ModuleSpanningTree::OnPostLocalTopicChange(userrec* user, chanrec* chan, const std::string &topic)
{
	std::deque<std::string> params;
	params.push_back(chan->name);
	params.push_back(":"+topic);
	Utils->DoOneToMany(user->nick,"TOPIC",params);
}

void ModuleSpanningTree::OnWallops(userrec* user, const std::string &text)
{
	if (IS_LOCAL(user))
	{
		std::deque<std::string> params;
		params.push_back(":"+text);
		Utils->DoOneToMany(user->nick,"WALLOPS",params);
	}
}

void ModuleSpanningTree::OnUserNotice(userrec* user, void* dest, int target_type, const std::string &text, char status, const CUList &exempt_list)
{
	if (target_type == TYPE_USER)
	{
		userrec* d = (userrec*)dest;
		if ((d->GetFd() < 0) && (IS_LOCAL(user)))
		{
			std::deque<std::string> params;
			params.clear();
			params.push_back(d->nick);
			params.push_back(":"+text);
			Utils->DoOneToOne(user->nick,"NOTICE",params,d->server);
		}
	}
	else if (target_type == TYPE_CHANNEL)
	{
		if (IS_LOCAL(user))
		{
			chanrec *c = (chanrec*)dest;
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
						Sock->WriteLine(":"+std::string(user->nick)+" NOTICE "+cname+" :"+text);
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
			Utils->DoOneToMany(user->nick,"NOTICE",par);
		}
	}
}

void ModuleSpanningTree::OnUserMessage(userrec* user, void* dest, int target_type, const std::string &text, char status, const CUList &exempt_list)
{
	if (target_type == TYPE_USER)
	{
		// route private messages which are targetted at clients only to the server
		// which needs to receive them
		userrec* d = (userrec*)dest;
		if ((d->GetFd() < 0) && (IS_LOCAL(user)))
		{
			std::deque<std::string> params;
			params.clear();
			params.push_back(d->nick);
			params.push_back(":"+text);
			Utils->DoOneToOne(user->nick,"PRIVMSG",params,d->server);
		}
	}
	else if (target_type == TYPE_CHANNEL)
	{
		if (IS_LOCAL(user))
		{
			chanrec *c = (chanrec*)dest;
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
						Sock->WriteLine(":"+std::string(user->nick)+" PRIVMSG "+cname+" :"+text);
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
			Utils->DoOneToMany(user->nick,"PRIVMSG",par);
		}
	}
}

void ModuleSpanningTree::OnBackgroundTimer(time_t curtime)
{
	AutoConnectServers(curtime);
	DoPingChecks(curtime);
}

void ModuleSpanningTree::OnUserJoin(userrec* user, chanrec* channel, bool &silent)
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
			params.push_back(std::string(channel->GetAllPrefixChars(user))+","+std::string(user->nick));
			Utils->DoOneToMany(ServerInstance->Config->ServerName,"FJOIN",params);
			/* First user in, sync the modes for the channel */
			params.pop_back();
			params.push_back(channel->ChanModes(true));
			Utils->DoOneToMany(ServerInstance->Config->ServerName,"FMODE",params);
		}
		else
		{
			std::deque<std::string> params;
			params.push_back(channel->name);
			params.push_back(ConvToStr(channel->age));
			Utils->DoOneToMany(user->nick,"JOIN",params);
		}
	}
}

void ModuleSpanningTree::OnChangeHost(userrec* user, const std::string &newhost)
{
	// only occurs for local clients
	if (user->registered != REG_ALL)
		return;
	std::deque<std::string> params;
	params.push_back(newhost);
	Utils->DoOneToMany(user->nick,"FHOST",params);
}

void ModuleSpanningTree::OnChangeName(userrec* user, const std::string &gecos)
{
	// only occurs for local clients
	if (user->registered != REG_ALL)
		return;
	std::deque<std::string> params;
	params.push_back(gecos);
	Utils->DoOneToMany(user->nick,"FNAME",params);
}

void ModuleSpanningTree::OnUserPart(userrec* user, chanrec* channel, const std::string &partmessage, bool &silent)
{
	if (IS_LOCAL(user))
	{
		std::deque<std::string> params;
		params.push_back(channel->name);
		if (!partmessage.empty())
			params.push_back(":"+partmessage);
		Utils->DoOneToMany(user->nick,"PART",params);
	}
}

void ModuleSpanningTree::OnUserConnect(userrec* user)
{
	char agestr[MAXBUF];
	if (IS_LOCAL(user))
	{
		std::deque<std::string> params;
		snprintf(agestr,MAXBUF,"%lu",(unsigned long)user->age);
		params.push_back(agestr);
		params.push_back(user->nick);
		params.push_back(user->host);
		params.push_back(user->dhost);
		params.push_back(user->ident);
		params.push_back("+"+std::string(user->FormatModes()));
		params.push_back(user->GetIPString());
		params.push_back(":"+std::string(user->fullname));
		Utils->DoOneToMany(ServerInstance->Config->ServerName,"NICK",params);
		// User is Local, change needs to be reflected!
		TreeServer* SourceServer = Utils->FindServer(user->server);
		if (SourceServer)
		{
			SourceServer->AddUserCount();
		}
	}
}

void ModuleSpanningTree::OnUserQuit(userrec* user, const std::string &reason, const std::string &oper_message)
{
	if ((IS_LOCAL(user)) && (user->registered == REG_ALL))
	{
		std::deque<std::string> params;

		if (oper_message != reason)
		{
			params.push_back(":"+oper_message);
			Utils->DoOneToMany(user->nick,"OPERQUIT",params);
		}
		params.clear();
		params.push_back(":"+reason);
		Utils->DoOneToMany(user->nick,"QUIT",params);
	}
	// Regardless, We need to modify the user Counts..
	TreeServer* SourceServer = Utils->FindServer(user->server);
	if (SourceServer)
	{
		SourceServer->DelUserCount();
	}
}

void ModuleSpanningTree::OnUserPostNick(userrec* user, const std::string &oldnick)
{
	if (IS_LOCAL(user))
	{
		std::deque<std::string> params;
		params.push_back(user->nick);
		Utils->DoOneToMany(oldnick,"NICK",params);
	}
}

void ModuleSpanningTree::OnUserKick(userrec* source, userrec* user, chanrec* chan, const std::string &reason, bool &silent)
{
	if ((source) && (IS_LOCAL(source)))
	{
		std::deque<std::string> params;
		params.push_back(chan->name);
		params.push_back(user->nick);
		params.push_back(":"+reason);
		Utils->DoOneToMany(source->nick,"KICK",params);
	}
	else if (!source)
	{
		std::deque<std::string> params;
		params.push_back(chan->name);
		params.push_back(user->nick);
		params.push_back(":"+reason);
		Utils->DoOneToMany(ServerInstance->Config->ServerName,"KICK",params);
	}
}

void ModuleSpanningTree::OnRemoteKill(userrec* source, userrec* dest, const std::string &reason, const std::string &operreason)
{
	std::deque<std::string> params;
	params.push_back(":"+reason);
	Utils->DoOneToMany(dest->nick,"OPERQUIT",params);
	params.clear();
	params.push_back(dest->nick);
	params.push_back(":"+reason);
	dest->SetOperQuit(operreason);
	Utils->DoOneToMany(source->nick,"KILL",params);
}

void ModuleSpanningTree::OnRehash(userrec* user, const std::string &parameter)
{
	if (!parameter.empty())
	{
		std::deque<std::string> params;
		params.push_back(parameter);
		Utils->DoOneToMany(user ? user->nick : ServerInstance->Config->ServerName, "REHASH", params);
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
void ModuleSpanningTree::OnOper(userrec* user, const std::string &opertype)
{
	if (IS_LOCAL(user))
	{
		std::deque<std::string> params;
		params.push_back(opertype);
		Utils->DoOneToMany(user->nick,"OPERTYPE",params);
	}
}

void ModuleSpanningTree::OnLine(userrec* source, const std::string &host, bool adding, char linetype, long duration, const std::string &reason)
{
	if (!source)
	{
		/* Server-set lines */
		char data[MAXBUF];
		snprintf(data,MAXBUF,"%c %s %s %lu %lu :%s", linetype, host.c_str(), ServerInstance->Config->ServerName, (unsigned long)ServerInstance->Time(false),
				(unsigned long)duration, reason.c_str());
		std::deque<std::string> params;
		params.push_back(data);
		Utils->DoOneToMany(ServerInstance->Config->ServerName, "ADDLINE", params);
	}
	else
	{
		if (IS_LOCAL(source))
		{
			char type[8];
			snprintf(type,8,"%cLINE",linetype);
			std::string stype = type;
			if (adding)
			{
				char sduration[MAXBUF];
				snprintf(sduration,MAXBUF,"%ld",duration);
				std::deque<std::string> params;
				params.push_back(host);
				params.push_back(sduration);
				params.push_back(":"+reason);
				Utils->DoOneToMany(source->nick,stype,params);
			}
			else
			{
				std::deque<std::string> params;
				params.push_back(host);
				Utils->DoOneToMany(source->nick,stype,params);
			}
		}
	}
}

void ModuleSpanningTree::OnAddGLine(long duration, userrec* source, const std::string &reason, const std::string &hostmask)
{
	OnLine(source,hostmask,true,'G',duration,reason);
}
	
void ModuleSpanningTree::OnAddZLine(long duration, userrec* source, const std::string &reason, const std::string &ipmask)
{
	OnLine(source,ipmask,true,'Z',duration,reason);
}

void ModuleSpanningTree::OnAddQLine(long duration, userrec* source, const std::string &reason, const std::string &nickmask)
{
	OnLine(source,nickmask,true,'Q',duration,reason);
}

void ModuleSpanningTree::OnAddELine(long duration, userrec* source, const std::string &reason, const std::string &hostmask)
{
	OnLine(source,hostmask,true,'E',duration,reason);
}

void ModuleSpanningTree::OnDelGLine(userrec* source, const std::string &hostmask)
{
	OnLine(source,hostmask,false,'G',0,"");
}

void ModuleSpanningTree::OnDelZLine(userrec* source, const std::string &ipmask)
{
	OnLine(source,ipmask,false,'Z',0,"");
}

void ModuleSpanningTree::OnDelQLine(userrec* source, const std::string &nickmask)
{
	OnLine(source,nickmask,false,'Q',0,"");
}

void ModuleSpanningTree::OnDelELine(userrec* source, const std::string &hostmask)
{
	OnLine(source,hostmask,false,'E',0,"");
}

void ModuleSpanningTree::OnMode(userrec* user, void* dest, int target_type, const std::string &text)
{
	if ((IS_LOCAL(user)) && (user->registered == REG_ALL))
	{
		std::deque<std::string> params;
		std::string command;

		if (target_type == TYPE_USER)
		{
			userrec* u = (userrec*)dest;
			params.push_back(u->nick);
			params.push_back(text);
			command = "MODE";
		}
		else
		{
			chanrec* c = (chanrec*)dest;
			params.push_back(c->name);
			params.push_back(ConvToStr(c->age));
			params.push_back(text);
			command = "FMODE";
		}
		Utils->DoOneToMany(user->nick, command, params);
	}
}

void ModuleSpanningTree::OnSetAway(userrec* user)
{
	if (IS_LOCAL(user))
	{
		std::deque<std::string> params;
		params.push_back(":"+std::string(user->awaymsg));
		Utils->DoOneToMany(user->nick,"AWAY",params);
	}
}

void ModuleSpanningTree::OnCancelAway(userrec* user)
{
	if (IS_LOCAL(user))
	{
		std::deque<std::string> params;
		params.clear();
		Utils->DoOneToMany(user->nick,"AWAY",params);
	}
}

void ModuleSpanningTree::ProtoSendMode(void* opaque, int target_type, void* target, const std::string &modeline)
{
	TreeSocket* s = (TreeSocket*)opaque;
	if (target)
	{
		if (target_type == TYPE_USER)
		{
			userrec* u = (userrec*)target;
			s->WriteLine(std::string(":")+ServerInstance->Config->ServerName+" FMODE "+u->nick+" "+ConvToStr(u->age)+" "+modeline);
		}
		else
		{
			chanrec* c = (chanrec*)target;
			s->WriteLine(std::string(":")+ServerInstance->Config->ServerName+" FMODE "+c->name+" "+ConvToStr(c->age)+" "+modeline);
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
			userrec* u = (userrec*)target;
			s->WriteLine(std::string(":")+ServerInstance->Config->ServerName+" METADATA "+u->nick+" "+extname+" :"+extdata);
		}
		else if (target_type == TYPE_CHANNEL)
		{
			chanrec* c = (chanrec*)target;
			s->WriteLine(std::string(":")+ServerInstance->Config->ServerName+" METADATA "+c->name+" "+extname+" :"+extdata);
		}
	}
	if (target_type == TYPE_OTHER)
	{
		s->WriteLine(std::string(":")+ServerInstance->Config->ServerName+" METADATA * "+extname+" :"+extdata);
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
		Utils->DoOneToMany(ServerInstance->Config->ServerName,"METADATA",*params);
	}
	else if (event->GetEventID() == "send_topic")
	{
		if (params->size() < 2)
			return;
		(*params)[1] = ":" + (*params)[1];
		params->insert(params->begin() + 1,ServerInstance->Config->ServerName);
		params->insert(params->begin() + 1,ConvToStr(ServerInstance->Time(true)));
		Utils->DoOneToMany(ServerInstance->Config->ServerName,"FTOPIC",*params);
	}
	else if (event->GetEventID() == "send_mode")
	{
		if (params->size() < 2)
			return;
		// Insert the TS value of the object, either userrec or chanrec
		time_t ourTS = 0;
		userrec* a = ServerInstance->FindNick((*params)[0]);
		if (a)
		{
			ourTS = a->age;
			Utils->DoOneToMany(ServerInstance->Config->ServerName,"MODE",*params);
			return;
		}
		else
		{
			chanrec* a = ServerInstance->FindChan((*params)[0]);
			if (a)
			{
				ourTS = a->age;
				params->insert(params->begin() + 1,ConvToStr(ourTS));
				Utils->DoOneToMany(ServerInstance->Config->ServerName,"FMODE",*params);
			}
		}
	}
	else if (event->GetEventID() == "send_mode_explicit")
	{
		if (params->size() < 2)
			return;
		Utils->DoOneToMany(ServerInstance->Config->ServerName,"MODE",*params);
	}
	else if (event->GetEventID() == "send_opers")
	{
		if (params->size() < 1)
			return;
		(*params)[0] = ":" + (*params)[0];
		Utils->DoOneToMany(ServerInstance->Config->ServerName,"OPERNOTICE",*params);
	}
	else if (event->GetEventID() == "send_modeset")
	{
		if (params->size() < 2)
			return;
		(*params)[1] = ":" + (*params)[1];
		Utils->DoOneToMany(ServerInstance->Config->ServerName,"MODENOTICE",*params);
	}
	else if (event->GetEventID() == "send_snoset")
	{
		if (params->size() < 2)
			return;
		(*params)[1] = ":" + (*params)[1];
		Utils->DoOneToMany(ServerInstance->Config->ServerName,"SNONOTICE",*params);
	}
	else if (event->GetEventID() == "send_push")
	{
		if (params->size() < 2)
			return;
			
		userrec *a = ServerInstance->FindNick((*params)[0]);
			
		if (!a)
			return;
			
		(*params)[1] = ":" + (*params)[1];
		Utils->DoOneToOne(ServerInstance->Config->ServerName, "PUSH", *params, a->server);
	}
}

ModuleSpanningTree::~ModuleSpanningTree()
{
	/* This will also free the listeners */
	delete Utils;
	if (SyncTimer)
		ServerInstance->Timers->DelTimer(SyncTimer);

	ServerInstance->Timers->DelTimer(RefreshTimer);

	ServerInstance->DoneWithInterface("InspSocketHook");
}

Version ModuleSpanningTree::GetVersion()
{
	return Version(1,1,0,2,VF_VENDOR,API_VERSION);
}

void ModuleSpanningTree::Implements(char* List)
{
	List[I_OnPreCommand] = List[I_OnGetServerDescription] = List[I_OnUserInvite] = List[I_OnPostLocalTopicChange] = 1;
	List[I_OnWallops] = List[I_OnUserNotice] = List[I_OnUserMessage] = List[I_OnBackgroundTimer] = 1;
	List[I_OnUserJoin] = List[I_OnChangeHost] = List[I_OnChangeName] = List[I_OnUserPart] = List[I_OnUserConnect] = 1;
	List[I_OnUserQuit] = List[I_OnUserPostNick] = List[I_OnUserKick] = List[I_OnRemoteKill] = List[I_OnRehash] = 1;
	List[I_OnOper] = List[I_OnAddGLine] = List[I_OnAddZLine] = List[I_OnAddQLine] = List[I_OnAddELine] = 1;
	List[I_OnDelGLine] = List[I_OnDelZLine] = List[I_OnDelQLine] = List[I_OnDelELine] = List[I_ProtoSendMode] = List[I_OnMode] = 1;
	List[I_OnStats] = List[I_ProtoSendMetaData] = List[I_OnEvent] = List[I_OnSetAway] = List[I_OnCancelAway] = List[I_OnPostCommand] = 1;
}

/* It is IMPORTANT that m_spanningtree is the last module in the chain
 * so that any activity it sees is FINAL, e.g. we arent going to send out
 * a NICK message before m_cloaking has finished putting the +x on the user,
 * etc etc.
 * Therefore, we return PRIORITY_LAST to make sure we end up at the END of
 * the module call queue.
 */
Priority ModuleSpanningTree::Prioritize()
{
	return PRIORITY_LAST;
}

MODULE_INIT(ModuleSpanningTree)

