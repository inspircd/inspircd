/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2008 Robin Burchell <robin+git@viroteck.net>
 *
 * This file is part of InspIRCd.  InspIRCd is free software: you can
 * redistribute it and/or modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation, version 2.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */


/* $Core */

#include "inspircd.h"
#include "socket.h"
#include "socketengine.h"

/* Private static member data must be declared in this manner */
irc::sockets::sockaddrs ListenSocketBase::client;
irc::sockets::sockaddrs ListenSocketBase::server;

ListenSocketBase::ListenSocketBase(InspIRCd* Instance, int port, const std::string &addr) : ServerInstance(Instance), desc("plaintext")
{
	irc::sockets::sockaddrs bind_to;

	bind_addr = addr;
	bind_port = port;

	// canonicalize address if it is defined
	if (!addr.empty() && irc::sockets::aptosa(addr.c_str(), port, &bind_to))
		irc::sockets::satoap(&bind_to, bind_addr, bind_port);

	this->SetFd(irc::sockets::OpenTCPSocket(bind_addr.c_str()));
	if (this->GetFd() > -1)
	{
		if (!Instance->BindSocket(this->fd,port,bind_addr.c_str()))
			this->fd = -1;
		Instance->SE->AddFd(this);
	}
}

ListenSocketBase::~ListenSocketBase()
{
	if (this->GetFd() > -1)
	{
		ServerInstance->SE->DelFd(this);
		ServerInstance->Logs->Log("SOCKET", DEBUG,"Shut down listener on fd %d", this->fd);
		if (ServerInstance->SE->Shutdown(this, 2) || ServerInstance->SE->Close(this))
			ServerInstance->Logs->Log("SOCKET", DEBUG,"Failed to cancel listener: %s", strerror(errno));
		this->fd = -1;
	}
}

/* Just seperated into another func for tidiness really.. */
void ListenSocketBase::AcceptInternal()
{
	ServerInstance->Logs->Log("SOCKET",DEBUG,"HandleEvent for Listensoket");
	int incomingSockfd;

	socklen_t length = sizeof(client);
	incomingSockfd = ServerInstance->SE->Accept(this, &client.sa, &length);

	if (incomingSockfd < 0)
	{
		ServerInstance->SE->Shutdown(incomingSockfd, 2);
		ServerInstance->SE->Close(incomingSockfd);
		ServerInstance->stats->statsRefused++;
		return;
	}

	socklen_t sz = sizeof(server);
	if (getsockname(incomingSockfd, &server.sa, &sz))
	{
		ServerInstance->Logs->Log("SOCKET", DEBUG, "Can't get peername: %s", strerror(errno));
		irc::sockets::aptosa(bind_addr.c_str(), bind_port, &server);
	}

	/*
	 * XXX -
	 * this is done as a safety check to keep the file descriptors within range of fd_ref_table.
	 * its a pretty big but for the moment valid assumption:
	 * file descriptors are handed out starting at 0, and are recycled as theyre freed.
	 * therefore if there is ever an fd over 65535, 65536 clients must be connected to the
	 * irc server at once (or the irc server otherwise initiating this many connections, files etc)
	 * which for the time being is a physical impossibility (even the largest networks dont have more
	 * than about 10,000 users on ONE server!)
	 */
	if (incomingSockfd >= ServerInstance->SE->GetMaxFds())
	{
		ServerInstance->Logs->Log("SOCKET", DEBUG, "Server is full");
		ServerInstance->SE->Shutdown(incomingSockfd, 2);
		ServerInstance->SE->Close(incomingSockfd);
		ServerInstance->stats->statsRefused++;
		return;
	}

	if (client.sa.sa_family == AF_INET6)
	{
		/*
		 * This case is the be all and end all patch to catch and nuke 4in6
		 * instead of special-casing shit all over the place and wreaking merry
		 * havoc with crap, instead, we just recreate sockaddr and strip ::ffff: prefix
		 * if it's a 4in6 IP.
		 *
		 * This is, of course, much improved over the older way of handling this
		 * (pretend it doesn't exist + hack around it -- yes, both were done!)
		 *
		 * Big, big thanks to danieldg for his work on this.
		 * -- w00t
		 */
		static const unsigned char prefix4in6[12] = { 0,0,0,0, 0,0,0,0, 0,0,0xFF,0xFF };
		if (!memcmp(prefix4in6, &client.in6.sin6_addr, 12))
		{
			// recreate as a sockaddr_in using the IPv4 IP
			uint16_t sport = client.in6.sin6_port;
			uint32_t addr = *reinterpret_cast<uint32_t*>(client.in6.sin6_addr.s6_addr + 12);
			client.in4.sin_family = AF_INET;
			client.in4.sin_port = sport;
			client.in4.sin_addr.s_addr = addr;

			sport = server.in6.sin6_port;
			addr = *reinterpret_cast<uint32_t*>(server.in6.sin6_addr.s6_addr + 12);
			server.in4.sin_family = AF_INET;
			server.in4.sin_port = sport;
			server.in4.sin_addr.s_addr = addr;
		}
	}

	std::string server_addr;
	std::string client_addr;
	int dummy_port;
	irc::sockets::satoap(&server, server_addr, dummy_port);
	irc::sockets::satoap(&client, client_addr, dummy_port);

	ServerInstance->SE->NonBlocking(incomingSockfd);
	ServerInstance->stats->statsAccept++;
	this->OnAcceptReady(server_addr, incomingSockfd, client_addr);
}

void ListenSocketBase::HandleEvent(EventType e, int err)
{
	switch (e)
	{
		case EVENT_ERROR:
			ServerInstance->Logs->Log("SOCKET",DEFAULT,"ListenSocket::HandleEvent() received a socket engine error event! well shit! '%s'", strerror(err));
			break;
		case EVENT_WRITE:
			ServerInstance->Logs->Log("SOCKET",DEBUG,"*** BUG *** ListenSocket::HandleEvent() got a WRITE event!!!");
			break;
		case EVENT_READ:
			this->AcceptInternal();
			break;
	}
}

void ClientListenSocket::OnAcceptReady(const std::string &ipconnectedto, int nfd, const std::string &incomingip)
{
	ServerInstance->Users->AddUser(ServerInstance, nfd, bind_port, false, &client.sa, ipconnectedto);
}
