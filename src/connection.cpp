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

using namespace std;

extern std::vector<Module*> modules;
extern std::vector<ircd_module*> factory;

extern int MODCOUNT;


packet::packet()
{
	srand(time(NULL));
	id = random();
}

packet::~packet()
{
}

connection::connection()
{
	key = GenKey();
	fd = 0;
}


bool connection::CreateListener(char* host, int p)
{
	sockaddr_in host_address;
	int flags;
	in_addr addy;
	int on = 0;
	struct linger linger = { 0 };
	
	fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (fd <= 0)
	{
		return false;
	}

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

	setsockopt(fd,SOL_SOCKET,SO_REUSEADDR,(const char*)&on,sizeof(on));
	linger.l_onoff = 1;
	linger.l_linger = 0;
	setsockopt(fd,SOL_SOCKET,SO_LINGER,(const char*)&linger,sizeof(linger));
	
	// attempt to increase socket sendq and recvq as high as its possible
	// to get them on linux.
	int sendbuf = 32768;
	int recvbuf = 32768;
	setsockopt(fd,SOL_SOCKET,SO_SNDBUF,(const void *)&sendbuf,sizeof(sendbuf)); 
	setsockopt(fd,SOL_SOCKET,SO_RCVBUF,(const void *)&recvbuf,sizeof(sendbuf));
	
	listen(this->fd,5);

	return true;
}

bool ircd_connector::SetHostAddress(char* host, int port)
{
	memset((void*)&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	inet_aton(host,&addr.sin_addr);
	addr.sin_port = htons(port);
	return true;
}

bool ircd_connector::MakeOutboundConnection(char* host, int port)
{
	hostent* hoste = gethostbyname(host);
	if (!hoste)
	{
		WriteOpers("Failed to look up hostname for %s, using as an ip address",host);
		this->SetHostAddress(host,port);
	}
	else
	{
		WriteOpers("Found hostname for %s",host);
		this->SetHostAddress(hoste->h_addr,port);
	}

	this->fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (this->fd >= 0)
	{
		if(connect(this->fd, (sockaddr*)&addr,sizeof(addr)))
		{
			WriteOpers("connect() failed for %s",host);
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
	}

	return false;
}


bool connection::BeginLink(char* targethost, int port, char* password, char* servername)
{
	char connect[MAXBUF];
	
	ircd_connector connector;
	
	if (this->fd)
	{
		if (connector.MakeOutboundConnection(targethost,port))
		{
			// targethost has been turned into an ip...
			// we dont want this as the server name.
			connector.SetServerName(servername);
			sprintf(connect,"S %s %s :%s",getservername().c_str(),password,getserverdesc().c_str());
			connector.SetState(STATE_NOAUTH_OUTBOUND);
			this->connectors.push_back(connector);
			return this->SendPacket(connect, servername);
		}
		else
		{
			WriteOpers("Could not create outbound connection to %s:%d",targethost,port);
		}
	}
	return false;
}

bool connection::AddIncoming(int fd,char* targethost)
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

void ircd_connector::SetServerName(std::string serv)
{
	this->servername = serv;
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
}

void ircd_connector::SetDescriptor(int fd)
{
	this->fd = fd;
}

bool connection::SendPacket(char *message, const char* host)
{
	ircd_connector* cn = this->FindHost(host);
	
	if (cn)
	{
		log(DEBUG,"main: Connection::SendPacket() sent '%s' to %s",message,cn->GetServerName().c_str());

		strncat(message,"\n",MAXBUF);
		// returns false if the packet could not be sent (e.g. target host down)
		if (send(cn->GetDescriptor(),message,strlen(message),0)<0)
		{
			log(DEBUG,"send() failed for Connection::SendPacket(): %s",strerror(errno));
			return false;
		}
		return true;
	}
}

// receives a packet from any where there is data waiting, first come, first served
// fills the message and host values with the host where the data came from.

bool connection::RecvPacket(std::deque<std::string> &messages, char* host)
{
	char data[32767];
	memset(data, 0, 32767);
	for (int i = 0; i < this->connectors.size(); i++)
	{
		// returns false if the packet could not be sent (e.g. target host down)
		int rcvsize = 0;
		if (rcvsize = recv(this->connectors[i].GetDescriptor(),data,32767,0))
		{
			if (rcvsize > 0)
			{
				char* l = strtok(data,"\n");
				while (l)
				{
					char sanitized[32767];
					memset(sanitized, 0, 32767);
					int ptt = 0;
					for (int pt = 0; pt < strlen(l); pt++)
					{
						if (l[pt] != '\r')
						{
							sanitized[ptt++] = l[pt];
						}
					}
					sanitized[ptt] = '\0';
					if (strlen(sanitized))
					{
						messages.push_back(sanitized);
						strncpy(host,this->connectors[i].GetServerName().c_str(),160);
						log(DEBUG,"main: Connection::RecvPacket() got '%s' from %s",sanitized,host);
						
					}
					l = strtok(NULL,"\n");
				}
				return true;
			}
		}
	}
	// nothing new yet -- message and host will be undefined
	return false;
}

long connection::GenKey()
{
	srand(time(NULL));
	return (random()*time(NULL));
}

