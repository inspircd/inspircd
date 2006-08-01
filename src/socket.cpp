/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd is copyright (C) 2002-2006 ChatSpike-Dev.
 *                       E-mail:
 *                <brain@chatspike.net>
 *           	  <Craig@chatspike.net>
 *     
 * Written by Craig Edwards, Craig McLure, and others.
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include <string>
#include "configreader.h"
#include "socket.h"
#include "inspircd.h"
#include "inspstring.h"
#include "helperfuncs.h"
#include "socketengine.h"
#include "message.h"

extern InspIRCd* ServerInstance;
extern ServerConfig* Config;
extern time_t TIME;

/** This will bind a socket to a port. It works for UDP/TCP.
 * If a hostname is given to bind to, the function will first
 * attempt to resolve the hostname, then bind to the IP the 
 * hostname resolves to. This is a blocking lookup blocking for
 * a maximum of one second before it times out, using the DNS
 * server specified in the configuration file.
 */ 
bool BindSocket(int sockfd, insp_sockaddr client, insp_sockaddr server, int port, char* addr)
{
	memset(&server,0,sizeof(server));
	insp_inaddr addy;
	bool resolved = false;
	char resolved_addr[128];

	if (*addr == '*')
		*addr = 0;

	if (*addr && (insp_aton(addr,&addy) < 1))
	{
		/* If they gave a hostname, bind to the IP it resolves to */
		if (CleanAndResolve(resolved_addr, addr, true, 1))
		{
			insp_aton(resolved_addr,&addy);
			log(DEFAULT,"Resolved binding '%s' -> '%s'",addr,resolved_addr);
#ifdef IPV6
			/* Todo: Deal with resolution of IPV6 */
			server.sin6_addr = addy;
#else
			server.sin_addr = addy;
#endif
			resolved = true;
		}
		else
		{
			log(DEFAULT,"WARNING: Could not resolve '%s' to an IP for binding to on port %d",addr,port);
			return false;
		}
	}
#ifdef IPV6
	server.sin6_family = AF_FAMILY;
#else
	server.sin_family = AF_FAMILY;
#endif
	if (!resolved)
	{
		if (!*addr)
		{
#ifdef IPV6
			memcpy(&addy, &server.sin6_addr, sizeof(in6_addr));
#else
			server.sin_addr.s_addr = htonl(INADDR_ANY);
#endif
		}
		else
		{
#ifdef IPV6
			memcpy(&addy, &server.sin6_addr, sizeof(in6_addr));
#else
			server.sin_addr = addy;
#endif
		}
	}
#ifdef IPV6
	server.sin6_port = htons(port);
#else
	server.sin_port = htons(port);
#endif
	if (bind(sockfd,(struct sockaddr*)&server,sizeof(server)) < 0)
	{
		return false;
	}
	else
	{
		log(DEBUG,"Bound port %s:%d",*addr ? addr : "*",port);
		if (listen(sockfd, Config->MaxConn) == -1)
		{
			log(DEFAULT,"ERROR in listen(): %s",strerror(errno));
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
int OpenTCPSocket()
{
	int sockfd;
	int on = 1;
	struct linger linger = { 0 };
  
	if ((sockfd = socket (AF_FAMILY, SOCK_STREAM, 0)) < 0)
	{
		log(DEFAULT,"Error creating TCP socket: %s",strerror(errno));
		return (ERROR);
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

bool HasPort(int port, char* addr)
{
	for (unsigned long count = 0; count < ServerInstance->stats->BoundPortCount; count++)
	{
		if ((port == Config->ports[count]) && (!strcasecmp(Config->addrs[count],addr)))
		{
			return true;
		}
	}
	return false;
}

int BindPorts(bool bail)
{
	char configToken[MAXBUF], Addr[MAXBUF], Type[MAXBUF];
	insp_sockaddr client, server;
	int clientportcount = 0;
	int BoundPortCount = 0;

	if (!bail)
	{
		int InitialPortCount = ServerInstance->stats->BoundPortCount;
		log(DEBUG,"Initial port count: %d",InitialPortCount);

		for (int count = 0; count < Config->ConfValueEnum(Config->config_data, "bind"); count++)
		{
			Config->ConfValue(Config->config_data, "bind", "port", count, configToken, MAXBUF);
			Config->ConfValue(Config->config_data, "bind", "address", count, Addr, MAXBUF);
			Config->ConfValue(Config->config_data, "bind", "type", count, Type, MAXBUF);

			if (((!*Type) || (!strcmp(Type,"clients"))) && (!HasPort(atoi(configToken),Addr)))
			{
				// modules handle server bind types now
				Config->ports[clientportcount+InitialPortCount] = atoi(configToken);
				if (*Addr == '*')
					*Addr = 0;

				strlcpy(Config->addrs[clientportcount+InitialPortCount],Addr,256);
				clientportcount++;
				log(DEBUG,"NEW binding %s:%s [%s] from config",Addr,configToken, Type);
			}
		}
		int PortCount = clientportcount;
		if (PortCount)
		{
			for (int count = InitialPortCount; count < InitialPortCount + PortCount; count++)
			{
				if ((Config->openSockfd[count] = OpenTCPSocket()) == ERROR)
				{
					log(DEBUG,"Bad fd %d binding port [%s:%d]",Config->openSockfd[count],Config->addrs[count],Config->ports[count]);
					return ERROR;
				}
				if (!BindSocket(Config->openSockfd[count],client,server,Config->ports[count],Config->addrs[count]))
				{
					log(DEFAULT,"Failed to bind port [%s:%d]: %s",Config->addrs[count],Config->ports[count],strerror(errno));
				}
				else
				{
					/* Associate the new open port with a slot in the socket engine */
					if (Config->openSockfd[count] > -1)
					{
						ServerInstance->SE->AddFd(Config->openSockfd[count],true,X_LISTEN);
						BoundPortCount++;
					}
				}
			}
			return InitialPortCount + BoundPortCount;
		}
		else
		{
			log(DEBUG,"There is nothing new to bind!");
		}
		return InitialPortCount;
	}

	for (int count = 0; count < Config->ConfValueEnum(Config->config_data, "bind"); count++)
	{
		Config->ConfValue(Config->config_data, "bind", "port", count, configToken, MAXBUF);
		Config->ConfValue(Config->config_data, "bind", "address", count, Addr, MAXBUF);
		Config->ConfValue(Config->config_data, "bind", "type", count, Type, MAXBUF);

		if ((!*Type) || (!strcmp(Type,"clients")))
		{
			// modules handle server bind types now
			Config->ports[clientportcount] = atoi(configToken);

			// If the client put bind "*", this is an unrealism.
			// We don't actually support this as documented, but
			// i got fed up of people trying it, so now it converts
			// it to an empty string meaning the same 'bind to all'.
			if (*Addr == '*')
				*Addr = 0;

			strlcpy(Config->addrs[clientportcount],Addr,256);
			clientportcount++;
			log(DEBUG,"Binding %s:%s [%s] from config",Addr,configToken, Type);
		}
	}

	int PortCount = clientportcount;

	for (int count = 0; count < PortCount; count++)
	{
		if ((Config->openSockfd[BoundPortCount] = OpenTCPSocket()) == ERROR)
		{
			log(DEBUG,"Bad fd %d binding port [%s:%d]",Config->openSockfd[BoundPortCount],Config->addrs[count],Config->ports[count]);
			return ERROR;
		}

		if (!BindSocket(Config->openSockfd[BoundPortCount],client,server,Config->ports[count],Config->addrs[count]))
		{
			log(DEFAULT,"Failed to bind port [%s:%d]: %s",Config->addrs[count],Config->ports[count],strerror(errno));
		}
		else
		{
			/* well we at least bound to one socket so we'll continue */
			BoundPortCount++;
		}
	}

	/* if we didn't bind to anything then abort */
	if (!BoundPortCount)
	{
		log(DEFAULT,"No ports bound, bailing!");
		printf("\nERROR: Could not bind any of %d ports! Please check your configuration.\n\n", PortCount);
		return ERROR;
	}

	return BoundPortCount;
}

const char* insp_ntoa(insp_inaddr n)
{
	static char buf[1024];
	inet_ntop(AF_FAMILY, &n, buf, sizeof(buf));
	return buf;
}

int insp_aton(const char* a, insp_inaddr* n)
{
	return inet_pton(AF_FAMILY, a, n);
}

