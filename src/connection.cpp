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

#include "inspircd.h"
#include "connection.h"
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
#include "modules.h"
#include "inspstring.h"
#include "helperfuncs.h"

extern char ServerName[MAXBUF];
extern serverrec* me[32];
extern bool has_been_netsplit;
extern std::vector<Module*> modules;
extern std::vector<ircd_module*> factory;

extern int MODCOUNT;

extern time_t TIME;


/**
 * The InspIRCd mesh network is maintained by a tree of objects which reference *themselves*.
 * Every local server has an array of 32 *serverrecs, known as me[]. Each of these represents
 * a local listening port, and is not null if the user has opened a listening port on the server.
 * It is assumed nobody will ever want to open more than 32 listening server ports at any one
 * time (i mean come on, why would you want more, the ircd works fine with ONE).
 * Each me[] entry has multiple classes within it of type ircd_connector. These are stored in a vector
 * and each represents a server linked via this socket. If the connection was created outbound,
 * the connection is bound to the default ip address by using me[defaultRoute] (defaultRoute being
 * a global variable which indicates the default server to make connections on). If the connection
 * was created inbound, it is attached to the port the connection came in on. There may be as many
 * ircd_connector objects as needed in each me[] entry. Each ircd_connector implements the specifics
 * of an ircd connection in the mesh, however each ircd may have multiple ircd_connector connections
 * to it, to maintain the mesh link.
 */

char* xsumtable = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";

// creates a random id for a line for detection of duplicate messages
std::string CreateSum()
{
	char sum[9];
	sum[0] = ':';
	sum[8] = '\0';
	for(int q = 1; q < 8; q++)
		sum[q] = xsumtable[rand()%52];
	return sum;
}

connection::connection()
{
	fd = -1;
}


ircd_connector::ircd_connector()
{
	fd = -1;
	port = 0;
        sendq = "";
        WriteError = "";
	nextping = TIME+120;
	replied = false;
}

char* ircd_connector::GetServerIP()
{
	return this->host;
}

int ircd_connector::GetServerPort()
{
	return this->port;
}

bool ircd_connector::SetHostAndPort(char* newhost, int newport)
{
	strncpy(this->host,newhost,160);
	this->port = newport;
	return true;
}

bool ircd_connector::SetHostAddress(char* newhost, int newport)
{
	strncpy(this->host,newhost,160);
	this->port = newport;
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

bool ircd_connector::AddBuffer(std::string a)
{
	std::string b = "";
	for (unsigned int i = 0; i < a.length(); i++)
		if (a[i] != '\r')
			b = b + a[i];

	std::stringstream stream(ircdbuffer);
	stream << b;
	log(DEBUG,"AddBuffer: %s",b.c_str());
	ircdbuffer = stream.str();
	return (ircdbuffer.length() < 1048576);
}

bool ircd_connector::BufferIsComplete()
{
	for (unsigned int i = 0; i < ircdbuffer.length(); i++)
		if (ircdbuffer[i] == '\n')
			return true;
	return false;
}

void ircd_connector::ClearBuffer()
{
	ircdbuffer = "";
}

std::string ircd_connector::GetBuffer()
{
	// Fix by Brain 28th Apr 2005
	// seems my stringstream code isnt liked by linux
	// EVEN THOUGH IT IS CORRECT! Fixed by using a different
	// (SLOWER) algorithm...
        char* line = (char*)ircdbuffer.c_str();
        std::string ret = "";
        while ((*line != '\n') && (strlen(line)))
        {
                ret = ret + *line;
                line++;
        }
        if ((*line == '\n') || (*line == '\r'))
                line++;
        ircdbuffer = line;
        return ret;
}

bool ircd_connector::AddWriteBuf(std::string data)
{
	log(DEBUG,"connector::AddWriteBuf(%s)",data.c_str());
        if (this->GetWriteError() != "")
                return false;
	if (this->GetState() == STATE_DISCONNECTED)
		return false;
        std::stringstream stream;
        stream << sendq << data;
        sendq = stream.str();
	return (sendq.length() < 1048576);
}

bool ircd_connector::HasBufferedOutput()
{
	return (sendq.length() > 0);
}

bool ircd_connector::CheckPing()
{
	if (TIME > this->nextping)
	{
		if (this->replied)
		{
			this->AddWriteBuf("?\n");
			this->nextping = TIME+120;
			this->replied = false;
			return true;
		}
		else
		{
			if (this->GetState() == STATE_CONNECTED)
			{
				this->SetWriteError("Ping timeout");
				this->CloseConnection();
				this->SetState(STATE_DISCONNECTED);
				WriteOpers("*** Ping timeout on link to %s (more routes may remain)",this->GetServerName().c_str());
				has_been_netsplit = true;
				return false;
			}
		}
	}
	return true;
}

void ircd_connector::ResetPing()
{
	log(DEBUG,"Reset ping counter");
	this->replied = true;
	this->nextping = TIME+120;
}

// send AS MUCH OF THE USERS SENDQ as we are able to (might not be all of it)
bool ircd_connector::FlushWriteBuf()
{
	char buffer[MAXBUF];
	if ((this->GetState() == STATE_NOAUTH_OUTBOUND) || (this->GetState() == STATE_COOKIE_OUTBOUND))
	{
		// if the outbound socket hasnt connected yet... return true and don't
		// actually do anything until it IS connected. This should probably
		// have a timeout somewhere, 10 secs should suffice. ;-)
                pollfd polls;
                polls.fd = this->fd;
                polls.events = POLLOUT;
                int ret = poll(&polls,1,1);
		if (ret < 1)
			return true;
		// this falls through and sends any waiting data, which can put it into the
		// connected state.
		if (this->GetState() == STATE_COOKIE_OUTBOUND)
		{
			log(DEBUG,"Moving cookie_outbound into STATE_CONNECTED state");
			this->SetState(STATE_CONNECTED);
                        for (int t = 0; t < 32; t++) if (me[t]) for (unsigned int l = 0; l < me[t]->connectors.size(); l++)
                        {
				if (me[t]->connectors[l].GetDescription() != "")
				{
					snprintf(buffer,MAXBUF,"%s = %s :%s\r\n",CreateSum().c_str(),me[t]->connectors[l].GetServerName().c_str(),me[t]->connectors[l].GetDescription().c_str());
                                        this->AddWriteBuf(buffer);
                                }
                        }
		}
		snprintf(buffer,MAXBUF,"%s v %s %s\r\n",CreateSum().c_str(),ServerName,GetVersionString().c_str());
		this->AddWriteBuf(buffer);
	}
        if ((sendq.length()) && (this->GetState() != STATE_DISCONNECTED))
        {
                char* tb = (char*)this->sendq.c_str();
                int n_sent = write(this->fd,tb,this->sendq.length());
		if (n_sent != 0)
		{
                	if (n_sent == -1)
                	{
                        	this->SetWriteError(strerror(errno));
				return false;
                	}
                	else
                	{
				log(DEBUG,"Wrote %d chars to socket",n_sent);
                        	// advance the queue
       	                	tb += n_sent;
       	                	this->sendq = tb;
				return true;
	                }
		}
        }
	return true;
}

void ircd_connector::SetWriteError(std::string error)
{
        if (this->WriteError == "")
                this->WriteError = error;
}

std::string ircd_connector::GetWriteError()
{
        return this->WriteError;
}


bool ircd_connector::MakeOutboundConnection(char* newhost, int newport)
{
	log(DEBUG,"MakeOutboundConnection: Original param: %s",newhost);
	ClearBuffer();
	hostent* hoste = gethostbyname(newhost);
	if (!hoste)
	{
		log(DEBUG,"MakeOutboundConnection: gethostbyname was NULL, setting %s",newhost);
		this->SetHostAddress(newhost,newport);
		SetHostAndPort(newhost,newport);
	}
	else
	{
		struct in_addr* ia = (in_addr*)hoste->h_addr;
		log(DEBUG,"MakeOutboundConnection: gethostbyname was valid, setting %s",inet_ntoa(*ia));
		this->SetHostAddress(inet_ntoa(*ia),newport);
		SetHostAndPort(inet_ntoa(*ia),newport);
	}

	this->fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (this->fd >= 0)
	{
		int flags = fcntl(this->fd, F_GETFL, 0);
		fcntl(this->fd, F_SETFL, flags | O_NONBLOCK);
		if(connect(this->fd, (sockaddr*)&this->addr,sizeof(this->addr)) == -1)
		{
			if (errno != EINPROGRESS)
			{
				WriteOpers("connect() failed for %s",host);
				return false;
			}
		}
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


void ircd_connector::SetVersionString(std::string newversion)
{
	log(DEBUG,"Set version of %s to %s",this->servername.c_str(),newversion.c_str());
	this->version = newversion;
}

std::string ircd_connector::GetVersionString()
{
	return this->version;
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


void ircd_connector::SetState(int newstate)
{
	this->state = newstate;
}

void ircd_connector::CloseConnection()
{
	log(DEBUG,"Closing connection");
	// flush the queues
	shutdown(this->fd,2);
	close(this->fd);
}

void ircd_connector::SetDescriptor(int newfd)
{
	this->fd = newfd;
}
