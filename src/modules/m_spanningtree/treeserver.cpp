/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2010 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"
#include "socket.h"
#include "xline.h"
#include "main.h"
#include "../spanningtree.h"

#include "utils.h"
#include "treeserver.h"

/** We use this constructor only to create the 'root' item, Utils->TreeRoot, which
 * represents our own server. Therefore, it has no route, no parent, and
 * no socket associated with it. Its version string is our own local version.
 */
TreeServer::TreeServer(SpanningTreeUtilities* Util)
	: Parent(NULL), Socket(NULL), ServerUser(ServerInstance->FakeClient), Utils(Util),
	ServerDesc(ServerInstance->Config->ServerDesc), VersionString(ServerInstance->GetVersionString()),
	age(ServerInstance->Time()), NextPing(0), LastPingMsec(0), rtt(0),
	StartBurst(age), LastPingWasGood(true), Warned(false), bursting(false), Hidden(false)
{
	AddHashEntry();
}

/** When we create a new server, we call this constructor to initialize it.
 * This constructor initializes the server's Route and Parent, and sets up
 * its ping counters so that it will be pinged one minute from now.
 */
TreeServer::TreeServer(SpanningTreeUtilities* Util, const std::string& Name, const std::string& Desc, const std::string &id,
		TreeServer* Above, TreeSocket* Sock, bool Hide)
	: Parent(Above), Socket(Sock), ServerUser(new FakeUser(id, Name)), Utils(Util), ServerDesc(Desc),
	UserCount(0), age(ServerInstance->Time()), NextPing(age + Util->PingFreq), LastPingMsec(0), rtt(0),
	StartBurst(age * 1000 + (ServerInstance->Time_ns() / 1000000)),
	LastPingWasGood(true), Warned(false), bursting(true), Hidden(Hide)
{
	ServerInstance->Logs->Log("m_spanningtree",DEBUG, "Started bursting at time %lu", StartBurst);
	AddHashEntry();
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
	long ts = ServerInstance->Time() * 1000 + (ServerInstance->Time_ns() / 1000000);
	unsigned long bursttime = ts - this->StartBurst;
	ServerInstance->SNO->WriteToSnoMask(Parent == Utils->TreeRoot ? 'l' : 'L', "Received end of netburst from \2%s\2 (burst time: %lu %s)",
		GetName().c_str(), (bursttime > 10000 ? bursttime / 1000 : bursttime), (bursttime > 10000 ? "secs" : "msecs"));
	AddServerEvent(Utils->Creator, GetName());
}

int TreeServer::QuitUsers(const std::string &reason)
{
	const char* reason_s = reason.c_str();
	std::vector<User*> time_to_die;
	for (user_hash::iterator n = ServerInstance->Users->clientlist->begin(); n != ServerInstance->Users->clientlist->end(); n++)
	{
		if (n->second->server == GetName())
		{
			time_to_die.push_back(n->second);
		}
	}
	for (std::vector<User*>::iterator n = time_to_die.begin(); n != time_to_die.end(); n++)
	{
		User* a = (User*)*n;
		if (!IS_LOCAL(a))
		{
			if (this->Utils->quiet_bursts)
				a->quietquit = true;

			if (ServerInstance->Config->HideSplits)
				ServerInstance->Users->QuitUser(a, "*.net *.split", reason_s);
			else
				ServerInstance->Users->QuitUser(a, reason_s);
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
	ServerInstance->Logs->Log("m_spanningtree",DEBUG, "Setting SID for " + ServerUser->server + " to " + ServerUser->uuid);
	Utils->serverlist[ServerUser->server] = this;
	Utils->sidlist[ServerUser->uuid] = this;
}

/** This method removes the reference to this object
 * from the hash_map which is used for linear searches.
 * It is only called by the default destructor.
 */
void TreeServer::DelHashEntry()
{
	server_hash::iterator iter = Utils->serverlist.find(GetName());
	if (iter != Utils->serverlist.end())
		Utils->serverlist.erase(iter);
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
	while (1)
	{
		std::vector<TreeServer*>::iterator a = Children.begin();
		if (a == Children.end())
			return true;
		TreeServer* s = *a;
		s->Tidy();
		s->cull();
		Children.erase(a);
		delete s;
	}
}

CullResult TreeServer::cull()
{
	if (ServerUser != ServerInstance->FakeClient)
		ServerUser->cull();
	return classbase::cull();
}

TreeServer::~TreeServer()
{
	/* We'd better tidy up after ourselves, eh? */
	this->DelHashEntry();

	server_hash::iterator iter = Utils->sidlist.find(GetID());
	if (iter != Utils->sidlist.end())
		Utils->sidlist.erase(iter);
	if (ServerUser != ServerInstance->FakeClient)
		delete ServerUser;
}
