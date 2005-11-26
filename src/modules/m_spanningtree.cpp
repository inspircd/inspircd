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
#include "users.h"
#include "channels.h"
#include "modules.h"
#include "socket.h"
#include "helperfuncs.h"
#include "inspircd.h"

enum ServerState { CONNECTING, WAIT_AUTH_1, WAIT_AUTH_2, CONNECTED };

class TreeServer;
class TreeSocket;

class TreeServer
{
	TreeServer* Parent;
	std::vector<TreeServer*> Children;
	std::string ServerName;
	std::string ServerDesc;
	std::string VersionString;
	int UserCount;
	int OperCount;
	ServerState State;
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

	TreeServer(std::string Name, std::string Desc, TreeServer* Above) : Parent(Above), ServerName(Name), ServerDesc(Desc)
	{
		VersionString = "";
		UserCount = OperCount = 0;
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

class TreeSocket : public InspSocket
{
	std::string myhost;
	
 public:

	TreeSocket(std::string host, int port, bool listening, unsigned long maxtime)
		: InspSocket(host, port, listening, maxtime)
	{
		Srv->Log(DEBUG,"Create new");
		myhost = host;
	}

	TreeSocket(int newfd)
		: InspSocket(newfd)
	{
	}
	
        virtual bool OnConnected()
	{
		return true;
	}
	
        virtual void OnError(InspSocketError e)
	{
	}

        virtual int OnDisconnect()
	{
		Srv->Log(DEBUG,"Disconnect");
		Srv->SendToModeMask("o",WM_AND,"*** DISCONNECTED!");
		return true;
	}

        virtual bool OnDataReady()
	{
		Srv->SendToModeMask("o",WM_AND,"*** DATA ***");
		char* data = this->Read();
		if (data)
		{
			Srv->SendToModeMask("o",WM_AND,data);
		}
		return (data != NULL);
	}

        virtual void OnTimeout()
	{
		Srv->SendToModeMask("o",WM_AND,"*** TIMED OUT ***");
	}

        virtual void OnClose()
	{
		Srv->SendToModeMask("o",WM_AND,"*** CLOSED ***");
	}

	virtual int OnIncomingConnection(int newsock, char* ip)
	{
		TreeSocket* s = new TreeSocket(newsock);
		Srv->AddSocket(s);
		return true;
	}
};

void handle_connecttest(char **parameters, int pcnt, userrec *user)
{
	std::string addr = parameters[0];
	TreeSocket* sock = new TreeSocket(addr,80,false,10);
	Srv->AddSocket(sock);
}

class ModuleSpanningTree : public Module
{
	std::vector<TreeSocket*> Bindings;

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

	void HandleLinks(char** parameters, int pcnt, userrec* user)
	{
		return;
	}

	void HandleLusers(char** parameters, int pcnt, userrec* user)
	{
		return;
	}

	void HandleMap(char** parameters, int pcnt, userrec* user)
	{
		return;
	}

	int HandleSquit(char** parameters, int pcnt, userrec* user)
	{
		return 1;
	}

	int HandleConnect(char** parameters, int pcnt, userrec* user)
	{
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

