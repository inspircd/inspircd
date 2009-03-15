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
#include "commands/cmd_whois.h"
#include "commands/cmd_stats.h"
#include "socket.h"
#include "xline.h"
#include "transport.h"
#include "m_hash.h"
#include "socketengine.h"

#include "m_spanningtree/main.h"
#include "m_spanningtree/utils.h"
#include "m_spanningtree/treeserver.h"
#include "m_spanningtree/link.h"
#include "m_spanningtree/treesocket.h"
#include "m_spanningtree/resolvers.h"
#include "m_spanningtree/handshaketimer.h"

/* $ModDep: m_spanningtree/resolvers.h m_spanningtree/main.h m_spanningtree/utils.h m_spanningtree/treeserver.h m_spanningtree/link.h m_spanningtree/treesocket.h m_hash.h m_spanningtree/handshaketimer.h */


/** Because most of the I/O gubbins are encapsulated within
 * BufferedSocket, we just call the superclass constructor for
 * most of the action, and append a few of our own values
 * to it.
 */
TreeSocket::TreeSocket(SpanningTreeUtilities* Util, InspIRCd* SI, std::string shost, int iport, unsigned long maxtime, const std::string &ServerName, const std::string &bindto, Module* HookMod)
	: BufferedSocket(SI, shost, iport, maxtime, bindto), Utils(Util), Hook(HookMod)
{
	myhost = ServerName;
	theirchallenge.clear();
	ourchallenge.clear();
	this->LinkState = CONNECTING;
	Utils->timeoutlist[this] = std::pair<std::string, int>(ServerName, maxtime);
	if (Hook)
		BufferedSocketHookRequest(this, (Module*)Utils->Creator, Hook).Send();
}

/** When a listening socket gives us a new file descriptor,
 * we must associate it with a socket without creating a new
 * connection. This constructor is used for this purpose.
 */
TreeSocket::TreeSocket(SpanningTreeUtilities* Util, InspIRCd* SI, int newfd, char* ip, Module* HookMod)
	: BufferedSocket(SI, newfd, ip), Utils(Util), Hook(HookMod)
{
	this->LinkState = WAIT_AUTH_1;
	theirchallenge.clear();
	ourchallenge.clear();
	sentcapab = false;
	/* If we have a transport module hooked to the parent, hook the same module to this
	 * socket, and set a timer waiting for handshake before we send CAPAB etc.
	 */
	if (Hook)
		BufferedSocketHookRequest(this, (Module*)Utils->Creator, Hook).Send();

	ServerInstance->Timers->AddTimer(new HandshakeTimer(ServerInstance, this, &(Utils->LinkBlocks[0]), this->Utils, 1));

	/* Fix by Brain - inbound sockets need a timeout, too. 30 secs should be pleanty */
	Utils->timeoutlist[this] = std::pair<std::string, int>("<unknown>", 30);
}

ServerState TreeSocket::GetLinkState()
{
	return this->LinkState;
}

Module* TreeSocket::GetHook()
{
	return this->Hook;
}

TreeSocket::~TreeSocket()
{
	if (Hook)
		BufferedSocketUnhookRequest(this, (Module*)Utils->Creator, Hook).Send();
	Utils->timeoutlist.erase(this);
}

/** When an outbound connection finishes connecting, we receive
 * this event, and must send our SERVER string to the other
 * side. If the other side is happy, as outlined in the server
 * to server docs on the inspircd.org site, the other side
 * will then send back its own server string.
 */
bool TreeSocket::OnConnected()
{
	if (this->LinkState == CONNECTING)
	{
		/* we do not need to change state here. */
		for (std::vector<Link>::iterator x = Utils->LinkBlocks.begin(); x < Utils->LinkBlocks.end(); x++)
		{
			if (x->Name == this->myhost)
			{
				ServerInstance->SNO->WriteToSnoMask('l', "Connection to \2%s\2[%s] started.", myhost.c_str(), (x->HiddenFromStats ? "<hidden>" : this->GetIP().c_str()));
				if (Hook)
				{
					BufferedSocketHookRequest(this, (Module*)Utils->Creator, Hook).Send();
					ServerInstance->SNO->WriteToSnoMask('l', "Connection to \2%s\2[%s] using transport \2%s\2", myhost.c_str(), (x->HiddenFromStats ? "<hidden>" : this->GetIP().c_str()),
							x->Hook.c_str());
				}
				this->OutboundPass = x->SendPass;
				sentcapab = false;

				/* found who we're supposed to be connecting to, send the neccessary gubbins. */
				if (this->GetHook())
					ServerInstance->Timers->AddTimer(new HandshakeTimer(ServerInstance, this, &(*x), this->Utils, 1));
				else
					this->SendCapabilities();

				return true;
			}
		}
	}
	/* There is a (remote) chance that between the /CONNECT and the connection
	 * being accepted, some muppet has removed the <link> block and rehashed.
	 * If that happens the connection hangs here until it's closed. Unlikely
	 * and rather harmless.
	 */
	ServerInstance->SNO->WriteToSnoMask('l', "Connection to \2%s\2 lost link tag(!)", myhost.c_str());
	return true;
}

void TreeSocket::OnError(BufferedSocketError e)
{
	Link* MyLink;

	switch (e)
	{
		case I_ERR_CONNECT:
			ServerInstance->SNO->WriteToSnoMask('l', "Connection failed: Connection to \002%s\002 refused", myhost.c_str());
			MyLink = Utils->FindLink(myhost);
			if (MyLink)
				Utils->DoFailOver(MyLink);
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

int TreeSocket::OnDisconnect()
{
	/* For the same reason as above, we don't
	 * handle OnDisconnect()
	 */
	return true;
}

void TreeSocket::SendError(const std::string &errormessage)
{
	/* Display the error locally as well as sending it remotely */
	ServerInstance->SNO->WriteToSnoMask('l', "Sent \2ERROR\2 to %s: %s", (this->InboundServerName.empty() ? this->GetIP().c_str() : this->InboundServerName.c_str()), errormessage.c_str());
	this->WriteLine("ERROR :"+errormessage);
	/* One last attempt to make sure the error reaches its target */
	this->FlushWriteBuffer();
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

		std::deque<std::string> params;
		params.push_back(Current->GetName());
		params.push_back(":"+reason);
		Utils->DoOneToAllButSender(Current->GetParent()->GetName(),"SQUIT",params,Current->GetName());
		if (Current->GetParent() == Utils->TreeRoot)
		{
			this->ServerInstance->SNO->WriteToSnoMask('l', "Server \002"+Current->GetName()+"\002 split: "+reason);
			LocalSquit = true;
		}
		else
		{
			this->ServerInstance->SNO->WriteToSnoMask('L', "Server \002"+Current->GetName()+"\002 split from server \002"+Current->GetParent()->GetName()+"\002 with reason: "+reason);
		}
		num_lost_servers = 0;
		num_lost_users = 0;
		std::string from = Current->GetParent()->GetName()+" "+Current->GetName();
		SquitServer(from, Current);
		Current->Tidy();
		Current->GetParent()->DelChild(Current);
		delete Current;
		if (LocalSquit)
			this->ServerInstance->SNO->WriteToSnoMask('l', "Netsplit complete, lost \002%d\002 users on \002%d\002 servers.", num_lost_users, num_lost_servers);
		else
			this->ServerInstance->SNO->WriteToSnoMask('L', "Netsplit complete, lost \002%d\002 users on \002%d\002 servers.", num_lost_users, num_lost_servers);
	}
	else
		ServerInstance->Logs->Log("m_spanningtree",DEFAULT,"Squit from unknown server");
}

/** This function is called when we receive data from a remote
 * server. We buffer the data in a std::string (it doesnt stay
 * there for long), reading using BufferedSocket::Read() which can
 * read up to 16 kilobytes in one operation.
 *
 * IF THIS FUNCTION RETURNS FALSE, THE CORE CLOSES AND DELETES
 * THE SOCKET OBJECT FOR US.
 */
bool TreeSocket::OnDataReady()
{
	const char* data = this->Read();
	/* Check that the data read is a valid pointer and it has some content */
	if (data && *data)
	{
		this->in_buffer.append(data);
		/* While there is at least one new line in the buffer,
		 * do something useful (we hope!) with it.
		 */
		while (in_buffer.find("\n") != std::string::npos)
		{
			std::string ret = in_buffer.substr(0,in_buffer.find("\n")-1);
			in_buffer = in_buffer.substr(in_buffer.find("\n")+1,in_buffer.length()-in_buffer.find("\n"));
			/* Use rfind here not find, as theres more
			 * chance of the \r being near the end of the
			 * string, not the start.
			 */
			if (ret.find("\r") != std::string::npos)
				ret = in_buffer.substr(0,in_buffer.find("\r")-1);
			/* Process this one, abort if it
			 * didnt return true.
			 */
			if (!this->ProcessLine(ret))
			{
				return false;
			}
		}
		return true;
	}
	/* EAGAIN returns an empty but non-NULL string, so this
	 * evaluates to TRUE for EAGAIN but to FALSE for EOF.
	 */
	return (data && !*data);
}
