#include <connection.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/errno.h>
#include <sys/ioctl.h>
#include <sys/utsname.h>
#include "inspircd.h"
#include "modules.h"

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
	
	fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
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

	return true;
}


bool connection::BeginLink(char* targethost, int port, char* password)
{
	char connect[MAXBUF];
	
	if (this->fd)
	{
		sprintf(connect,"S %s %s :%s",getservername().c_str(),password,getserverdesc().c_str());
		this->haspassed = false;
		return this->SendPacket(connect, targethost, port, 0);
	}
	return false;
}

// targethost: in dot notation a.b.c.d
void connection::TerminateLink(char* targethost)
{
}

// host: in dot notation a.b.c.d
// port: host byte order
bool connection::SendPacket(char *message, char* host, int port, long ourkey)
{
	sockaddr_in host_address;
	in_addr addy;
	packet p;

	memset((void*)&host_address, 0, sizeof(host_address));

	host_address.sin_family = AF_INET;
	inet_aton(host,&addy);
	host_address.sin_addr = addy;

	host_address.sin_port = htons(port);

	strcpy(p.data,message);
	p.type = PT_SYN_WITH_DATA;
	p.key = ourkey;


	FOREACH_MOD OnPacketTransmit(p.data);

	log(DEBUG,"main: Connection::SendPacket() sent '%s' to %s:%d",p.data,host,port);

	// returns false if the packet could not be sent (e.g. target host down)
	if (sendto(this->fd,&p,sizeof(p),0,(sockaddr*)&host_address,sizeof(host_address))<0)
	{
		log(DEBUG,"sendto() failed for Connection::SendPacket() with a packet of size %d",sizeof(p));
		return false;
	}
	return true;

}

bool connection::SendSYN(char* host, int port)
{
	sockaddr_in host_address;
	in_addr addy;
	packet p;

	memset((void*)&host_address, 0, sizeof(host_address));

	host_address.sin_family = AF_INET;
	inet_aton(host,&addy);
	host_address.sin_addr = addy;

	host_address.sin_port = htons(port);

	p.type = PT_SYN_ONLY;
	p.key = key;
	strcpy(p.data,"");

	if (sendto(fd,&p,sizeof(p),0,(sockaddr*)&host_address,sizeof(host_address))<0)
	{
		return false;
	}
	return true;

}

bool connection::SendACK(char* host, int port, int reply_id)
{
	sockaddr_in host_address;
	in_addr addy;
	packet p;

	memset((void*)&host_address, 0, sizeof(host_address));

	host_address.sin_family = AF_INET;
	inet_aton(host,&addy);
	host_address.sin_addr = addy;

	host_address.sin_port = htons(port);

	p.type = PT_ACK_ONLY;
	p.key = key;
	p.id = reply_id;
	strcpy(p.data,"");

	if (sendto(fd,&p,sizeof(p),0,(sockaddr*)&host_address,sizeof(host_address))<0)
	{
		return false;
	}
	return true;

}


// Generates a server key. This is pseudo-random.
// the server always uses the same server-key in all communications
// across the network. All other servers must remember the server key
// of servers in the network, e.g.:
//
// ServerA:  key=5555555555
// ServerB:  key=6666666666
// I am ServerC: key=77777777777
//
// If ServerC sees a packet from ServerA, and the key stored for ServerA
// is 0, then cache the key as the servers key.
// after this point, any packet from ServerA which does not contain its key,
// 555555555, will be silently dropped.
// This should prevent blind spoofing, as to fake a server you must know its
// assigned key, and to do that you must receive messages that are origintated
// from it or hack the running executable.
//
// During the AUTH phase (when server passwords are checked, the key in any
// packet MUST be 0). Only the initial SERVER/PASS packets may have a key
// of 0 (and any ACK responses to them).
//

long connection::GenKey()
{
	srand(time(NULL));
	return (random()*time(NULL));
}

// host: in dot notation a.b.c.d
// port: host byte order
bool connection::RecvPacket(char *message, char* host, int &prt, long &theirkey)
{
	// returns false if no packet waiting for receive, e.g. EAGAIN or ECONNRESET
	sockaddr_in host_address;
	socklen_t host_address_size;
	packet p;
	
	memset((void*)&host_address, 0, sizeof(host_address));

	host_address.sin_family=AF_INET;
	host_address_size=sizeof(host_address);

	if (recvfrom(fd,&p,sizeof(p),0,(sockaddr*)&host_address,&host_address_size)<0)
	{
		return false;
	}

	log(DEBUG,"connection::RecvPacket(): received packet type %d '%s'",p.type,p.data);

	if (p.type == PT_SYN_ONLY)
	{
		strcpy(message,p.data);
		strcpy(host,inet_ntoa(host_address.sin_addr));
		prt = ntohs(host_address.sin_port);
		SendACK(host,this->port,p.id);
		return false;
	}

	if (p.type == PT_ACK_ONLY)
	{
		strcpy(message,p.data);
		strcpy(host,inet_ntoa(host_address.sin_addr));
		prt = ntohs(host_address.sin_port);
		return false;
	}

	if (p.type == PT_SYN_WITH_DATA)
	{
		strcpy(message,p.data);
		strcpy(host,inet_ntoa(host_address.sin_addr));
		theirkey = p.key;
		prt = ntohs(host_address.sin_port);
		SendACK(host,this->port,p.id);
		return true;
	}

	log(DEBUG,"connection::RecvPacket(): Invalid packet type %d (protocol error)",p.type);
	return true;
}

