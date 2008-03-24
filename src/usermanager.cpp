/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2008 InspIRCd Development Team
 * See: http://www.inspircd.org/wiki/index.php/Credits
 *
 * This program is free but copyrighted software; see
 *	    the file COPYING for details.
 *
 * ---------------------------------------------------
 */

/* $Core: libIRCDusermanager */

#include "inspircd.h"
#include "xline.h"
#include "bancache.h"

/* add a client connection to the sockets list */
void UserManager::AddClient(InspIRCd* Instance, int socket, int port, bool iscached, int socketfamily, sockaddr* ip)
{
	/* NOTE: Calling this one parameter constructor for User automatically
	 * allocates a new UUID and places it in the hash_map.
	 */
	User* New = NULL;
	try
	{
		New = new User(Instance);
	}
	catch (...)
	{
		Instance->Logs->Log("USERS", DEFAULT,"*** WTF *** Duplicated UUID! -- Crack smoking monkies have been unleashed.");
		Instance->SNO->WriteToSnoMask('A', "WARNING *** Duplicate UUID allocated!");
		return;
	}

	char ipaddr[MAXBUF];
#ifdef IPV6
	if (socketfamily == AF_INET6)
		inet_ntop(AF_INET6, &((const sockaddr_in6*)ip)->sin6_addr, ipaddr, sizeof(ipaddr));
	else
#endif
		inet_ntop(AF_INET, &((const sockaddr_in*)ip)->sin_addr, ipaddr, sizeof(ipaddr));


	/* Give each of the modules an attempt to hook the user for I/O */
	FOREACH_MOD_I(Instance, I_OnHookUserIO, OnHookUserIO(New));

	if (New->io)
	{
		try
		{
			New->io->OnRawSocketAccept(socket, ipaddr, port);
		}
		catch (CoreException& modexcept)
		{
			ServerInstance->Logs->Log("SOCKET", DEBUG,"%s threw an exception: %s", modexcept.GetSource(), modexcept.GetReason());
		}
	}

	Instance->Logs->Log("USERS", DEBUG,"New user fd: %d", socket);

	int j = 0;

	this->unregistered_count++;

	(*(this->clientlist))[New->uuid] = New;

	/* The users default nick is their UUID */
	strlcpy(New->nick, New->uuid, NICKMAX - 1);

	New->server = Instance->FindServerNamePtr(Instance->Config->ServerName);
	/* We don't need range checking here, we KNOW 'unknown\0' will fit into the ident field. */
	strcpy(New->ident, "unknown");

	New->registered = REG_NONE;
	New->signon = Instance->Time() + Instance->Config->dns_timeout;
	New->lastping = 1;

	New->SetSockAddr(socketfamily, ipaddr, port);

	New->SetFd(socket);

	/* Smarter than your average bear^H^H^H^Hset of strlcpys. */
	for (const char* temp = New->GetIPString(); *temp && j < 64; temp++, j++)
		New->dhost[j] = New->host[j] = *temp;
	New->dhost[j] = New->host[j] = 0;

	Instance->Users->AddLocalClone(New);
	Instance->Users->AddGlobalClone(New);

	/*
	 * First class check. We do this again in FullConnect after DNS is done, and NICK/USER is recieved.
	 * See my note down there for why this is required. DO NOT REMOVE. :) -- w00t
	 */
	ConnectClass* i = New->SetClass();

	if (!i)
	{
		User::QuitUser(Instance, New, "Access denied by configuration");
		return;
	}

	/*
	 * Check connect class settings and initialise settings into User.
	 * This will be done again after DNS resolution. -- w00t
	 */
	New->CheckClass();

	this->local_users.push_back(New);

	if ((this->local_users.size() > Instance->Config->SoftLimit) || (this->local_users.size() >= MAXCLIENTS))
	{
		Instance->SNO->WriteToSnoMask('A', "Warning: softlimit value has been reached: %d clients", Instance->Config->SoftLimit);
		User::QuitUser(Instance, New,"No more connections allowed");
		return;
	}

	/*
	 * XXX -
	 * this is done as a safety check to keep the file descriptors within range of fd_ref_table.
	 * its a pretty big but for the moment valid assumption:
	 * file descriptors are handed out starting at 0, and are recycled as theyre freed.
	 * therefore if there is ever an fd over 65535, 65536 clients must be connected to the
	 * irc server at once (or the irc server otherwise initiating this many connections, files etc)
	 * which for the time being is a physical impossibility (even the largest networks dont have more
	 * than about 10,000 users on ONE server!)
	 */
#ifndef WINDOWS
	if ((unsigned int)socket >= MAX_DESCRIPTORS)
	{
		User::QuitUser(Instance, New, "Server is full");
		return;
	}
#endif
	/*
	 * even with bancache, we still have to keep User::exempt current.
	 * besides that, if we get a positive bancache hit, we still won't fuck
	 * them over if they are exempt. -- w00t
	 */
	New->exempt = (Instance->XLines->MatchesLine("E",New) != NULL);

	if (BanCacheHit *b = Instance->BanCache->GetHit(New->GetIPString()))
	{
		if (!b->Type.empty() && !New->exempt)
		{
			/* user banned */
			Instance->Logs->Log("BANCACHE", DEBUG, std::string("BanCache: Positive hit for ") + New->GetIPString());
			if (*Instance->Config->MoronBanner)
				New->WriteServ("NOTICE %s :*** %s", New->nick, Instance->Config->MoronBanner);
			User::QuitUser(Instance, New, b->Reason);
			return;
		}
		else
		{
			Instance->Logs->Log("BANCACHE", DEBUG, std::string("BanCache: Negative hit for ") + New->GetIPString());
		}
	}
	else
	{
		if (!New->exempt)
		{
			XLine* r = Instance->XLines->MatchesLine("Z",New);

			if (r)
			{
				r->Apply(New);
				return;
			}
		}
	}

	if (!Instance->SE->AddFd(New))
	{
		Instance->Logs->Log("USERS", DEBUG,"Internal error on new connection");
		User::QuitUser(Instance, New, "Internal error handling connection");
	}

	/* NOTE: even if dns lookups are *off*, we still need to display this.
	 * BOPM and other stuff requires it.
	 */
	New->WriteServ("NOTICE Auth :*** Looking up your hostname...");

	if (Instance->Config->NoUserDns)
	{
		New->dns_done = true;
	}
	else
	{
		New->StartDNSLookup();
	}
}

void UserManager::AddLocalClone(User *user)
{
	clonemap::iterator x = local_clones.find(user->GetIPString());
	if (x != local_clones.end())
		x->second++;
	else
		local_clones[user->GetIPString()] = 1;
}

void UserManager::AddGlobalClone(User *user)
{
	clonemap::iterator y = global_clones.find(user->GetIPString());
	if (y != global_clones.end())
		y->second++;
	else
		global_clones[user->GetIPString()] = 1;
}

void UserManager::RemoveCloneCounts(User *user)
{
	clonemap::iterator x = local_clones.find(user->GetIPString());
	if (x != local_clones.end())
	{
		x->second--;
		if (!x->second)
		{
			local_clones.erase(x);
		}
	}
	
	clonemap::iterator y = global_clones.find(user->GetIPString());
	if (y != global_clones.end())
	{
		y->second--;
		if (!y->second)
		{
			global_clones.erase(y);
		}
	}
}

unsigned long UserManager::GlobalCloneCount(User *user)
{
	clonemap::iterator x = global_clones.find(user->GetIPString());
	if (x != global_clones.end())
		return x->second;
	else
		return 0;
}

unsigned long UserManager::LocalCloneCount(User *user)
{
	clonemap::iterator x = local_clones.find(user->GetIPString());
	if (x != local_clones.end())
		return x->second;
	else
		return 0;
}

/* this function counts all users connected, wether they are registered or NOT. */
unsigned int UserManager::UserCount()
{
	/*
	 * XXX: Todo:
	 *  As part of this restructuring, move clientlist/etc fields into usermanager.
	 * 	-- w00t
	 */
	return this->clientlist->size();
}

/* this counts only registered users, so that the percentages in /MAP don't mess up */
unsigned int UserManager::RegisteredUserCount()
{
	return this->clientlist->size() - this->UnregisteredUserCount();
}

/* return how many users are opered */
unsigned int UserManager::OperCount()
{
	return this->all_opers.size();
}

/* return how many users are unregistered */
unsigned int UserManager::UnregisteredUserCount()
{
	return this->unregistered_count;
}

/* return how many local registered users there are */
unsigned int UserManager::LocalUserCount()
{
	/* Doesnt count unregistered clients */
	return (this->local_users.size() - this->UnregisteredUserCount());
}

void UserManager::ServerNoticeAll(const char* text, ...)
{
	if (!text)
		return;

	char textbuffer[MAXBUF];
	char formatbuffer[MAXBUF];
	va_list argsPtr;
	va_start (argsPtr, text);
	vsnprintf(textbuffer, MAXBUF, text, argsPtr);
	va_end(argsPtr);

	snprintf(formatbuffer,MAXBUF,"NOTICE $%s :%s", ServerInstance->Config->ServerName, textbuffer);

	for (std::vector<User*>::const_iterator i = local_users.begin(); i != local_users.end(); i++)
	{
		User* t = *i;
		t->WriteServ(std::string(formatbuffer));
	}
}

void UserManager::ServerPrivmsgAll(const char* text, ...)
{
	if (!text)
		return;

	char textbuffer[MAXBUF];
	char formatbuffer[MAXBUF];
	va_list argsPtr;
	va_start (argsPtr, text);
	vsnprintf(textbuffer, MAXBUF, text, argsPtr);
	va_end(argsPtr);

	snprintf(formatbuffer,MAXBUF,"PRIVMSG $%s :%s", ServerInstance->Config->ServerName, textbuffer);

	for (std::vector<User*>::const_iterator i = local_users.begin(); i != local_users.end(); i++)
	{
		User* t = *i;
		t->WriteServ(std::string(formatbuffer));
	}
}

void UserManager::WriteMode(const char* modes, int flags, const char* text, ...)
{
	char textbuffer[MAXBUF];
	int modelen;
	va_list argsPtr;

	if (!text || !modes || !flags)
	{
		ServerInstance->Logs->Log("USERS", DEFAULT,"*** BUG *** WriteMode was given an invalid parameter");
		return;
	}

	va_start(argsPtr, text);
	vsnprintf(textbuffer, MAXBUF, text, argsPtr);
	va_end(argsPtr);
	modelen = strlen(modes);

	if (flags == WM_AND)
	{
		for (std::vector<User*>::const_iterator i = local_users.begin(); i != local_users.end(); i++)
		{
			User* t = *i;
			bool send_to_user = true;

			for (int n = 0; n < modelen; n++)
			{
				if (!t->IsModeSet(modes[n]))
				{
					send_to_user = false;
					break;
				}
			}
			if (send_to_user)
			{
				t->WriteServ("NOTICE %s :%s", t->nick, textbuffer);
			}
		}
	}
	else if (flags == WM_OR)
	{
		for (std::vector<User*>::const_iterator i = local_users.begin(); i != local_users.end(); i++)
		{
			User* t = *i;
			bool send_to_user = false;

			for (int n = 0; n < modelen; n++)
			{
				if (t->IsModeSet(modes[n]))
				{
					send_to_user = true;
					break;
				}
			}

			if (send_to_user)
			{
				t->WriteServ("NOTICE %s :%s", t->nick, textbuffer);
			}
		}
	}
}

/* return how many users have a given mode e.g. 'a' */
int UserManager::ModeCount(const char mode)
{
	ModeHandler* mh = this->ServerInstance->Modes->FindMode(mode, MODETYPE_USER);

	if (mh)
		return mh->GetCount();
	else
		return 0;
}



