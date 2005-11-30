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

#include <stdio.h>
#include <string>
#include <stdlib.h>
#include <time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include "users.h"
#include "channels.h"
#include "modules.h"
#include "inspircd.h"

/* $ModDesc: Provides support for RFC 1413 ident lookups */

Server *Srv;

// State engine constants. We have three states,
// connecting, waiting for data, and finished.

#define IDENT_STATE_CONNECT	1
#define IDENT_STATE_WAITDATA	2
#define IDENT_STATE_DONE	3

// Ident lookups are done by attaching an RFC1413 class to the
// userrec record using the Extensible system.
// The RFC1413 class is written especially for this module but
// it should be relatively standalone for anyone else who wishes
// to have a nonblocking ident lookup in a program :)
// the class operates on a simple state engine, each state of the
// connection incrementing a state counter, leading through to
// a concluding state which terminates the lookup.

class RFC1413
{
 protected:
	int fd;			 // file descriptor
	userrec* u;		 // user record that the lookup is associated with
	sockaddr_in addr;	 // address we're connecting to
	in_addr addy;		 // binary ip address
	int state;		 // state (this class operates on a state engine)
	char ibuf[MAXBUF];	 // input buffer
	sockaddr_in sock_us;	 // our port number
	sockaddr_in sock_them;	 // their port number
	socklen_t uslen;	 // length of our port number
	socklen_t themlen;	 // length of their port number
	int nrecv;		 // how many bytes we've received
	time_t timeout_end;	 // how long until the operation times out
	bool timeout;		 // true if we've timed out and should bail
	char ident_request[128]; // buffer used to make up the request string
 public:

	// The destructor makes damn sure the socket is freed :)

	~RFC1413()
	{
		if (this->fd != -1)
		{
			shutdown(this->fd,2);
			close(this->fd);
			this->fd = -1;
		}
	}

	// establish an ident connection, maxtime is the time to spend trying
	// returns true if successful, false if something was catastrophically wrong.
	// note that failed connects are not reported here but detected in RFC1413::Poll()
	// as the socket is nonblocking

	bool Connect(userrec* user, int maxtime)
	{
		timeout_end = time(NULL)+maxtime;
		timeout = false;
		if ((this->fd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
		{
			// theres been a boo-boo... no more fd's left for us, woe is me!
			Srv->Log(DEBUG,"Ident: socket failed for: "+std::string(user->ip));
			return false;
		}
		inet_aton(user->ip,&addy);
		addr.sin_family = AF_INET;
		addr.sin_addr = addy;
		addr.sin_port = htons(113);

                int flags;
                flags = fcntl(this->fd, F_GETFL, 0);
                fcntl(this->fd, F_SETFL, flags | O_NONBLOCK);

		if(connect(this->fd, (sockaddr*)&this->addr,sizeof(this->addr)) == -1)
		{
			// theres been an error, but EINPROGRESS just means 'right, im on it, call me later'
			if (errno != EINPROGRESS)
			{
				// ... so that error isnt fatal, like the rest.
				Srv->Log(DEBUG,"Ident: connect failed for: "+std::string(user->ip));
				shutdown(this->fd,2);
                                close(this->fd);
				this->fd = -1;
	                        return false;
			}
                }
		Srv->Log(DEBUG,"Ident: successful connect associated with user "+std::string(user->nick));
		this->u = user;
		this->state = IDENT_STATE_CONNECT;
		return true;
	}

	// Poll the socket to see if we have an ident result, and if we do apply it to the user.
	// returns false if we cannot poll for some reason (e.g. timeout).

	bool Poll()
	{
		if (time(NULL) > timeout_end)
		{
			timeout = true;
			Srv->SendServ(u->fd,"NOTICE "+std::string(u->nick)+" :*** Could not find your ident, using "+std::string(u->ident)+" instead.");
			shutdown(this->fd,2);
			close(this->fd);
			this->fd = -1;
			return false;
		}
		pollfd polls;
		polls.fd = this->fd;
		if (state == IDENT_STATE_CONNECT)
		{
			// during state IDENT_STATE_CONNECT (leading up to the connect)
			// we're watching for writeability
			polls.events = POLLOUT;
		}
		else
		{
			// the rest of the time we're waiting for data
			// back on the socket, or a socket close
			polls.events = POLLIN;
		}
		int ret = poll(&polls,1,1);

		if (ret > 0)
		{
			switch (this->state)
			{
				case IDENT_STATE_CONNECT:
					uslen = sizeof(sock_us);
					themlen = sizeof(sock_them);
					if ((getsockname(this->u->fd,(sockaddr*)&sock_us,&uslen) || getpeername(this->u->fd, (sockaddr*)&sock_them, &themlen)))
					{
						Srv->Log(DEBUG,"Ident: failed to get socket names, bailing to state 3");
						shutdown(this->fd,2);
                                                close(this->fd);
						this->fd = -1;
						state = IDENT_STATE_DONE;
					}
					else
					{
						// send the request in the following format: theirsocket,oursocket
						snprintf(ident_request,127,"%d,%d\r\n",ntohs(sock_them.sin_port),ntohs(sock_us.sin_port));
						send(this->fd,ident_request,strlen(ident_request),0);
						Srv->Log(DEBUG,"Sent ident request, moving to state 2");
						state = IDENT_STATE_WAITDATA;
					}
				break;
				case IDENT_STATE_WAITDATA:
					nrecv = recv(this->fd,ibuf,sizeof(ibuf),0);
					if (nrecv > 0)
					{
						// we have the response line in the following format:
						// 6193, 23 : USERID : UNIX : stjohns
						// 6195, 23 : ERROR : NO-USER
						ibuf[nrecv] = '\0';
						Srv->Log(DEBUG,"Received ident response: "+std::string(ibuf));
						shutdown(this->fd,2);
						close(this->fd);
						this->fd = -1;
						char* savept;
						char* section = strtok_r(ibuf,":",&savept);
						while (section)
						{
							if (strstr(section,"USERID"))
							{
								section = strtok_r(NULL,":",&savept);
								if (section)
								{
									// ID type, usually UNIX or OTHER... we dont want it, so read the next token
									section = strtok_r(NULL,":",&savept);
									if (section)
									{
										while ((*section == ' ') && (strlen(section)>0)) section++; // strip leading spaces
										int t = strlen(section);
										for (int j = 0; j < t; j++)
											if ((section[j] < 33) || (section[j]>126))
												section[j] = '\0'; // truncate at invalid chars
										if (strlen(section))
										{
											strlcpy(u->ident,section,IDENTMAX);
											Srv->Log(DEBUG,"IDENT SET: "+std::string(u->ident));
											Srv->SendServ(u->fd,"NOTICE "+std::string(u->nick)+" :*** Found your ident: "+std::string(u->ident));
										}
										break;
									}
								}
							}
							section = strtok_r(NULL,":",&savept);
						}
						state = IDENT_STATE_DONE;
					}
				break;
				case IDENT_STATE_DONE:
					shutdown(this->fd,2);
					close(this->fd);
					this->fd = -1;
					Srv->Log(DEBUG,"Ident lookup is complete!");
				break;
				default:
					Srv->Log(DEBUG,"Ident: invalid ident state!!!");
				break;
			}
		}
		return true;
	}

	// returns true if the operation is completed,
	// either due to complete request, or a timeout

	bool Done()
	{
		return ((state == IDENT_STATE_DONE) || (timeout == true));
	}
};

class ModuleIdent : public Module
{

	ConfigReader* Conf;
	int IdentTimeout;

 public:
	void ReadSettings()
	{
		Conf = new ConfigReader;
		IdentTimeout = Conf->ReadInteger("ident","timeout",0,true);
		delete Conf;
	}

	ModuleIdent()
	{
		Srv = new Server;
		ReadSettings();
	}

	virtual void OnRehash(std::string parameter)
	{
		ReadSettings();
	}

	virtual void OnUserRegister(userrec* user)
	{
		// when the new user connects, before they authenticate with USER/NICK/PASS, we do
		// their ident lookup.

		RFC1413* ident = new RFC1413;
		Srv->SendServ(user->fd,"NOTICE "+std::string(user->nick)+" :*** Looking up your ident...");
		if (ident->Connect(user,IdentTimeout))
		{
			// attach the object to the user record
			user->Extend("ident_data",(char*)ident);
			// start it off polling (always good to have a head start)
			// because usually connect has completed by now
			ident->Poll();
		}
		else
		{
			// something went wrong, call an irc-ambulance!
			Srv->SendServ(user->fd,"NOTICE "+std::string(user->nick)+" :*** Could not look up your ident.");
			delete ident;
		}
	}

	virtual bool OnCheckReady(userrec* user)
	{
		RFC1413* ident = (RFC1413*)user->GetExt("ident_data");
		if (ident)
		{
			// this user has a pending ident lookup, poll it
			ident->Poll();
			// is it done?
			if (ident->Done())
			{
				// their ident is done, zap the structures
				Srv->Log(DEBUG,"Ident: removing ident gubbins");
				user->Shrink("ident_data");
				delete ident;
				// ...and send them on their way
				return true;
			}
			// nope, we hold them in this state, they dont go anywhere
			return false;
		}
		return true;
	}

        virtual void OnUserDisconnect(userrec* user)
        {
                // when the user quits tidy up any ident lookup they have pending to keep things tidy
                // and to prevent a memory and FD leaks
		RFC1413* ident = (RFC1413*)user->GetExt("ident_data");
                if (ident)
                {
                        delete ident;
                        user->Shrink("ident_data");
                }
        }
	
	virtual ~ModuleIdent()
	{
		delete Srv;
	}
	
	virtual Version GetVersion()
	{
		return Version(1,0,0,1,VF_VENDOR);
	}
	
};

class ModuleIdentFactory : public ModuleFactory
{
 public:
	ModuleIdentFactory()
	{
	}
	
	~ModuleIdentFactory()
	{
	}
	
	virtual Module * CreateModule()
	{
		return new ModuleIdent;
	}
	
};


extern "C" void * init_module( void )
{
	return new ModuleIdentFactory;
}

