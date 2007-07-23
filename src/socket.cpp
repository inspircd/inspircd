/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2007 InspIRCd Development Team
 * See: http://www.inspircd.org/wiki/index.php/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"
#include <string>
#include "configreader.h"
#include "socket.h"
#include "socketengine.h"
#include "wildcard.h"

using namespace irc::sockets;

/* Used when comparing CIDR masks for the modulus bits left over.
 * A lot of ircd's seem to do this:
 * ((-1) << (8 - (mask % 8)))
 * But imho, it sucks in comparison to a nice neat lookup table.
 */
const unsigned char inverted_bits[8] = {	0x00, /* 00000000 - 0 bits - never actually used */
				0x80, /* 10000000 - 1 bits */
				0xC0, /* 11000000 - 2 bits */
				0xE0, /* 11100000 - 3 bits */
				0xF0, /* 11110000 - 4 bits */
				0xF8, /* 11111000 - 5 bits */
				0xFC, /* 11111100 - 6 bits */
				0xFE  /* 11111110 - 7 bits */
};


ListenSocket::ListenSocket(InspIRCd* Instance, int port, char* addr) : ServerInstance(Instance), desc("plaintext"), bind_addr(addr), bind_port(port)
{
	this->SetFd(OpenTCPSocket(addr));
	if (this->GetFd() > -1)
	{
		if (!Instance->BindSocket(this->fd,port,addr))
			this->fd = -1;
#ifdef IPV6
		if ((!*addr) || (strchr(addr,':')))
			this->family = AF_INET6;
		else
#endif
		this->family = AF_INET;
		Instance->SE->AddFd(this);
	}
}

ListenSocket::~ListenSocket()
{
	if (this->GetFd() > -1)
	{
		ServerInstance->SE->DelFd(this);
		ServerInstance->Log(DEBUG,"Shut down listener on fd %d", this->fd);
		if (shutdown(this->fd, 2) || close(this->fd))
			ServerInstance->Log(DEBUG,"Failed to cancel listener: %s", strerror(errno));
		this->fd = -1;
	}
}

void ListenSocket::HandleEvent(EventType et, int errornum)
{
	sockaddr* sock_us = new sockaddr[2];	// our port number
	sockaddr* client = new sockaddr[2];
	socklen_t uslen, length;		// length of our port number
	int incomingSockfd, in_port;

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

	incomingSockfd = _accept (this->GetFd(), (sockaddr*)client, &length);

	if ((incomingSockfd > -1) && (!_getsockname(incomingSockfd, sock_us, &uslen)))
	{
		char buf[MAXBUF];
#ifdef IPV6
		if (this->family == AF_INET6)
		{
			inet_ntop(AF_INET6, &((const sockaddr_in6*)client)->sin6_addr, buf, sizeof(buf));
			in_port = ntohs(((sockaddr_in6*)sock_us)->sin6_port);
		}
		else
#endif
		{
			inet_ntop(AF_INET, &((const sockaddr_in*)client)->sin_addr, buf, sizeof(buf));
			in_port = ntohs(((sockaddr_in*)sock_us)->sin_port);
		}

		NonBlocking(incomingSockfd);
		if (ServerInstance->Config->GetIOHook(in_port))
		{
			try
			{
				ServerInstance->Config->GetIOHook(in_port)->OnRawSocketAccept(incomingSockfd, buf, in_port);
			}
			catch (CoreException& modexcept)
			{
				ServerInstance->Log(DEBUG,"%s threw an exception: %s", modexcept.GetSource(), modexcept.GetReason());
			}
		}
		ServerInstance->stats->statsAccept++;
		userrec::AddClient(ServerInstance, incomingSockfd, in_port, false, this->family, client);
	}
	else
	{
		shutdown(incomingSockfd,2);
		close(incomingSockfd);
		ServerInstance->stats->statsRefused++;
	}
	delete[] client;
	delete[] sock_us;
}

/* Match raw bytes using CIDR bit matching, used by higher level MatchCIDR() */
bool irc::sockets::MatchCIDRBits(unsigned char* address, unsigned char* mask, unsigned int mask_bits)
{
	unsigned int modulus = mask_bits % 8; /* Number of whole bytes in the mask */
	unsigned int divisor = mask_bits / 8; /* Remaining bits in the mask after whole bytes are dealt with */

	/* First compare the whole bytes, if they dont match, return false */
	if (memcmp(address, mask, divisor))
		return false;

	/* Now if there are any remainder bits, we compare them with logic AND */
	if (modulus)
		if ((address[divisor] & inverted_bits[modulus]) != (mask[divisor] & inverted_bits[modulus]))
			/* If they dont match, return false */
			return false;

	/* The address matches the mask, to mask_bits bits of mask */
	return true;
}

/* Match CIDR, but dont attempt to match() against leading *!*@ sections */
bool irc::sockets::MatchCIDR(const char* address, const char* cidr_mask)
{
	return MatchCIDR(address, cidr_mask, false);
}

/* Match CIDR strings, e.g. 127.0.0.1 to 127.0.0.0/8 or 3ffe:1:5:6::8 to 3ffe:1::0/32
 * If you have a lot of hosts to match, youre probably better off building your mask once
 * and then using the lower level MatchCIDRBits directly.
 *
 * This will also attempt to match any leading usernames or nicknames on the mask, using
 * match(), when match_with_username is true.
 */
bool irc::sockets::MatchCIDR(const char* address, const char* cidr_mask, bool match_with_username)
{
	unsigned char addr_raw[16];
	unsigned char mask_raw[16];
	unsigned int bits = 0;
	char* mask = NULL;

	/* The caller is trying to match ident@<mask>/bits.
	 * Chop off the ident@ portion, use match() on it
	 * seperately.
	 */
	if (match_with_username)
	{
		/* Duplicate the strings, and try to find the position
		 * of the @ symbol in each */
		char* address_dupe = strdup(address);
		char* cidr_dupe = strdup(cidr_mask);
	
		/* Use strchr not strrchr, because its going to be nearer to the left */
		char* username_mask_pos = strrchr(cidr_dupe, '@');
		char* username_addr_pos = strrchr(address_dupe, '@');

		/* Both strings have an @ symbol in them */
		if (username_mask_pos && username_addr_pos)
		{
			/* Zero out the location of the @ symbol */
			*username_mask_pos = *username_addr_pos = 0;

			/* Try and match() the strings before the @
			 * symbols, and recursively call MatchCIDR without
			 * username matching enabled to match the host part.
			 */
			bool result = (match(address_dupe, cidr_dupe) && MatchCIDR(username_addr_pos + 1, username_mask_pos + 1, false));

			/* Free the stuff we created */
			free(address_dupe);
			free(cidr_dupe);

			/* Return a result */
			return result;
		}
		else
		{
			/* One or both didnt have an @ in,
			 * just match as CIDR
			 */
			free(address_dupe);
			free(cidr_dupe);
			mask = strdup(cidr_mask);
		}
	}
	else
	{
		/* Make a copy of the cidr mask string,
		 * we're going to change it
		 */
		mask = strdup(cidr_mask);
	}

	in_addr  address_in4;
	in_addr  mask_in4;


	/* Use strrchr for this, its nearer to the right */
	char* bits_chars = strrchr(mask,'/');

	if (bits_chars)
	{
		bits = atoi(bits_chars + 1);
		*bits_chars = 0;
	}
	else
	{
		/* No 'number of bits' field! */
		free(mask);
		return false;
	}

#ifdef SUPPORT_IP6LINKS
	in6_addr address_in6;
	in6_addr mask_in6;

	if (inet_pton(AF_INET6, address, &address_in6) > 0)
	{
		if (inet_pton(AF_INET6, mask, &mask_in6) > 0)
		{
			memcpy(&addr_raw, &address_in6.s6_addr, 16);
			memcpy(&mask_raw, &mask_in6.s6_addr, 16);

			if (bits > 128)
				bits = 128;
		}
		else
		{
			/* The address was valid ipv6, but the mask
			 * that goes with it wasnt.
			 */
			free(mask);
			return false;
		}
	}
	else
#endif
	if (inet_pton(AF_INET, address, &address_in4) > 0)
	{
		if (inet_pton(AF_INET, mask, &mask_in4) > 0)
		{
			memcpy(&addr_raw, &address_in4.s_addr, 4);
			memcpy(&mask_raw, &mask_in4.s_addr, 4);

			if (bits > 32)
				bits = 32;
		}
		else
		{
			/* The address was valid ipv4,
			 * but the mask that went with it wasnt.
			 */
			free(mask);
			return false;
		}
	}
	else
	{
		/* The address was neither ipv4 or ipv6 */
		free(mask);
		return false;
	}

	/* Low-level-match the bits in the raw data */
	free(mask);
	return MatchCIDRBits(addr_raw, mask_raw, bits);
}

void irc::sockets::Blocking(int s)
{
#ifndef WIN32
	int flags = fcntl(s, F_GETFL, 0);
	fcntl(s, F_SETFL, flags ^ O_NONBLOCK);
#else
	unsigned long opt = 0;
	ioctlsocket(s, FIONBIO, &opt);
#endif
}

void irc::sockets::NonBlocking(int s)
{
#ifndef WIN32
	int flags = fcntl(s, F_GETFL, 0);
	fcntl(s, F_SETFL, flags | O_NONBLOCK);
#else
	unsigned long opt = 1;
	ioctlsocket(s, FIONBIO, &opt);
#endif
}

/** This will bind a socket to a port. It works for UDP/TCP.
 * It can only bind to IP addresses, if you wish to bind to hostnames
 * you should first resolve them using class 'Resolver'.
 */ 
bool InspIRCd::BindSocket(int sockfd, int port, char* addr, bool dolisten)
{
	/* We allocate 2 of these, because sockaddr_in6 is larger than sockaddr (ugh, hax) */
	sockaddr* server = new sockaddr[2];
	memset(server,0,sizeof(sockaddr)*2);

	int ret, size;

	if (*addr == '*')
		*addr = 0;

#ifdef IPV6
	if (*addr)
	{
		/* There is an address here. Is it ipv6? */
		if (strchr(addr,':'))
		{
			/* Yes it is */
			in6_addr addy;
			if (inet_pton(AF_INET6, addr, &addy) < 1)
			{
				delete[] server;
				return false;
			}

			((sockaddr_in6*)server)->sin6_family = AF_INET6;
			memcpy(&(((sockaddr_in6*)server)->sin6_addr), &addy, sizeof(in6_addr));
			((sockaddr_in6*)server)->sin6_port = htons(port);
			size = sizeof(sockaddr_in6);
		}
		else
		{
			/* No, its not */
			in_addr addy;
			if (inet_pton(AF_INET, addr, &addy) < 1)
			{
				delete[] server;
				return false;
			}

			((sockaddr_in*)server)->sin_family = AF_INET;
			((sockaddr_in*)server)->sin_addr = addy;
			((sockaddr_in*)server)->sin_port = htons(port);
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
			((sockaddr_in*)server)->sin_family = AF_INET;
			((sockaddr_in*)server)->sin_addr.s_addr = htonl(INADDR_ANY);
			((sockaddr_in*)server)->sin_port = 0;
			size = sizeof(sockaddr_in);
		}
		else
		{
			/* Theres no address here, default to ipv6 bind to all */
			((sockaddr_in6*)server)->sin6_family = AF_INET6;
			memset(&(((sockaddr_in6*)server)->sin6_addr), 0, sizeof(in6_addr));
			((sockaddr_in6*)server)->sin6_port = htons(port);
			size = sizeof(sockaddr_in6);
		}
	}
#else
	/* If we aren't built with ipv6, the choice becomes simple */
	((sockaddr_in*)server)->sin_family = AF_INET;
	if (*addr)
	{
		/* There is an address here. */
		in_addr addy;
		if (inet_pton(AF_INET, addr, &addy) < 1)
		{
			delete[] server;
			return false;
		}
		((sockaddr_in*)server)->sin_addr = addy;
	}
	else
	{
		/* Bind ipv4 to all */
		((sockaddr_in*)server)->sin_addr.s_addr = htonl(INADDR_ANY);
	}
	/* Bind ipv4 port number */
	((sockaddr_in*)server)->sin_port = htons(port);
	size = sizeof(sockaddr_in);
#endif
	ret = bind(sockfd, server, size);
	delete[] server;

	if (ret < 0)
	{
		return false;
	}
	else
	{
		if (dolisten)
		{
			if (listen(sockfd, Config->MaxConn) == -1)
			{
				this->Log(DEFAULT,"ERROR in listen(): %s",strerror(errno));
				return false;
			}
			else
			{
				this->Log(DEBUG,"New socket binding for %d with listen: %s:%d", sockfd, addr, port);
				NonBlocking(sockfd);
				return true;
			}
		}
		else
		{
			this->Log(DEBUG,"New socket binding for %d without listen: %s:%d", sockfd, addr, port);
			return true;
		}
	}
}

// Open a TCP Socket
int irc::sockets::OpenTCPSocket(char* addr, int socktype)
{
	int sockfd;
	int on = 1;
	struct linger linger = { 0 };
#ifdef IPV6
	if (strchr(addr,':') || (!*addr))
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

int InspIRCd::BindPorts(bool bail, int &ports_found, FailedPortList &failed_ports)
{
	char configToken[MAXBUF], Addr[MAXBUF], Type[MAXBUF];
	int bound = 0;
	bool started_with_nothing = (Config->ports.size() == 0);
	std::vector<std::pair<std::string, int> > old_ports;

	/* XXX: Make a copy of the old ip/port pairs here */
	for (std::vector<ListenSocket*>::iterator o = Config->ports.begin(); o != Config->ports.end(); ++o)
		old_ports.push_back(make_pair((*o)->GetIP(), (*o)->GetPort()));

	for (int count = 0; count < Config->ConfValueEnum(Config->config_data, "bind"); count++)
	{
		Config->ConfValue(Config->config_data, "bind", "port", count, configToken, MAXBUF);
		Config->ConfValue(Config->config_data, "bind", "address", count, Addr, MAXBUF);
		Config->ConfValue(Config->config_data, "bind", "type", count, Type, MAXBUF);

		if ((!*Type) || (!strcmp(Type,"clients")))
		{
			irc::portparser portrange(configToken, false);
			int portno = -1;
			while ((portno = portrange.GetToken()))
			{
				if (*Addr == '*')
					*Addr = 0;

				bool skip = false;
				for (std::vector<ListenSocket*>::iterator n = Config->ports.begin(); n != Config->ports.end(); ++n)
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
					ListenSocket* ll = new ListenSocket(this, portno, Addr);
					if (ll->GetFd() > -1)
					{
						bound++;
						Config->ports.push_back(ll);
					}
					else
					{
						failed_ports.push_back(std::make_pair(Addr, portno));
					}
					ports_found++;
				}
			}
		}
	}

	/* XXX: Here, anything left in our copy list, close as removed */
	if (!started_with_nothing)
	{
		for (size_t k = 0; k < old_ports.size(); ++k)
		{
			for (std::vector<ListenSocket*>::iterator n = Config->ports.begin(); n != Config->ports.end(); ++n)
			{
				if (((*n)->GetIP() == old_ports[k].first) && ((*n)->GetPort() == old_ports[k].second))
				{
					this->Log(DEFAULT,"Port binding %s:%d was removed from the config file, closing.", old_ports[k].first.c_str(), old_ports[k].second);
					delete *n;
					Config->ports.erase(n);
					break;
				}
			}
		}
	}

	return bound;
}

const char* irc::sockets::insp_ntoa(insp_inaddr n)
{
	static char buf[1024];
	inet_ntop(AF_FAMILY, &n, buf, sizeof(buf));
	return buf;
}

int irc::sockets::insp_aton(const char* a, insp_inaddr* n)
{
	return inet_pton(AF_FAMILY, a, n);
}

