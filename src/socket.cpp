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
using irc::sockets::sockaddrs;

/** This will bind a socket to a port. It works for UDP/TCP.
 * It can only bind to IP addresses, if you wish to bind to hostnames
 * you should first resolve them using class 'Resolver'.
 */
bool InspIRCd::BindSocket(int sockfd, int port, const char* addr, bool dolisten)
{
	sockaddrs servaddr;
	memset(&servaddr, 0, sizeof(servaddr));

	int ret, size;

	if (*addr == '*')
		addr = "";

#ifdef IPV6
	if (*addr)
	{
		/* There is an address here. Is it ipv6? */
		if (strchr(addr,':'))
		{
			if (inet_pton(AF_INET6, addr, &servaddr.in6.sin6_addr) < 1)
			{
				return false;
			}
			servaddr.in6.sin6_family = AF_INET6;
			servaddr.in6.sin6_port = htons(port);
			size = sizeof(sockaddr_in6);
		}
		else
		{
			if (inet_pton(AF_INET, addr, &servaddr.in4.sin_addr) < 1)
			{
				return false;
			}
			servaddr.in4.sin_family = AF_INET;
			servaddr.in4.sin_port = htons(port);
			size = sizeof(sockaddr_in);
		}
	}
	else
	{
		if (port == -1)
		{
			/* Port -1: Means UDP IPV4 port binding - Special case
			 * used by DNS engine.
			 */
			servaddr.in4.sin_family = AF_INET;
			servaddr.in4.sin_addr.s_addr = htonl(INADDR_ANY);
			servaddr.in4.sin_port = 0;
			size = sizeof(sockaddr_in);
		}
		else
		{
			/* There's no address here, default to ipv6 bind to all */
			servaddr.in6.sin6_family = AF_INET6;
			servaddr.in6.sin6_port = htons(port);
			size = sizeof(sockaddr_in6);
		}
	}
#else
	/* If we aren't built with ipv6, the choice becomes simple */
	servaddr.in4.sin_family = AF_INET;
	if (*addr)
	{
		/* There is an address here. */
		if (inet_pton(AF_INET, addr, &servaddr.in4.sin_addr) < 1)
		{
			return false;
		}
	}
	else
	{
		/* Bind ipv4 to all */
		servaddr.in4.sin_addr.s_addr = htonl(INADDR_ANY);
	}
	/* Bind ipv4 port number */
	servaddr.in4.sin_port = htons(port);
	size = sizeof(sockaddr_in);
#endif
	ret = SE->Bind(sockfd, &servaddr.sa, size);

	if (ret < 0)
	{
		return false;
	}
	else
	{
		if (dolisten)
		{
			if (SE->Listen(sockfd, Config->MaxConn) == -1)
			{
				this->Logs->Log("SOCKET",DEFAULT,"ERROR in listen(): %s",strerror(errno));
				return false;
			}
			else
			{
				this->Logs->Log("SOCKET",DEBUG,"New socket binding for %d with listen: %s:%d", sockfd, addr, port);
				SE->NonBlocking(sockfd);
				return true;
			}
		}
		else
		{
			this->Logs->Log("SOCKET",DEBUG,"New socket binding for %d without listen: %s:%d", sockfd, addr, port);
			return true;
		}
	}
}

// Open a TCP Socket
int irc::sockets::OpenTCPSocket(const char* addr, int socktype)
{
	int sockfd;
	int on = 1;
	addr = addr;
	struct linger linger = { 0, 0 };
#ifdef IPV6
	if (!*addr)
	{
		sockfd = socket (PF_INET6, socktype, 0);
		if (sockfd < 0)
			sockfd = socket (PF_INET, socktype, 0);
	}
	else if (strchr(addr,':'))
		sockfd = socket (PF_INET6, socktype, 0);
	else
		sockfd = socket (PF_INET, socktype, 0);
	if (sockfd < 0)
#else
	if ((sockfd = socket (PF_INET, socktype, 0)) < 0)
#endif
	{
		return ERROR;
	}
	else
	{
		setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (char*)&on, sizeof(on));
		/* This is BSD compatible, setting l_onoff to 0 is *NOT* http://web.irc.org/mla/ircd-dev/msg02259.html */
		linger.l_onoff = 1;
		linger.l_linger = 1;
		setsockopt(sockfd, SOL_SOCKET, SO_LINGER, (char*)&linger,sizeof(linger));
		return (sockfd);
	}
}

// XXX: it would be VERY nice to genericize this so all listen stuff (server/client) could use the one function. -- w00t
int InspIRCd::BindPorts(FailedPortList &failed_ports)
{
	char configToken[MAXBUF], Addr[MAXBUF], Type[MAXBUF];
	int bound = 0;
	bool started_with_nothing = (ports.size() == 0);
	std::vector<std::pair<std::string, int> > old_ports;

	/* XXX: Make a copy of the old ip/port pairs here */
	for (std::vector<ListenSocketBase *>::iterator o = ports.begin(); o != ports.end(); ++o)
		old_ports.push_back(make_pair((*o)->GetIP(), (*o)->GetPort()));

	for (int count = 0; count < Config->ConfValueEnum("bind"); count++)
	{
		Config->ConfValue("bind", "port", count, configToken, MAXBUF);
		Config->ConfValue("bind", "address", count, Addr, MAXBUF);
		Config->ConfValue("bind", "type", count, Type, MAXBUF);

		if (strncmp(Addr, "::ffff:", 7) == 0)
			this->Logs->Log("SOCKET",DEFAULT, "Using 4in6 (::ffff:) isn't recommended. You should bind IPv4 addresses directly instead.");

		if ((!*Type) || (!strcmp(Type,"clients")))
		{
			irc::portparser portrange(configToken, false);
			int portno = -1;
			while (0 != (portno = portrange.GetToken()))
			{
				if (*Addr == '*')
					*Addr = 0;

				bool skip = false;
				for (std::vector<ListenSocketBase *>::iterator n = ports.begin(); n != ports.end(); ++n)
				{
					if (((*n)->GetIP() == Addr) && ((*n)->GetPort() == portno))
					{
						skip = true;
						/* XXX: Here, erase from our copy of the list */
						for (std::vector<std::pair<std::string, int> >::iterator k = old_ports.begin(); k != old_ports.end(); ++k)
						{
							if ((k->first == Addr) && (k->second == portno))
							{
								old_ports.erase(k);
								break;
							}
						}
					}
				}
				if (!skip)
				{
					ClientListenSocket *ll = new ClientListenSocket(this, portno, Addr);
					if (ll->GetFd() > -1)
					{
						bound++;
						ports.push_back(ll);
					}
					else
					{
						failed_ports.push_back(std::make_pair((*Addr ? Addr : "*") + std::string(":") + ConvToStr(portno), strerror(errno)));
					}
				}
			}
		}
	}

	/* XXX: Here, anything left in our copy list, close as removed */
	if (!started_with_nothing)
	{
		for (size_t k = 0; k < old_ports.size(); ++k)
		{
			for (std::vector<ListenSocketBase *>::iterator n = ports.begin(); n != ports.end(); ++n)
			{
				if (((*n)->GetIP() == old_ports[k].first) && ((*n)->GetPort() == old_ports[k].second))
				{
					this->Logs->Log("SOCKET",DEFAULT,"Port binding %s:%d was removed from the config file, closing.", old_ports[k].first.c_str(), old_ports[k].second);
					delete *n;
					ports.erase(n);
					break;
				}
			}
		}
	}

	return bound;
}

int irc::sockets::aptosa(const char* addr, int port, irc::sockets::sockaddrs* sa)
{
	memset(sa, 0, sizeof(*sa));
	if (!addr || !*addr)
	{
#ifdef IPV6
		sa->in6.sin6_family = AF_INET6;
		sa->in6.sin6_port = htons(port);
#else
		sa->in4.sin_family = AF_INET;
		sa->in4.sin_port = htons(port);
#endif
		return true;
	}
	else if (inet_pton(AF_INET, addr, &sa->in4.sin_addr) > 0)
	{
		sa->in4.sin_family = AF_INET;
		sa->in4.sin_port = htons(port);
		return true;
	}
	else if (inet_pton(AF_INET6, addr, &sa->in6.sin6_addr) > 0)
	{
		sa->in6.sin6_family = AF_INET6;
		sa->in6.sin6_port = htons(port);
		return true;
	}
	return false;
}

int irc::sockets::satoap(const irc::sockets::sockaddrs* sa, std::string& addr, int &port) {
	char addrv[INET6_ADDRSTRLEN+1];
	if (sa->sa.sa_family == AF_INET)
	{
		if (!inet_ntop(AF_INET, &sa->in4.sin_addr, addrv, sizeof(addrv)))
			return false;
		addr = addrv;
		port = ntohs(sa->in4.sin_port);
		return true;
	}
	else if (sa->sa.sa_family == AF_INET6)
	{
		if (!inet_ntop(AF_INET6, &sa->in6.sin6_addr, addrv, sizeof(addrv)))
			return false;
		addr = addrv;
		port = ntohs(sa->in6.sin6_port);
		return true;
	}
	return false;
}

int irc::sockets::sa_size(irc::sockets::sockaddrs& sa)
{
	if (sa.sa.sa_family == AF_INET)
		return sizeof(sa.in4);
	if (sa.sa.sa_family == AF_INET6)
		return sizeof(sa.in6);
	return 0;
}
