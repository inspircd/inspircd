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
	int fd;			// file descriptor
	userrec* u;		// user record that the lookup is associated with
	sockaddr_in addr;	// address we're connecting to
	in_addr addy;		// binary ip address
	int state;		// state (this class operates on a state engine)
	char ibuf[MAXBUF];	// input buffer
	sockaddr_in sock_us;	// our port number
	sockaddr_in sock_them;	// their port number
	socklen_t uslen;	// length of our port number
	socklen_t themlen;	// length of their port number
	int nrecv;		// how many bytes we've received
	time_t timeout_end;	// how long until the operation times out
	bool timeout;		// true if we've timed out and should bail
 public:

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
			if (errno != EINPROGRESS)
			{
				Srv->Log(DEBUG,"Ident: connect failed for: "+std::string(user->ip));
	                        return false;
			}
                }
		Srv->Log(DEBUG,"Ident: successful connect associated with user "+std::string(user->nick));
		this->u = user;
		this->state = 1;
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
			return false;
		}
		pollfd polls;
		polls.fd = this->fd;
		if (state == 1)
		{
			polls.events = POLLOUT;
		}
		else
		{
			polls.events = POLLIN;
		}
		int ret = poll(&polls,1,1);

		if (ret > 0)
		{
			switch (this->state)
			{
				case 1:
					Srv->Log(DEBUG,"*** IDENT IN STATE 1");
					uslen = sizeof(sock_us);
					themlen = sizeof(sock_them);
					if ((getsockname(this->u->fd,(sockaddr*)&sock_us,&uslen) || getpeername(this->u->fd, (sockaddr*)&sock_them, &themlen)))
					{
						Srv->Log(DEBUG,"Ident: failed to get socket names, bailing to state 3");
						state = 3;
					}
					else
					{
						// send the request in the following format: theirsocket,oursocket
						Write(this->fd,"%d,%d",ntohs(sock_them.sin_port),ntohs(sock_us.sin_port));
						Srv->Log(DEBUG,"Sent ident request, moving to state 2");
						state = 2;
					}
				break;
				case 2:
					Srv->Log(DEBUG,"*** IDENT IN STATE 2");
					nrecv = recv(this->fd,ibuf,sizeof(ibuf),0);
					if (nrecv > 0)
					{
						// we have the response line in the following format:
						// 6193, 23 : USERID : UNIX : stjohns
						// 6195, 23 : ERROR : NO-USER
						ibuf[nrecv] = '\0';
						Srv->Log(DEBUG,"Received ident response: "+std::string(ibuf));
						close(this->fd);
						shutdown(this->fd,2);
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
						state = 3;
					}
				break;
				case 3:
					Srv->Log(DEBUG,"Ident lookup is complete!");
				break;
				default:
					Srv->Log(DEBUG,"Ident: invalid ident state!!!");
				break;
			}
		}
	}

	// returns true if the operation is completed,
	// either due to complete request, or a timeout

	bool Done()
	{
		return ((state == 3) || (timeout == true));
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

	virtual void OnRehash()
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

