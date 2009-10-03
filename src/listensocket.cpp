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

/* $Core */

#include "inspircd.h"
#include "socket.h"
#include "socketengine.h"

/* Private static member data must be declared in this manner */
irc::sockets::sockaddrs ListenSocketBase::client;
irc::sockets::sockaddrs ListenSocketBase::server;

ListenSocketBase::ListenSocketBase(int port, const std::string &addr) : desc("plaintext")
{
	irc::sockets::sockaddrs bind_to;

	// canonicalize address if it is defined
	if (!irc::sockets::aptosa(addr, port, &bind_to))
	{
		// malformed address
		bind_addr = addr;
		bind_port = port;
		bind_desc = addr + ":" + ConvToStr(port);
		this->fd = -1;
	}
	else
	{
		irc::sockets::satoap(&bind_to, bind_addr, bind_port);
		bind_desc = irc::sockets::satouser(&bind_to);

		this->fd = irc::sockets::OpenTCPSocket(bind_addr);
	}

	if (this->fd > -1)
	{
		int rv = ServerInstance->SE->Bind(this->fd, &bind_to.sa, sizeof(bind_to));
		if (rv >= 0)
			rv = ServerInstance->SE->Listen(this->fd, ServerInstance->Config->MaxConn);

		if (rv < 0)
		{
			ServerInstance->SE->Shutdown(this, 2);
			ServerInstance->SE->Close(this);
			this->fd = -1;
		}
		else
		{
			ServerInstance->SE->NonBlocking(this->fd);
			ServerInstance->SE->AddFd(this, FD_WANT_POLL_READ | FD_WANT_NO_WRITE);
		}
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
		ServerInstance->Logs->Log("SOCKET", DEBUG, "Can't get peername: %s", strerror(errno));

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

	ServerInstance->SE->NonBlocking(incomingSockfd);
	ServerInstance->stats->statsAccept++;
	this->OnAcceptReady(incomingSockfd);
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

void ClientListenSocket::OnAcceptReady(int nfd)
{
	ServerInstance->Users->AddUser(nfd, this, &client, &server);
}
