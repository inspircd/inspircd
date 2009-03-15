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


/* Private static member data must be initialized in this manner */
unsigned int ListenSocketBase::socketcount = 0;
sockaddr* ListenSocketBase::sock_us = NULL;
sockaddr* ListenSocketBase::client = NULL;
sockaddr* ListenSocketBase::raddr = NULL;

ListenSocketBase::ListenSocketBase(InspIRCd* Instance, int port, const std::string &addr) : ServerInstance(Instance), desc("plaintext"), bind_addr(addr), bind_port(port)
{
	this->SetFd(irc::sockets::OpenTCPSocket(addr.c_str()));
	if (this->GetFd() > -1)
	{
		if (!Instance->BindSocket(this->fd,port,addr.c_str()))
			this->fd = -1;
#ifdef IPV6
		if ((!*addr.c_str()) || (strchr(addr.c_str(),':')))
			this->family = AF_INET6;
		else
#endif
		this->family = AF_INET;
		Instance->SE->AddFd(this);
	}
	/* Saves needless allocations */
	if (socketcount == 0)
	{
		/* All instances of ListenSocket share these, so reference count it */
		ServerInstance->Logs->Log("SOCKET", DEBUG,"Allocate sockaddr structures");
		sock_us = new sockaddr[2];
		client = new sockaddr[2];
		raddr = new sockaddr[2];
	}
	socketcount++;
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
	socketcount--;
	if (socketcount == 0)
	{
		delete[] sock_us;
		delete[] client;
		delete[] raddr;
	}
}

/* Just seperated into another func for tidiness really.. */
void ListenSocketBase::AcceptInternal()
{
	ServerInstance->Logs->Log("SOCKET",DEBUG,"HandleEvent for Listensoket");
	socklen_t uslen, length;		// length of our port number
	int incomingSockfd;

#ifdef IPV6
	if (this->family == AF_INET6)
	{
		uslen = sizeof(sockaddr_in6);
		length = sizeof(sockaddr_in6);
	}
	else
#endif
	{
		uslen = sizeof(sockaddr_in);
		length = sizeof(sockaddr_in);
	}

	incomingSockfd = ServerInstance->SE->Accept(this, (sockaddr*)client, &length);

	if (incomingSockfd < 0 ||
		  ServerInstance->SE->GetSockName(this, sock_us, &uslen) == -1)
	{
		ServerInstance->SE->Shutdown(incomingSockfd, 2);
		ServerInstance->SE->Close(incomingSockfd);
		ServerInstance->stats->statsRefused++;
		return;
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

	static char buf[MAXBUF];
	static char target[MAXBUF];

	*target = *buf = '\0';

#ifdef IPV6
	if (this->family == AF_INET6)
	{
		inet_ntop(AF_INET6, &((const sockaddr_in6*)client)->sin6_addr, buf, sizeof(buf));
		socklen_t raddrsz = sizeof(sockaddr_in6);
		if (getsockname(incomingSockfd, (sockaddr*) raddr, &raddrsz) == 0)
			inet_ntop(AF_INET6, &((const sockaddr_in6*)raddr)->sin6_addr, target, sizeof(target));
		else
			ServerInstance->Logs->Log("SOCKET", DEBUG, "Can't get peername: %s", strerror(errno));

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
		static const unsigned char prefix4in6[12] = { 0,0,0,0,  0,0,0,0, 0,0,0xFF,0xFF };
		if (!memcmp(prefix4in6, &((const sockaddr_in6*)client)->sin6_addr, 12))
		{
			// strip leading ::ffff: from the IPs
			memmove(buf, buf+7, sizeof(buf)-7);
			memmove(target, target+7, sizeof(target)-7);

			// recreate as a sockaddr_in using the IPv4 IP
			uint16_t sport = ((const sockaddr_in6*)client)->sin6_port;
			struct sockaddr_in* clientv4 = (struct sockaddr_in*)client;
			clientv4->sin_family = AF_INET;
			clientv4->sin_port = sport;
			inet_pton(AF_INET, buf, &clientv4->sin_addr);
		}
	}
	else
#endif
	{
		inet_ntop(AF_INET, &((const sockaddr_in*)client)->sin_addr, buf, sizeof(buf));
		socklen_t raddrsz = sizeof(sockaddr_in);
		if (getsockname(incomingSockfd, (sockaddr*) raddr, &raddrsz) == 0)
			inet_ntop(AF_INET, &((const sockaddr_in*)raddr)->sin_addr, target, sizeof(target));
		else
			ServerInstance->Logs->Log("SOCKET", DEBUG, "Can't get peername: %s", strerror(errno));
	}

	ServerInstance->SE->NonBlocking(incomingSockfd);
	ServerInstance->stats->statsAccept++;
	this->OnAcceptReady(target, incomingSockfd, buf);
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
	ServerInstance->Users->AddUser(ServerInstance, nfd, bind_port, false, client, ipconnectedto);
}
