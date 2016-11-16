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
#include "iohook.h"

#ifndef _WIN32
#include <netinet/tcp.h>
#endif

ListenSocket::ListenSocket(ConfigTag* tag, const irc::sockets::sockaddrs& bind_to)
	: bind_tag(tag)
{
	irc::sockets::satoap(bind_to, bind_addr, bind_port);
	bind_desc = bind_to.str();

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

	if (tag->getBool("free"))
	{
		socklen_t enable = 1;
#if defined IP_FREEBIND // Linux 2.4+
		setsockopt(fd, SOL_IP, IP_FREEBIND, &enable, sizeof(enable));
#elif defined IP_BINDANY // FreeBSD
		setsockopt(fd, IPPROTO_IP, IP_BINDANY, &enable, sizeof(enable));
#elif defined SO_BINDANY // NetBSD/OpenBSD
		setsockopt(fd, SOL_SOCKET, SO_BINDANY, &enable, sizeof(enable));
#else
		(void)enable;
#endif
	}

	SocketEngine::SetReuse(fd);
	int rv = SocketEngine::Bind(this->fd, bind_to);
	if (rv >= 0)
		rv = SocketEngine::Listen(this->fd, ServerInstance->Config->MaxConn);

	// Default defer to on for TLS listeners because in TLS the client always speaks first
	int timeout = tag->getInt("defer", (tag->getString("ssl").empty() ? 0 : 3));
	if (timeout && !rv)
	{
#if defined TCP_DEFER_ACCEPT
		setsockopt(fd, IPPROTO_TCP, TCP_DEFER_ACCEPT, &timeout, sizeof(timeout));
#elif defined SO_ACCEPTFILTER
		struct accept_filter_arg afa;
		memset(&afa, 0, sizeof(afa));
		strcpy(afa.af_name, "dataready");
		setsockopt(fd, SOL_SOCKET, SO_ACCEPTFILTER, &afa, sizeof(afa));
#endif
	}

	if (rv < 0)
	{
		int errstore = errno;
		SocketEngine::Shutdown(this, 2);
		SocketEngine::Close(this->GetFd());
		this->fd = -1;
		errno = errstore;
	}
	else
	{
		SocketEngine::NonBlocking(this->fd);
		SocketEngine::AddFd(this, FD_WANT_POLL_READ | FD_WANT_NO_WRITE);

		this->ResetIOHookProvider();
	}
}

ListenSocket::~ListenSocket()
{
	if (this->GetFd() > -1)
	{
		ServerInstance->Logs->Log("SOCKET", LOG_DEBUG, "Shut down listener on fd %d", this->fd);
		SocketEngine::Shutdown(this, 2);
		if (SocketEngine::Close(this) != 0)
			ServerInstance->Logs->Log("SOCKET", LOG_DEBUG, "Failed to cancel listener: %s", strerror(errno));
	}
}

void ListenSocket::OnEventHandlerRead()
{
	irc::sockets::sockaddrs client;
	irc::sockets::sockaddrs server;

	socklen_t length = sizeof(client);
	int incomingSockfd = SocketEngine::Accept(this, &client.sa, &length);

	ServerInstance->Logs->Log("SOCKET", LOG_DEBUG, "Accepting connection on socket %s fd %d", bind_desc.c_str(), incomingSockfd);
	if (incomingSockfd < 0)
	{
		ServerInstance->stats.Refused++;
		return;
	}

	socklen_t sz = sizeof(server);
	if (getsockname(incomingSockfd, &server.sa, &sz))
	{
		ServerInstance->Logs->Log("SOCKET", LOG_DEBUG, "Can't get peername: %s", strerror(errno));
		irc::sockets::aptosa(bind_addr, bind_port, server);
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

	SocketEngine::NonBlocking(incomingSockfd);

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
		ServerInstance->stats.Accept++;
	}
	else
	{
		ServerInstance->stats.Refused++;
		ServerInstance->Logs->Log("SOCKET", LOG_DEFAULT, "Refusing connection on %s - %s",
			bind_desc.c_str(), res == MOD_RES_DENY ? "Connection refused by module" : "Module for this port not found");
		SocketEngine::Close(incomingSockfd);
	}
}

void ListenSocket::ResetIOHookProvider()
{
	iohookprovs[0].SetProvider(bind_tag->getString("hook"));

	// Check that all non-last hooks support being in the middle
	for (IOHookProvList::iterator i = iohookprovs.begin(); i != iohookprovs.end()-1; ++i)
	{
		IOHookProvRef& curr = *i;
		// Ignore if cannot be in the middle
		if ((curr) && (!curr->IsMiddle()))
			curr.SetProvider(std::string());
	}

	std::string provname = bind_tag->getString("ssl");
	if (!provname.empty())
		provname.insert(0, "ssl/");

	// SSL should be the last
	iohookprovs.back().SetProvider(provname);
}
