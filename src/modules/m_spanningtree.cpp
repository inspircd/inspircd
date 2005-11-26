/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  Inspire is copyright (C) 2002-2004 ChatSpike-Dev.
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

/* $ModDesc: Povides a spanning tree server link protocol */

Server *Srv;

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
	
        virtual bool OnConnected()
	{
		Srv->Log(DEBUG,"Connected");
		Srv->SendToModeMask("o",WM_AND,"*** CONNECTED!");
		this->Write("GET / HTTP/1.1\r\nHost: " + myhost + "\r\nConnection: Close\r\n\r\n");
		Srv->SendToModeMask("o",WM_AND,"*** DATA WRITTEN ***");
		Srv->Log(DEBUG,"Wrote");
		return true;
	}
	
        virtual void OnError(InspSocketError e)
	{
		char x[1024];
		Srv->Log(DEBUG,"Error");
		sprintf(x,"*** ERROR %d",(int)e);
		Srv->SendToModeMask("o",WM_AND,x);
	}
	
        virtual int OnDisconnect()
	{
		Srv->Log(DEBUG,"Disconnect");
		Srv->SendToModeMask("o",WM_AND,"*** DISCONNECTED!");
		return true;
	}
	
        virtual bool OnDataReady()
	{
		Srv->Log(DEBUG,"Data");
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
		Srv->Log(DEBUG,"Timeout");
		Srv->SendToModeMask("o",WM_AND,"*** TIMED OUT ***");
	}
	
        virtual void OnClose()
	{
		Srv->SendToModeMask("o",WM_AND,"*** CLOSED ***");
	}
	
	virtual int OnIncomingConnection()
	{
		Srv->SendToModeMask("o",WM_AND,"*** INCOMING ***");
		return true;
	}
};

void handle_connecttest(char **parameters, int pcnt, userrec *user)
{
	// create a new class of type TreeSocket.
	std::string a = parameters[0];
	TreeSocket* s = new TreeSocket(a,80,false,10);
	Srv->Log(DEBUG,"Create TreeSocket");
	Srv->AddSocket(s);
	Srv->Log(DEBUG,"Added socket");
}

class ModuleSpanningTree : public Module
{
 public:
	ModuleSpanningTree()
	{
		Srv = new Server;
		Srv->AddCommand("CONNECTTEST",handle_connecttest,'o',1,"m_spanningtree.so");
		Srv->Log(DEBUG,"ModCreate");
	}
	
	virtual void OnUserJoin(userrec* user, chanrec* channel)
	{
	}
	
	virtual ~ModuleSpanningTree()
	{
		delete Srv;
	}
	
	virtual Version GetVersion()
	{
		return Version(1,0,0,0,VF_STATIC|VF_VENDOR);
	}
	
	virtual void OnUserConnect(userrec* user)
	{
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

