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
	}

	TreeServer(std::string Name, std::string Desc) : ServerName(Name), ServerDesc(Desc)
	{
	}

	TreeServer(std::string Name, std::string Desc, TreeServer* Above) : Parent(Above), ServerName(Name), ServerDesc(Desc)
	{
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

/* $ModDesc: Povides a spanning tree server link protocol */

Server *Srv;

// To attach sockets to the core of the ircd, we must use the InspSocket
// class which can be found in socket.h. This class allows modules to create
// listening and outbound sockets and attach sockets to existing (connected)
// file descriptors. These file descriptors can then be associated with the
// core of the ircd and bound to the socket engine.
// To use InspSocket, we must inherit from it, as shown in TreeSocket below.

class TreeSocket : public InspSocket
{
	std::string myhost;
	
 public:

	// InspSocket has several constructors used for various situations.
	// This constructor is used to create a socket which may be used
	// for both inbound and outbound (listen() and connect()) operations.
	// When you inherit InspSocket you MUST call the superclass constructor
	// within InspSocket, as shown below, unless you plan to completely
	// override all behaviour of the class, which would prove to be more
	// trouble than it's worth unless you're doing something really fancy.
	
	TreeSocket(std::string host, int port, bool listening, unsigned long maxtime)
		: InspSocket(host, port, listening, maxtime)
	{
		Srv->Log(DEBUG,"Create new");
		myhost = host;
	}

	// This simpler constructor of InspSocket is used when you wish to
	// associate an existing file descriptor with an InspSocket class,
	// or a class inherited from InspSocket. As before, you must call
	// the superclass. Not doing so will get your module into a whole
	// world of hurt. Similarly, your inherited class MUST implement
	// the constructors you use even if all it does is call the parent.

	TreeSocket(int newfd)
		: InspSocket(newfd)
	{
	}
	
	// This method is called when an outbound socket (connect() style)
	// finishes connecting. Connections are asyncronous, so you should
	// not just assume that immediately after you instantiate a socket
	// it is connected or failed. This takes time, and when the results
	// are available for you, this method will be called.

        virtual bool OnConnected()
	{
		this->Write("GET / HTTP/1.1\r\nHost: " + myhost + "\r\nConnection: Close\r\n\r\n");
		return true;
	}

	// When errors occur on the connection, this event will be triggered.
	// Check the programmer docs for information on possible values for
	// the InspSocketError type.
	
        virtual void OnError(InspSocketError e)
	{
		char x[1024];
		Srv->Log(DEBUG,"Error");
		sprintf(x,"*** ERROR %d",(int)e);
		Srv->SendToModeMask("o",WM_AND,x);
	}
	
	// When a socket disconnects, this method is triggered. You cannot
	// prevent the disconnection.

        virtual int OnDisconnect()
	{
		Srv->Log(DEBUG,"Disconnect");
		Srv->SendToModeMask("o",WM_AND,"*** DISCONNECTED!");
		return true;
	}

	// When data is ready to be read from a socket, this method will
	// be triggered, and within it, you should call this->Read() to
	// read any pending data. Up to 10 kilobytes of data may be returned
	// for each call to Read(), and you should not call Read() more
	// than once per method call. You should also not call Read()
	// outside of OnDataReady(), doing so will just result in Read()
	// returning NULL. If Read() returns NULL and you are within the
	// OnDataReady() event this usually indicates an EOF condition
	// and the socket should be closed by returning false from this
	// method. If you return false, the core will remove your socket
	// from its list, and handle the cleanup (such as deleting the
	// pointer) for you. This means you do not need to track your
	// socket resources once they are associated with the core.

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

	// For outbound (connect style()) sockets only, the connection
	// may time out, meaning that the time taken to connect was
	// more than you specified when constructing the object.
	// If this occurs, the OnTimeout method, as well as OnError,
	// will be called to notify your class of the event.
	
        virtual void OnTimeout()
	{
		Srv->SendToModeMask("o",WM_AND,"*** TIMED OUT ***");
	}

	// For any type of socket, when the file descriptor is freed
	// with close(), under any situation, the OnClose() method
	// will be called.

        virtual void OnClose()
	{
		Srv->SendToModeMask("o",WM_AND,"*** CLOSED ***");
	}

	// When a connection comes in over an inbound (listen() style)
	// socket, the OnIncomingConnection method is triggered. You
	// will be given a new file descriptor, and the ip of the
	// connecting host. Most of the time, you will want to
	// instantiate another class inherited from InspSocket,
	// using the (int) constructor which will associate that class
	// with the new file descriptor. You will usually then need
	// to add that socket to the core, so that you will receive
	// notifications for its activity.
	
	virtual int OnIncomingConnection(int newsock, char* ip)
	{
		TreeSocket* s = new TreeSocket(newsock);
		Srv->AddSocket(s);
		return true;
	}
};

// This is a test function which creates an outbound socket to a given
// hostname. It provides an example of how to create a new outbound
// socket to a host.

void handle_connecttest(char **parameters, int pcnt, userrec *user)
{
	std::string addr = parameters[0];
	TreeSocket* sock = new TreeSocket(addr,80,false,10);
	Srv->AddSocket(sock);
}

class ModuleSpanningTree : public Module
{
 public:
	ModuleSpanningTree()
	{
		Srv = new Server;
		
		Srv->AddCommand("CONNECTTEST",handle_connecttest,'o',1,"m_spanningtree.so");

		// This is an example of how to create a new inbound socket to a host.
		// Please remember that so long as you AddSocket() your sockets, you
		// do not need to track resources and do not need to delete your classes
		// when you are finished with them.

		TreeSocket* listeningsock = new TreeSocket("127.0.0.1",11111,true,10);
		Srv->AddSocket(listeningsock);
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

