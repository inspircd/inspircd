#include <connection.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/errno.h>
#include <sys/ioctl.h>
#include <sys/utsname.h>
#include <errno.h>
#include <vector>
#include "inspircd.h"
#include "modules.h"

extern std::vector<Module*> modules;
extern std::vector<ircd_module*> factory;

extern int MODCOUNT;

#define STATE_CLEAR 1
#define STATE_WAIT_FOR_ACK 2

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
	state = STATE_CLEAR;
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
		log(DEBUG,"sendto() failed for Connection::SendPacket() with a packet of size %d: %s",sizeof(p),strerror(errno));
		return false;
	}
	this->state = STATE_WAIT_FOR_ACK;


	// host_address remains unchanged. we only want to receive from where we just sent the packet to.
	
	// retry the packet up to 5 times
	for (int retries = 0; retries < 5; retries++)
	{
		socklen_t host_address_size;
		host_address.sin_family=AF_INET;
		host_address_size=sizeof(host_address);
	
		// wait for ack, or timeout.
		// if reached a timeout, send again.
		// the packet id in the ack must match that in the original packet
		// this MUST operate in lock/step fashion!!!
		int cycles = 0;
		packet p2;
		do 
		{
			fd_set sfd;
			timeval tval;
			tval.tv_usec = 100;
			tval.tv_sec = 0;
			FD_ZERO(&sfd);
			FD_SET(fd,&sfd);
			int res = select(65535, &sfd, NULL, NULL, &tval);
			cycles++;
		}
		while ((recvfrom(fd,&p2,sizeof(p2),0,(sockaddr*)&host_address,&host_address_size)<0) && (cycles < 10));
		
		if (cycles >= 10)
		{
			log(DEFAULT,"ERROR! connection::SendPacket() waited >10000 nanosecs for an ACK. Will resend up to 5 times");
		}
		else
		{
			if (p2.type != PT_ACK_ONLY)
			{
				packet_buf pb;
				pb.p.id = p.id;
				pb.p.key = p.key;
				pb.p.type = p.type;
				strcpy(pb.host,inet_ntoa(host_address.sin_addr));
				pb.port = ntohs(host_address.sin_port);
				this->buffer.push_back(pb);
				
				log(DEFAULT,"ERROR! connection::SendPacket() received a data response and was expecting an ACK!!!");
				this->state = STATE_CLEAR;
				return true;
			}

			if (p2.id != p.id)
			{
				log(DEFAULT,"ERROR! connection::SendPacket() received an ack for a packet it didnt send!");
				this->state = STATE_CLEAR;
				return false;
			}
			else
			{
				log(DEFAULT,"Successfully received ACK");
				this->state = STATE_CLEAR;
				return true;
				break;
			}
		}
	}
	log(DEFAULT,"We never received an ack. Something fishy going on, host is dead.");
	this->state = STATE_CLEAR;
	return false;

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

	//int recvfrom(int s, void *buf, size_t len, int flags, struct sockaddr *from, socklen_t *fromlen);
	if (recvfrom(fd,&p,sizeof(p),0,(sockaddr*)&host_address,&host_address_size)<0)
	{
		if (this->buffer.size())
		{
			log(DEBUG,"Fetching a buffered packet");

			strcpy(message,buffer[0].p.data);
			theirkey = buffer[0].p.key;
			strcpy(host,buffer[0].host);
			prt = buffer[0].port;
			
			buffer.erase(0);
			
			return true;
		}
		return false;
	}

	log(DEBUG,"connection::RecvPacket(): received packet type %d '%s' from '%s'",p.type,p.data,inet_ntoa(host_address.sin_addr));

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
		this->state = STATE_CLEAR;
		return false;
	}

	if (p.type == PT_SYN_WITH_DATA)
	{
		strcpy(message,p.data);
		strcpy(host,inet_ntoa(host_address.sin_addr));
		theirkey = p.key;
		prt = ntohs(host_address.sin_port); // the port we received it on
		SendACK(host,prt,p.id);

		if (this->buffer.size())
		{
			log(DEBUG,"Fetching a buffered packet");
			packet_buf pb;
			pb.p.id = p.id;
			pb.p.key = p.key;
			pb.p.type = p.type;
			strcpy(pb.host,inet_ntoa(host_address.sin_addr));
			pb.port = ntohs(host_address.sin_port);
			this->buffer.push_back(pb);

			strcpy(message,buffer[0].p.data);
			theirkey = buffer[0].p.key;
			strcpy(host,buffer[0].host);
			prt = buffer[0].port;
			
			buffer.erase(0);
		}

		return true;
	}

	log(DEBUG,"connection::RecvPacket(): Invalid packet type %d (protocol error)",p.type);
	return true;
}

