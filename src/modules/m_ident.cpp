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

/* $ModDesc: Provides support for RFC1413 ident lookups */

/* --------------------------------------------------------------
 * Note that this is the third incarnation of m_ident. The first
 * two attempts were pretty crashy, mainly due to the fact we tried
 * to use InspSocket/BufferedSocket to make them work. This class
 * is ok for more heavyweight tasks, it does a lot of things behind
 * the scenes that are not good for ident sockets and it has a huge
 * memory footprint!
 *
 * To fix all the issues that we had in the old ident modules (many
 * nasty race conditions that would cause segfaults etc) we have
 * rewritten this module to use a simplified socket object based
 * directly off EventHandler. As EventHandler only has low level
 * readability, writeability and error events tied directly to the
 * socket engine, this makes our lives easier as nothing happens to
 * our ident lookup class that is outside of this module, or out-
 * side of the control of the class. There are no timers, internal
 * events, or such, which will cause the socket to be deleted,
 * queued for deletion, etc. In fact, theres not even any queueing!
 *
 * Using this framework we have a much more stable module.
 *
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

class IdentRequestSocket : public EventHandler
{
 private:

	 User *user;			/* User we are attached to */
	 InspIRCd* ServerInstance;	/* Server instance */
	 bool done;			/* True if lookup is finished */
	 std::string result;		/* Holds the ident string if done */

 public:

	IdentRequestSocket(InspIRCd *Server, User* u, const std::string &bindip) : user(u), ServerInstance(Server), result(u->ident)
	{
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
				((sockaddr_in6*)addr)->sin6_port = htons(113);
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
				((sockaddr_in*)addr)->sin_port = htons(113);
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

		char req[32];

		/* Build request in the form 'localport,remoteport\r\n' */
		#ifndef IPV6
		int req_size = snprintf(req, sizeof(req), "%d,%d\r\n", ntohs(raddr.sin_port), ntohs(laddr.sin_port));
		#else
		int req_size = snprintf(req, sizeof(req), "%d,%d\r\n", ntohs(raddr.sin6_port), ntohs(laddr.sin6_port));
		#endif
		
		/* Send failed if we didnt write the whole ident request --
		 * might as well give up if this happens!
		 */
		if (ServerInstance->SE->Send(this, req, req_size, 0) < req_size)
			done = true;
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

	/* Note: if the lookup succeeded, will contain 'ident', otherwise
	 * will contain '~ident'. Use *GetResult() to determine lookup success.
	 */
	const char* GetResult()
	{
		return result.c_str();
	}

	void ReadResponse()
	{
		/* We don't really need to buffer for incomplete replies here, since IDENT replies are
		 * extremely short - there is *no* sane reason it'd be in more than one packet
		 */
		char ibuf[MAXBUF];
		int recvresult = ServerInstance->SE->Recv(this, ibuf, MAXBUF-1, 0);

		/* Cant possibly be a valid response shorter than 3 chars,
		 * because the shortest possible response would look like: '1,1'
		 */
		if (recvresult < 3)
		{
			Close();
			done = true;
			return;
		}

		ServerInstance->Log(DEBUG,"ReadResponse()");

		irc::sepstream sep(ibuf, ':');
		std::string token;
		for (int i = 0; sep.GetToken(token); i++)
		{
			/* We only really care about the 4th portion */
			if (i < 3)
				continue;

			char ident[IDENTMAX + 2];

			/* Truncate the ident at any characters we don't like, skip leading spaces */
			int k = 0;
			for (const char *j = token.c_str(); *j && (k < IDENTMAX + 1); j++)
			{
				if (*j == ' ')
					continue;

				/* Rules taken from InspIRCd::IsIdent */
				if (((*j >= 'A') && (*j <= '}')) || ((*j >= '0') && (*j <= '9')) || (*j == '-') || (*j == '.'))
				{
					ident[k++] = *j;
					continue;
				}

				break;
			}

			ident[k] = '\0';

			/* Re-check with IsIdent, in case that changes and this doesn't (paranoia!) */
			if (*ident && ServerInstance->IsIdent(ident))
			{
				result = ident;
			}

			break;
		}

		/* Close (but dont delete from memory) our socket
		 * and flag as done
		 */
		Close();
		done = true;
		return;
	}
};

class ModuleIdent : public Module
{
 private:
	int RequestTimeout;
 public:
	ModuleIdent(InspIRCd *Me)
		: Module(Me)
	{
		OnRehash(NULL, "");
		Implementation eventlist[] = { I_OnRehash, I_OnUserRegister, I_OnCheckReady, I_OnCleanup, I_OnUserDisconnect };
		ServerInstance->Modules->Attach(eventlist, this, 5);
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
		/* User::ident is currently the username field from USER; with m_ident loaded, that
		 * should be preceded by a ~. The field is actually IDENTMAX+2 characters wide. */
		memmove(user->ident + 1, user->ident, IDENTMAX);
		user->ident[0] = '~';
		/* Ensure that it is null terminated */
		user->ident[IDENTMAX + 1] = '\0';

		user->WriteServ("NOTICE Auth :*** Looking up your ident...");

		// Get the IP that the user is connected to, and bind to that for the outgoing connection
		#ifndef IPV6
		sockaddr_in laddr;
		#else
		sockaddr_in6 laddr;
		#endif
		socklen_t laddrsz = sizeof(laddr);

		if (getsockname(user->GetFd(), (sockaddr*) &laddr, &laddrsz) != 0)
		{
			user->WriteServ("NOTICE Auth :*** Could not find your ident, using %s instead.", user->ident);
			return 0;
		}

		#ifndef IPV6
		const char *ip = inet_ntoa(laddr.sin_addr);
		#else
		char ip[INET6_ADDRSTRLEN + 1];
		inet_ntop(laddr.sin6_family, &laddr.sin6_addr, ip, INET6_ADDRSTRLEN);
		#endif

		IdentRequestSocket *isock = NULL;
		try
		{
			isock = new IdentRequestSocket(ServerInstance, user, ip);
		}
		catch (ModuleException &e)
		{
			ServerInstance->Log(DEBUG,"Ident exception: %s", e.GetReason());
			return 0;
		}

		user->Extend("ident_socket", isock);
		return 0;
	}

	/* This triggers pretty regularly, we can use it in preference to
	 * creating a Timer object and especially better than creating a
	 * Timer per ident lookup!
	 */
	virtual bool OnCheckReady(User *user)
	{
		ServerInstance->Log(DEBUG,"OnCheckReady %s", user->nick);

		/* Does user have an ident socket attached at all? */
		IdentRequestSocket *isock = NULL;
		if (!user->GetExt("ident_socket", isock))
		{
			ServerInstance->Log(DEBUG, "No ident socket :(");
			return true;
		}

		ServerInstance->Log(DEBUG, "Has ident_socket");

		time_t compare = isock->age;
		compare += RequestTimeout;

		/* Check for timeout of the socket */
		if (ServerInstance->Time() >= compare)
		{
			/* Ident timeout */
			user->WriteServ("NOTICE Auth :*** Ident request timed out.");
			ServerInstance->Log(DEBUG, "Timeout");
			/* The user isnt actually disconnecting,
			 * we call this to clean up the user
			 */
			OnUserDisconnect(user);
			return true;
		}

		/* Got a result yet? */
		if (!isock->HasResult())
		{
			ServerInstance->Log(DEBUG, "No result yet");
			return false;
		}

		ServerInstance->Log(DEBUG, "Yay, result!");

		/* wooo, got a result (it will be good, or bad) */
		if (*(isock->GetResult()) != '~')
			user->WriteServ("NOTICE Auth :*** Found your ident, '%s'", isock->GetResult());
		else
			user->WriteServ("NOTICE Auth :*** Could not find your ident, using %s instead.", isock->GetResult());

		/* Copy the ident string to the user */
		strlcpy(user->ident, isock->GetResult(), IDENTMAX+1);

		/* The user isnt actually disconnecting, we call this to clean up the user */
		OnUserDisconnect(user);
		return true;
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
		IdentRequestSocket *isock = NULL;
		if (user->GetExt("ident_socket", isock))
		{
			isock->Close();
			delete isock;
			user->Shrink("ident_socket");
			ServerInstance->Log(DEBUG, "Removed ident socket from %s", user->nick);
		}
	}
};

MODULE_INIT(ModuleIdent)

