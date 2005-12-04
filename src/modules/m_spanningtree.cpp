/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  Inspire is copyright (C) 2002-2005 ChatSpike-Dev.
 *                       E-mail:
 *                <brain@chatspike.net>
 *           	  <Craig@chatspike.net>
 *     
 * Written by Craig Edwards, Craig McLure, and others.
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

using namespace std;

#include <stdio.h>
#include <vector>
#include <deque>
#include "globals.h"
#include "inspircd_config.h"
#ifdef GCC3
#include <ext/hash_map>
#else
#include <hash_map>
#endif
#include "users.h"
#include "channels.h"
#include "modules.h"
#include "socket.h"
#include "helperfuncs.h"
#include "inspircd.h"
#include "inspstring.h"
#include "hashcomp.h"
#include "message.h"

#ifdef GCC3
#define nspace __gnu_cxx
#else
#define nspace std
#endif

class ModuleSpanningTree;
static ModuleSpanningTree* TreeProtocolModule;

extern std::vector<Module*> modules;
extern std::vector<ircd_module*> factory;
extern int MODCOUNT;

enum ServerState { LISTENER, CONNECTING, WAIT_AUTH_1, WAIT_AUTH_2, CONNECTED };

typedef nspace::hash_map<std::string, userrec*, nspace::hash<string>, irc::StrHashComp> user_hash;
typedef nspace::hash_map<std::string, chanrec*, nspace::hash<string>, irc::StrHashComp> chan_hash;

extern user_hash clientlist;
extern chan_hash chanlist;

class TreeServer;
class TreeSocket;

bool DoOneToOne(std::string prefix, std::string command, std::deque<std::string> params, std::string target);
bool DoOneToAllButSender(std::string prefix, std::string command, std::deque<std::string> params, std::string omit);
bool DoOneToMany(std::string prefix, std::string command, std::deque<std::string> params);
bool DoOneToAllButSenderRaw(std::string data,std::string omit, std::string prefix,std::string command,std::deque<std::string> params);
void ReadConfiguration(bool rebind);

class TreeServer
{
	TreeServer* Parent;
	std::vector<TreeServer*> Children;
	std::string ServerName;
	std::string ServerDesc;
	std::string VersionString;
	int UserCount;
	int OperCount;
	TreeSocket* Socket;	// for directly connected servers this points at the socket object
	
 public:

	TreeServer()
	{
		Parent = NULL;
		ServerName = "";
		ServerDesc = "";
		VersionString = "";
		UserCount = OperCount = 0;
	}

	TreeServer(std::string Name, std::string Desc) : ServerName(Name), ServerDesc(Desc)
	{
		Parent = NULL;
		VersionString = "";
		UserCount = OperCount = 0;
	}

	TreeServer(std::string Name, std::string Desc, TreeServer* Above, TreeSocket* Sock) : Parent(Above), ServerName(Name), ServerDesc(Desc), Socket(Sock)
	{
		VersionString = "";
		UserCount = OperCount = 0;
	}

	std::string GetName()
	{
		return this->ServerName;
	}

	std::string GetDesc()
	{
		return this->ServerDesc;
	}

	std::string GetVersion()
	{
		return this->VersionString;
	}

	int GetUserCount()
	{
		return this->UserCount;
	}

	int GetOperCount()
	{
		return this->OperCount;
	}

	TreeSocket* GetSocket()
	{
		return this->Socket;
	}

	TreeServer* GetParent()
	{
		return this->Parent;
	}

	unsigned int ChildCount()
	{
		return Children.size();
	}

	TreeServer* GetChild(unsigned int n)
	{
		if (n < Children.size())
		{
			return Children[n];
		}
		else
		{
			return NULL;
		}
	}

	void AddChild(TreeServer* Child)
	{
		Children.push_back(Child);
	}

	bool DelChild(TreeServer* Child)
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

	// removes child nodes of this node, and of that node, etc etc
	bool Tidy()
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
				delete s;
				stillchildren = true;
				break;
			}
		}
		return true;
	}
};

class Link
{
 public:
	 std::string Name;
	 std::string IPAddr;
	 int Port;
	 std::string SendPass;
	 std::string RecvPass;
};

/* $ModDesc: Povides a spanning tree server link protocol */

Server *Srv;
ConfigReader *Conf;
TreeServer *TreeRoot;
std::vector<Link> LinkBlocks;

TreeServer* RouteEnumerate(TreeServer* Current, std::string ServerName)
{
	if (Current->GetName() == ServerName)
		return Current;
	for (unsigned int q = 0; q < Current->ChildCount(); q++)
	{
		TreeServer* found = RouteEnumerate(Current->GetChild(q),ServerName);
		if (found)
		{
			return found;
		}
	}
	return NULL;
}

// Returns the locally connected server we must route a
// message through to reach server 'ServerName'. This
// only applies to one-to-one and not one-to-many routing.
TreeServer* BestRouteTo(std::string ServerName)
{
	if (ServerName.c_str() == TreeRoot->GetName())
	{
		return NULL;
	}
	// first, find the server by recursively walking the tree
	TreeServer* Found = RouteEnumerate(TreeRoot,ServerName);
	// did we find it? If not, they did something wrong, abort.
	if (!Found)
	{
		return NULL;
	}
	else
	{
		// The server exists, follow its parent nodes until
		// the parent of the current is 'TreeRoot', we know
		// then that this is a directly-connected server.
		while ((Found) && (Found->GetParent() != TreeRoot))
		{
			Found = Found->GetParent();
		}
		return Found;
	}
}

bool LookForServer(TreeServer* Current, std::string ServerName)
{
	if (ServerName == Current->GetName())
		return true;
	for (unsigned int q = 0; q < Current->ChildCount(); q++)
	{
		if (LookForServer(Current->GetChild(q),ServerName))
			return true;
	}
	return false;
}

TreeServer* Found;

void RFindServer(TreeServer* Current, std::string ServerName)
{
	if ((ServerName == Current->GetName()) && (!Found))
	{
		Found = Current;
		return;
	}
	if (!Found)
	{
		for (unsigned int q = 0; q < Current->ChildCount(); q++)
		{
			if (!Found)
				RFindServer(Current->GetChild(q),ServerName);
		}
	}
	return;
}

TreeServer* FindServer(std::string ServerName)
{
	Found = NULL;
	RFindServer(TreeRoot,ServerName);
	return Found;
}

bool IsServer(std::string ServerName)
{
	return LookForServer(TreeRoot,ServerName);
}

class TreeSocket : public InspSocket
{
	std::string myhost;
	std::string in_buffer;
	ServerState LinkState;
	std::string InboundServerName;
	std::string InboundDescription;
	int num_lost_users;
	int num_lost_servers;
	
 public:

	TreeSocket(std::string host, int port, bool listening, unsigned long maxtime)
		: InspSocket(host, port, listening, maxtime)
	{
		myhost = host;
		this->LinkState = LISTENER;
	}

	TreeSocket(std::string host, int port, bool listening, unsigned long maxtime, std::string ServerName)
		: InspSocket(host, port, listening, maxtime)
	{
		myhost = ServerName;
		this->LinkState = CONNECTING;
	}

	TreeSocket(int newfd, char* ip)
		: InspSocket(newfd, ip)
	{
		this->LinkState = WAIT_AUTH_1;
	}
	
        virtual bool OnConnected()
	{
		if (this->LinkState == CONNECTING)
		{
			Srv->SendOpers("*** Connection to "+myhost+"["+this->GetIP()+"] established.");
			// we should send our details here.
			// if the other side is satisfied, they send theirs.
			// we do not need to change state here.
			for (std::vector<Link>::iterator x = LinkBlocks.begin(); x < LinkBlocks.end(); x++)
			{
				if (x->Name == this->myhost)
				{
					// found who we're supposed to be connecting to, send the neccessary gubbins.
					this->WriteLine("SERVER "+Srv->GetServerName()+" "+x->SendPass+" 0 :"+Srv->GetServerDescription());
					return true;
				}
			}
		}
		return true;
	}
	
        virtual void OnError(InspSocketError e)
	{
	}

        virtual int OnDisconnect()
	{
		return true;
	}

	// recursively send the server tree with distances as hops
	void SendServers(TreeServer* Current, TreeServer* s, int hops)
	{
		char command[1024];
		for (unsigned int q = 0; q < Current->ChildCount(); q++)
		{
			TreeServer* recursive_server = Current->GetChild(q);
			if (recursive_server != s)
			{
				// :source.server SERVER server.name hops :Description
				snprintf(command,1024,":%s SERVER %s * %d :%s",Current->GetName().c_str(),recursive_server->GetName().c_str(),hops,recursive_server->GetDesc().c_str());
				this->WriteLine(command);
				// down to next level
				this->SendServers(recursive_server, s, hops+1);
			}
		}
	}

	void SquitServer(TreeServer* Current)
	{
		// recursively squit the servers attached to 'Current'
		for (unsigned int q = 0; q < Current->ChildCount(); q++)
		{
			TreeServer* recursive_server = Current->GetChild(q);
			this->SquitServer(recursive_server);
		}
		// Now we've whacked the kids, whack self
		num_lost_servers++;
		bool quittingpeople = true;
		while (quittingpeople)
		{
			quittingpeople = false;
			for (user_hash::iterator u = clientlist.begin(); u != clientlist.end(); u++)
			{
				if (!strcasecmp(u->second->server,Current->GetName().c_str()))
				{
					Srv->QuitUser(u->second,Current->GetName()+" "+std::string(Srv->GetServerName()));
					num_lost_users++;
					quittingpeople = true;
					break;
				}
			}
		}
	}

	void Squit(TreeServer* Current,std::string reason)
	{
		if (Current)
		{
			std::deque<std::string> params;
			params.push_back(Current->GetName());
			params.push_back(":"+reason);
			DoOneToAllButSender(Current->GetParent()->GetName(),"SQUIT",params,Current->GetName());
			if (Current->GetParent() == TreeRoot)
			{
				Srv->SendOpers("Server \002"+Current->GetName()+"\002 split: "+reason);
			}
			else
			{
				Srv->SendOpers("Server \002"+Current->GetName()+"\002 split from server \002"+Current->GetParent()->GetName()+"\002 with reason: "+reason);
			}
			num_lost_servers = 0;
			num_lost_users = 0;
			SquitServer(Current);
			Current->Tidy();
			Current->GetParent()->DelChild(Current);
			delete Current;
			WriteOpers("Netsplit complete, lost \002%d\002 users on \002%d\002 servers.", num_lost_users, num_lost_servers);
		}
		else
		{
			log(DEFAULT,"Squit from unknown server");
		}
	}

	bool ForceMode(std::string source, std::deque<std::string> params)
	{
		userrec* who = new userrec;
		who->fd = FD_MAGIC_NUMBER;
		if (params.size() < 2)
			return true;
		char* modelist[255];
		for (unsigned int q = 0; q < params.size(); q++)
		{
			modelist[q] = (char*)params[q].c_str();
		}
		Srv->SendMode(modelist,params.size(),who);
		DoOneToAllButSender(source,"FMODE",params,source);
		delete who;
		return true;
	}

	bool ForceTopic(std::string source, std::deque<std::string> params)
	{
		// FTOPIC %s %lu %s :%s
		if (params.size() != 4)
			return true;
		std::string channel = params[0];
		time_t ts = atoi(params[1].c_str());
		std::string setby = params[2];
		std::string topic = params[3];

		chanrec* c = Srv->FindChannel(channel);
		if (c)
		{
			if ((ts >= c->topicset) || (!*c->topic))
			{
				strlcpy(c->topic,topic.c_str(),MAXTOPIC);
				strlcpy(c->setby,setby.c_str(),NICKMAX);
				c->topicset = ts;
				WriteChannelWithServ((char*)source.c_str(), c, "TOPIC %s :%s", c->name, c->topic);
			}
			
		}
		
		// all done, send it on its way
		params[3] = ":" + params[3];
		DoOneToAllButSender(source,"FTOPIC",params,source);

		return true;
	}

	bool ForceJoin(std::string source, std::deque<std::string> params)
	{
		if (params.size() < 3)
			return true;

		char first[MAXBUF];
		char modestring[MAXBUF];
		char* mode_users[127];
		mode_users[0] = first;
		mode_users[1] = modestring;
		strcpy(mode_users[1],"+");
		unsigned int modectr = 2;
		
		userrec* who = NULL;
		std::string channel = params[0];
		time_t TS = atoi(params[1].c_str());
		char* key = "";
		
		chanrec* chan = Srv->FindChannel(channel);
		if (chan)
		{
			key = chan->key;
		}
		strlcpy(mode_users[0],channel.c_str(),MAXBUF);

		// default is a high value, which if we dont have this
		// channel will let the other side apply their modes.
		time_t ourTS = time(NULL)+20;
		chanrec* us = Srv->FindChannel(channel);
		if (us)
		{
			ourTS = us->age;
		}

		log(DEBUG,"FJOIN detected, our TS=%lu, their TS=%lu",ourTS,TS);

		// do this first, so our mode reversals are correctly received by other servers
		// if there is a TS collision.
		DoOneToAllButSender(source,"FJOIN",params,source);
		
		for (unsigned int usernum = 2; usernum < params.size(); usernum++)
		{
			// process one channel at a time, applying modes.
			char* usr = (char*)params[usernum].c_str();
			char permissions = *usr;
			switch (permissions)
			{
				case '@':
					usr++;
					mode_users[modectr++] = usr;
					strlcat(modestring,"o",MAXBUF);
				break;
				case '%':
					usr++;
					mode_users[modectr++] = usr;
					strlcat(modestring,"h",MAXBUF);
				break;
				case '+':
					usr++;
					mode_users[modectr++] = usr;
					strlcat(modestring,"v",MAXBUF);
				break;
			}
			who = Srv->FindNick(usr);
			if (who)
			{
				Srv->JoinUserToChannel(who,channel,key);
				if (modectr >= (MAXMODES-1))
				{
					// theres a mode for this user. push them onto the mode queue, and flush it
					// if there are more than MAXMODES to go.
					if (ourTS >= TS)
					{
						log(DEBUG,"Our our channel newer than theirs, accepting their modes");
						Srv->SendMode(mode_users,modectr,who);
					}
					else
					{
						log(DEBUG,"Their channel newer than ours, bouncing their modes");
						// bouncy bouncy!
						std::deque<std::string> params;
						// modes are now being UNSET...
						*mode_users[1] = '-';
						for (unsigned int x = 0; x < modectr; x++)
						{
							params.push_back(mode_users[x]);
						}
						// tell everyone to bounce the modes. bad modes, bad!
						DoOneToMany(Srv->GetServerName(),"FMODE",params);
					}
					strcpy(mode_users[1],"+");
					modectr = 2;
				}
			}
		}
		// there werent enough modes built up to flush it during FJOIN,
		// or, there are a number left over. flush them out.
		if ((modectr > 2) && (who))
		{
			if (ourTS >= TS)
			{
				log(DEBUG,"Our our channel newer than theirs, accepting their modes");
				Srv->SendMode(mode_users,modectr,who);
			}
			else
			{
				log(DEBUG,"Their channel newer than ours, bouncing their modes");
				std::deque<std::string> params;
				*mode_users[1] = '-';
				for (unsigned int x = 0; x < modectr; x++)
				{
					params.push_back(mode_users[x]);
				}
				DoOneToMany(Srv->GetServerName(),"FMODE",params);
			}
		}
		return true;
	}

	bool IntroduceClient(std::string source, std::deque<std::string> params)
	{
		if (params.size() < 8)
			return true;
		// NICK age nick host dhost ident +modes ip :gecos
		//       0   1    2    3      4     5    6   7
		std::string nick = params[1];
		std::string host = params[2];
		std::string dhost = params[3];
		std::string ident = params[4];
		time_t age = atoi(params[0].c_str());
		std::string modes = params[5];
		while (*(modes.c_str()) == '+')
		{
			char* m = (char*)modes.c_str();
			m++;
			modes = m;
		}
		std::string ip = params[6];
		std::string gecos = params[7];
		char* tempnick = (char*)nick.c_str();
		log(DEBUG,"Introduce client %s!%s@%s",tempnick,ident.c_str(),host.c_str());
		
		user_hash::iterator iter;
		iter = clientlist.find(tempnick);
		if (iter != clientlist.end())
		{
			// nick collision
			log(DEBUG,"Nick collision on %s!%s@%s: %lu %lu",tempnick,ident.c_str(),host.c_str(),(unsigned long)age,(unsigned long)iter->second->age);
			this->WriteLine(":"+Srv->GetServerName()+" KILL "+tempnick+" :Nickname collision");
			return true;
		}

		clientlist[tempnick] = new userrec();
		clientlist[tempnick]->fd = FD_MAGIC_NUMBER;
		strlcpy(clientlist[tempnick]->nick, tempnick,NICKMAX);
		strlcpy(clientlist[tempnick]->host, host.c_str(),160);
		strlcpy(clientlist[tempnick]->dhost, dhost.c_str(),160);
		clientlist[tempnick]->server = (char*)FindServerNamePtr(source.c_str());
		strlcpy(clientlist[tempnick]->ident, ident.c_str(),IDENTMAX);
		strlcpy(clientlist[tempnick]->fullname, gecos.c_str(),MAXGECOS);
		clientlist[tempnick]->registered = 7;
		clientlist[tempnick]->signon = age;
		strlcpy(clientlist[tempnick]->modes, modes.c_str(),53);
		strlcpy(clientlist[tempnick]->ip,ip.c_str(),16);
		for (int i = 0; i < MAXCHANS; i++)
		{
			clientlist[tempnick]->chans[i].channel = NULL;
			clientlist[tempnick]->chans[i].uc_modes = 0;
		}
		params[7] = ":" + params[7];
		DoOneToAllButSender(source,"NICK",params,source);
		return true;
	}

	void SendFJoins(TreeServer* Current, chanrec* c)
	{
		char list[MAXBUF];
		snprintf(list,MAXBUF,":%s FJOIN %s %lu",Srv->GetServerName().c_str(),c->name,(unsigned long)c->age);
		std::vector<char*> *ulist = c->GetUsers();
		for (unsigned int i = 0; i < ulist->size(); i++)
		{
			char* o = (*ulist)[i];
			userrec* otheruser = (userrec*)o;
			strlcat(list," ",MAXBUF);
			strlcat(list,cmode(otheruser,c),MAXBUF);
			strlcat(list,otheruser->nick,MAXBUF);
			if (strlen(list)>(480-NICKMAX))
			{
				this->WriteLine(list);
				snprintf(list,MAXBUF,":%s FJOIN %s %lu",Srv->GetServerName().c_str(),c->name,(unsigned long)c->age);
			}
		}
		if (list[strlen(list)-1] != ':')
		{
			this->WriteLine(list);
		}
	}

	void SendChannelModes(TreeServer* Current)
	{
		char data[MAXBUF];
		for (chan_hash::iterator c = chanlist.begin(); c != chanlist.end(); c++)
		{
			SendFJoins(Current, c->second);
			snprintf(data,MAXBUF,":%s FMODE %s +%s",Srv->GetServerName().c_str(),c->second->name,chanmodes(c->second));
			this->WriteLine(data);
			if (*c->second->topic)
			{
				snprintf(data,MAXBUF,":%s FTOPIC %s %lu %s :%s",Srv->GetServerName().c_str(),c->second->name,(unsigned long)c->second->topicset,c->second->setby,c->second->topic);
				this->WriteLine(data);
			}
			for (BanList::iterator b = c->second->bans.begin(); b != c->second->bans.end(); b++)
			{
				snprintf(data,MAXBUF,":%s FMODE %s +b %s",Srv->GetServerName().c_str(),c->second->name,b->data);
				this->WriteLine(data);
			}
			FOREACH_MOD OnSyncChannel(c->second,(Module*)TreeProtocolModule,(void*)this);
		}
	}

	// send all users and their channels
	void SendUsers(TreeServer* Current)
	{
		char data[MAXBUF];
		for (user_hash::iterator u = clientlist.begin(); u != clientlist.end(); u++)
		{
			if (u->second->registered == 7)
			{
				snprintf(data,MAXBUF,":%s NICK %lu %s %s %s %s +%s %s :%s",u->second->server,(unsigned long)u->second->age,u->second->nick,u->second->host,u->second->dhost,u->second->ident,u->second->modes,u->second->ip,u->second->fullname);
				this->WriteLine(data);
				if (strchr(u->second->modes,'o'))
				{
					this->WriteLine(":"+std::string(u->second->nick)+" OPERTYPE "+std::string(u->second->oper));
				}
				//char* chl = chlist(u->second,u->second);
				//if (*chl)
				//{
				//	this->WriteLine(":"+std::string(u->second->nick)+" FJOIN "+std::string(chl));
				//}
				FOREACH_MOD OnSyncUser(u->second,(Module*)TreeProtocolModule,(void*)this);
			}
		}
	}

	void DoBurst(TreeServer* s)
	{
		Srv->SendOpers("*** Bursting to "+s->GetName()+".");
		this->WriteLine("BURST");
		// Send server tree
		this->SendServers(TreeRoot,s,1);
		// Send users and their channels
		this->SendUsers(s);
		// Send everything else (channel modes etc)
		this->SendChannelModes(s);
		this->WriteLine("ENDBURST");
	}

        virtual bool OnDataReady()
	{
		char* data = this->Read();
		if (data)
		{
			this->in_buffer += data;
			while (in_buffer.find("\n") != std::string::npos)
			{
				char* line = (char*)in_buffer.c_str();
				std::string ret = "";
			        while ((*line != '\n') && (strlen(line)))
				{
					ret = ret + *line;
					line++;
				}
				if ((*line == '\n') || (*line == '\r'))
					line++;
				in_buffer = line;
				if (!this->ProcessLine(ret))
				{
					return false;
				}
			}
		}
		return (data != NULL);
	}

	int WriteLine(std::string line)
	{
		return this->Write(line + "\r\n");
	}

	bool Error(std::deque<std::string> params)
	{
		if (params.size() < 1)
			return false;
		std::string Errmsg = params[0];
		std::string SName = myhost;
		if (InboundServerName != "")
		{
			SName = InboundServerName;
		}
		Srv->SendOpers("*** ERROR from "+SName+": "+Errmsg);
		// we will return false to cause the socket to close.
		return false;
	}

	bool OperType(std::string prefix, std::deque<std::string> params)
	{
		if (params.size() != 1)
			return true;
		std::string opertype = params[0];
		userrec* u = Srv->FindNick(prefix);
		if (u)
		{
			strlcpy(u->oper,opertype.c_str(),NICKMAX);
			if (!strchr(u->modes,'o'))
			{
				strcat(u->modes,"o");
			}
			DoOneToAllButSender(u->server,"OPERTYPE",params,u->server);
		}
		return true;
	}

	bool RemoteRehash(std::string prefix, std::deque<std::string> params)
	{
		if (params.size() < 1)
			return true;
		std::string servermask = params[0];
		if (Srv->MatchText(Srv->GetServerName(),servermask))
		{
			Srv->SendOpers("*** Remote rehash initiated from server \002"+prefix+"\002.");
			Srv->RehashServer();
			ReadConfiguration(false);
		}
		DoOneToAllButSender(prefix,"REHASH",params,prefix);
		return true;
	}

	bool RemoteKill(std::string prefix, std::deque<std::string> params)
	{
		if (params.size() != 2)
			return true;
		std::string nick = params[0];
		std::string reason = params[1];
		userrec* u = Srv->FindNick(prefix);
		userrec* who = Srv->FindNick(nick);
		if (who)
		{
			std::string sourceserv = prefix;
			if (u)
			{
				sourceserv = u->server;
			}
			params[1] = ":" + params[1];
			DoOneToAllButSender(prefix,"KILL",params,sourceserv);
			Srv->QuitUser(who,reason);
		}
		return true;
	}

	bool RemoteServer(std::string prefix, std::deque<std::string> params)
	{
		if (params.size() < 4)
			return false;
		std::string servername = params[0];
		std::string password = params[1];
		// hopcount is not used for a remote server, we calculate this ourselves
		std::string description = params[3];
		TreeServer* ParentOfThis = FindServer(prefix);
		if (!ParentOfThis)
		{
			this->WriteLine("ERROR :Protocol error - Introduced remote server from unknown server "+prefix);
			return false;
		}
		TreeServer* CheckDupe = FindServer(servername);
		if (CheckDupe)
		{
			this->WriteLine("ERROR :Server "+servername+" already exists on server "+CheckDupe->GetParent()->GetName()+"!");
			return false;
		}
		TreeServer* Node = new TreeServer(servername,description,ParentOfThis,NULL);
		ParentOfThis->AddChild(Node);
		params[3] = ":" + params[3];
		DoOneToAllButSender(prefix,"SERVER",params,prefix);
		Srv->SendOpers("*** Server \002"+prefix+"\002 introduced server \002"+servername+"\002 ("+description+")");
		return true;
	}

	bool Outbound_Reply_Server(std::deque<std::string> params)
	{
		if (params.size() < 4)
			return false;
		std::string servername = params[0];
		std::string password = params[1];
		int hops = atoi(params[2].c_str());
		if (hops)
		{
			this->WriteLine("ERROR :Server too far away for authentication");
			return false;
		}
		std::string description = params[3];
		for (std::vector<Link>::iterator x = LinkBlocks.begin(); x < LinkBlocks.end(); x++)
		{
			if ((x->Name == servername) && (x->RecvPass == password))
			{
				TreeServer* CheckDupe = FindServer(servername);
				if (CheckDupe)
				{
					this->WriteLine("ERROR :Server "+servername+" already exists on server "+CheckDupe->GetParent()->GetName()+"!");
					return false;
				}
				// Begin the sync here. this kickstarts the
				// other side, waiting in WAIT_AUTH_2 state,
				// into starting their burst, as it shows
				// that we're happy.
				this->LinkState = CONNECTED;
				// we should add the details of this server now
				// to the servers tree, as a child of the root
				// node.
				TreeServer* Node = new TreeServer(servername,description,TreeRoot,this);
				TreeRoot->AddChild(Node);
				params[3] = ":" + params[3];
				DoOneToAllButSender(TreeRoot->GetName(),"SERVER",params,servername);
				this->DoBurst(Node);
				return true;
			}
		}
		this->WriteLine("ERROR :Invalid credentials");
		return false;
	}

	bool Inbound_Server(std::deque<std::string> params)
	{
		if (params.size() < 4)
			return false;
		std::string servername = params[0];
		std::string password = params[1];
		int hops = atoi(params[2].c_str());
		if (hops)
		{
			this->WriteLine("ERROR :Server too far away for authentication");
			return false;
		}
		std::string description = params[3];
		for (std::vector<Link>::iterator x = LinkBlocks.begin(); x < LinkBlocks.end(); x++)
		{
			if ((x->Name == servername) && (x->RecvPass == password))
			{
				TreeServer* CheckDupe = FindServer(servername);
				if (CheckDupe)
				{
					this->WriteLine("ERROR :Server "+servername+" already exists on server "+CheckDupe->GetParent()->GetName()+"!");
					return false;
				}
				Srv->SendOpers("*** Verified incoming server connection from \002"+servername+"\002["+this->GetIP()+"] ("+description+")");
				this->InboundServerName = servername;
				this->InboundDescription = description;
				// this is good. Send our details: Our server name and description and hopcount of 0,
				// along with the sendpass from this block.
				this->WriteLine("SERVER "+Srv->GetServerName()+" "+x->SendPass+" 0 :"+Srv->GetServerDescription());
				// move to the next state, we are now waiting for THEM.
				this->LinkState = WAIT_AUTH_2;
				return true;
			}
		}
		this->WriteLine("ERROR :Invalid credentials");
		return false;
	}

	std::deque<std::string> Split(std::string line, bool stripcolon)
	{
		std::deque<std::string> n;
		if (!strchr(line.c_str(),' '))
		{
			n.push_back(line);
			return n;
		}
		std::stringstream s(line);
		std::string param = "";
		n.clear();
		int item = 0;
		while (!s.eof())
		{
			char c;
			s.get(c);
			if (c == ' ')
			{
				n.push_back(param);
				param = "";
				item++;
			}
			else
			{
				if (!s.eof())
				{
					param = param + c;
				}
				if ((param == ":") && (item > 0))
				{
					param = "";
					while (!s.eof())
					{
						s.get(c);
						if (!s.eof())
						{
							param = param + c;
						}
					}
					n.push_back(param);
					param = "";
				}
			}
		}
		if (param != "")
		{
			n.push_back(param);
		}
		return n;
	}

	bool ProcessLine(std::string line)
	{
		char* l = (char*)line.c_str();
		while ((strlen(l)) && (l[strlen(l)-1] == '\r') || (l[strlen(l)-1] == '\n'))
			l[strlen(l)-1] = '\0';
		line = l;
		if (line == "")
			return true;
		Srv->Log(DEBUG,"IN: '"+line+"'");
		std::deque<std::string> params = this->Split(line,true);
		std::string command = "";
		std::string prefix = "";
		if (((params[0].c_str())[0] == ':') && (params.size() > 1))
		{
			prefix = params[0];
			command = params[1];
			char* pref = (char*)prefix.c_str();
			prefix = ++pref;
			params.pop_front();
			params.pop_front();
		}
		else
		{
			prefix = "";
			command = params[0];
			params.pop_front();
		}
		
		switch (this->LinkState)
		{
			TreeServer* Node;
			
			case WAIT_AUTH_1:
				// Waiting for SERVER command from remote server. Server initiating
				// the connection sends the first SERVER command, listening server
				// replies with theirs if its happy, then if the initiator is happy,
				// it starts to send its net sync, which starts the merge, otherwise
				// it sends an ERROR.
				if (command == "SERVER")
				{
					return this->Inbound_Server(params);
				}
				else if (command == "ERROR")
				{
					return this->Error(params);
				}
			break;
			case WAIT_AUTH_2:
				// Waiting for start of other side's netmerge to say they liked our
				// password.
				if (command == "SERVER")
				{
					// cant do this, they sent it to us in the WAIT_AUTH_1 state!
					// silently ignore.
					return true;
				}
				else if (command == "BURST")
				{
					this->LinkState = CONNECTED;
					Node = new TreeServer(InboundServerName,InboundDescription,TreeRoot,this);
	                                TreeRoot->AddChild(Node);
					params.clear();
					params.push_back(InboundServerName);
					params.push_back("*");
					params.push_back("1");
					params.push_back(":"+InboundDescription);
					DoOneToAllButSender(TreeRoot->GetName(),"SERVER",params,InboundServerName);
	                                this->DoBurst(Node);
				}
				else if (command == "ERROR")
				{
					return this->Error(params);
				}
				
			break;
			case LISTENER:
				this->WriteLine("ERROR :Internal error -- listening socket accepted its own descriptor!!!");
				return false;
			break;
			case CONNECTING:
				if (command == "SERVER")
				{
					// another server we connected to, which was in WAIT_AUTH_1 state,
					// has just sent us their credentials. If we get this far, theyre
					// happy with OUR credentials, and they are now in WAIT_AUTH_2 state.
					// if we're happy with this, we should send our netburst which
					// kickstarts the merge.
					return this->Outbound_Reply_Server(params);
				}
				else if (command == "ERROR")
				{
					return this->Error(params);
				}
			break;
			case CONNECTED:
				// This is the 'authenticated' state, when all passwords
				// have been exchanged and anything past this point is taken
				// as gospel.
				std::string target = "";
				if ((command == "NICK") && (params.size() > 1))
				{
					return this->IntroduceClient(prefix,params);
				}
				else if (command == "FJOIN")
				{
					return this->ForceJoin(prefix,params);
				}
				else if (command == "SERVER")
				{
					return this->RemoteServer(prefix,params);
				}
				else if (command == "ERROR")
				{
					return this->Error(params);
				}
				else if (command == "OPERTYPE")
				{
					return this->OperType(prefix,params);
				}
				else if (command == "FMODE")
				{
					return this->ForceMode(prefix,params);
				}
				else if (command == "KILL")
				{
					return this->RemoteKill(prefix,params);
				}
				else if (command == "FTOPIC")
				{
					return this->ForceTopic(prefix,params);
				}
				else if (command == "REHASH")
				{
					return this->RemoteRehash(prefix,params);
				}
				else if (command == "SQUIT")
				{
					if (params.size() == 2)
					{
						this->Squit(FindServer(params[0]),params[1]);
					}
					return true;
				}
				else
				{
					// not a special inter-server command.
					// Emulate the actual user doing the command,
					// this saves us having a huge ugly parser.
					userrec* who = Srv->FindNick(prefix);
					std::string sourceserv = this->myhost;
					if (this->InboundServerName != "")
					{
						sourceserv = this->InboundServerName;
					}
					if (who)
					{
						// its a user
						target = who->server;
						char* strparams[127];
						for (unsigned int q = 0; q < params.size(); q++)
						{
							strparams[q] = (char*)params[q].c_str();
						}
						Srv->CallCommandHandler(command, strparams, params.size(), who);
					}
					else
					{
						// its not a user. Its either a server, or somethings screwed up.
						if (IsServer(prefix))
						{
							target = Srv->GetServerName();
						}
						else
						{
							log(DEBUG,"Command with unknown origin '%s'",prefix.c_str());
							return true;
						}
					}
					return DoOneToAllButSenderRaw(line,sourceserv,prefix,command,params);

				}
				return true;
			break;
		}
		return true;
	}

	virtual std::string GetName()
	{
		std::string sourceserv = this->myhost;
		if (this->InboundServerName != "")
		{
			sourceserv = this->InboundServerName;
		}
		return sourceserv;
	}

	virtual void OnTimeout()
	{
		if (this->LinkState == CONNECTING)
		{
			Srv->SendOpers("*** CONNECT: Connection to \002"+myhost+"\002 timed out.");
		}
	}

	virtual void OnClose()
	{
		// Connection closed.
		// If the connection is fully up (state CONNECTED)
		// then propogate a netsplit to all peers.
		std::string quitserver = this->myhost;
		if (this->InboundServerName != "")
		{
			quitserver = this->InboundServerName;
		}
		TreeServer* s = FindServer(quitserver);
		if (s)
		{
			Squit(s,"Remote host closed the connection");
		}
	}

	virtual int OnIncomingConnection(int newsock, char* ip)
	{
		TreeSocket* s = new TreeSocket(newsock, ip);
		Srv->AddSocket(s);
		return true;
	}
};

void AddThisServer(TreeServer* server, std::deque<TreeServer*> &list)
{
	for (unsigned int c = 0; c < list.size(); c++)
	{
		if (list[c] == server)
		{
			return;
		}
	}
	list.push_back(server);
}

// returns a list of DIRECT servernames for a specific channel
std::deque<TreeServer*> GetListOfServersForChannel(chanrec* c)
{
	std::deque<TreeServer*> list;
	std::vector<char*> *ulist = c->GetUsers();
	for (unsigned int i = 0; i < ulist->size(); i++)
	{
		char* o = (*ulist)[i];
		userrec* otheruser = (userrec*)o;
		if (std::string(otheruser->server) != Srv->GetServerName())
		{
			TreeServer* best = BestRouteTo(otheruser->server);
			if (best)
				AddThisServer(best,list);
		}
	}
	return list;
}

bool DoOneToAllButSenderRaw(std::string data,std::string omit,std::string prefix,std::string command,std::deque<std::string> params)
{
	TreeServer* omitroute = BestRouteTo(omit);
	if ((command == "NOTICE") || (command == "PRIVMSG"))
	{
		if ((params.size() >= 2) && (*(params[0].c_str()) != '$'))
		{
			if (*(params[0].c_str()) != '#')
			{
				// special routing for private messages/notices
				userrec* d = Srv->FindNick(params[0]);
				if (d)
				{
					std::deque<std::string> par;
					par.clear();
					par.push_back(params[0]);
					par.push_back(":"+params[1]);
					DoOneToOne(prefix,command,par,d->server);
					return true;
				}
			}
			else
			{
				log(DEBUG,"Channel privmsg going to chan %s",params[0].c_str());
				chanrec* c = Srv->FindChannel(params[0]);
				if (c)
				{
					std::deque<TreeServer*> list = GetListOfServersForChannel(c);
					log(DEBUG,"Got a list of %d servers",list.size());
					for (unsigned int i = 0; i < list.size(); i++)
					{
						TreeSocket* Sock = list[i]->GetSocket();
						if ((Sock) && (list[i]->GetName() != omit) && (omitroute != list[i]))
						{
							log(DEBUG,"Writing privmsg to server %s",list[i]->GetName().c_str());
							Sock->WriteLine(data);
						}
					}
					return true;
				}
			}
		}
	}
	for (unsigned int x = 0; x < TreeRoot->ChildCount(); x++)
	{
		TreeServer* Route = TreeRoot->GetChild(x);
		if ((Route->GetSocket()) && (Route->GetName() != omit) && (omitroute != Route))
		{
			TreeSocket* Sock = Route->GetSocket();
			Sock->WriteLine(data);
		}
	}
	return true;
}

bool DoOneToAllButSender(std::string prefix, std::string command, std::deque<std::string> params, std::string omit)
{
	TreeServer* omitroute = BestRouteTo(omit);
	std::string FullLine = ":" + prefix + " " + command;
	for (unsigned int x = 0; x < params.size(); x++)
	{
		FullLine = FullLine + " " + params[x];
	}
	for (unsigned int x = 0; x < TreeRoot->ChildCount(); x++)
	{
		TreeServer* Route = TreeRoot->GetChild(x);
		// Send the line IF:
		// The route has a socket (its a direct connection)
		// The route isnt the one to be omitted
		// The route isnt the path to the one to be omitted
		if ((Route->GetSocket()) && (Route->GetName() != omit) && (omitroute != Route))
		{
			TreeSocket* Sock = Route->GetSocket();
			Sock->WriteLine(FullLine);
		}
	}
	return true;
}

bool DoOneToMany(std::string prefix, std::string command, std::deque<std::string> params)
{
	std::string FullLine = ":" + prefix + " " + command;
	for (unsigned int x = 0; x < params.size(); x++)
	{
		FullLine = FullLine + " " + params[x];
	}
	for (unsigned int x = 0; x < TreeRoot->ChildCount(); x++)
	{
		TreeServer* Route = TreeRoot->GetChild(x);
		if (Route->GetSocket())
		{
			TreeSocket* Sock = Route->GetSocket();
			Sock->WriteLine(FullLine);
		}
	}
	return true;
}

bool DoOneToOne(std::string prefix, std::string command, std::deque<std::string> params, std::string target)
{
	TreeServer* Route = BestRouteTo(target);
	if (Route)
	{
		std::string FullLine = ":" + prefix + " " + command;
		for (unsigned int x = 0; x < params.size(); x++)
		{
			FullLine = FullLine + " " + params[x];
		}
		if (Route->GetSocket())
		{
			TreeSocket* Sock = Route->GetSocket();
			Sock->WriteLine(FullLine);
		}
		return true;
	}
	else
	{
		return true;
	}
}

std::vector<TreeSocket*> Bindings;

void ReadConfiguration(bool rebind)
{
	if (rebind)
	{
		for (int j =0; j < Conf->Enumerate("bind"); j++)
		{
			std::string Type = Conf->ReadValue("bind","type",j);
			std::string IP = Conf->ReadValue("bind","address",j);
			long Port = Conf->ReadInteger("bind","port",j,true);
			if (Type == "servers")
			{
				if (IP == "*")
				{
					IP = "";
				}
				TreeSocket* listener = new TreeSocket(IP.c_str(),Port,true,10);
				if (listener->GetState() == I_LISTENING)
				{
					Srv->AddSocket(listener);
					Bindings.push_back(listener);
				}
				else
				{
					log(DEFAULT,"m_spanningtree: Warning: Failed to bind server port %d",Port);
					listener->Close();
					delete listener;
				}
			}
		}
	}
	LinkBlocks.clear();
	for (int j =0; j < Conf->Enumerate("link"); j++)
	{
		Link L;
		L.Name = Conf->ReadValue("link","name",j);
		L.IPAddr = Conf->ReadValue("link","ipaddr",j);
		L.Port = Conf->ReadInteger("link","port",j,true);
		L.SendPass = Conf->ReadValue("link","sendpass",j);
		L.RecvPass = Conf->ReadValue("link","recvpass",j);
		LinkBlocks.push_back(L);
		log(DEBUG,"m_spanningtree: Read server %s with host %s:%d",L.Name.c_str(),L.IPAddr.c_str(),L.Port);
	}
}

	
class ModuleSpanningTree : public Module
{
	std::vector<TreeSocket*> Bindings;
	int line;
	int NumServers;

 public:

	ModuleSpanningTree()
	{
		Srv = new Server;
		Conf = new ConfigReader;
		Bindings.clear();

		// Create the root of the tree
		TreeRoot = new TreeServer(Srv->GetServerName(),Srv->GetServerDescription());

		ReadConfiguration(true);
	}

	void ShowLinks(TreeServer* Current, userrec* user, int hops)
	{
		std::string Parent = TreeRoot->GetName();
		if (Current->GetParent())
		{
			Parent = Current->GetParent()->GetName();
		}
		for (unsigned int q = 0; q < Current->ChildCount(); q++)
		{
			ShowLinks(Current->GetChild(q),user,hops+1);
		}
		WriteServ(user->fd,"364 %s %s %s :%d %s",user->nick,Current->GetName().c_str(),Parent.c_str(),hops,Current->GetDesc().c_str());
	}

	int CountLocalServs()
	{
		return TreeRoot->ChildCount();
	}

	void CountServsRecursive(TreeServer* Current)
	{
		NumServers++;
		for (unsigned int q = 0; q < Current->ChildCount(); q++)
		{
			CountServsRecursive(Current->GetChild(q));
		}
	}
	
	int CountServs()
	{
		NumServers = 0;
		CountServsRecursive(TreeRoot);
		return NumServers;
	}

	void HandleLinks(char** parameters, int pcnt, userrec* user)
	{
		ShowLinks(TreeRoot,user,0);
		WriteServ(user->fd,"365 %s * :End of /LINKS list.",user->nick);
		return;
	}

	void HandleLusers(char** parameters, int pcnt, userrec* user)
	{
		WriteServ(user->fd,"251 %s :There are %d users and %d invisible on %d servers",user->nick,usercnt()-usercount_invisible(),usercount_invisible(),this->CountServs());
		WriteServ(user->fd,"252 %s %d :operator(s) online",user->nick,usercount_opers());
		WriteServ(user->fd,"253 %s %d :unknown connections",user->nick,usercount_unknown());
		WriteServ(user->fd,"254 %s %d :channels formed",user->nick,chancount());
		WriteServ(user->fd,"254 %s :I have %d clients and %d servers",user->nick,local_count(),this->CountLocalServs());
		return;
	}

	// WARNING: NOT THREAD SAFE - DONT GET ANY SMART IDEAS.

	void ShowMap(TreeServer* Current, userrec* user, int depth, char matrix[128][80])
	{
		if (line < 128)
		{
			for (int t = 0; t < depth; t++)
			{
				matrix[line][t] = ' ';
			}
			strlcpy(&matrix[line][depth],Current->GetName().c_str(),80);
			line++;
			for (unsigned int q = 0; q < Current->ChildCount(); q++)
			{
				ShowMap(Current->GetChild(q),user,depth+2,matrix);
			}
		}
	}

	// Ok, prepare to be confused.
	// After much mulling over how to approach this, it struck me that
	// the 'usual' way of doing a /MAP isnt the best way. Instead of
	// keeping track of a ton of ascii characters, and line by line
	// under recursion working out where to place them using multiplications
	// and divisons, we instead render the map onto a backplane of characters
	// (a character matrix), then draw the branches as a series of "L" shapes
	// from the nodes. This is not only friendlier on CPU it uses less stack.

	void HandleMap(char** parameters, int pcnt, userrec* user)
	{
		// This array represents a virtual screen which we will
		// "scratch" draw to, as the console device of an irc
		// client does not provide for a proper terminal.
		char matrix[128][80];
		for (unsigned int t = 0; t < 128; t++)
		{
			matrix[t][0] = '\0';
		}
		line = 0;
		// The only recursive bit is called here.
		ShowMap(TreeRoot,user,0,matrix);
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
			WriteServ(user->fd,"006 %s :%s",user->nick,&matrix[t][0]);
		}
	        WriteServ(user->fd,"007 %s :End of /MAP",user->nick);
		return;
	}

	int HandleSquit(char** parameters, int pcnt, userrec* user)
	{
		TreeServer* s = FindServer(parameters[0]);
		if (s)
		{
			TreeSocket* sock = s->GetSocket();
			if (sock)
			{
				WriteOpers("*** SQUIT: Server \002%s\002 removed from network by %s",parameters[0],user->nick);
				sock->Squit(s,"Server quit by "+std::string(user->nick)+"!"+std::string(user->ident)+"@"+std::string(user->host));
				sock->Close();
			}
			else
			{
				WriteServ(user->fd,"NOTICE %s :*** SQUIT: The server \002%s\002 is not directly connected.",user->nick,parameters[0]);
			}
		}
		else
		{
			 WriteServ(user->fd,"NOTICE %s :*** SQUIT: The server \002%s\002 does not exist on the network.",user->nick,parameters[0]);
		}
		return 1;
	}

	int HandleConnect(char** parameters, int pcnt, userrec* user)
	{
		for (std::vector<Link>::iterator x = LinkBlocks.begin(); x < LinkBlocks.end(); x++)
		{
			if (Srv->MatchText(x->Name.c_str(),parameters[0]))
			{
				TreeServer* CheckDupe = FindServer(x->Name);
				if (!CheckDupe)
				{
					WriteServ(user->fd,"NOTICE %s :*** CONNECT: Connecting to server: \002%s\002 (%s:%d)",user->nick,x->Name.c_str(),x->IPAddr.c_str(),x->Port);
					TreeSocket* newsocket = new TreeSocket(x->IPAddr,x->Port,false,10,x->Name);
					Srv->AddSocket(newsocket);
					return 1;
				}
				else
				{
					WriteServ(user->fd,"NOTICE %s :*** CONNECT: Server \002%s\002 already exists on the network and is connected via \002%s\002",user->nick,x->Name.c_str(),CheckDupe->GetParent()->GetName().c_str());
					return 1;
				}
			}
		}
		WriteServ(user->fd,"NOTICE %s :*** CONNECT: No server matching \002%s\002 could be found in the config file.",user->nick,parameters[0]);
		return 1;
	}

	virtual int OnPreCommand(std::string command, char **parameters, int pcnt, userrec *user)
	{
		if (command == "CONNECT")
		{
			return this->HandleConnect(parameters,pcnt,user);
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
		else if (Srv->IsValidModuleCommand(command, pcnt, user))
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
			DoOneToMany(user->nick,command,params);
		}
		return 0;
	}

	virtual void OnGetServerDescription(std::string servername,std::string &description)
	{
		TreeServer* s = FindServer(servername);
		if (s)
		{
			description = s->GetDesc();
		}
	}

	virtual void OnUserInvite(userrec* source,userrec* dest,chanrec* channel)
	{
		if (std::string(source->server) == Srv->GetServerName())
		{
			std::deque<std::string> params;
			params.push_back(dest->nick);
			params.push_back(channel->name);
			DoOneToMany(source->nick,"INVITE",params);
		}
	}

	virtual void OnPostLocalTopicChange(userrec* user, chanrec* chan, std::string topic)
	{
		std::deque<std::string> params;
		params.push_back(chan->name);
		params.push_back(":"+topic);
		DoOneToMany(user->nick,"TOPIC",params);
	}

	virtual void OnUserNotice(userrec* user, void* dest, int target_type, std::string text)
	{
		if (target_type == TYPE_USER)
		{
			userrec* d = (userrec*)dest;
			if ((std::string(d->server) != Srv->GetServerName()) && (std::string(user->server) == Srv->GetServerName()))
			{
				std::deque<std::string> params;
				params.clear();
				params.push_back(d->nick);
				params.push_back(":"+text);
				DoOneToOne(user->nick,"NOTICE",params,d->server);
			}
		}
		else
		{
			if (std::string(user->server) == Srv->GetServerName())
			{
				chanrec *c = (chanrec*)dest;
				std::deque<TreeServer*> list = GetListOfServersForChannel(c);
				for (unsigned int i = 0; i < list.size(); i++)
				{
					TreeSocket* Sock = list[i]->GetSocket();
					if (Sock)
						Sock->WriteLine(":"+std::string(user->nick)+" NOTICE "+std::string(c->name)+" :"+text);
				}
			}
		}
	}

	virtual void OnUserMessage(userrec* user, void* dest, int target_type, std::string text)
	{
		if (target_type == TYPE_USER)
		{
			// route private messages which are targetted at clients only to the server
			// which needs to receive them
			userrec* d = (userrec*)dest;
			if ((std::string(d->server) != Srv->GetServerName()) && (std::string(user->server) == Srv->GetServerName()))
			{
				std::deque<std::string> params;
				params.clear();
				params.push_back(d->nick);
				params.push_back(":"+text);
				DoOneToOne(user->nick,"PRIVMSG",params,d->server);
			}
		}
		else
		{
			if (std::string(user->server) == Srv->GetServerName())
			{
				chanrec *c = (chanrec*)dest;
				std::deque<TreeServer*> list = GetListOfServersForChannel(c);
				for (unsigned int i = 0; i < list.size(); i++)
				{
					TreeSocket* Sock = list[i]->GetSocket();
					if (Sock)
						Sock->WriteLine(":"+std::string(user->nick)+" PRIVMSG "+std::string(c->name)+" :"+text);
				}
			}
		}
	}

	virtual void OnUserJoin(userrec* user, chanrec* channel)
	{
		// Only do this for local users
		if (std::string(user->server) == Srv->GetServerName())
		{
			std::deque<std::string> params;
			params.clear();
			params.push_back(channel->name);
			if (*channel->key)
			{
				// if the channel has a key, force the join by emulating the key.
				params.push_back(channel->key);
			}
			if (channel->GetUserCounter() > 1)
			{
				// not the first in the channel
				DoOneToMany(user->nick,"JOIN",params);
			}
			else
			{
				// first in the channel, set up their permissions
				// and the channel TS with FJOIN.
				char ts[24];
				snprintf(ts,24,"%lu",(unsigned long)channel->age);
				params.clear();
				params.push_back(channel->name);
				params.push_back(ts);
				params.push_back("@"+std::string(user->nick));
				DoOneToMany(Srv->GetServerName(),"FJOIN",params);
			}
		}
	}

	virtual void OnUserPart(userrec* user, chanrec* channel)
	{
		if (std::string(user->server) == Srv->GetServerName())
		{
			std::deque<std::string> params;
			params.clear();
			params.push_back(channel->name);
			DoOneToMany(user->nick,"PART",params);
		}
	}

	virtual void OnUserConnect(userrec* user)
	{
		char agestr[MAXBUF];
		if (std::string(user->server) == Srv->GetServerName())
		{
			std::deque<std::string> params;
			snprintf(agestr,MAXBUF,"%lu",(unsigned long)user->age);
			params.clear();
			params.push_back(agestr);
			params.push_back(user->nick);
			params.push_back(user->host);
			params.push_back(user->dhost);
			params.push_back(user->ident);
			params.push_back("+"+std::string(user->modes));
			params.push_back(user->ip);
			params.push_back(":"+std::string(user->fullname));
			DoOneToMany(Srv->GetServerName(),"NICK",params);
		}
	}

	virtual void OnUserQuit(userrec* user, std::string reason)
	{
		if (std::string(user->server) == Srv->GetServerName())
		{
			std::deque<std::string> params;
			params.push_back(":"+reason);
			DoOneToMany(user->nick,"QUIT",params);
		}
	}

	virtual void OnUserPostNick(userrec* user, std::string oldnick)
	{
		if (std::string(user->server) == Srv->GetServerName())
		{
			std::deque<std::string> params;
			params.push_back(user->nick);
			DoOneToMany(oldnick,"NICK",params);
		}
	}

	virtual void OnUserKick(userrec* source, userrec* user, chanrec* chan, std::string reason)
	{
		if (std::string(source->server) == Srv->GetServerName())
		{
			std::deque<std::string> params;
			params.push_back(chan->name);
			params.push_back(user->nick);
			params.push_back(":"+reason);
			DoOneToMany(source->nick,"KICK",params);
		}
	}

	virtual void OnRemoteKill(userrec* source, userrec* dest, std::string reason)
	{
		std::deque<std::string> params;
		params.push_back(dest->nick);
		params.push_back(":"+reason);
		DoOneToMany(source->nick,"KILL",params);
	}

	virtual void OnRehash(std::string parameter)
	{
		if (parameter != "")
		{
			std::deque<std::string> params;
			params.push_back(parameter);
			DoOneToMany(Srv->GetServerName(),"REHASH",params);
			// check for self
			if (Srv->MatchText(Srv->GetServerName(),parameter))
			{
				Srv->SendOpers("*** Remote rehash initiated from server \002"+Srv->GetServerName()+"\002.");
				Srv->RehashServer();
			}
		}
		ReadConfiguration(false);
	}

	// note: the protocol does not allow direct umode +o except
	// via NICK with 8 params. sending OPERTYPE infers +o modechange
	// locally.
	virtual void OnOper(userrec* user, std::string opertype)
	{
		if (std::string(user->server) == Srv->GetServerName())
		{
			std::deque<std::string> params;
			params.push_back(opertype);
			DoOneToMany(user->nick,"OPERTYPE",params);
		}
	}

	virtual void OnMode(userrec* user, void* dest, int target_type, std::string text)
	{
		if (std::string(user->server) == Srv->GetServerName())
		{
			if (target_type == TYPE_USER)
			{
				userrec* u = (userrec*)dest;
				std::deque<std::string> params;
				params.push_back(u->nick);
				params.push_back(text);
				DoOneToMany(user->nick,"MODE",params);
			}
			else
			{
				chanrec* c = (chanrec*)dest;
				std::deque<std::string> params;
				params.push_back(c->name);
				params.push_back(text);
				DoOneToMany(user->nick,"MODE",params);
			}
		}
	}

	virtual void ProtoSendMode(void* opaque, int target_type, void* target, std::string modeline)
	{
		TreeSocket* s = (TreeSocket*)opaque;
		if (target)
		{
			if (target_type == TYPE_USER)
			{
				userrec* u = (userrec*)target;
				s->WriteLine(":"+Srv->GetServerName()+" FMODE "+u->nick+" "+modeline);
			}
			else
			{
				chanrec* c = (chanrec*)target;
				s->WriteLine(":"+Srv->GetServerName()+" FMODE "+c->name+" "+modeline);
			}
		}
	}

	virtual ~ModuleSpanningTree()
	{
		delete Srv;
	}

	virtual Version GetVersion()
	{
		return Version(1,0,0,0,VF_STATIC|VF_VENDOR);
	}
};


class ModuleSpanningTreeFactory : public ModuleFactory
{
 public:
	ModuleSpanningTreeFactory()
	{
	}
	
	~ModuleSpanningTreeFactory()
	{
	}
	
	virtual Module * CreateModule()
	{
		TreeProtocolModule = new ModuleSpanningTree;
		return TreeProtocolModule;
	}
	
};


extern "C" void * init_module( void )
{
	return new ModuleSpanningTreeFactory;
}
