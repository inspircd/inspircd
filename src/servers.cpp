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

using namespace std;

#include "inspircd_config.h" 
#include "servers.h"
#include "inspircd.h"
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <sys/errno.h>
#include <sys/ioctl.h>
#include <sys/utsname.h>
#include <vector>
#include <string>
#include <deque>
#include <sstream>
#include <map>
#include "inspstring.h"
#include "helperfuncs.h"
#include "connection.h"

extern time_t TIME;
extern int MaxConn;

extern serverrec* me[32];

extern bool has_been_netsplit;

std::deque<std::string> xsums;

serverrec::serverrec()
{
	strlcpy(name,"",256);
	pingtime = 0;
	lastping = TIME;
	usercount_i = usercount = opercount = version = 0;
	hops_away = 1;
	signon = TIME;
	jupiter = false;
	fd = 0;
	sync_soon = false;
	strlcpy(nickserv,"",NICKMAX);
	connectors.clear();
}

 
serverrec::~serverrec()
{
}

serverrec::serverrec(char* n, long ver, bool jupe)
{
	strlcpy(name,n,256);
	lastping = TIME;
	usercount_i = usercount = opercount = 0;
	version = ver;
	hops_away = 1;
	signon = TIME;
	jupiter = jupe;
	fd = 0;
	sync_soon = false;
	strlcpy(nickserv,"",NICKMAX);
	connectors.clear();
}

bool serverrec::CreateListener(char* newhost, int p)
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

        if (!strcmp(newhost,""))
        {
                host_address.sin_addr.s_addr = htonl(INADDR_ANY);
        }
        else
        {
                inet_aton(newhost,&addy);
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

        listen(this->fd, MaxConn);

        return true;
}


bool serverrec::BeginLink(char* targethost, int newport, char* password, char* servername, int myport)
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
                if (connector.MakeOutboundConnection(targethost,newport))
                {
                        // targethost has been turned into an ip...
                        // we dont want this as the server name.
                        connector.SetServerName(servername);
                        snprintf(connect,MAXBUF,"S %s %s %lu %s :%s",getservername().c_str(),password,(unsigned long)myport,GetRevision().c_str(),getserverdesc().c_str());
                        connector.SetState(STATE_NOAUTH_OUTBOUND);
                        connector.SetHostAndPort(targethost, newport);
                        this->connectors.push_back(connector);
			// this packet isn't actually sent until the socket connects -- the STATE_NOAUTH_OUTBOUND state
			// queues outbound data until the socket is polled as writeable (e.g. the connection is established)
                        return this->SendPacket(connect, servername);
                }
                else
                {
                        connector.SetState(STATE_DISCONNECTED);
                        WriteOpers("Could not create outbound connection to %s:%d",targethost,newport);
                }
        }
        return false;
}


bool serverrec::MeshCookie(char* targethost, int newport, unsigned long cookie, char* servername)
{
        char connect[MAXBUF];

        ircd_connector connector;

        WriteOpers("Establishing meshed link to %s:%d",servername,newport);

        if (this->fd)
        {
                if (connector.MakeOutboundConnection(targethost,newport))
                {
                        // targethost has been turned into an ip...
                        // we dont want this as the server name.
                        connector.SetServerName(servername);
                        snprintf(connect,MAXBUF,"- %lu %s :%s",cookie,getservername().c_str(),getserverdesc().c_str());
                        connector.SetState(STATE_NOAUTH_OUTBOUND);
                        connector.SetHostAndPort(targethost, newport);
                        this->connectors.push_back(connector);
                        return this->SendPacket(connect, servername);
                }
                else
                {
                        connector.SetState(STATE_DISCONNECTED);
                        WriteOpers("Could not create outbound connection to %s:%d",targethost,newport);
                }
        }
        return false;
}

bool serverrec::AddIncoming(int newfd, char* targethost, int sourceport)
{
        ircd_connector connector;

        // targethost has been turned into an ip...
        // we dont want this as the server name.
        connector.SetServerName(targethost);
        connector.SetDescriptor(newfd);
        connector.SetState(STATE_NOAUTH_INBOUND);
        int flags = fcntl(newfd, F_GETFL, 0);
        fcntl(newfd, F_SETFL, flags | O_NONBLOCK);
        int sendbuf = 32768;
        int recvbuf = 32768;
        setsockopt(newfd,SOL_SOCKET,SO_SNDBUF,(const void *)&sendbuf,sizeof(sendbuf));
        setsockopt(newfd,SOL_SOCKET,SO_RCVBUF,(const void *)&recvbuf,sizeof(sendbuf));
        connector.SetHostAndPort(targethost, sourceport);
        connector.SetState(STATE_NOAUTH_INBOUND);
        log(DEBUG,"serverrec::AddIncoming() Added connection: %s:%d",targethost,sourceport);
        this->connectors.push_back(connector);
        return true;
}

void serverrec::TerminateLink(char* targethost)
{
        // this locates the targethost in the serverrec::connectors vector of the class,
       // and terminates it by sending it an SQUIT token and closing its descriptor.
        // TerminateLink with a null string causes a terminate of ALL links
}

// Returns a pointer to the connector for 'host'
ircd_connector* serverrec::FindHost(std::string findhost)
{
        for (int i = 0; i < this->connectors.size(); i++)
        {
                if (this->connectors[i].GetServerName() == findhost)
                {
                        return &this->connectors[i];
                }
        }
        return NULL;
}


// Checks to see if we can still reach a server at all (e.g. is it in ANY routing table?)
bool IsRoutable(std::string servername)
{
	for (int x = 0; x < 32; x++) if (me[x])
	{
        	ircd_connector* cn = me[x]->FindHost(servername.c_str());
        	if (cn)
        	{
                	if (cn->GetState() == STATE_DISCONNECTED)
                	{
                        	for (int k = 0; k < me[x]->connectors.size(); k++)
                        	{
                                	for (int m = 0; m < me[x]->connectors[k].routes.size(); m++)
                                	{
                                        	if (!strcasecmp(me[x]->connectors[k].routes[m].c_str(),servername.c_str()))
                                        	{
                                                	return true;
                                        	}
                                	}
                        	}
       	                	return false;
       	        	}
			else return true;
		}
	}
	return false;
}


void serverrec::FlushWriteBuffers()
{
	for (int i = 0; i < this->connectors.size(); i++)
	{
		// don't try and ping a NOAUTH_OUTBOUND state, its not authed yet!
		if ((this->connectors[i].GetState() == STATE_NOAUTH_OUTBOUND) && (TIME > this->connectors[i].age+30))
		{
			// however if we reach this timer its connected timed out :)
			WriteOpers("*** Connection to %s timed out",this->connectors[i].GetServerName().c_str());
			DoSplit(this->connectors[i].GetServerName().c_str());
			return;
		}
		else if ((this->connectors[i].GetState() == STATE_NOAUTH_INBOUND) && (TIME > this->connectors[i].age+30))
		{
			WriteOpers("*** Connection from %s timed out",this->connectors[i].GetServerName().c_str());
			DoSplit(this->connectors[i].GetServerName().c_str());
			return;
		}
		else if (this->connectors[i].GetState() != STATE_DISCONNECTED)
		{
			if (!this->connectors[i].CheckPing())
			{
				WriteOpers("*** Lost single connection to %s: Ping timeout",this->connectors[i].GetServerName().c_str());
				this->connectors[i].CloseConnection();
				this->connectors[i].SetState(STATE_DISCONNECTED);
				if (!IsRoutable(this->connectors[i].GetServerName()))
				{
					WriteOpers("*** Server %s is no longer routable, disconnecting.",this->connectors[i].GetServerName().c_str());
					DoSplit(this->connectors[i].GetServerName().c_str());
				}
				has_been_netsplit = true;
			}
		}
                if (this->connectors[i].HasBufferedOutput())
                {
			if (!this->connectors[i].FlushWriteBuf())
			{
				// if we're here the write() caused an error, we cannot proceed
				WriteOpers("*** Lost single connection to %s, link inactive and retrying: %s",this->connectors[i].GetServerName().c_str(),this->connectors[i].GetWriteError().c_str());
				this->connectors[i].CloseConnection();
	                        this->connectors[i].SetState(STATE_DISCONNECTED);
                                if (!IsRoutable(this->connectors[i].GetServerName()))
                                {
                                        WriteOpers("*** Server %s is no longer routable, disconnecting.",this->connectors[i].GetServerName().c_str());
                                        DoSplit(this->connectors[i].GetServerName().c_str());
                                }
				has_been_netsplit = true;
			}
                }
	}
}

bool serverrec::SendPacket(char *message, const char* sendhost)
{
        if ((!message) || (!sendhost))
                return true;

        ircd_connector* cn = this->FindHost(sendhost);

        if (!strchr(message,'\n'))
        {
                strlcat(message,"\n",MAXBUF);
        }

        if (cn)
        {
                log(DEBUG,"main: serverrec::SendPacket() sent '%s' to %s",message,cn->GetServerName().c_str());

                if (cn->GetState() == STATE_DISCONNECTED)
                {
                        // fix: can only route one hop to avoid a loop
                        if (strncmp(message,"R ",2))
                        {
                                log(DEBUG,"Not a double reroute");
                                // this route is down, we must re-route the packet through an available point in the mesh.
                                for (int k = 0; k < this->connectors.size(); k++)
                                {
                                        log(DEBUG,"Check connector %d: %s",k,this->connectors[k].GetServerName().c_str());
                                        // search for another point in the mesh which can 'reach' where we want to go
                                        for (int m = 0; m < this->connectors[k].routes.size(); m++)
                                        {
                                                if (!strcasecmp(this->connectors[k].routes[m].c_str(),sendhost))
                                                {
                                                        log(DEBUG,"Found alternative route for packet: %s",this->connectors[k].GetServerName().c_str());
                                                        char buffer[MAXBUF];
                                                        snprintf(buffer,MAXBUF,"R %s %s",sendhost,message);
                                                        this->SendPacket(buffer,this->connectors[k].GetServerName().c_str());
                                                        return true;
                                                }
                                        }
                                }
                        }
                        char buffer[MAXBUF];
                        snprintf(buffer,MAXBUF,"& %s",sendhost);
			WriteOpers("*** All connections to %s lost.",sendhost);
                        NetSendToAllExcept(sendhost,buffer);
                        DoSplit(sendhost);
                        return false;
                }

                // returns false if the packet could not be sent (e.g. target host down)
                if (!cn->AddWriteBuf(message))
                {
			// if we're here, there was an error pending, and the send cannot proceed
                        log(DEBUG,"cn->AddWriteBuf() failed for serverrec::SendPacket(): %s",cn->GetWriteError().c_str());
                        log(DEBUG,"Disabling connector: %s",cn->GetServerName().c_str());
                        cn->CloseConnection();
                        cn->SetState(STATE_DISCONNECTED);
			WriteOpers("*** Lost single connection to %s, link inactive and retrying: %s",cn->GetServerName().c_str(),cn->GetWriteError().c_str());
                        // retry the packet along a new route so either arrival OR failure are gauranteed (bugfix)
                        return this->SendPacket(message,sendhost);
                }
		if (!cn->FlushWriteBuf())
		{
			// if we're here the write() caused an error, we cannot proceed
			log(DEBUG,"cn->FlushWriteBuf() failed for serverrec::SendPacket(): %s",cn->GetWriteError().c_str());
			log(DEBUG,"Disabling connector: %s",cn->GetServerName().c_str());
			cn->CloseConnection();
			cn->SetState(STATE_DISCONNECTED);
			WriteOpers("*** Lost single connection to %s, link inactive and retrying: %s",cn->GetServerName().c_str(),cn->GetWriteError().c_str());
			// retry the packet along a new route so either arrival OR failure are gauranteed
			return this->SendPacket(message,sendhost);
		}
                return true;
        }
}

bool already_have_sum(std::string sum)
{
        for (int i = 0; i < xsums.size(); i++)
        {
                if (xsums[i] == sum)
                {
                        return true;
                }
        }
        if (xsums.size() >= 128)
        {
                xsums.pop_front();
        }
        xsums.push_back(sum);
        return false;
}

// receives a packet from any where there is data waiting, first come, first served
// fills the message and host values with the host where the data came from.

bool serverrec::RecvPacket(std::deque<std::string> &messages, char* recvhost,std::deque<std::string> &sums)
{
        char data[65536];
        memset(data, 0, 65536);
        for (int i = 0; i < this->connectors.size(); i++)
        {
                if (this->connectors[i].GetState() != STATE_DISCONNECTED)
                {
                        // returns false if the packet could not be sent (e.g. target host down)
                        int rcvsize = 0;

                        // check if theres any data on this socket
                        // if not, continue onwards to the next.
                        pollfd polls;
                        polls.fd = this->connectors[i].GetDescriptor();
                        polls.events = POLLIN;
                        int ret = poll(&polls,1,1);
                        if (ret <= 0) continue;

                        rcvsize = recv(this->connectors[i].GetDescriptor(),data,65000,0);
                        data[rcvsize] = '\0';
                        if (rcvsize == -1)
                        {
                                if (errno != EAGAIN)
                                {
                                        log(DEBUG,"recv() failed for serverrec::RecvPacket(): %s",strerror(errno));
                                        log(DEBUG,"Disabling connector: %s",this->connectors[i].GetServerName().c_str());
                                        this->connectors[i].CloseConnection();
                                        this->connectors[i].SetState(STATE_DISCONNECTED);
                                	if (!IsRoutable(this->connectors[i].GetServerName()))
                        	        {
                	                        WriteOpers("*** Server %s is no longer routable, disconnecting.",this->connectors[i].GetServerName().c_str());
        	                                DoSplit(this->connectors[i].GetServerName().c_str());
	                                }
					has_been_netsplit = true;
                                }
                        }
                        int pushed = 0;
                        if (rcvsize > 0)
                        {
                                if (!this->connectors[i].AddBuffer(data))
				{
					WriteOpers("*** Read buffer for %s exceeds maximum, closing connection!",this->connectors[i].GetServerName().c_str());
					this->connectors[i].CloseConnection();
					this->connectors[i].SetState(STATE_DISCONNECTED);
                                	if (!IsRoutable(this->connectors[i].GetServerName()))
                        	        {
                	                        WriteOpers("*** Server %s is no longer routable, disconnecting.",this->connectors[i].GetServerName().c_str());
        	                                DoSplit(this->connectors[i].GetServerName().c_str());
	                                }
					has_been_netsplit = true;
				}
                                if (this->connectors[i].BufferIsComplete())
                                {
					this->connectors[i].ResetPing();
                                        while (this->connectors[i].BufferIsComplete())
                                        {
                                                std::string text = this->connectors[i].GetBuffer();
                                                if (text != "")
                                                {
                                                        if ((text[0] == ':') && (text.find(" ") != std::string::npos))
                                                        {
                                                                std::string orig = text;
                                                                log(DEBUG,"Original: %s",text.c_str());
                                                                std::string sum = text.substr(1,text.find(" ")-1);
                                                                text = text.substr(text.find(" ")+1,text.length());
                                                                std::string possible_token = text.substr(1,text.find(" ")-1);
                                                                if (possible_token.length() > 1)
                                                                {
                                                                        sums.push_back("*");
                                                                        text = orig;
                                                                        log(DEBUG,"Non-mesh, non-tokenized string passed up the chain");
                                                                }
                                                                else
                                                                {
                                                                        log(DEBUG,"Packet sum: '%s'",sum.c_str());
                                                                        if ((already_have_sum(sum)) && (sum != "*"))
                                                                        {
                                                                                // we don't accept dupes
                                                                                continue;
                                                                        }
                                                                        sums.push_back(sum.c_str());
                                                                }
                                                        }
                                                        else sums.push_back("*");
                                                        messages.push_back(text.c_str());
                                                        strlcpy(recvhost,this->connectors[i].GetServerName().c_str(),160);
                                                        log(DEBUG,"serverrec::RecvPacket() %d:%s->%s",pushed++,recvhost,text.c_str());
                                                }
                                        }
                                        return true;
                                }
                        }
                }
        }
        // nothing new yet -- message and host will be undefined
        return false;
}

