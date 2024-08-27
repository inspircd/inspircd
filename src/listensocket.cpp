/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2021 Dominic Hamon
 *   Copyright (C) 2019-2020 Matt Schatz <genius3000@g3k.solutions>
 *   Copyright (C) 2013-2016 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2013, 2016-2023 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2013 Daniel Vassdal <shutter@canternet.org>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2012 ChrisTX <xpipe@hotmail.de>
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
# include <netinet/tcp.h>
# include <sys/stat.h>
# include <unistd.h>
#endif

namespace
{
	// Removes a dead UNIX socket so we can bind over it.
	bool RemoveSocket(ListenSocket* ls)
	{
		const bool replace = ls->bind_tag->getBool("replace", true);
		if (!replace || !irc::sockets::isunix(ls->bind_sa.str()))
			return true;

		return unlink(ls->bind_sa.str().c_str()) != -1;
	}

	int SetDeferAccept(ListenSocket* ls)
	{
		// Default defer to on for TLS listeners because in TLS the client always speaks first.
		unsigned int timeoutdef = ls->bind_tag->getString("sslprofile").empty() ? 0 : 5;
		int timeout = static_cast<int>(ls->bind_tag->getDuration("defer", timeoutdef, 0, 60));
		if (!timeout)
			return 0;

#if defined TCP_DEFER_ACCEPT
		return SocketEngine::SetOption(ls, IPPROTO_TCP, TCP_DEFER_ACCEPT, timeout);
#elif defined SO_ACCEPTFILTER
		struct accept_filter_arg afa = { 0 };
		strcpy(afa.af_name, "dataready");
		return SocketEngine::SetOption(ls, SOL_SOCKET, SO_ACCEPTFILTER, afa);
#else
		return 0;
#endif
	}

	// Allows binding to an IP address which is not available yet.
	int SetFreeBind(ListenSocket* ls)
	{
#if defined IP_FREEBIND // Linux 2.4+
		return SocketEngine::SetOption<int>(ls, SOL_IP, IP_FREEBIND, 1);
#elif defined IP_BINDANY // FreeBSD
		return SocketEngine::SetOption<int>(ls, IPPROTO_IP, IP_BINDANY, 1);
#elif defined SO_BINDANY // NetBSD, OpenBSD
		return SocketEngine::SetOption<int>(ls, SOL_SOCKET, SO_BINDANY, 1);
#else
		return 0;
#endif
	}

	// Sets the filesystem permissions for a UNIX socket.
	int SetPermissions(ListenSocket* ls)
	{
		const std::string permissionstr = ls->bind_tag->getString("permissions");
		unsigned long permissions = strtoul(permissionstr.c_str(), nullptr, 8);
		if (!permissions || permissions > 07777)
			return 0;

		// This cast is safe thanks to the above check.
		return chmod(ls->bind_sa.str().c_str(), static_cast<int>(permissions));
	}

	// Allow binding on IPv4 with an IPv6 socket.
	void SetIPv6Only(ListenSocket* ls)
	{
#ifdef IPV6_V6ONLY
		/* This OS supports IPv6 sockets that can also listen for IPv4
		 * connections. If listening on all interfaces we enable both v4 and v6
		 * to allow for simpler configuration on dual-stack hosts. Otherwise,
		 * if it is "::" or an IPv6 address we disable support so that an IPv4
		 * bind will work on the same port (by us or another application).
		 */
		const std::string address = ls->bind_tag->getString("address");

		// IMPORTANT: This must be >= sizeof(DWORD) on Windows.
		const int enable = (address.empty() || address == "*") ? 0 : 1;

		// Intentionally ignore the result of this so we can fall back to default behaviour.
		SocketEngine::SetOption(ls, IPPROTO_IPV6, IPV6_V6ONLY, enable);
#endif
	}
}

ListenSocket::ListenSocket(const std::shared_ptr<ConfigTag>& tag, const irc::sockets::sockaddrs& bind_to, sa_family_t protocol)
	: bind_tag(tag)
	, bind_sa(bind_to)
	, bind_protocol(protocol)
{
	if (bind_to.family() == AF_UNIX && !RemoveSocket(this))
		return;

	SetFd(socket(bind_to.family(), SOCK_STREAM, protocol));
	if (!HasFd())
		return;

	// Its okay if these fails.
	if (bind_to.family() == AF_INET6)
		SetIPv6Only(this);
	SocketEngine::SetOption<int>(this, SOL_SOCKET, SO_REUSEADDR, 1);

	int rv = 0;
	if (bind_to.is_ip() && tag->getBool("free"))
		rv = SetFreeBind(this);

	if (rv != -1)
		rv = SocketEngine::Bind(this, bind_to);

	if (rv != -1)
		rv = SocketEngine::Listen(this, ServerInstance->Config->MaxConn);

	if (rv != -1)
	{
		if (bind_to.family() == AF_UNIX)
			rv = SetPermissions(this);

		else if (bind_to.is_ip() && protocol == IPPROTO_TCP)
			rv = SetDeferAccept(this);
	}

	if (rv == -1)
	{
#ifdef _WIN32
		int errstore = WSAGetLastError();
#else
		int errstore = errno;
#endif
		SocketEngine::Shutdown(this, 2);
		SocketEngine::Close(GetFd());
		SetFd(-1);
#ifdef _WIN32
		WSASetLastError(errstore);
#else
		errno = errstore;
#endif
	}
	else
	{
		SocketEngine::NonBlocking(GetFd());
		SocketEngine::AddFd(this, FD_WANT_POLL_READ | FD_WANT_NO_WRITE);

		this->ResetIOHookProvider();
	}
}

ListenSocket::~ListenSocket()
{
	if (this->HasFd())
	{
		ServerInstance->Logs.Debug("SOCKET", "Shut down listener on fd {}", GetFd());
		SocketEngine::Shutdown(this, 2);

		if (SocketEngine::Close(this) != 0)
			ServerInstance->Logs.Warning("SOCKET", "Failed to close listener: {}", strerror(errno));

		if (bind_sa.family() == AF_UNIX && unlink(bind_sa.un.sun_path))
			ServerInstance->Logs.Warning("SOCKET", "Failed to unlink UNIX socket: {}", strerror(errno));
	}
}

void ListenSocket::OnEventHandlerRead()
{
	irc::sockets::sockaddrs client(false);
	socklen_t length = sizeof(client);
	int incomingfd = SocketEngine::Accept(this, &client.sa, &length);
	if (incomingfd < 0)
	{
		ServerInstance->Logs.Debug("SOCKET", "Refused connection to {}: {}",
			bind_sa.str(), strerror(errno));
		ServerInstance->Stats.Refused++;
		return;
	}

	ServerInstance->Logs.Debug("SOCKET", "Accepted connection to {} with fd {}",
			bind_sa.str(), incomingfd);

	irc::sockets::sockaddrs server(bind_sa);
	length = sizeof(server);
	if (getsockname(incomingfd, &server.sa, &length))
	{
		ServerInstance->Logs.Debug("SOCKET", "Unable to get peer name for fd {}: {}",
			incomingfd, strerror(errno));
	}

	if (client.family() == AF_INET6)
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
		static constexpr unsigned char prefix4in6[12] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xFF, 0xFF };
		if (!memcmp(prefix4in6, &client.in6.sin6_addr, 12))
		{
			// recreate as a sockaddr_in using the IPv4 IP
			in_port_t sport = client.in6.sin6_port;
			client.in4.sin_family = AF_INET;
			client.in4.sin_port = sport;
			memcpy(&client.in4.sin_addr.s_addr, client.in6.sin6_addr.s6_addr + 12, sizeof(uint32_t));

			sport = server.in6.sin6_port;
			server.in4.sin_family = AF_INET;
			server.in4.sin_port = sport;
			memcpy(&server.in4.sin_addr.s_addr, server.in6.sin6_addr.s6_addr + 12, sizeof(uint32_t));
		}
	}
	else if (client.family() == AF_UNIX)
	{
		// Clients connecting via UNIX sockets don't have paths so give them
		// the server path as defined in RFC 1459 section 8.1.1.
		//
		// strcpy is safe here because sizeof(sockaddr_un.sun_path) is equal on both.
		strcpy(client.un.sun_path, server.un.sun_path);
	}

	SocketEngine::NonBlocking(incomingfd);

	ModResult res;
	FIRST_MOD_RESULT(OnAcceptConnection, res, (incomingfd, this, client, server));
	if (res == MOD_RES_ALLOW)
	{
		ServerInstance->Stats.Accept++;
		return;
	}

	ServerInstance->Stats.Refused++;
	ServerInstance->Logs.Normal("SOCKET", "Refusing connection on {} - {}", bind_sa.str(),
		res == MOD_RES_DENY ? "Connection refused by module" : "Module for this port not found");
	SocketEngine::Close(incomingfd);
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
			curr.ClearProvider();
	}

	std::string provname = bind_tag->getString("sslprofile");
	if (!provname.empty())
		provname.insert(0, "ssl/");

	// TLS should be the last
	iohookprovs.back().SetProvider(provname);
}
