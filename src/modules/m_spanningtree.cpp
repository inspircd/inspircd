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

enum ServerState { LISTENER, CONNECTING, WAIT_AUTH_1, WAIT_AUTH_2, CONNECTED };

typedef nspace::hash_map<std::string, userrec*, nspace::hash<string>, irc::StrHashComp> user_hash;
extern user_hash clientlist;

class TreeServer;
class TreeSocket;

bool DoOneToOne(std::string prefix, std::string command, std::deque<std::string> params, std::string target);
bool DoOneToAllButSender(std::string prefix, std::string command, std::deque<std::string> params, std::string omit);
bool DoOneToMany(std::string prefix, std::string command, std::deque<std::string> params);

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
	log(DEBUG,"Finding best route to %s",ServerName.c_str());
	// first, find the server by recursively walking the tree
	TreeServer* Found = RouteEnumerate(TreeRoot,ServerName);
	// did we find it? If not, they did something wrong, abort.
	if (!Found)
	{
		log(DEBUG,"Failed to find %s by walking tree!",ServerName.c_str());
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
		log(DEBUG,"Route to %s is via %s",ServerName.c_str(),Found->GetName().c_str());
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
		log(DEBUG,"Found server %s desc %s",Current->GetName().c_str(),Current->GetDesc().c_str());
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
		Srv->Log(DEBUG,"Create new listening");
		myhost = host;
		this->LinkState = LISTENER;
	}

	TreeSocket(std::string host, int port, bool listening, unsigned long maxtime, std::string ServerName)
		: InspSocket(host, port, listening, maxtime)
	{
		Srv->Log(DEBUG,"Create new outbound");
		myhost = ServerName;
		this->LinkState = CONNECTING;
	}

	TreeSocket(int newfd, char* ip)
		: InspSocket(newfd, ip)
	{
		Srv->Log(DEBUG,"Associate new inbound");
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
		log(DEBUG,"Outbound connection ERROR: Could not find the right link block!");
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
		log(DEBUG,"Deleted %s",Current->GetName().c_str());
		num_lost_servers++;
		bool quittingpeople = true;
		while (quittingpeople)
		{
			quittingpeople = false;
			for (user_hash::iterator u = clientlist.begin(); u != clientlist.end(); u++)
			{
				if (!strcasecmp(u->second->server,Current->GetName().c_str()))
				{
					log(DEBUG,"Quitting user %s of server %s",u->second->nick,u->second->server);
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
			log(DEBUG,"Squit from unknown server");
		}
	}

	bool ForceJoin(std::string source, std::deque<std::string> params)
	{
		if (params.size() < 1)
			return true;
		for (unsigned int channelnum = 0; channelnum < params.size(); channelnum++)
		{
			// process one channel at a time, applying modes.
			char* channel = (char*)params[channelnum].c_str();
			char permissions = *channel;
			char* mode = NULL;
			switch (permissions)
			{
				case '@':
					channel++;
					mode = "+o";
				break;
				case '%':
					channel++;
					mode = "+h";
				break;
				case '+':
					channel++;
					mode = "+v";
				break;
			}
			userrec* who = Srv->FindNick(source);
			if (who)
			{
				char* key = "";
				chanrec* chan = Srv->FindChannel(channel);
				if ((chan) && (*chan->key))
				{
					key = chan->key;
				}
				Srv->JoinUserToChannel(who,channel,key);
				if (mode)
				{
					char* modelist[3];
					modelist[0] = channel;
					modelist[1] = mode;
					modelist[2] = who->nick;
					Srv->SendMode(modelist,3,who);
				}
				DoOneToAllButSender(source,"FJOIN",params,who->server);
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
		std::string ip = params[6];
		std::string gecos = params[7];
		char* tempnick = (char*)nick.c_str();
		log(DEBUG,"Introduce client %s!%s@%s",tempnick,ident.c_str(),host.c_str());
		
		user_hash::iterator iter;
		iter = clientlist.find(tempnick);
		if (iter != clientlist.end())
		{
			// nick collision
			log(DEBUG,"Nick collision on %s!%s@%s",tempnick,ident.c_str(),host.c_str());
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
		strlcpy(clientlist[tempnick]->ip,ip.c_str(),16);
		for (int i = 0; i < MAXCHANS; i++)
		{
			clientlist[tempnick]->chans[i].channel = NULL;
			clientlist[tempnick]->chans[i].uc_modes = 0;
		}
		DoOneToAllButSender(source,"NICK",params,source);
		return true;
	}

	// send all users and their channels
	void SendUsers(TreeServer* Current)
	{
		char data[MAXBUF];
		for (user_hash::iterator u = clientlist.begin(); u != clientlist.end(); u++)
		{
			snprintf(data,MAXBUF,":%s NICK %lu %s %s %s %s +%s %s :%s",u->second->server,(unsigned long)u->second->age,u->second->nick,u->second->host,u->second->dhost,u->second->ident,u->second->modes,u->second->ip,u->second->fullname);
			this->WriteLine(data);
			if (strchr(u->second->modes,'o'))
			{
				this->WriteLine(":"+std::string(u->second->nick)+" OPERTYPE "+std::string(u->second->oper));
			}
			char* chl = chlist(u->second,u->second);
			if (*chl)
			{
				this->WriteLine(":"+std::string(u->second->nick)+" FJOIN "+std::string(chl));
			}
		}
	}

	void DoBurst(TreeServer* s)
	{
		log(DEBUG,"Beginning network burst");
		Srv->SendOpers("*** Bursting to "+s->GetName()+".");
		this->WriteLine("BURST");
		// Send server tree
		this->SendServers(TreeRoot,s,1);
		// Send users and their channels
		this->SendUsers(s);
		// TODO: Send everything else (channel modes etc)
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

	bool RemoteServer(std::string prefix, std::deque<std::string> params)
	{
		if (params.size() < 4)
			return false;
		std::string servername = params[0];
		std::string password = params[1];
		int hops = atoi(params[2].c_str());
		std::string description = params[3];
		if (!hops)
		{
			this->WriteLine("ERROR :Protocol error - Introduced remote server with incorrect hopcount!");
			return false;
		}
		TreeServer* ParentOfThis = FindServer(prefix);
		if (!ParentOfThis)
		{
			this->WriteLine("ERROR :Protocol error - Introduced remote server from unknown server "+prefix);
			return false;
		}
		TreeServer* Node = new TreeServer(servername,description,ParentOfThis,NULL);
		ParentOfThis->AddChild(Node);
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
		std::stringstream s(line);
		std::string param = "";
		n.clear();
		int item = 0;
		while (!s.eof())
		{
			s >> param;
			if ((param.c_str()[0] == ':') && (item))
			{
				char* str = (char*)param.c_str();
				str++;
				param = str;
				std::string append;
				while (!s.eof())
				{
					append = "";
					s >> append;
					if (append != "")
					{
						param = param + " " + append;
					}
				}
			}
			item++;
			if ((strchr(param.c_str(),' ')) && (!stripcolon))
			{
				param = ":"+param;
			}
			n.push_back(param);
		}
		return n;
	}

	bool ProcessLine(std::string line)
	{
		Srv->Log(DEBUG,"inbound-line: '"+line+"'");

		std::deque<std::string> rawparams = this->Split(line,false);
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
						log(DEBUG,"*** CALL COMMAND HANDLER FOR %s, SOURCE: '%s'",command.c_str(),who->nick);
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
					return DoOneToAllButSender(prefix,command,rawparams,sourceserv);

				}
				return true;
			break;	
		}
		return true;
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
			std::deque<std::string> params;
			params.push_back(quitserver);
			params.push_back(":Remote host closed the connection");
			DoOneToAllButSender(Srv->GetServerName(),"SQUIT",params,quitserver);
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

bool DoOneToAllButSender(std::string prefix, std::string command, std::deque<std::string> params, std::string omit)
{
	log(DEBUG,"ALLBUTONE: Comes from %s SHOULD NOT go back to %s",prefix.c_str(),omit.c_str());
	// TODO: Special stuff with privmsg and notice
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
		if ((Route->GetSocket()) && (Route->GetName() != omit) && (BestRouteTo(omit) != Route))
		{
			TreeSocket* Sock = Route->GetSocket();
			log(DEBUG,"Sending to %s",Route->GetName().c_str());
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
		log(DEBUG,"Could not route message with target %s: %s",target.c_str(),command.c_str());
		return true;
	}
}


class ModuleSpanningTree : public Module
{
	std::vector<TreeSocket*> Bindings;
	int line;

 public:

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

	void HandleLinks(char** parameters, int pcnt, userrec* user)
	{
		ShowLinks(TreeRoot,user,0);
		WriteServ(user->fd,"365 %s * :End of /LINKS list.",user->nick);
		return;
	}

	void HandleLusers(char** parameters, int pcnt, userrec* user)
	{
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
		return 1;
	}

	int HandleConnect(char** parameters, int pcnt, userrec* user)
	{
		for (std::vector<Link>::iterator x = LinkBlocks.begin(); x < LinkBlocks.end(); x++)
		{
			if (Srv->MatchText(x->Name.c_str(),parameters[0]))
			{
				WriteServ(user->fd,"NOTICE %s :*** CONNECT: Connecting to server: %s (%s:%d)",user->nick,x->Name.c_str(),x->IPAddr.c_str(),x->Port);
				TreeSocket* newsocket = new TreeSocket(x->IPAddr,x->Port,false,10,x->Name);
				Srv->AddSocket(newsocket);
				return 1;
			}
		}
		WriteServ(user->fd,"NOTICE %s :*** CONNECT: No matching server could be found in the config file.",user->nick);
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
		return 0;
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
				std::deque<std::string> params;
				params.push_back(c->name);
				params.push_back(":"+text);
				DoOneToMany(user->nick,"PRIVMSG",params);
			}
		}
	}

	virtual void OnUserJoin(userrec* user, chanrec* channel)
	{
		// Only do this for local users
		if (std::string(user->server) == Srv->GetServerName())
		{
			log(DEBUG,"**** User on %s JOINS %s",user->server,channel->name);
			std::deque<std::string> params;
			params.clear();
			params.push_back(channel->name);
			if (*channel->key)
			{
				log(DEBUG,"**** With key %s",channel->key);
				// if the channel has a key, force the join by emulating the key.
				params.push_back(channel->key);
			}
			DoOneToMany(user->nick,"JOIN",params);
		}
	}

	virtual void OnUserPart(userrec* user, chanrec* channel)
	{
		if (std::string(user->server) == Srv->GetServerName())
		{
			log(DEBUG,"**** User on %s PARTS %s",user->server,channel->name);
			std::deque<std::string> params;
			params.clear();
			params.push_back(channel->name);
			DoOneToMany(user->nick,"PART",params);
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
		return new ModuleSpanningTree;
	}
	
};


extern "C" void * init_module( void )
{
	return new ModuleSpanningTreeFactory;
}

