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

#include <string>
#include "configreader.h"
#include "socket.h"
#include "inspircd.h"
#include "socketengine.h"
#include "wildcard.h"

using namespace irc::sockets;

/* Used when comparing CIDR masks for the modulus bits left over.
 * A lot of ircd's seem to do this:
 * ((-1) << (8 - (mask % 8)))
 * But imho, it sucks in comparison to a nice neat lookup table.
 */
const char inverted_bits[8] = {	0x00, /* 00000000 - 0 bits - never actually used */
				0x80, /* 10000000 - 1 bits */
				0xC0, /* 11000000 - 2 bits */
				0xE0, /* 11100000 - 3 bits */
				0xF0, /* 11110000 - 4 bits */
				0xF8, /* 11111000 - 5 bits */
				0xFC, /* 11111100 - 6 bits */
				0xFE  /* 11111110 - 7 bits */
};


ListenSocket::ListenSocket(InspIRCd* Instance, int sockfd, insp_sockaddr client, insp_sockaddr server, int port, char* addr) : ServerInstance(Instance), desc("plaintext")
{
	this->SetFd(sockfd);
	Instance->Log(DEBUG,"CRAP");
	if (!Instance->BindSocket(this->fd,client,server,port,addr))
		this->fd = -1;
}

ListenSocket::~ListenSocket()
{
	if (this->GetFd() > -1)
	{
		shutdown(this->fd, 2);
		close(this->fd);
		this->fd = -1;
	}
}

void ListenSocket::HandleEvent(EventType et, int errornum)
{
	insp_sockaddr sock_us;	// our port number
	socklen_t uslen;	// length of our port number
	insp_sockaddr client;
	socklen_t length;
	int incomingSockfd, in_port;
	uslen = sizeof(sock_us);
	length = sizeof(client);
	incomingSockfd = accept (this->GetFd(),(struct sockaddr*)&client, &length);
	
	if ((incomingSockfd > -1) && (!getsockname(incomingSockfd, (sockaddr*)&sock_us, &uslen)))
	{
#ifdef IPV6
		in_port = ntohs(sock_us.sin6_port);
#else
		in_port = ntohs(sock_us.sin_port);
#endif
		NonBlocking(incomingSockfd);
		if (ServerInstance->Config->GetIOHook(in_port))
		{
			try
			{
#ifdef IPV6
				ServerInstance->Config->GetIOHook(in_port)->OnRawSocketAccept(incomingSockfd, insp_ntoa(client.sin6_addr), in_port);
#else
				ServerInstance->Config->GetIOHook(in_port)->OnRawSocketAccept(incomingSockfd, insp_ntoa(client.sin_addr), in_port);
#endif
			}
			catch (CoreException& modexcept)
			{
				ServerInstance->Log(DEBUG,"%s threw an exception: %s", modexcept.GetSource(), modexcept.GetReason());
			}
		}
		ServerInstance->stats->statsAccept++;
#ifdef IPV6
		userrec::AddClient(ServerInstance, incomingSockfd, in_port, false, client.sin6_addr);
#else
		userrec::AddClient(ServerInstance, incomingSockfd, in_port, false, client.sin_addr);
#endif
	}
	else
	{
		shutdown(incomingSockfd,2);
		close(incomingSockfd);
		ServerInstance->stats->statsRefused++;
	}
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
	int flags = fcntl(s, F_GETFL, 0);
	fcntl(s, F_SETFL, flags ^ O_NONBLOCK);
}

void irc::sockets::NonBlocking(int s)
{
	int flags = fcntl(s, F_GETFL, 0);
	fcntl(s, F_SETFL, flags | O_NONBLOCK);
}

/** This will bind a socket to a port. It works for UDP/TCP.
 * It can only bind to IP addresses, if you wish to bind to hostnames
 * you should first resolve them using class 'Resolver'.
 */ 
bool InspIRCd::BindSocket(int sockfd, insp_sockaddr clientn, insp_sockaddr servern, int port, char* addr)
{
	sockaddr* server = new sockaddr();
	memset(server,0,sizeof(server));

	if (*addr == '*')
		*addr = 0;

	Log(DEBUG,"NEW CRAP HERE");

	/*if ((*addr) && (insp_aton(addr,&addy) < 1))
		return false;*/

#ifdef IPV6
	if (*addr)
	{
		printf("Address %s not empty\n", addr);
		/* There is an address here. Is it ipv6? */
		if (strchr(addr,':'))
		{
			printf("Address %s is ipv6\n", addr);
			/* Yes it is */
			in6_addr addy;
			inet_pton(AF_INET6, addr, &addy);

			((sockaddr_in6*)server)->sin6_family = AF_INET6;
			memcpy(&((sockaddr_in6*)server)->sin6_addr, &addy, sizeof(in6_addr));
			((sockaddr_in6*)server)->sin6_port = htons(port);
		}
		else
		{
			/* No, its not */
			printf("Address %s is ipv4\n", addr);
			in_addr addy;
			inet_pton(AF_INET, addr, &addy);

			((sockaddr_in*)server)->sin_family = AF_INET;
			((sockaddr_in*)server)->sin_addr = addy;
			((sockaddr_in*)server)->sin_port = htons(port);
		}
	}
	else
	{
		printf("Address empty\n");
		/* Theres no address here, default to ipv6 bind to all */
		memset(&((sockaddr_in6*)server)->sin6_addr, 0, sizeof(in6_addr));
		((sockaddr_in6*)server)->sin6_port = htons(port);
	}
#else
	/* If we aren't built with ipv6, the choice becomes simple */
	(sockaddr_in*)server->sin_family = AF_INET;
	if (*addr)
	{
		/* There is an address here. */
		in_addr addy;
		inet_pton(AF_INET, &addy, addr);

		((sockaddr_in*)server)->sin_addr = addy;
	}
	else
	{
		/* Bind ipv4 to all */
		((sockaddr_in*)server)->sin_addr.s_addr = htonl(INADDR_ANY);
	}
	/* Bind ipv4 port number */
	((sockaddr_in*)server)->sin_port = htons(port);
#endif
	int ret = bind(sockfd, server, sizeof(server));

	delete server;

	if (ret < 0)
	{
		return false;
	}
	else
	{
		if (listen(sockfd, Config->MaxConn) == -1)
		{
			this->Log(DEFAULT,"ERROR in listen(): %s",strerror(errno));
			return false;
		}
		else
		{
			NonBlocking(sockfd);
			return true;
		}
	}
}


// Open a TCP Socket
int irc::sockets::OpenTCPSocket()
{
	int sockfd;
	int on = 1;
	struct linger linger = { 0 };
  
	if ((sockfd = socket (AF_FAMILY, SOCK_STREAM, 0)) < 0)
	{
		printf("SOCKET FAIL: %s\n", strerror(errno));
		return ERROR;
	}
	else
	{
		setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
		/* This is BSD compatible, setting l_onoff to 0 is *NOT* http://web.irc.org/mla/ircd-dev/msg02259.html */
		linger.l_onoff = 1;
		linger.l_linger = 1;
		setsockopt(sockfd, SOL_SOCKET, SO_LINGER, &linger,sizeof(linger));
		return (sockfd);
	}
}

/* XXX: Probably belongs in class InspIRCd */
bool InspIRCd::HasPort(int port, char* addr)
{
	for (unsigned long count = 0; count < stats->BoundPortCount; count++)
	{
		if ((port == Config->ports[count]) && (!strcasecmp(Config->addrs[count],addr)))
		{
			return true;
		}
	}
	return false;
}

/* XXX: Probably belongs in class InspIRCd */
int InspIRCd::BindPorts(bool bail, int &ports_found, FailedPortList &failed_ports)
{
	Log(DEBUG,"BINDING PORTS");
	char configToken[MAXBUF], Addr[MAXBUF], Type[MAXBUF];
	insp_sockaddr client, server;
	int clientportcount = 0;
	int BoundPortCount = 0;

	ports_found = 0;

	int InitialPortCount = stats->BoundPortCount;

	for (int count = 0; count < Config->ConfValueEnum(Config->config_data, "bind"); count++)
	{
		Log(DEBUG,"FOUND PORT");
		Config->ConfValue(Config->config_data, "bind", "port", count, configToken, MAXBUF);
		Config->ConfValue(Config->config_data, "bind", "address", count, Addr, MAXBUF);
		Config->ConfValue(Config->config_data, "bind", "type", count, Type, MAXBUF);

		if ((!*Type) || (!strcmp(Type,"clients")))
		{
			irc::portparser portrange(configToken, false);
			long portno = -1;
			while ((portno = portrange.GetToken()))
			{
				if (!HasPort(portno, Addr))
				{
					ports_found++;
					Config->ports[clientportcount+InitialPortCount] = portno;
					if (*Addr == '*')
						*Addr = 0;

					strlcpy(Config->addrs[clientportcount+InitialPortCount],Addr,256);
					clientportcount++;
				}
			}
		}

		if (!bail)
		{
			int PortCount = clientportcount;
			if (PortCount)
			{
				BoundPortCount = stats->BoundPortCount;
				for (int count = InitialPortCount; count < InitialPortCount + PortCount; count++)
				{
					int fd = OpenTCPSocket();
					if (fd == ERROR)
					{
						failed_ports.push_back(std::make_pair(Config->addrs[count],Config->ports[count]));
						Log(DEBUG,"SOCKET FAIL");
					}
					else
					{
						Log(DEBUG,"BIND");
						Config->openSockfd[BoundPortCount] = new ListenSocket(this,fd,client,server,Config->ports[count],Config->addrs[count]);
						if (Config->openSockfd[BoundPortCount]->GetFd() > -1)
						{
							if (!SE->AddFd(Config->openSockfd[BoundPortCount]))
							{
								this->Log(DEFAULT,"ERK! Failed to add listening port to socket engine!");
								shutdown(Config->openSockfd[BoundPortCount]->GetFd(),2);
								close(Config->openSockfd[BoundPortCount]->GetFd());
								delete Config->openSockfd[BoundPortCount];
								failed_ports.push_back(std::make_pair(Config->addrs[count],Config->ports[count]));
							}
							else
								BoundPortCount++;
						}
					}
				}
				return InitialPortCount + BoundPortCount;
			}
			return InitialPortCount;
		}
	}

	int PortCount = clientportcount;

	for (int count = 0; count < PortCount; count++)
	{
		int fd = OpenTCPSocket();
		if (fd == ERROR)
		{
			Log(DEBUG,"SOCKET FAIL");
			failed_ports.push_back(std::make_pair(Config->addrs[count],Config->ports[count]));
		}
		else
		{
			Log(DEBUG,"BIND");
			Config->openSockfd[BoundPortCount] = new ListenSocket(this,fd,client,server,Config->ports[count],Config->addrs[count]);
			if (Config->openSockfd[BoundPortCount]->GetFd() > -1)
			{
				BoundPortCount++;
			}
			else
				failed_ports.push_back(std::make_pair(Config->addrs[count],Config->ports[count]));
		}
	}
	return BoundPortCount;
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

