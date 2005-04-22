/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  Inspire is copyright (C) 2002-2004 ChatSpike-Dev.
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

#include <connection.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/errno.h>
#include <sys/ioctl.h>
#include <sys/utsname.h>
#include <vector>
#include <string>
#include <deque>
#include "inspircd.h"
#include "modules.h"
#include "inspstring.h"

using namespace std;


extern std::vector<Module*> modules;
extern std::vector<ircd_module*> factory;

extern int MODCOUNT;

extern time_t TIME;

connection::connection()
{
	fd = 0;
}


bool connection::CreateListener(char* host, int p)
{
	sockaddr_in host_address;
	int flags;
	in_addr addy;
	int on = 0;
	struct linger linger = { 0 };
	
	this->port = p;
	
	fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (fd <= 0)
	{
		return false;
	}

	setsockopt(fd,SOL_SOCKET,SO_REUSEADDR,(const char*)&on,sizeof(on));
	linger.l_onoff = 1;
	linger.l_linger = 1;
	setsockopt(fd,SOL_SOCKET,SO_LINGER,(const char*)&linger,sizeof(linger));
	
	// attempt to increase socket sendq and recvq as high as its possible
	// to get them on linux.
	int sendbuf = 32768;
	int recvbuf = 32768;
	setsockopt(fd,SOL_SOCKET,SO_SNDBUF,(const void *)&sendbuf,sizeof(sendbuf)); 
	setsockopt(fd,SOL_SOCKET,SO_RCVBUF,(const void *)&recvbuf,sizeof(sendbuf));

	memset((void*)&host_address, 0, sizeof(host_address));

	host_address.sin_family = AF_INET;

	if (!strcmp(host,""))
	{
		host_address.sin_addr.s_addr = htonl(INADDR_ANY);
	}
	else
	{
  		inet_aton(host,&addy);
		host_address.sin_addr = addy;
	}

	host_address.sin_port = htons(p);

	if (bind(fd,(sockaddr*)&host_address,sizeof(host_address))<0)
	{
		return false;
	}

	// make the socket non-blocking
	flags = fcntl(fd, F_GETFL, 0);
	fcntl(fd, F_SETFL, flags | O_NONBLOCK);

	this->port = p;

	listen(this->fd,5);

	return true;
}

char* ircd_connector::GetServerIP()
{
	return this->host;
}

int ircd_connector::GetServerPort()
{
	return this->port;
}

bool ircd_connector::SetHostAndPort(char* host, int port)
{
	strncpy(this->host,host,160);
	this->port = port;
	return true;
}

bool ircd_connector::SetHostAddress(char* host, int port)
{
	strncpy(this->host,host,160);
	this->port = port;
	memset((void*)&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	inet_aton(host,&addr.sin_addr);
	addr.sin_port = htons(port);
	return true;
}

void ircd_connector::SetServerPort(int p)
{
	this->port = p;
}

void ircd_connector::AddBuffer(char a)
{
	if (a != '\r')
		ircdbuffer = ircdbuffer + a;
}

bool ircd_connector::BufferIsComplete()
{
	if (ircdbuffer.length())
	{
		return (ircdbuffer[ircdbuffer.length()-1] == '\n');
	}
	return false;
}

void ircd_connector::ClearBuffer()
{
	ircdbuffer = "";
}

std::string ircd_connector::GetBuffer()
{
	if (ircdbuffer.length() < 510)
	{
		return ircdbuffer;
	}
	else
	{
		return ircdbuffer.substr(510);
	}
}

bool ircd_connector::MakeOutboundConnection(char* host, int port)
{
	log(DEBUG,"MakeOutboundConnection: Original param: %s",host);
	ClearBuffer();
	hostent* hoste = gethostbyname(host);
	if (!hoste)
	{
		log(DEBUG,"MakeOutboundConnection: gethostbyname was NULL, setting %s",host);
		this->SetHostAddress(host,port);
		SetHostAndPort(host,port);
	}
	else
	{
		struct in_addr* ia = (in_addr*)hoste->h_addr;
		log(DEBUG,"MakeOutboundConnection: gethostbyname was valid, setting %s",inet_ntoa(*ia));
		this->SetHostAddress(inet_ntoa(*ia),port);
		SetHostAndPort(inet_ntoa(*ia),port);
	}

	this->fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (this->fd >= 0)
	{
		if(connect(this->fd, (sockaddr*)&this->addr,sizeof(this->addr)))
		{
			WriteOpers("connect() failed for %s",host);
			RemoveServer(this->servername.c_str());
			return false;
		}
		int flags = fcntl(this->fd, F_GETFL, 0);
		fcntl(this->fd, F_SETFL, flags | O_NONBLOCK);
		int sendbuf = 32768;
		int recvbuf = 32768;
		setsockopt(this->fd,SOL_SOCKET,SO_SNDBUF,(const void *)&sendbuf,sizeof(sendbuf)); 
		setsockopt(this->fd,SOL_SOCKET,SO_RCVBUF,(const void *)&recvbuf,sizeof(sendbuf));
		return true;
	}
	else
	{
		WriteOpers("socket() failed!");
		RemoveServer(this->servername.c_str());
	}

	return false;
}


bool connection::BeginLink(char* targethost, int port, char* password, char* servername, int myport)
{
	char connect[MAXBUF];
	
	ircd_connector connector;
	ircd_connector *cn = this->FindHost(servername);


	if (cn)
	{
		WriteOpers("CONNECT aborted: Server %s already exists",servername);
		return false;
	}

	
	if (this->fd)
	{
		if (connector.MakeOutboundConnection(targethost,port))
		{
			// targethost has been turned into an ip...
			// we dont want this as the server name.
			connector.SetServerName(servername);
			snprintf(connect,MAXBUF,"S %s %s %d %d :%s",getservername().c_str(),password,myport,GetRevision(),getserverdesc().c_str());
			connector.SetState(STATE_NOAUTH_OUTBOUND);
			connector.SetHostAndPort(targethost, port);
			this->connectors.push_back(connector);
			return this->SendPacket(connect, servername);
		}
		else
		{
			connector.SetState(STATE_DISCONNECTED);
			WriteOpers("Could not create outbound connection to %s:%d",targethost,port);
		}
	}
	return false;
}

bool connection::MeshCookie(char* targethost, int port, long cookie, char* servername)
{
	char connect[MAXBUF];
	
	ircd_connector connector;
	
	WriteOpers("Establishing meshed link to %s:%d",servername,port);

	if (this->fd)
	{
		if (connector.MakeOutboundConnection(targethost,port))
		{
			// targethost has been turned into an ip...
			// we dont want this as the server name.
			connector.SetServerName(servername);
			snprintf(connect,MAXBUF,"- %d %s :%s",cookie,getservername().c_str(),getserverdesc().c_str());
			connector.SetState(STATE_NOAUTH_OUTBOUND);
			connector.SetHostAndPort(targethost, port);
			connector.SetState(STATE_CONNECTED);
			this->connectors.push_back(connector);
			return this->SendPacket(connect, servername);
		}
		else
		{
			connector.SetState(STATE_DISCONNECTED);
			WriteOpers("Could not create outbound connection to %s:%d",targethost,port);
		}
	}
	return false;
}

bool connection::AddIncoming(int fd, char* targethost, int sourceport)
{
	char connect[MAXBUF];
	
	ircd_connector connector;
	
	// targethost has been turned into an ip...
	// we dont want this as the server name.
	connector.SetServerName(targethost);
	connector.SetDescriptor(fd);
	connector.SetState(STATE_NOAUTH_INBOUND);
	int flags = fcntl(fd, F_GETFL, 0);
	fcntl(fd, F_SETFL, flags | O_NONBLOCK);
	int sendbuf = 32768;
	int recvbuf = 32768;
	setsockopt(fd,SOL_SOCKET,SO_SNDBUF,(const void *)&sendbuf,sizeof(sendbuf)); 
	setsockopt(fd,SOL_SOCKET,SO_RCVBUF,(const void *)&recvbuf,sizeof(sendbuf));
	connector.SetHostAndPort(targethost, sourceport);
	connector.SetState(STATE_NOAUTH_INBOUND);
	log(DEBUG,"connection::AddIncoming() Added connection: %s:%d",targethost,sourceport);
	this->connectors.push_back(connector);
	return true;
}

void connection::TerminateLink(char* targethost)
{
	// this locates the targethost in the connection::connectors vector of the class,
 	// and terminates it by sending it an SQUIT token and closing its descriptor.
	// TerminateLink with a null string causes a terminate of ALL links
}


// Returns a pointer to the connector for 'host'
ircd_connector* connection::FindHost(std::string host)
{
	for (int i = 0; i < this->connectors.size(); i++)
	{
		if (this->connectors[i].GetServerName() == host)
		{
			return &this->connectors[i];
		}
	}
	return NULL;
}

std::string ircd_connector::GetServerName()
{
	return this->servername;
}

std::string ircd_connector::GetDescription()
{
	return this->description;
}

void ircd_connector::SetServerName(std::string serv)
{
	this->servername = serv;
}

void ircd_connector::SetDescription(std::string desc)
{
	this->description = desc;
}


int ircd_connector::GetDescriptor()
{
	return this->fd;
}

int ircd_connector::GetState()
{
	return this->state;
}


void ircd_connector::SetState(int state)
{
	this->state = state;
	if (state == STATE_DISCONNECTED)
	{
		NetSendMyRoutingTable();
	}
}

void ircd_connector::CloseConnection()
{
	int flags = fcntl(this->fd, F_GETFL, 0);
	fcntl(this->fd, F_SETFL, flags ^ O_NONBLOCK);
	close(this->fd);
	flags = fcntl(this->fd, F_GETFL, 0);
	fcntl(this->fd, F_SETFL, flags | O_NONBLOCK);
}

void ircd_connector::SetDescriptor(int fd)
{
	this->fd = fd;
}

bool connection::SendPacket(char *message, const char* host)
{
	if ((!message) || (!host))
		return true;

	ircd_connector* cn = this->FindHost(host);
	
	if (!strchr(message,'\n'))
	{
		strlcat(message,"\n",MAXBUF);
	}

	if (cn)
	{
		log(DEBUG,"main: Connection::SendPacket() sent '%s' to %s",message,cn->GetServerName().c_str());
		
		if (cn->GetState() == STATE_DISCONNECTED)
		{
			log(DEBUG,"Main route to %s is down, seeking alternative",host);
			// fix: can only route one hop to avoid a loop
			if (strlcat(message,"R ",2))
			{
				// this route is down, we must re-route the packet through an available point in the mesh.
				for (int k = 0; k < this->connectors.size(); k++)
				{
					// search for another point in the mesh which can 'reach' where we want to go
					for (int m = 0; m < this->connectors[k].routes.size(); m++)
					{
						if (!strcasecmp(this->connectors[k].routes[m].c_str(),host))
						{
							log(DEBUG,"Found alternative route for packet: %s",this->connectors[k].GetServerName().c_str());
							char buffer[MAXBUF];
							snprintf(buffer,MAXBUF,"R %s %s",host,message);
							this->SendPacket(buffer,this->connectors[k].GetServerName().c_str());
							return true;
						}
					}
				}
			}
			char buffer[MAXBUF];
			snprintf(buffer,MAXBUF,"& %s",host);
			NetSendToAllExcept(host,buffer);
			log(DEBUG,"There are no routes to %s, we're gonna boot the server off!",host);
			DoSplit(host);
			return false;
		}

		// returns false if the packet could not be sent (e.g. target host down)
		if (send(cn->GetDescriptor(),message,strlen(message),0)<0)
		{
			log(DEBUG,"send() failed for Connection::SendPacket(): %s",strerror(errno));
			log(DEBUG,"Disabling connector: %s",cn->GetServerName().c_str());
			cn->CloseConnection();
			cn->SetState(STATE_DISCONNECTED);
			// retry the packet along a new route so either arrival OR failure are gauranteed (bugfix)
			return this->SendPacket(message,host);
		}
		return true;
	}
}

// receives a packet from any where there is data waiting, first come, first served
// fills the message and host values with the host where the data came from.

bool connection::RecvPacket(std::deque<std::string> &messages, char* host)
{
	char data[4096];
	memset(data, 0, 4096);
	for (int i = 0; i < this->connectors.size(); i++)
	{
		if (this->connectors[i].GetState() != STATE_DISCONNECTED)
		{
			// returns false if the packet could not be sent (e.g. target host down)
			int rcvsize = 0;
			rcvsize = recv(this->connectors[i].GetDescriptor(),data,1,0);
			if (rcvsize == -1)
			{
				if (errno != EAGAIN)
				{
					log(DEBUG,"recv() failed for Connection::RecvPacket(): %s",strerror(errno));
					log(DEBUG,"Disabling connector: %s",this->connectors[i].GetServerName().c_str());
					this->connectors[i].CloseConnection();
					this->connectors[i].SetState(STATE_DISCONNECTED);
				}
			}
			if (rcvsize > 0)
			{
				this->connectors[i].AddBuffer(data[0]);
				if (this->connectors[i].BufferIsComplete())
				{
					messages.push_back(this->connectors[i].GetBuffer().c_str());
					strlcpy(host,this->connectors[i].GetServerName().c_str(),160);
					log(DEBUG,"main: Connection::RecvPacket() got '%s' from %s",this->connectors[i].GetBuffer().c_str(),host);
					this->connectors[i].ClearBuffer();
					return true;
				}
			}
		}
	}
	// nothing new yet -- message and host will be undefined
	return false;
}

long connection::GenKey()
{
	return (random()*time(NULL));
}

