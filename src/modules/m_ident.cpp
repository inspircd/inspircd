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

using irc::sockets::NonBlocking;

/* $ModDesc: Provides support for RFC1413 ident lookups */

class IdentRequestSocket : public EventHandler
{
 private:

	 userrec *user;
	 InspIRCd* ServerInstance;
	 bool done;
	 std::string result;

 public:

	IdentRequestSocket(InspIRCd *Server, userrec* u, const std::string &bindip) : user(u), ServerInstance(Server), result(u->ident)
	{
		/* connect here on instantiation, throw on immediate failure */

		socklen_t size = 0;

#ifdef IPV6
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

		sockaddr* s = new sockaddr[2];
		sockaddr* addr = new sockaddr[2];
	
#ifdef IPV6
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

		if (bind(GetFd(), s, size) < 0)
		{
			this->Close();
			delete[] s;
			delete[] addr;
			throw ModuleException("failed to bind()");
		}

		delete[] s;
		NonBlocking(GetFd());

		if (connect(GetFd(), (sockaddr*)addr, size) == -1 && errno != EINPROGRESS)
		{
			this->Close();
			delete[] addr;
			throw ModuleException("connect() failed");
		}

		delete[] addr;

		if (!ServerInstance->SE->AddFd(this))
		{
			this->Close();
			throw ModuleException("out of fds");
		}

		ServerInstance->SE->WantWrite(this);
		/* XXX Writeable socket, readable close? what happens?! */
		/*Close();*/
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

		#ifndef IPV6
		int req_size = snprintf(req, sizeof(req), "%d,%d\r\n", ntohs(raddr.sin_port), ntohs(laddr.sin_port));
		#else
		int req_size = snprintf(req, sizeof(req), "%d,%d\r\n", ntohs(raddr.sin6_port), ntohs(laddr.sin6_port));
		#endif
		
		if (send(GetFd(), req, req_size, 0) < req_size)
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
				Close();
				done = true;
			break;
		}
	}

	void Close()
	{
		if (GetFd() > -1)
		{
			ServerInstance->Log(DEBUG,"Close ident socket %d", GetFd());
			ServerInstance->SE->DelFd(this);
			close(GetFd());
			shutdown(GetFd(), SHUT_WR);
			this->SetFd(-1);
		}
	}

	bool HasResult()
	{
		return done;
	}

	const char* GetResult()
	{
		return result.c_str();
	}

	void ReadResponse()
	{
		// We don't really need to buffer for incomplete replies here, since IDENT replies are
		// extremely short - there is *no* sane reason it'd be in more than one packet

		char ibuf[MAXBUF];
		int recvresult = recv(GetFd(), ibuf, MAXBUF-1, 0);

		/* Cant possibly be a valid response shorter than 3 chars */
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
			// We only really care about the 4th portion
			if (i < 3)
				continue;

			char ident[IDENTMAX + 2];

			// Truncate the ident at any characters we don't like, skip leading spaces
			int k = 0;
			for (const char *j = token.c_str(); *j && (k < IDENTMAX + 1); j++)
			{
				if (*j == ' ')
					continue;

				// Rules taken from InspIRCd::IsIdent
				if (((*j >= 'A') && (*j <= '}')) || ((*j >= '0') && (*j <= '9')) || (*j == '-') || (*j == '.'))
				{
					ident[k++] = *j;
					continue;
				}

				break;
			}

			ident[k] = '\0';

			// Redundant check with IsIdent, in case that changes and this doesn't (paranoia!)
			if (*ident && ServerInstance->IsIdent(ident))
			{
				result = ident;
				ServerInstance->next_call = ServerInstance->Time();
			}

			break;
		}

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
	}
	
	virtual Version GetVersion()
	{
		return Version(1, 1, 1, 0, VF_VENDOR, API_VERSION);
	}
	
	virtual void Implements(char *List)
	{
		List[I_OnRehash] = List[I_OnUserRegister] = List[I_OnCheckReady] = List[I_OnCleanup] = List[I_OnUserDisconnect] = 1;
	}
	
	virtual void OnRehash(userrec *user, const std::string &param)
	{
		ConfigReader MyConf(ServerInstance);
		
		RequestTimeout = MyConf.ReadInteger("ident", "timeout", 0, true);
		if (!RequestTimeout)
			RequestTimeout = 5;
	}
	
	virtual int OnUserRegister(userrec *user)
	{
		/* User::ident is currently the username field from USER; with m_ident loaded, that
		 * should be preceded by a ~. The field is actually IDENTMAX+2 characters wide. */
		memmove(user->ident + 1, user->ident, IDENTMAX);
		user->ident[0] = '~';
		// Ensure that it is null terminated
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

	virtual bool OnCheckReady(userrec *user)
	{
		ServerInstance->Log(DEBUG,"OnCheckReady %s", user->nick);

		/* Does user have an ident socket attached at all? */
		IdentRequestSocket *isock = NULL;
		if (!user->GetExt("ident_socket", isock))
		{
			ServerInstance->Log(DEBUG, "No ident socket :(");
			return true;
		}

		time_t compare = isock->age;
		compare += RequestTimeout;

		if (ServerInstance->next_call > compare)
			ServerInstance->next_call = compare;

		ServerInstance->Log(DEBUG, "Has ident_socket. Time=%ld age=%ld RequestTimeout=%ld compare=%ld has result=%d", ServerInstance->Time(), isock->age, RequestTimeout, compare, isock->HasResult());

		if (ServerInstance->Time() >= compare)
		{
			/* Ident timeout */
			user->WriteServ("NOTICE Auth :*** Ident request timed out.");
			ServerInstance->Log(DEBUG, "Timeout");
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

		/* wooo, got a result! */
		if (*(isock->GetResult()) != '~')
			user->WriteServ("NOTICE Auth :*** Found your ident, '%s'", isock->GetResult());
		else
			user->WriteServ("NOTICE Auth :*** Could not find your ident, using %s instead.", isock->GetResult());

		strlcpy(user->ident, isock->GetResult(), IDENTMAX+1);
		OnUserDisconnect(user);
		return true;
	}

	virtual void OnCleanup(int target_type, void *item)
	{
		/* Module unloading, tidy up users */
		if (target_type == TYPE_USER)
			OnUserDisconnect((userrec*)item);
	}

	virtual void OnUserDisconnect(userrec *user)
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

MODULE_INIT(ModuleIdent);

