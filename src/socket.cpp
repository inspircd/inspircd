/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2008 InspIRCd Development Team
 * See: http://www.inspircd.org/wiki/index.php/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

/* $Core: libIRCDsocket */

#include "inspircd.h"
#include "socket.h"
#include "socketengine.h"

/* Private static member data must be initialized in this manner */
unsigned int ListenSocket::socketcount = 0;
sockaddr* ListenSocket::sock_us = NULL;
sockaddr* ListenSocket::client = NULL;
sockaddr* ListenSocket::raddr = NULL;

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

ListenSocket::~ListenSocket()
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
		{
			ServerInstance->Logs->Log("SOCKET",DEBUG,"HandleEvent for Listensoket");
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

			incomingSockfd = ServerInstance->SE->Accept(this, (sockaddr*)client, &length);

			if ((incomingSockfd > -1) && (!ServerInstance->SE->GetSockName(this, sock_us, &uslen)))
			{
				char buf[MAXBUF];
				char target[MAXBUF];	

				*target = *buf = '\0';

#ifdef IPV6
				if (this->family == AF_INET6)
				{
					in_port = ntohs(((sockaddr_in6*)sock_us)->sin6_port);
					inet_ntop(AF_INET6, &((const sockaddr_in6*)client)->sin6_addr, buf, sizeof(buf));
					socklen_t raddrsz = sizeof(sockaddr_in6);
					if (getpeername(incomingSockfd, (sockaddr*) raddr, &raddrsz) == 0)
						inet_ntop(AF_INET6, &((const sockaddr_in6*)raddr)->sin6_addr, target, sizeof(target));
					else
						ServerInstance->Logs->Log("SOCKET", DEBUG, "Can't get peername: %s", strerror(errno));
				}
				else
#endif
				{
					inet_ntop(AF_INET, &((const sockaddr_in*)client)->sin_addr, buf, sizeof(buf));
					in_port = ntohs(((sockaddr_in*)sock_us)->sin_port);
					socklen_t raddrsz = sizeof(sockaddr_in);
					if (getpeername(incomingSockfd, (sockaddr*) raddr, &raddrsz) == 0)
						inet_ntop(AF_INET, &((const sockaddr_in*)raddr)->sin_addr, target, sizeof(target));
					else
						ServerInstance->Logs->Log("SOCKET", DEBUG, "Can't get peername: %s", strerror(errno));
				}

				ServerInstance->SE->NonBlocking(incomingSockfd);
				ServerInstance->stats->statsAccept++;
				ServerInstance->Users->AddUser(ServerInstance, incomingSockfd, in_port, false, this->family, client, target);	
			}
			else
			{
				ServerInstance->SE->Shutdown(incomingSockfd, 2);
				ServerInstance->SE->Close(incomingSockfd);
				ServerInstance->stats->statsRefused++;
			}
		}
		break;
	}
}

/** This will bind a socket to a port. It works for UDP/TCP.
 * It can only bind to IP addresses, if you wish to bind to hostnames
 * you should first resolve them using class 'Resolver'.
 */ 
bool InspIRCd::BindSocket(int sockfd, int port, const char* addr, bool dolisten)
{
	/* We allocate 2 of these, because sockaddr_in6 is larger than sockaddr (ugh, hax) */
	sockaddr* servaddr = new sockaddr[2];
	memset(servaddr,0,sizeof(sockaddr)*2);

	int ret, size;

	if (*addr == '*')
		addr = "";

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
				delete[] servaddr;
				return false;
			}

			((sockaddr_in6*)servaddr)->sin6_family = AF_INET6;
			memcpy(&(((sockaddr_in6*)servaddr)->sin6_addr), &addy, sizeof(in6_addr));
			((sockaddr_in6*)servaddr)->sin6_port = htons(port);
			size = sizeof(sockaddr_in6);
		}
		else
		{
			/* No, its not */
			in_addr addy;
			if (inet_pton(AF_INET, addr, &addy) < 1)
			{
				delete[] servaddr;
				return false;
			}

			((sockaddr_in*)servaddr)->sin_family = AF_INET;
			((sockaddr_in*)servaddr)->sin_addr = addy;
			((sockaddr_in*)servaddr)->sin_port = htons(port);
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
			((sockaddr_in*)servaddr)->sin_family = AF_INET;
			((sockaddr_in*)servaddr)->sin_addr.s_addr = htonl(INADDR_ANY);
			((sockaddr_in*)servaddr)->sin_port = 0;
			size = sizeof(sockaddr_in);
		}
		else
		{
			/* Theres no address here, default to ipv6 bind to all */
			((sockaddr_in6*)servaddr)->sin6_family = AF_INET6;
			memset(&(((sockaddr_in6*)servaddr)->sin6_addr), 0, sizeof(in6_addr));
			((sockaddr_in6*)servaddr)->sin6_port = htons(port);
			size = sizeof(sockaddr_in6);
		}
	}
#else
	/* If we aren't built with ipv6, the choice becomes simple */
	((sockaddr_in*)servaddr)->sin_family = AF_INET;
	if (*addr)
	{
		/* There is an address here. */
		in_addr addy;
		if (inet_pton(AF_INET, addr, &addy) < 1)
		{
			delete[] servaddr;
			return false;
		}
		((sockaddr_in*)servaddr)->sin_addr = addy;
	}
	else
	{
		/* Bind ipv4 to all */
		((sockaddr_in*)servaddr)->sin_addr.s_addr = htonl(INADDR_ANY);
	}
	/* Bind ipv4 port number */
	((sockaddr_in*)servaddr)->sin_port = htons(port);
	size = sizeof(sockaddr_in);
#endif
	ret = SE->Bind(sockfd, servaddr, size);
	delete[] servaddr;

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
int irc::sockets::OpenTCPSocket(char* addr, int socktype)
{
	int sockfd;
	int on = 1;
	addr = addr;
	struct linger linger = { 0, 0 };
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

int InspIRCd::BindPorts(bool, int &ports_found, FailedPortList &failed_ports)
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
					this->Logs->Log("SOCKET",DEFAULT,"Port binding %s:%d was removed from the config file, closing.", old_ports[k].first.c_str(), old_ports[k].second);
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


