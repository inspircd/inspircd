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

using namespace std;

#include <stdio.h>
#include <string>
#include "users.h"
#include "channels.h"
#include "modules.h"
#include "inspircd.h"

extern userrec* fd_ref_table[MAX_DESCRIPTORS];

/* $ModDesc: Provides support for RFC 1413 ident lookups */

// Version 1.5.0.0 - Updated to use InspSocket, faster and neater.

class RFC1413 : public InspSocket
{
 protected:
	Server* Srv;		 // Server* class used for core communications
	sockaddr_in sock_us;	 // our port number
	sockaddr_in sock_them;	 // their port number
	socklen_t uslen;	 // length of our port number
	socklen_t themlen;	 // length of their port number
	char ident_request[128]; // buffer used to make up the request string
 public:

	userrec* u;		 // user record that the lookup is associated with
	int ufd;

	RFC1413(userrec* user, int maxtime, Server* S) : InspSocket((char*)inet_ntoa(user->ip4), 113, false, maxtime), Srv(S), u(user), ufd(user->fd)
	{
		Srv->Log(DEBUG,"Ident: associated.");
	}

	virtual void OnTimeout()
	{
		// When we timeout, the connection failed within the allowed timeframe,
		// so we just display a notice, and tidy off the ident_data.
		if (u && (fd_ref_table[ufd] == u))
		{
			u->Shrink("ident_data");
			Srv->SendServ(u->fd,"NOTICE "+std::string(u->nick)+" :*** Could not find your ident, using "+std::string(u->ident)+" instead.");
		}
	}

	virtual bool OnDataReady()
	{
		char* ibuf = this->Read();
		if (ibuf)
		{
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
							while (*section == ' ') section++; // strip leading spaces
							for (char* j = section; *j; j++)
							if ((*j < 33) || (*j > 126))
								*j = '\0'; // truncate at invalid chars
							if (*section)
							{
								if (u && (fd_ref_table[ufd] == u))
								{
									strlcpy(u->ident,section,IDENTMAX);
									Srv->Log(DEBUG,"IDENT SET: "+std::string(u->ident));
									Srv->SendServ(u->fd,"NOTICE "+std::string(u->nick)+" :*** Found your ident: "+std::string(u->ident));
								}
							}
							return false;
						}
					}
				}
				section = strtok_r(NULL,":",&savept);
			}
		}
		return false;
	}

	virtual void OnClose()
	{
		// tidy up after ourselves when the connection is done.
		// We receive this event straight after a timeout, too.
		//
		//
		// OK, now listen up. The weird looking check here is
		// REQUIRED. Don't try and optimize it away.
		//
		// When a socket is closed, it is not immediately removed
		// from the socket list, there can be a short delay
		// before it is culled from the list. This means that
		// without this check, there is a chance that a user
		// may not exist when we come to ::Shrink them, which
		// results in a segfault. The value of "u" may not
		// always be NULL at this point, so, what we do is
		// check against the fd_ref_table, to see if (1) the user
		// exists, and (2) its the SAME user, on the same file
		// descriptor that they were when the lookup began.
		//
		// Fixes issue reported by webs, 7 Jun 2006
		if (u && (fd_ref_table[ufd] == u))
		{
			u->Shrink("ident_data");
		}
	}

	virtual void OnError(InspSocketError e)
	{
		if (u && (fd_ref_table[ufd] == u))
		{
			u->Shrink("ident_data");
		}
	}

	virtual bool OnConnected()
	{
		if (u && (fd_ref_table[ufd] == u))
		{
			uslen = sizeof(sock_us);
			themlen = sizeof(sock_them);
			if ((getsockname(this->u->fd,(sockaddr*)&sock_us,&uslen) || getpeername(this->u->fd, (sockaddr*)&sock_them, &themlen)))
			{
				Srv->Log(DEBUG,"Ident: failed to get socket names, bailing");
				return false;
			}
			else
			{
				// send the request in the following format: theirsocket,oursocket
				snprintf(ident_request,127,"%d,%d\r\n",ntohs(sock_them.sin_port),ntohs(sock_us.sin_port));
				this->Write(ident_request);
				Srv->Log(DEBUG,"Sent ident request, waiting for reply");
				return true;
			}
		}
		else
		{
			return true;
		}
	}
};

class ModuleIdent : public Module
{

	ConfigReader* Conf;
	Server* Srv;
	int IdentTimeout;

 public:
	void ReadSettings()
	{
		Conf = new ConfigReader;
		IdentTimeout = Conf->ReadInteger("ident","timeout",0,true);
		if (!IdentTimeout)
			IdentTimeout = 1;
		DELETE(Conf);
	}

	ModuleIdent(Server* Me)
		: Module::Module(Me)
	{
		Srv = Me;
		ReadSettings();
	}

	void Implements(char* List)
	{
		List[I_OnCleanup] = List[I_OnRehash] = List[I_OnUserRegister] = List[I_OnCheckReady] = List[I_OnUserDisconnect] = 1;
	}

	virtual void OnRehash(const std::string &parameter)
	{
		ReadSettings();
	}

	virtual void OnUserRegister(userrec* user)
	{
		/*
		 * when the new user connects, before they authenticate with USER/NICK/PASS, we do
		 * their ident lookup. We do this by instantiating an object of type RFC1413, which
		 * is derived from InspSocket, and inserting it into the socket engine using the
		 * Server::AddSocket() call.
		 */
		Srv->SendServ(user->fd,"NOTICE "+std::string(user->nick)+" :*** Looking up your ident...");
		RFC1413* ident = new RFC1413(user, IdentTimeout, Srv);
		if (ident->GetState() != I_ERROR)
		{
			user->Extend("ident_data", (char*)ident);
			Srv->AddSocket(ident);
		}
		else
		{
			Srv->SendServ(user->fd,"NOTICE "+std::string(user->nick)+" :*** Could not find your ident, using "+std::string(user->ident)+" instead.");
			DELETE(ident);
		}
	}

	virtual bool OnCheckReady(userrec* user)
	{
		/*
		 * The socket engine will clean up their ident request for us when it completes,
		 * either due to timeout or due to closing, so, we just hold them until they dont
		 * have an ident field any more.
		 */
		RFC1413* ident;
		return (!user->GetExt("ident_data", ident));
	}

	virtual void OnCleanup(int target_type, void* item)
	{
		if (target_type == TYPE_USER)
		{
			userrec* user = (userrec*)item;
			RFC1413* ident;
			if (user->GetExt("ident_data", ident))
			{
				// FIX: If the user record is deleted, the socket wont be removed
				// immediately so there is chance of the socket trying to write to
				// a user which has now vanished! To prevent this, set ident::u
				// to NULL and check it so that we dont write users who have gone away.
				ident->u = NULL;
				Srv->RemoveSocket(ident);
			}
		}
	}

	virtual void OnUserDisconnect(userrec* user)
	{
		/*
		 * when the user quits tidy up any ident lookup they have pending to keep things tidy.
		 * When we call RemoveSocket, the abstractions tied into the system evnetually work their
		 * way to RFC1459::OnClose(), which shrinks off the ident_data for us, so we dont need
		 * to do it here. If we don't tidy this up, there may still be lingering idents for users
		 * who have quit, as class RFC1459 is only loosely bound to userrec* via a pair of pointers
		 * and this would leave at least one of the invalid ;)
		 */
		RFC1413* ident;
		if (user->GetExt("ident_data", ident))
		{
			ident->u = NULL;
			Srv->RemoveSocket(ident);
		}
	}
	
	virtual ~ModuleIdent()
	{
	}
	
	virtual Version GetVersion()
	{
		return Version(1,5,0,0,VF_VENDOR);
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
	
	virtual Module * CreateModule(Server* Me)
	{
		return new ModuleIdent(Me);
	}
	
};


extern "C" void * init_module( void )
{
	return new ModuleIdentFactory;
}

