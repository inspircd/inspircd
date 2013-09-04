/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2007, 2009 John Brooks <john.brooks@dereferenced.net>
 *   Copyright (C) 2006-2008 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2005-2008 Craig Edwards <craigedwards@brainbox.cc>
 *   Copyright (C) 2008 Thomas Stagner <aquanight@inspircd.org>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *
 * This file is part of InspIRCd.  InspIRCd is free software: you can
 * redistribute it and/or modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation, version 2.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
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
 public:
	LocalUser *user;			/* User we are attached to */
	std::string result;		/* Holds the ident string if done */
	time_t age;
	bool done;			/* True if lookup is finished */

	IdentRequestSocket(LocalUser* u) : user(u)
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

		/* Close (but don't delete from memory) our socket
		 * and flag as done since the ident lookup has finished
		 */
		Close();
		done = true;

		/* Cant possibly be a valid response shorter than 3 chars,
		 * because the shortest possible response would look like: '1,1'
		 */
		if (recvresult < 3)
			return;

		ServerInstance->Logs->Log("m_ident",DEBUG,"ReadResponse()");

		/* Truncate at the first null character, but first make sure
		 * there is at least one null char (at the end of the buffer).
		 */
		ibuf[recvresult] = '\0';
		std::string buf(ibuf);

		/* <2 colons: invalid
		 *  2 colons: reply is an error
		 * >3 colons: there is a colon in the ident
		 */
		if (std::count(buf.begin(), buf.end(), ':') != 3)
			return;

		std::string::size_type lastcolon = buf.rfind(':');

		/* Truncate the ident at any characters we don't like, skip leading spaces */
		for (std::string::const_iterator i = buf.begin()+lastcolon+1; i != buf.end(); ++i)
		{
			if (result.size() == ServerInstance->Config->Limits.IdentMax)
				/* Ident is getting too long */
				break;

			if (*i == ' ')
				continue;

			/* Add the next char to the result and see if it's still a valid ident,
			 * according to IsIdent(). If it isn't, then erase what we just added and
			 * we're done.
			 */
			result += *i;
			if (!ServerInstance->IsIdent(result.c_str()))
			{
				result.erase(result.end()-1);
				break;
			}
		}
	}
};

class ModuleIdent : public Module
{
	int RequestTimeout;
	SimpleExtItem<IdentRequestSocket> ext;
 public:
	ModuleIdent() : ext("ident_socket", this)
	{
	}

	void init()
	{
		ServerInstance->Modules->AddService(ext);
		OnRehash(NULL);
		Implementation eventlist[] = {
			I_OnRehash, I_OnUserInit, I_OnCheckReady,
			I_OnUserDisconnect, I_OnSetConnectClass
		};
		ServerInstance->Modules->Attach(eventlist, this, sizeof(eventlist)/sizeof(Implementation));
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
		RequestTimeout = ServerInstance->Config->ConfValue("ident")->getInt("timeout", 5);
		if (!RequestTimeout)
			RequestTimeout = 5;
	}

	void OnUserInit(LocalUser *user)
	{
		ConfigTag* tag = user->MyClass->config;
		if (!tag->getBool("useident", true))
			return;

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
		}
		else if (!isock->HasResult())
		{
			// time still good, no result yet... hold the registration
			ServerInstance->Logs->Log("m_ident",DEBUG, "No result yet");
			return MOD_RES_DENY;
		}

		ServerInstance->Logs->Log("m_ident",DEBUG, "Yay, result!");

		/* wooo, got a result (it will be good, or bad) */
		if (isock->result.empty())
		{
			user->ident.insert(user->ident.begin(), 1, '~');
			user->WriteServ("NOTICE Auth :*** Could not find your ident, using %s instead.", user->ident.c_str());
		}
		else
		{
			user->ident = isock->result;
			user->WriteServ("NOTICE Auth :*** Found your ident, '%s'", user->ident.c_str());
		}

		user->InvalidateCache();
		isock->Close();
		ext.unset(user);
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
		{
			LocalUser* user = IS_LOCAL((User*) item);
			if (user)
				OnUserDisconnect(user);
		}
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

