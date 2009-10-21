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
	int ret;

	if ((*addr == '*' || *addr == '\0') && port == -1)
	{
		/* Port -1: Means UDP IPV4 port binding - Special case
		 * used by DNS engine.
		 */
		memset(&servaddr, 0, sizeof(servaddr));
		servaddr.in4.sin_family = AF_INET;
	}
	else if (!irc::sockets::aptosa(addr, port, &servaddr))
		return false;

	ret = SE->Bind(sockfd, &servaddr.sa, sa_size(servaddr));

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
int irc::sockets::OpenTCPSocket(const std::string& addr, int socktype)
{
	int sockfd;
	int on = 1;
	struct linger linger = { 0, 0 };
	if (addr.empty())
	{
#ifdef IPV6
		sockfd = socket (PF_INET6, socktype, 0);
		if (sockfd < 0)
#endif
			sockfd = socket (PF_INET, socktype, 0);
	}
	else if (addr.find(':') != std::string::npos)
		sockfd = socket (PF_INET6, socktype, 0);
	else
		sockfd = socket (PF_INET, socktype, 0);

	if (sockfd < 0)
	{
		return ERROR;
	}
	else
	{
		setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (char*)&on, sizeof(on));
		/* This is BSD compatible, setting l_onoff to 0 is *NOT* http://web.irc.org/mla/ircd-dev/msg02259.html */
		linger.l_onoff = 1;
		linger.l_linger = 1;
		setsockopt(sockfd, SOL_SOCKET, SO_LINGER, (char*)&linger, sizeof(linger));
		return (sockfd);
	}
}

int InspIRCd::BindPorts(FailedPortList &failed_ports)
{
	int bound = 0;
	std::vector<ListenSocket*> old_ports(ports.begin(), ports.end());

	ConfigTagList tags = ServerInstance->Config->ConfTags("bind");
	for(ConfigIter i = tags.first; i != tags.second; ++i)
	{
		ConfigTag* tag = i->second;
		std::string porttag = tag->getString("port");
		std::string Addr = tag->getString("address");

		if (strncmp(Addr.c_str(), "::ffff:", 7) == 0)
			this->Logs->Log("SOCKET",DEFAULT, "Using 4in6 (::ffff:) isn't recommended. You should bind IPv4 addresses directly instead.");

		irc::portparser portrange(porttag, false);
		int portno = -1;
		while (0 != (portno = portrange.GetToken()))
		{
			irc::sockets::sockaddrs bindspec;
			irc::sockets::aptosa(Addr, portno, &bindspec);
			std::string bind_readable = irc::sockets::satouser(&bindspec);

			bool skip = false;
			for (std::vector<ListenSocket*>::iterator n = old_ports.begin(); n != old_ports.end(); ++n)
			{
				if ((**n).bind_desc == bind_readable)
				{
					skip = true;
					old_ports.erase(n);
					break;
				}
			}
			if (!skip)
			{
				ListenSocket *ll = new ListenSocket(tag, Addr, portno);
				if (ll->GetFd() > -1)
				{
					bound++;
					ports.push_back(ll);
				}
				else
				{
					failed_ports.push_back(std::make_pair(bind_readable, strerror(errno)));
					delete ll;
				}
			}
		}
	}

	std::vector<ListenSocket*>::iterator n = ports.begin();
	for (std::vector<ListenSocket*>::iterator o = old_ports.begin(); o != old_ports.end(); ++o)
	{
		while (n != ports.end() && *n != *o)
			n++;
		if (n == ports.end())
		{
			this->Logs->Log("SOCKET",ERROR,"Port bindings slipped out of vector, aborting close!");
			break;
		}

		this->Logs->Log("SOCKET",DEFAULT, "Port binding %s was removed from the config file, closing.",
			(**n).bind_desc.c_str());
		delete *n;

		// this keeps the iterator valid, pointing to the next element
		n = ports.erase(n);
	}

	return bound;
}

bool irc::sockets::aptosa(const std::string& addr, int port, irc::sockets::sockaddrs* sa)
{
	memset(sa, 0, sizeof(*sa));
	if (addr.empty())
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
	else if (inet_pton(AF_INET, addr.c_str(), &sa->in4.sin_addr) > 0)
	{
		sa->in4.sin_family = AF_INET;
		sa->in4.sin_port = htons(port);
		return true;
	}
	else if (inet_pton(AF_INET6, addr.c_str(), &sa->in6.sin6_addr) > 0)
	{
		sa->in6.sin6_family = AF_INET6;
		sa->in6.sin6_port = htons(port);
		return true;
	}
	return false;
}

bool irc::sockets::satoap(const irc::sockets::sockaddrs* sa, std::string& addr, int &port) {
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

static const char all_zero[16] = {0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0 };

std::string irc::sockets::satouser(const irc::sockets::sockaddrs* sa) {
	char buffer[MAXBUF];
	if (sa->sa.sa_family == AF_INET)
	{
		if (sa->in4.sin_addr.s_addr == 0)
		{
			sprintf(buffer, "*:%u", ntohs(sa->in4.sin_port));
		}
		else
		{
			const uint8_t* bits = reinterpret_cast<const uint8_t*>(&sa->in4.sin_addr);
			sprintf(buffer, "%d.%d.%d.%d:%u", bits[0], bits[1], bits[2], bits[3], ntohs(sa->in4.sin_port));
		}
	}
	else if (sa->sa.sa_family == AF_INET6)
	{
		if (!memcmp(all_zero, &sa->in6.sin6_addr, 16))
		{
			sprintf(buffer, "*:%u", ntohs(sa->in6.sin6_port));
		}
		else
		{
			buffer[0] = '[';
			if (!inet_ntop(AF_INET6, &sa->in6.sin6_addr, buffer+1, MAXBUF - 10))
				return "<unknown>"; // should never happen, buffer is large enough
			int len = strlen(buffer);
			// no need for snprintf, buffer has at least 9 chars left, max short len = 5
			sprintf(buffer + len, "]:%u", ntohs(sa->in6.sin6_port));
		}
	}
	else
		return "<unknown>";
	return std::string(buffer);
}

int irc::sockets::sa_size(const irc::sockets::sockaddrs& sa)
{
	if (sa.sa.sa_family == AF_INET)
		return sizeof(sa.in4);
	if (sa.sa.sa_family == AF_INET6)
		return sizeof(sa.in6);
	return 0;
}
