/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
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


#include "inspircd.h"
#include "socket.h"
#include "socketengine.h"

ListenSocket::ListenSocket(ConfigTag* tag, const irc::sockets::sockaddrs& bind_to)
	: bind_tag(tag)
{
	irc::sockets::satoap(bind_to, bind_addr, bind_port);
	bind_desc = irc::sockets::satouser(bind_to);

	fd = socket(bind_to.sa.sa_family, SOCK_STREAM, 0);

	if (this->fd == -1)
		return;

#ifdef IPV6_V6ONLY
	/* This OS supports IPv6 sockets that can also listen for IPv4
	 * connections. If our address is "*" or empty, enable both v4 and v6 to
	 * allow for simpler configuration on dual-stack hosts. Otherwise, if it
	 * is "::" or an IPv6 address, disable support so that an IPv4 bind will
	 * work on the port (by us or another application).
	 */
	if (bind_to.sa.sa_family == AF_INET6)
	{
		std::string addr = tag->getString("address");
		/* This must be >= sizeof(DWORD) on Windows */
		const int enable = (addr.empty() || addr == "*") ? 0 : 1;
		/* This must be before bind() */
		setsockopt(fd, IPPROTO_IPV6, IPV6_V6ONLY, reinterpret_cast<const char *>(&enable), sizeof(enable));
		// errors ignored intentionally
	}
#endif

	ServerInstance->SE->SetReuse(fd);
	int rv = ServerInstance->SE->Bind(this->fd, bind_to);
	if (rv >= 0)
		rv = ServerInstance->SE->Listen(this->fd, ServerInstance->Config->MaxConn);

	if (rv < 0)
	{
		int errstore = errno;
		ServerInstance->SE->Shutdown(this, 2);
		ServerInstance->SE->Close(this);
		this->fd = -1;
		errno = errstore;
	}
	else
	{
		ServerInstance->SE->NonBlocking(this->fd);
		ServerInstance->SE->AddFd(this, FD_WANT_POLL_READ | FD_WANT_NO_WRITE);
	}
}

ListenSocket::~ListenSocket()
{
	if (this->GetFd() > -1)
	{
		ServerInstance->SE->DelFd(this);
		ServerInstance->Logs->Log("SOCKET", DEBUG,"Shut down listener on fd %d", this->fd);
		ServerInstance->SE->Shutdown(this, 2);
		if (ServerInstance->SE->Close(this) != 0)
			ServerInstance->Logs->Log("SOCKET", DEBUG,"Failed to cancel listener: %s", strerror(errno));
		this->fd = -1;
	}
}

/* Just seperated into another func for tidiness really.. */
void ListenSocket::AcceptInternal()
{
	irc::sockets::sockaddrs client;
	irc::sockets::sockaddrs server;

	socklen_t length = sizeof(client);
	int incomingSockfd = ServerInstance->SE->Accept(this, &client.sa, &length);

	ServerInstance->Logs->Log("SOCKET",DEBUG,"HandleEvent for Listensocket %s nfd=%d", bind_desc.c_str(), incomingSockfd);
	if (incomingSockfd < 0)
	{
		ServerInstance->stats->statsRefused++;
		return;
	}

	socklen_t sz = sizeof(server);
	if (getsockname(incomingSockfd, &server.sa, &sz))
	{
		ServerInstance->Logs->Log("SOCKET", DEBUG, "Can't get peername: %s", strerror(errno));
		irc::sockets::aptosa(bind_addr, bind_port, server);
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
			client.in4.sin_family = AF_INET;
			client.in4.sin_port = sport;
			memcpy(&client.in4.sin_addr.s_addr, client.in6.sin6_addr.s6_addr + 12, sizeof(uint32_t));

			sport = server.in6.sin6_port;
			server.in4.sin_family = AF_INET;
			server.in4.sin_port = sport;
			memcpy(&server.in4.sin_addr.s_addr, server.in6.sin6_addr.s6_addr + 12, sizeof(uint32_t));
		}
	}

	ServerInstance->SE->NonBlocking(incomingSockfd);

	ModResult res;
	FIRST_MOD_RESULT(OnAcceptConnection, res, (incomingSockfd, this, &client, &server));
	if (res == MOD_RES_PASSTHRU)
	{
		std::string type = bind_tag->getString("type", "clients");
		if (type == "clients")
		{
			ServerInstance->Users->AddUser(incomingSockfd, this, &client, &server);
			res = MOD_RES_ALLOW;
		}
	}
	if (res == MOD_RES_ALLOW)
	{
		ServerInstance->stats->statsAccept++;
	}
	else
	{
		ServerInstance->stats->statsRefused++;
		ServerInstance->Logs->Log("SOCKET",DEFAULT,"Refusing connection on %s - %s",
			bind_desc.c_str(), res == MOD_RES_DENY ? "Connection refused by module" : "Module for this port not found");
		ServerInstance->SE->Close(incomingSockfd);
	}
}

void ListenSocket::HandleEvent(EventType e, int err)
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
