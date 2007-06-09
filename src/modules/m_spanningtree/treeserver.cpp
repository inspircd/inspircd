/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2007 InspIRCd Development Team
 * See: http://www.inspircd.org/wiki/index.php/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

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

#include "m_spanningtree/utils.h"
#include "m_spanningtree/treeserver.h"

/* $ModDep: m_spanningtree/utils.h m_spanningtree/treeserver.h */

TreeServer::TreeServer(SpanningTreeUtilities* Util, InspIRCd* Instance) : ServerInstance(Instance), Utils(Util)
{
	Parent = NULL;
	ServerName.clear();
	ServerDesc.clear();
	VersionString.clear();
	UserCount = OperCount = 0;
	rtt = LastPing = 0;
	Hidden = false;
	VersionString = ServerInstance->GetVersionString();
}

/** We use this constructor only to create the 'root' item, Utils->TreeRoot, which
 * represents our own server. Therefore, it has no route, no parent, and
 * no socket associated with it. Its version string is our own local version.
 */
TreeServer::TreeServer(SpanningTreeUtilities* Util, InspIRCd* Instance, std::string Name, std::string Desc) : ServerInstance(Instance), ServerName(Name.c_str()), ServerDesc(Desc), Utils(Util)
{
	Parent = NULL;
	VersionString.clear();
	UserCount = ServerInstance->UserCount();
	OperCount = ServerInstance->OperCount();
	VersionString = ServerInstance->GetVersionString();
	Route = NULL;
	Socket = NULL; /* Fix by brain */
	rtt = LastPing = 0;
	Hidden = false;
	AddHashEntry();
}

/** When we create a new server, we call this constructor to initialize it.
 * This constructor initializes the server's Route and Parent, and sets up
 * its ping counters so that it will be pinged one minute from now.
 */
TreeServer::TreeServer(SpanningTreeUtilities* Util, InspIRCd* Instance, std::string Name, std::string Desc, TreeServer* Above, TreeSocket* Sock, bool Hide)
	: ServerInstance(Instance), Parent(Above), ServerName(Name.c_str()), ServerDesc(Desc), Socket(Sock), Utils(Util), Hidden(Hide)
{
	VersionString.clear();
	UserCount = OperCount = 0;
	this->SetNextPingTime(time(NULL) + 60);
	this->SetPingFlag();
	rtt = LastPing = 0;
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
}

int TreeServer::QuitUsers(const std::string &reason)
{
	const char* reason_s = reason.c_str();
	std::vector<userrec*> time_to_die;
	for (user_hash::iterator n = ServerInstance->clientlist->begin(); n != ServerInstance->clientlist->end(); n++)
	{
		if (!strcmp(n->second->server, this->ServerName.c_str()))
		{
			time_to_die.push_back(n->second);
		}
	}
	for (std::vector<userrec*>::iterator n = time_to_die.begin(); n != time_to_die.end(); n++)
	{
		userrec* a = (userrec*)*n;
		if (!IS_LOCAL(a))
		{
			if (ServerInstance->Config->HideSplits)
				userrec::QuitUser(ServerInstance, a, "*.net *.split", reason_s);
			else
				userrec::QuitUser(ServerInstance, a, reason_s);

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

int TreeServer::GetUserCount()
{
	return UserCount;
}

void TreeServer::AddUserCount()
{
	UserCount++;
}

void TreeServer::DelUserCount()
{
	UserCount--;
}

int TreeServer::GetOperCount()
{
	return OperCount;
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
	for (std::vector<TreeServer*>::iterator a = Children.begin(); a < Children.end(); a++)
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
		for (std::vector<TreeServer*>::iterator a = Children.begin(); a < Children.end(); a++)
		{
			TreeServer* s = (TreeServer*)*a;
			s->Tidy();
			Children.erase(a);
			DELETE(s);
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
}


