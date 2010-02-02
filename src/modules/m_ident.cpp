/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2010 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *	    the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"

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
	LocalUser *user;			/* User we are attached to */
	bool done;			/* True if lookup is finished */
	std::string result;		/* Holds the ident string if done */
 public:
	time_t age;

	IdentRequestSocket(LocalUser* u) : user(u), result(u->ident)
	{
		age = ServerInstance->Time();

		SetFd(socket(user->server_sa.sa.sa_family, SOCK_STREAM, 0));

		if (GetFd() == -1)
			throw ModuleException("Could not create socket");

		done = false;

		irc::sockets::sockaddrs bindaddr;
		irc::sockets::sockaddrs connaddr;

		memcpy(&bindaddr, &user->server_sa, sizeof(bindaddr));
		memcpy(&connaddr, &user->client_sa, sizeof(connaddr));

		if (connaddr.sa.sa_family == AF_INET6)
		{
			bindaddr.in6.sin6_port = 0;
			connaddr.in6.sin6_port = htons(113);
		}
		else
		{
			bindaddr.in4.sin_port = 0;
			connaddr.in4.sin_port = htons(113);
		}

		/* Attempt to bind (ident requests must come from the ip the query is referring to */
		if (ServerInstance->SE->Bind(GetFd(), bindaddr) < 0)
		{
			this->Close();
			throw ModuleException("failed to bind()");
		}

		ServerInstance->SE->NonBlocking(GetFd());

		/* Attempt connection (nonblocking) */
		if (ServerInstance->SE->Connect(this, &connaddr.sa, connaddr.sa_size()) == -1 && errno != EINPROGRESS)
		{
			this->Close();
			throw ModuleException("connect() failed");
		}

		/* Add fd to socket engine */
		if (!ServerInstance->SE->AddFd(this, FD_WANT_NO_READ | FD_WANT_POLL_WRITE))
		{
			this->Close();
			throw ModuleException("out of fds");
		}
	}

	virtual void OnConnected()
	{
		ServerInstance->Logs->Log("m_ident",DEBUG,"OnConnected()");
		ServerInstance->SE->ChangeEventMask(this, FD_WANT_POLL_READ | FD_WANT_NO_WRITE);

		char req[32];

		/* Build request in the form 'localport,remoteport\r\n' */
		int req_size;
		if (user->client_sa.sa.sa_family == AF_INET6)
			req_size = snprintf(req, sizeof(req), "%d,%d\r\n",
				ntohs(user->client_sa.in6.sin6_port), ntohs(user->server_sa.in6.sin6_port));
		else
			req_size = snprintf(req, sizeof(req), "%d,%d\r\n",
				ntohs(user->client_sa.in4.sin_port), ntohs(user->server_sa.in4.sin_port));

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
				ServerInstance->Logs->Log("m_ident",DEBUG,"EVENT_ERROR");
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
			ServerInstance->Logs->Log("m_ident",DEBUG,"Close ident socket %d", GetFd());
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

		ServerInstance->Logs->Log("m_ident",DEBUG,"ReadResponse()");

		irc::sepstream sep(ibuf, ':');
		std::string token;
		for (int i = 0; sep.GetToken(token); i++)
		{
			/* We only really care about the 4th portion */
			if (i < 3)
				continue;

			std::string ident;

			/* Truncate the ident at any characters we don't like, skip leading spaces */
			size_t k = 0;
			for (const char *j = token.c_str(); *j && (k < ServerInstance->Config->Limits.IdentMax + 1); j++)
			{
				if (*j == ' ')
					continue;

				/* Rules taken from InspIRCd::IsIdent */
				if (((*j >= 'A') && (*j <= '}')) || ((*j >= '0') && (*j <= '9')) || (*j == '-') || (*j == '.'))
				{
					ident += *j;
					continue;
				}

				break;
			}

			/* Re-check with IsIdent, in case that changes and this doesn't (paranoia!) */
			if (!ident.empty() && ServerInstance->IsIdent(ident.c_str()))
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
	int RequestTimeout;
	SimpleExtItem<IdentRequestSocket> ext;
 public:
	ModuleIdent() : ext("ident_socket", this)
	{
		OnRehash(NULL);
		Implementation eventlist[] = {
			I_OnRehash, I_OnUserRegister, I_OnCheckReady,
			I_OnUserDisconnect, I_OnSetConnectClass
		};
		ServerInstance->Modules->Attach(eventlist, this, 5);
	}

	~ModuleIdent()
	{
	}

	virtual Version GetVersion()
	{
		return Version("Provides support for RFC1413 ident lookups", VF_VENDOR);
	}

	virtual void OnRehash(User *user)
	{
		ConfigReader Conf;

		RequestTimeout = Conf.ReadInteger("ident", "timeout", 0, true);
		if (!RequestTimeout)
			RequestTimeout = 5;
	}

	virtual ModResult OnUserRegister(LocalUser *user)
	{
		ConfigTag* tag = user->MyClass->config;
		if (!tag->getBool("useident", true))
			return MOD_RES_PASSTHRU;

		/* User::ident is currently the username field from USER; with m_ident loaded, that
		 * should be preceded by a ~. The field is actually IdentMax+2 characters wide. */
		if (user->ident.length() > ServerInstance->Config->Limits.IdentMax + 1)
			user->ident.assign(user->ident, 0, ServerInstance->Config->Limits.IdentMax);
		user->ident.insert(0, "~");

		user->WriteServ("NOTICE Auth :*** Looking up your ident...");

		try
		{
			IdentRequestSocket *isock = new IdentRequestSocket(IS_LOCAL(user));
			ext.set(user, isock);
		}
		catch (ModuleException &e)
		{
			ServerInstance->Logs->Log("m_ident",DEBUG,"Ident exception: %s", e.GetReason());
		}

		return MOD_RES_PASSTHRU;
	}

	/* This triggers pretty regularly, we can use it in preference to
	 * creating a Timer object and especially better than creating a
	 * Timer per ident lookup!
	 */
	virtual ModResult OnCheckReady(LocalUser *user)
	{
		/* Does user have an ident socket attached at all? */
		IdentRequestSocket *isock = ext.get(user);
		if (!isock)
		{
			ServerInstance->Logs->Log("m_ident",DEBUG, "No ident socket :(");
			return MOD_RES_PASSTHRU;
		}

		ServerInstance->Logs->Log("m_ident",DEBUG, "Has ident_socket");

		time_t compare = isock->age;
		compare += RequestTimeout;

		/* Check for timeout of the socket */
		if (ServerInstance->Time() >= compare)
		{
			/* Ident timeout */
			user->WriteServ("NOTICE Auth :*** Ident request timed out.");
			ServerInstance->Logs->Log("m_ident",DEBUG, "Timeout");
			/* The user isnt actually disconnecting,
			 * we call this to clean up the user
			 */
			OnUserDisconnect(user);
			return MOD_RES_PASSTHRU;
		}

		/* Got a result yet? */
		if (!isock->HasResult())
		{
			ServerInstance->Logs->Log("m_ident",DEBUG, "No result yet");
			return MOD_RES_DENY;
		}

		ServerInstance->Logs->Log("m_ident",DEBUG, "Yay, result!");

		/* wooo, got a result (it will be good, or bad) */
		if (*(isock->GetResult()) != '~')
			user->WriteServ("NOTICE Auth :*** Found your ident, '%s'", isock->GetResult());
		else
			user->WriteServ("NOTICE Auth :*** Could not find your ident, using %s instead.", isock->GetResult());

		/* Copy the ident string to the user */
		user->ChangeIdent(isock->GetResult());

		/* The user isnt actually disconnecting, we call this to clean up the user */
		OnUserDisconnect(user);
		return MOD_RES_PASSTHRU;
	}

	ModResult OnSetConnectClass(LocalUser* user, ConnectClass* myclass)
	{
		if (myclass->config->getBool("requireident") && user->ident[0] == '~')
			return MOD_RES_DENY;
		return MOD_RES_PASSTHRU;
	}

	virtual void OnCleanup(int target_type, void *item)
	{
		/* Module unloading, tidy up users */
		if (target_type == TYPE_USER)
			OnUserDisconnect((LocalUser*)item);
	}

	virtual void OnUserDisconnect(LocalUser *user)
	{
		/* User disconnect (generic socket detatch event) */
		IdentRequestSocket *isock = ext.get(user);
		if (isock)
		{
			isock->Close();
			ext.unset(user);
		}
	}
};

MODULE_INIT(ModuleIdent)

