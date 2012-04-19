/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2012 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"
#include "commands/cmd_whois.h"
#include "commands/cmd_stats.h"
#include "socket.h"
#include "xline.h"
#include "transport.h"

#include "m_spanningtree/utils.h"
#include "m_spanningtree/treeserver.h"

/* $ModDep: m_spanningtree/utils.h m_spanningtree/treeserver.h */

/** We use this constructor only to create the 'root' item, Utils->TreeRoot, which
 * represents our own server. Therefore, it has no route, no parent, and
 * no socket associated with it. Its version string is our own local version.
 */
TreeServer::TreeServer(SpanningTreeUtilities* Util, InspIRCd* Instance, std::string Name, std::string Desc, const std::string &id)
						: ServerInstance(Instance), ServerName(Name.c_str()), ServerDesc(Desc), Utils(Util)
{
	bursting = false;
	Parent = NULL;
	VersionString.clear();
	ServerUserCount = ServerOperCount = 0;
	VersionString = ServerInstance->GetVersionString();
	Route = NULL;
	Socket = NULL; /* Fix by brain */
	StartBurst = rtt = 0;
	Warned = Hidden = false;
	AddHashEntry();
	SetID(id);
}

/** When we create a new server, we call this constructor to initialize it.
 * This constructor initializes the server's Route and Parent, and sets up
 * its ping counters so that it will be pinged one minute from now.
 */
TreeServer::TreeServer(SpanningTreeUtilities* Util, InspIRCd* Instance, std::string Name, std::string Desc, const std::string &id, TreeServer* Above, TreeSocket* Sock, bool Hide)
	: ServerInstance(Instance), Parent(Above), ServerName(Name.c_str()), ServerDesc(Desc), Socket(Sock), Utils(Util), Hidden(Hide)
{
	bursting = true;
	VersionString.clear();
	ServerUserCount = ServerOperCount = 0;
	SetNextPingTime(ServerInstance->Time() + Utils->PingFreq);
	SetPingFlag();
	Warned = false;
	rtt = 0;

	timeval t;
	gettimeofday(&t, NULL);
	long ts = (t.tv_sec * 1000) + (t.tv_usec / 1000);
	this->StartBurst = ts;
	Instance->Logs->Log("m_spanningtree",DEBUG, "Started bursting at time %lu", ts);

	/* find the 'route' for this server (e.g. the one directly connected
	 * to the local server, which we can use to reach it)
	 *
	 * In the following example, consider we have just added a TreeServer
	 * class for server G on our network, of which we are server A.
	 * To route traffic to G (marked with a *) we must send the data to
	 * B (marked with a +) so this algorithm initializes the 'Route'
	 * value to point at whichever server traffic must be routed through
	 * to get here. If we were to try this algorithm with server B,
	 * the Route pointer would point at its own object ('this').
	 *
	 *            A
	 *           / \
	 *        + B   C
	 *         / \   \
	 *        D   E   F
	 *       /         \
	 *    * G           H
	 *
	 * We only run this algorithm when a server is created, as
	 * the routes remain constant while ever the server exists, and
	 * do not need to be re-calculated.
	 */

	Route = Above;
	if (Route == Utils->TreeRoot)
	{
		Route = this;
	}
	else
	{
		while (this->Route->GetParent() != Utils->TreeRoot)
		{
			this->Route = Route->GetParent();
		}
	}

	/* Because recursive code is slow and takes a lot of resources,
	 * we store two representations of the server tree. The first
	 * is a recursive structure where each server references its
	 * children and its parent, which is used for netbursts and
	 * netsplits to dump the whole dataset to the other server,
	 * and the second is used for very fast lookups when routing
	 * messages and is instead a hash_map, where each item can
	 * be referenced by its server name. The AddHashEntry()
	 * call below automatically inserts each TreeServer class
	 * into the hash_map as it is created. There is a similar
	 * maintainance call in the destructor to tidy up deleted
	 * servers.
	 */

	this->AddHashEntry();

	SetID(id);
}

std::string& TreeServer::GetID()
{
	return sid;
}

void TreeServer::FinishBurstInternal()
{
	this->bursting = false;
	SetNextPingTime(ServerInstance->Time() + Utils->PingFreq);
	SetPingFlag();
	for(unsigned int q=0; q < ChildCount(); q++)
	{
		TreeServer* child = GetChild(q);
		child->FinishBurstInternal();
	}
}

void TreeServer::FinishBurst()
{
	FinishBurstInternal();
	ServerInstance->XLines->ApplyLines();
	timeval t;
	gettimeofday(&t, NULL);
	long ts = (t.tv_sec * 1000) + (t.tv_usec / 1000);
	unsigned long bursttime = ts - this->StartBurst;
	ServerInstance->SNO->WriteToSnoMask('l', "Received end of netburst from \2%s\2 (burst time: %lu %s)",
		ServerName.c_str(), (bursttime > 10000 ? bursttime / 1000 : bursttime), (bursttime > 10000 ? "secs" : "msecs"));
	Event rmode((char*)ServerName.c_str(),  (Module*)Utils->Creator, "new_server");
	rmode.Send(ServerInstance);
}

void TreeServer::SetID(const std::string &id)
{
	ServerInstance->Logs->Log("m_spanningtree",DEBUG, "Setting SID to " + id);
	sid = id;
	Utils->sidlist[sid] = this;
}

int TreeServer::QuitUsers(const std::string &reason)
{
	const char* reason_s = reason.c_str();
	std::vector<User*> time_to_die;
	for (user_hash::iterator n = ServerInstance->Users->clientlist->begin(); n != ServerInstance->Users->clientlist->end(); n++)
	{
		if (!strcmp(n->second->server, this->ServerName.c_str()))
		{
			time_to_die.push_back(n->second);
		}
	}
	for (std::vector<User*>::iterator n = time_to_die.begin(); n != time_to_die.end(); n++)
	{
		User* a = (User*)*n;
		if (!IS_LOCAL(a))
		{
			if (ServerInstance->Config->HideSplits)
				ServerInstance->Users->QuitUser(a, "*.net *.split", reason_s);
			else
				ServerInstance->Users->QuitUser(a, reason_s);

			if (this->Utils->quiet_bursts)
				ServerInstance->GlobalCulls.MakeSilent(a);
		}
	}
	return time_to_die.size();
}

/** This method is used to add the structure to the
 * hash_map for linear searches. It is only called
 * by the constructors.
 */
void TreeServer::AddHashEntry()
{
	server_hash::iterator iter = Utils->serverlist.find(this->ServerName.c_str());
	if (iter == Utils->serverlist.end())
		Utils->serverlist[this->ServerName.c_str()] = this;
}

/** This method removes the reference to this object
 * from the hash_map which is used for linear searches.
 * It is only called by the default destructor.
 */
void TreeServer::DelHashEntry()
{
	server_hash::iterator iter = Utils->serverlist.find(this->ServerName.c_str());
	if (iter != Utils->serverlist.end())
		Utils->serverlist.erase(iter);
}

/** These accessors etc should be pretty self-
 * explanitory.
 */
TreeServer* TreeServer::GetRoute()
{
	return Route;
}

std::string TreeServer::GetName()
{
	return ServerName.c_str();
}

std::string TreeServer::GetDesc()
{
	return ServerDesc;
}

std::string TreeServer::GetVersion()
{
	return VersionString;
}

void TreeServer::SetNextPingTime(time_t t)
{
	this->NextPing = t;
	LastPingWasGood = false;
}

time_t TreeServer::NextPingTime()
{
	return NextPing;
}

bool TreeServer::AnsweredLastPing()
{
	return LastPingWasGood;
}

void TreeServer::SetPingFlag()
{
	LastPingWasGood = true;
}

unsigned int TreeServer::GetUserCount()
{
	return ServerUserCount;
}

void TreeServer::SetUserCount(int diff)
{
	ServerUserCount += diff;
}

void TreeServer::SetOperCount(int diff)
{
	ServerOperCount += diff;
}

unsigned int TreeServer::GetOperCount()
{
	return ServerOperCount;
}

TreeSocket* TreeServer::GetSocket()
{
	return Socket;
}

TreeServer* TreeServer::GetParent()
{
	return Parent;
}

void TreeServer::SetVersion(const std::string &Version)
{
	VersionString = Version;
}

unsigned int TreeServer::ChildCount()
{
	return Children.size();
}

TreeServer* TreeServer::GetChild(unsigned int n)
{
	if (n < Children.size())
	{
		/* Make sure they  cant request
		 * an out-of-range object. After
		 * all we know what these programmer
		 * types are like *grin*.
		 */
		return Children[n];
	}
	else
	{
		return NULL;
	}
}

void TreeServer::AddChild(TreeServer* Child)
{
	Children.push_back(Child);
}

bool TreeServer::DelChild(TreeServer* Child)
{
	for (std::vector<TreeServer*>::iterator a = Children.begin(); a != Children.end(); a++)
	{
		if (*a == Child)
		{
			Children.erase(a);
			return true;
		}
	}
	return false;
}

/** Removes child nodes of this node, and of that node, etc etc.
 * This is used during netsplits to automatically tidy up the
 * server tree. It is slow, we don't use it for much else.
 */
bool TreeServer::Tidy()
{
	bool stillchildren = true;
	while (stillchildren)
	{
		stillchildren = false;
		for (std::vector<TreeServer*>::iterator a = Children.begin(); a != Children.end(); a++)
		{
			TreeServer* s = (TreeServer*)*a;
			s->Tidy();
			Children.erase(a);
			delete s;
			stillchildren = true;
			break;
		}
	}
	return true;
}

TreeServer::~TreeServer()
{
	/* We'd better tidy up after ourselves, eh? */
	this->DelHashEntry();

	server_hash::iterator iter = Utils->sidlist.find(GetID());
	if (iter != Utils->sidlist.end())
		Utils->sidlist.erase(iter);
}
