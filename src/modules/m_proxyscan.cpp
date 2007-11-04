/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2007 InspIRCd Development Team
 * See: http://www.inspircd.org/wiki/index.php/Credits
 *
 * This program is free but copyrighted software; see
 *	    the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"
#include "users.h"
#include "channels.h"
#include "modules.h"

/* $ModDesc: Scans locally connecting clients for proxies. */

/*
 * How this works: basic overview.
 *  We create a socket type (derived from EventHandler -- don't feel like
 *  belting my head against the wall ala ident). For each test in the
 *  configuration file for each locally connecting client, we create a ProxySocket
 *  and run the associated test.
 *
 * The user is allowed to connect (delaying the connect might take ages..), and they
 * will be destroyed *if* a bad response comes back to a test.
 */

/*
 * A few things to note:
 * 
 *   O  The only place that may *delete* an active or inactive
 *      ident socket is OnUserDisconnect in the module class.
 *      Because this is out of scope of the socket class there is
 *      no possibility that the socket may ever try to delete
 *      itself.
 *
 *   O  Closure of the ident socket with the Close() method will
 *      not cause removal of the socket from memory or detatchment
 *      from its 'parent' User class. It will only flag it as an 
 *      inactive socket in the socket engine.
 *
 *   O  Timeouts are handled in OnCheckReaady at the same time as
 *      checking if the ident socket has a result. This is done
 *      by checking if the age the of the class (its instantiation
 *      time) plus the timeout value is greater than the current time.
 *
 *  O   The ident socket is able to but should not modify its
 *      'parent' user directly. Instead the ident socket class sets
 *      a completion flag and during the next call to OnCheckReady,
 *      the completion flag will be checked and any result copied to
 *      that user's class. This again ensures a single point of socket
 *      deletion for safer, neater code.
 *
 *  O   The code in the constructor of the ident socket is taken from
 *      BufferedSocket but majorly thinned down. It works for both
 *      IPv4 and IPv6.
 *
 *  O   In the event that the ident socket throws a ModuleException,
 *      nothing is done. This is counted as total and complete
 *      failure to create a connection.
 * --------------------------------------------------------------
 */

class ProxySocket : public EventHandler
{
 private:
	User *user;			/* User we are attached to */
	InspIRCd *ServerInstance;	/* Server instance */
	char challenge[10];		/* what is sent on connect, as bytes */
	int clen;
	char response[20];		/* what we kill for on recieve, as bytes */
	int rlen;
	bool done;
 public:
	ProxySocket(InspIRCd *Server, User* u, const std::string &bindip, int port, char *cstr, int clen, char *rstr, int rlen)
	{
		user = u;
		ServerInstance = Server;
		this->clen = clen;
		this->rlen = rlen;

		int i;

		/* byte for byte copies of challenge and response. */
		for (i = 0; i != clen; i++)
		{
			this->challenge[i] = cstr[i];
		}

		for (i = 0; i != rlen; i++)
		{
			this->response[i] = rstr[i];
		}
		
		socklen_t size = 0;
#ifdef IPV6
		/* Does this look like a v6 ip address? */
		bool v6 = false;
		if ((bindip.empty()) || bindip.find(':') != std::string::npos)
		v6 = true;

		if (v6)
			SetFd(socket(AF_INET6, SOCK_STREAM, 0));
		else
#endif
			SetFd(socket(AF_INET, SOCK_STREAM, 0));

		if (GetFd() == -1)
			throw ModuleException("Could not create socket");

		/* We allocate two of these because sizeof(sockaddr_in6) > sizeof(sockaddr_in) */
		sockaddr* s = new sockaddr[2];
		sockaddr* addr = new sockaddr[2];
	
#ifdef IPV6
		/* Horrid icky nasty ugly berkely socket crap. */
		if (v6)
		{
			in6_addr addy;
			in6_addr n;
			if (inet_pton(AF_INET6, user->GetIPString(), &addy) > 0)
			{
				((sockaddr_in6*)addr)->sin6_family = AF_INET6;
				memcpy(&((sockaddr_in6*)addr)->sin6_addr, &addy, sizeof(addy));
				((sockaddr_in6*)addr)->sin6_port = htons(port);
				size = sizeof(sockaddr_in6);
				inet_pton(AF_INET6, bindip.c_str(), &n);
				memcpy(&((sockaddr_in6*)s)->sin6_addr, &n, sizeof(sockaddr_in6));
				((sockaddr_in6*)s)->sin6_port = 0;
				((sockaddr_in6*)s)->sin6_family = AF_INET6;
			}
		}
		else
#endif
		{
			in_addr addy;
			in_addr n;
			if (inet_aton(user->GetIPString(), &addy) > 0)
			{
				((sockaddr_in*)addr)->sin_family = AF_INET;
				((sockaddr_in*)addr)->sin_addr = addy;
				((sockaddr_in*)addr)->sin_port = htons(port);
				size = sizeof(sockaddr_in);
				inet_aton(bindip.c_str(), &n);
				((sockaddr_in*)s)->sin_addr = n;
				((sockaddr_in*)s)->sin_port = 0;
				((sockaddr_in*)s)->sin_family = AF_INET;
			}
		}

		/* Attempt to bind (ident requests must come from the ip the query is referring to */
		if (ServerInstance->SE->Bind(GetFd(), s, size) < 0)
		{
			this->Close();
			delete[] s;
			delete[] addr;
			throw ModuleException("failed to bind()");
		}

		delete[] s;
		ServerInstance->SE->NonBlocking(GetFd());

		/* Attempt connection (nonblocking) */
		if (ServerInstance->SE->Connect(this, (sockaddr*)addr, size) == -1 && errno != EINPROGRESS)
		{
			this->Close();
			delete[] addr;
			throw ModuleException("connect() failed");
		}

		delete[] addr;

		/* Add fd to socket engine */
		if (!ServerInstance->SE->AddFd(this))
		{
			this->Close();
			throw ModuleException("out of fds");
		}

		/* Important: We set WantWrite immediately after connect()
		 * because a successful connection will trigger a writability event
		 */
		ServerInstance->SE->WantWrite(this);
	}

	virtual void OnConnected()
	{
		ServerInstance->Log(DEBUG,"OnConnected()");

		/* Both sockaddr_in and sockaddr_in6 can be safely casted to sockaddr, especially since the
		 * only members we use are in a part of the struct that should always be identical (at the
		 * byte level). */
		#ifndef IPV6
		sockaddr_in laddr, raddr;
		#else
		sockaddr_in6 laddr, raddr;
		#endif

		socklen_t laddrsz = sizeof(laddr);
		socklen_t raddrsz = sizeof(raddr);

		if ((getsockname(user->GetFd(), (sockaddr*) &laddr, &laddrsz) != 0) || (getpeername(user->GetFd(), (sockaddr*) &raddr, &raddrsz) != 0))
		{
			done = true;
			return;
		}

		/* Send failed if we didnt write the whole ident request --
		 * might as well give up if this happens!
		 */
		ServerInstance->Log(DEBUG, "Sending");
		if (ServerInstance->SE->Send(this, this->challenge, this->clen, 0) < this->clen)
		{
			ServerInstance->Log(DEBUG, "Send incomplete");
			done = true;
		}
	}

	virtual void HandleEvent(EventType et, int errornum = 0)
	{
		switch (et)
		{
			case EVENT_READ:
				/* fd readable event, received ident response */
				ReadResponse();
			break;
			case EVENT_WRITE:
				/* fd writeable event, successfully connected! */
				OnConnected();
			break;
			case EVENT_ERROR:
				/* fd error event, ohshi- */
				ServerInstance->Log(DEBUG,"EVENT_ERROR");
				/* We *must* Close() here immediately or we get a
				 * huge storm of EVENT_ERROR events!
				 */
				Close();
				done = true;
			break;
		}
	}

	void Close()
	{
		/* Remove ident socket from engine, and close it, but dont detatch it
		 * from its parent user class, or attempt to delete its memory.
		 */
		if (GetFd() > -1)
		{
			ServerInstance->Log(DEBUG,"Close ident socket %d", GetFd());
			ServerInstance->SE->DelFd(this);
			ServerInstance->SE->Close(GetFd());
			ServerInstance->SE->Shutdown(GetFd(), SHUT_WR);
			this->SetFd(-1);
		}
	}

	bool HasResult()
	{
		return done;
	}

	void ReadResponse()
	{
		/* We don't really need to buffer for incomplete replies here, since IDENT replies are
		 * extremely short - there is *no* sane reason it'd be in more than one packet
		 */
		char ibuf[MAXBUF];
		int recvresult = ServerInstance->SE->Recv(this, ibuf, MAXBUF-1, 0);

		ServerInstance->Log(DEBUG,"ReadResponse(): %s -- %d", ibuf, recvresult);

		bool match = true;
		int i;

		for (i = 0; i != this->rlen && i != recvresult; i++)
		{
			if (this->response[i] != ibuf[i])
			{
				ServerInstance->Log(DEBUG, "No match at pos %d: %c ne %c", i, this->response[i], ibuf[i]);
				/* no match */
				match = false;
			}
		}

		if (match == true)
		{
			User::QuitUser(ServerInstance, this->user, "Open proxy detected.");
		}

		/* Close (but dont delete from memory) our socket
		 * and flag as done
		 */
		Close();
		done = true;
		return;
	}
};

class ModuleProxy : public Module
{
 private:
	int RequestTimeout;
 public:
	ModuleProxy(InspIRCd *Me)
		: Module(Me)
	{
		OnRehash(NULL, "");
		Implementation eventlist[] = { I_OnRehash, I_OnUserRegister, I_OnCleanup, I_OnUserDisconnect };
		ServerInstance->Modules->Attach(eventlist, this, 4);
	}
	
	virtual Version GetVersion()
	{
		return Version(1, 1, 1, 0, VF_VENDOR, API_VERSION);
	}
	
	
	virtual void OnRehash(User *user, const std::string &param)
	{
		ConfigReader MyConf(ServerInstance);
		
		RequestTimeout = MyConf.ReadInteger("ident", "timeout", 0, true);
		if (!RequestTimeout)
			RequestTimeout = 5;
	}
	
	virtual int OnUserRegister(User *user)
	{
		user->WriteServ("NOTICE Auth :*** Checking you for proxies...");

		// Get the IP that the user is connected to, and bind to that for the outgoing connection
		#ifndef IPV6
		sockaddr_in laddr;
		#else
		sockaddr_in6 laddr;
		#endif
		socklen_t laddrsz = sizeof(laddr);

		if (getsockname(user->GetFd(), (sockaddr*) &laddr, &laddrsz) != 0)
		{
			return 0;
		}

		#ifndef IPV6
		const char *ip = inet_ntoa(laddr.sin_addr);
		#else
		char ip[INET6_ADDRSTRLEN + 1];
		inet_ntop(laddr.sin6_family, &laddr.sin6_addr, ip, INET6_ADDRSTRLEN);
		#endif

		ProxySocket *p = NULL;
		try
		{
			p = new ProxySocket(ServerInstance, user, ip, 80, "GET /\n", 7, "Nothing here.", 12);
		}
		catch (ModuleException &e)
		{
			ServerInstance->Log(DEBUG,"Proxy exception: %s", e.GetReason());
			return 0;
		}

		user->Extend("proxy_socket", p);
		return 0;
	}

	virtual void OnCleanup(int target_type, void *item)
	{
		/* Module unloading, tidy up users */
		if (target_type == TYPE_USER)
			OnUserDisconnect((User*)item);
	}

	virtual void OnUserDisconnect(User *user)
	{
		/* User disconnect (generic socket detatch event) */
		ProxySocket *p = NULL;
		if (user->GetExt("proxy_socket", p))
		{
			p->Close();
			delete p;
			user->Shrink("proxy_socket");
			ServerInstance->Log(DEBUG, "Removed proxy socket from %s", user->nick);
		}
	}
};

MODULE_INIT(ModuleProxy)

