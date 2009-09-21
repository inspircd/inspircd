/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2009 InspIRCd Development Team
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
#include "../transport.h"
#include "../m_hash.h"
#include "socketengine.h"

#include "main.h"
#include "utils.h"
#include "treeserver.h"
#include "link.h"
#include "treesocket.h"
#include "resolvers.h"
#include "handshaketimer.h"

/* $ModDep: m_spanningtree/resolvers.h m_spanningtree/main.h m_spanningtree/utils.h m_spanningtree/treeserver.h m_spanningtree/link.h m_spanningtree/treesocket.h m_hash.h m_spanningtree/handshaketimer.h */


/** Because most of the I/O gubbins are encapsulated within
 * BufferedSocket, we just call the superclass constructor for
 * most of the action, and append a few of our own values
 * to it.
 */
TreeSocket::TreeSocket(SpanningTreeUtilities* Util, std::string shost, int iport, unsigned long maxtime, const std::string &ServerName, const std::string &bindto, Autoconnect* myac, Module* HookMod)
	: Utils(Util), IP(shost), myautoconnect(myac)
{
	age = ServerInstance->Time();
	myhost = ServerName;
	capab_phase = 0;
	proto_version = 0;
	LinkState = CONNECTING;
	DoConnect(shost, iport, maxtime, bindto);
	Utils->timeoutlist[this] = std::pair<std::string, int>(ServerName, maxtime);
	if (HookMod)
		BufferedSocketHookRequest(this, Utils->Creator, HookMod).Send();
	hstimer = NULL;
}

/** When a listening socket gives us a new file descriptor,
 * we must associate it with a socket without creating a new
 * connection. This constructor is used for this purpose.
 */
TreeSocket::TreeSocket(SpanningTreeUtilities* Util, int newfd, char* ip, Autoconnect* myac, Module* HookMod)
	: BufferedSocket(newfd), Utils(Util), IP(ip), myautoconnect(myac)
{
	age = ServerInstance->Time();
	LinkState = WAIT_AUTH_1;
	capab_phase = 0;
	proto_version = 0;
	/* If we have a transport module hooked to the parent, hook the same module to this
	 * socket, and set a timer waiting for handshake before we send CAPAB etc.
	 */
	if (HookMod)
		BufferedSocketHookRequest(this, Utils->Creator, HookMod).Send();

	hstimer = new HandshakeTimer(ServerInstance, this, &(Utils->LinkBlocks[0]), this->Utils, 1);
	ServerInstance->Timers->AddTimer(hstimer);

	/* Fix by Brain - inbound sockets need a timeout, too. 30 secs should be pleanty */
	Utils->timeoutlist[this] = std::pair<std::string, int>("<unknown>", 30);
}

ServerState TreeSocket::GetLinkState()
{
	return this->LinkState;
}

void TreeSocket::CleanNegotiationInfo()
{
	ModuleList.clear();
	OptModuleList.clear();
	CapKeys.clear();
	ourchallenge.clear();
	theirchallenge.clear();
	OutboundPass.clear();
}

TreeSocket::~TreeSocket()
{
	if (GetIOHook())
		BufferedSocketUnhookRequest(this, Utils->Creator, GetIOHook()).Send();
	if (hstimer)
		ServerInstance->Timers->DelTimer(hstimer);
	Utils->timeoutlist.erase(this);
}

/** When an outbound connection finishes connecting, we receive
 * this event, and must send our SERVER string to the other
 * side. If the other side is happy, as outlined in the server
 * to server docs on the inspircd.org site, the other side
 * will then send back its own server string.
 */
void TreeSocket::OnConnected()
{
	if (this->LinkState == CONNECTING)
	{
		/* we do not need to change state here. */
		for (std::vector<Link>::iterator x = Utils->LinkBlocks.begin(); x < Utils->LinkBlocks.end(); x++)
		{
			if (x->Name == this->myhost)
			{
				ServerInstance->SNO->WriteToSnoMask('l', "Connection to \2%s\2[%s] started.", myhost.c_str(), (x->HiddenFromStats ? "<hidden>" : this->IP.c_str()));
				this->OutboundPass = x->SendPass;
				if (GetIOHook())
				{
					ServerInstance->SNO->WriteToSnoMask('l', "Connection to \2%s\2[%s] using transport \2%s\2", myhost.c_str(), (x->HiddenFromStats ? "<hidden>" : this->IP.c_str()), x->Hook.c_str());
					hstimer = new HandshakeTimer(ServerInstance, this, &(*x), this->Utils, 1);
					ServerInstance->Timers->AddTimer(hstimer);
				}
				else
					this->SendCapabilities(1);
				return;
			}
		}
	}
	/* There is a (remote) chance that between the /CONNECT and the connection
	 * being accepted, some muppet has removed the <link> block and rehashed.
	 * If that happens the connection hangs here until it's closed. Unlikely
	 * and rather harmless.
	 */
	ServerInstance->SNO->WriteToSnoMask('l', "Connection to \2%s\2 lost link tag(!)", myhost.c_str());
}

void TreeSocket::OnError(BufferedSocketError e)
{
	switch (e)
	{
		case I_ERR_CONNECT:
			ServerInstance->SNO->WriteToSnoMask('l', "Connection failed: Connection to \002%s\002 refused", myhost.c_str());
			Utils->DoFailOver(myautoconnect);
		break;
		case I_ERR_SOCKET:
			ServerInstance->SNO->WriteToSnoMask('l', "Connection failed: Could not create socket (%s)", strerror(errno));
		break;
		case I_ERR_BIND:
			ServerInstance->SNO->WriteToSnoMask('l', "Connection failed: Error binding socket to address or port (%s)", strerror(errno));
		break;
		case I_ERR_WRITE:
			ServerInstance->SNO->WriteToSnoMask('l', "Connection failed: I/O error on connection (%s)", errno ? strerror(errno) : "Connection closed unexpectedly");
		break;
		case I_ERR_NOMOREFDS:
			ServerInstance->SNO->WriteToSnoMask('l', "Connection failed: Operating system is out of file descriptors!");
		break;
		default:
			if ((errno) && (errno != EINPROGRESS) && (errno != EAGAIN))
				ServerInstance->SNO->WriteToSnoMask('l', "Connection to \002%s\002 failed with OS error: %s", myhost.c_str(), strerror(errno));
		break;
	}
}

void TreeSocket::SendError(const std::string &errormessage)
{
	/* Display the error locally as well as sending it remotely */
	ServerInstance->SNO->WriteToSnoMask('l', "Sent \2ERROR\2 to %s: %s", (this->InboundServerName.empty() ? this->IP.c_str() : this->InboundServerName.c_str()), errormessage.c_str());
	WriteLine("ERROR :"+errormessage);
}

/** This function forces this server to quit, removing this server
 * and any users on it (and servers and users below that, etc etc).
 * It's very slow and pretty clunky, but luckily unless your network
 * is having a REAL bad hair day, this function shouldnt be called
 * too many times a month ;-)
 */
void TreeSocket::SquitServer(std::string &from, TreeServer* Current)
{
	/* recursively squit the servers attached to 'Current'.
	 * We're going backwards so we don't remove users
	 * while we still need them ;)
	 */
	for (unsigned int q = 0; q < Current->ChildCount(); q++)
	{
		TreeServer* recursive_server = Current->GetChild(q);
		this->SquitServer(from,recursive_server);
	}
	/* Now we've whacked the kids, whack self */
	num_lost_servers++;
	num_lost_users += Current->QuitUsers(from);
}

/** This is a wrapper function for SquitServer above, which
 * does some validation first and passes on the SQUIT to all
 * other remaining servers.
 */
void TreeSocket::Squit(TreeServer* Current, const std::string &reason)
{
	bool LocalSquit = false;

	if ((Current) && (Current != Utils->TreeRoot))
	{
		Event rmode((char*)Current->GetName().c_str(), (Module*)Utils->Creator, "lost_server");
		rmode.Send(ServerInstance);

		parameterlist params;
		params.push_back(Current->GetName());
		params.push_back(":"+reason);
		Utils->DoOneToAllButSender(Current->GetParent()->GetName(),"SQUIT",params,Current->GetName());
		if (Current->GetParent() == Utils->TreeRoot)
		{
			ServerInstance->SNO->WriteToSnoMask('l', "Server \002"+Current->GetName()+"\002 split: "+reason);
			LocalSquit = true;
		}
		else
		{
			ServerInstance->SNO->WriteToSnoMask('L', "Server \002"+Current->GetName()+"\002 split from server \002"+Current->GetParent()->GetName()+"\002 with reason: "+reason);
		}
		num_lost_servers = 0;
		num_lost_users = 0;
		std::string from = Current->GetParent()->GetName()+" "+Current->GetName();
		SquitServer(from, Current);
		Current->Tidy();
		Current->GetParent()->DelChild(Current);
		delete Current;
		if (LocalSquit)
			ServerInstance->SNO->WriteToSnoMask('l', "Netsplit complete, lost \002%d\002 users on \002%d\002 servers.", num_lost_users, num_lost_servers);
		else
			ServerInstance->SNO->WriteToSnoMask('L', "Netsplit complete, lost \002%d\002 users on \002%d\002 servers.", num_lost_users, num_lost_servers);
	}
	else
		ServerInstance->Logs->Log("m_spanningtree",DEFAULT,"Squit from unknown server");
}

/** This function is called when we receive data from a remote
 * server.
 */
void TreeSocket::OnDataReady()
{
	Utils->Creator->loopCall = true;
	/* While there is at least one new line in the buffer,
	 * do something useful (we hope!) with it.
	 */
	while (recvq.find("\n") != std::string::npos)
	{
		std::string ret = recvq.substr(0,recvq.find("\n")-1);
		recvq = recvq.substr(recvq.find("\n")+1,recvq.length()-recvq.find("\n"));
		/* Use rfind here not find, as theres more
		 * chance of the \r being near the end of the
		 * string, not the start.
		 */
		if (ret.find("\r") != std::string::npos)
			ret = recvq.substr(0,recvq.find("\r")-1);
		/* Process this one, abort if it
		 * didnt return true.
		 */
		if (!this->ProcessLine(ret))
		{
			SetError("ProcessLine returned false");
			break;
		}
	}
	Utils->Creator->loopCall = false;
}
