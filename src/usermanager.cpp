/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2009 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *	    the file COPYING for details.
 *
 * ---------------------------------------------------
 */

/* $Core */

#include "inspircd.h"
#include "xline.h"
#include "bancache.h"

/* add a client connection to the sockets list */
void UserManager::AddUser(InspIRCd* Instance, int socket, int port, bool iscached, sockaddr* ip, const std::string &targetip)
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
		Instance->SNO->WriteToSnoMask('a', "WARNING *** Duplicate UUID allocated!");
		return;
	}

	char ipaddr[MAXBUF];
#ifdef IPV6
	if (ip->sa_family == AF_INET6)
		inet_ntop(AF_INET6, &((const sockaddr_in6*)ip)->sin6_addr, ipaddr, sizeof(ipaddr));
	else
#endif
		inet_ntop(AF_INET, &((const sockaddr_in*)ip)->sin_addr, ipaddr, sizeof(ipaddr));

	New->SetFd(socket);
	New->SetSockAddr(ip->sa_family, ipaddr, port);

	/* Give each of the modules an attempt to hook the user for I/O */
	FOREACH_MOD_I(Instance, I_OnHookUserIO, OnHookUserIO(New, targetip));

	if (New->GetIOHook())
	{
		try
		{
			New->GetIOHook()->OnRawSocketAccept(socket, ipaddr, port);
		}
		catch (CoreException& modexcept)
		{
			ServerInstance->Logs->Log("SOCKET", DEBUG,"%s threw an exception: %s", modexcept.GetSource(), modexcept.GetReason());
		}
	}

	Instance->Logs->Log("USERS", DEBUG,"New user fd: %d", socket);

	this->unregistered_count++;

	(*(this->clientlist))[New->uuid] = New;

	/* The users default nick is their UUID */
	New->nick.assign(New->uuid, 0, ServerInstance->Config->Limits.NickMax);

	New->server = Instance->FindServerNamePtr(Instance->Config->ServerName);
	New->ident.assign("unknown");

	New->registered = REG_NONE;
	New->signon = Instance->Time() + Instance->Config->dns_timeout;
	New->lastping = 1;

	/* Smarter than your average bear^H^H^H^Hset of strlcpys. */
	New->dhost.assign(New->GetIPString(), 0, 64);
	New->host.assign(New->GetIPString(), 0, 64);

	Instance->Users->AddLocalClone(New);
	Instance->Users->AddGlobalClone(New);

	/*
	 * First class check. We do this again in FullConnect after DNS is done, and NICK/USER is recieved.
	 * See my note down there for why this is required. DO NOT REMOVE. :) -- w00t
	 */
	ConnectClass* i = New->SetClass();

	if (!i)
	{
		this->QuitUser(New, "Access denied by configuration");
		return;
	}

	/*
	 * Check connect class settings and initialise settings into User.
	 * This will be done again after DNS resolution. -- w00t
	 */
	New->CheckClass();

	this->local_users.push_back(New);

	if ((this->local_users.size() > Instance->Config->SoftLimit) || (this->local_users.size() >= (unsigned int)Instance->SE->GetMaxFds()))
	{
		Instance->SNO->WriteToSnoMask('a', "Warning: softlimit value has been reached: %d clients", Instance->Config->SoftLimit);
		this->QuitUser(New,"No more connections allowed");
		return;
	}

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
				New->WriteServ("NOTICE %s :*** %s", New->nick.c_str(), Instance->Config->MoronBanner);
			this->QuitUser(New, b->Reason);
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
		this->QuitUser(New, "Internal error handling connection");
	}

	/* NOTE: even if dns lookups are *off*, we still need to display this.
	 * BOPM and other stuff requires it.
	 */
	New->WriteServ("NOTICE Auth :*** Looking up your hostname...");

	if (Instance->Config->NoUserDns)
	{
		New->WriteServ("NOTICE %s :*** Skipping host resolution (disabled by server administrator)", New->nick.c_str());
		New->dns_done = true;
	}
	else
	{
		New->StartDNSLookup();
	}
}

void UserManager::QuitUser(User *user, const std::string &quitreason, const char* operreason)
{
	if (user->quitting)
	{
		ServerInstance->Logs->Log("CULLLIST",DEBUG, "*** Warning *** - You tried to quit a user (%s) twice. Did your module call QuitUser twice?", user->nick.c_str());
		return;
	}

	if (IS_FAKE(user))
	{
		ServerInstance->Logs->Log("CULLLIST",DEBUG, "*** Warning *** - You tried to quit a fake user (%s)", user->nick.c_str());
		return;
	}

	user->quitting = true;

	ServerInstance->Logs->Log("USERS", DEBUG, "QuitUser: %s '%s'", user->nick.c_str(), quitreason.c_str());
	user->Write("ERROR :Closing link: (%s@%s) [%s]", user->ident.c_str(), user->host.c_str(), *operreason ? operreason : quitreason.c_str());

	user->quietquit = false;
	user->quitmsg = quitreason;

	std::string reason;
	std::string oper_reason;
	reason.assign(quitreason, 0, ServerInstance->Config->Limits.MaxQuit);
	if (!*operreason)
	{
		user->operquitmsg = quitreason;
		oper_reason.assign(quitreason, 0, ServerInstance->Config->Limits.MaxQuit);
	}
	else
	{
		user->operquitmsg = operreason;
		oper_reason.assign(operreason, 0, ServerInstance->Config->Limits.MaxQuit);
	}

	ServerInstance->GlobalCulls.AddItem(user);

	if (user->registered == REG_ALL)
	{
		FOREACH_MOD_I(ServerInstance,I_OnUserQuit,OnUserQuit(user, reason, oper_reason));
		user->WriteCommonQuit(reason, oper_reason);
	}

	FOREACH_MOD_I(ServerInstance,I_OnUserDisconnect,OnUserDisconnect(user));

	ServerInstance->Users->clientlist->erase(user->nick);
	ServerInstance->Users->uuidlist->erase(user->uuid);
}


void UserManager::AddLocalClone(User *user)
{
	int range = 32;
	clonemap::iterator x;
	switch (user->GetProtocolFamily())
	{
#ifdef SUPPORT_IP6LINKS
		case AF_INET6:
		{
			range = ServerInstance->Config->c_ipv6_range;
		}
		break;
#endif
		case AF_INET:
		{
			range = ServerInstance->Config->c_ipv4_range;
		}
		break;
	}

	x = local_clones.find(user->GetCIDRMask(range));
	if (x != local_clones.end())
		x->second++;
	else
		local_clones[user->GetCIDRMask(range)] = 1;
}

void UserManager::AddGlobalClone(User *user)
{
	int range = 32;
	clonemap::iterator x;
	switch (user->GetProtocolFamily())
	{
#ifdef SUPPORT_IP6LINKS
		case AF_INET6:
		{
			range = ServerInstance->Config->c_ipv6_range;
		}
		break;
#endif
		case AF_INET:
		{
			range = ServerInstance->Config->c_ipv4_range;
		}
		break;
	}

	x = global_clones.find(user->GetCIDRMask(range));
	if (x != global_clones.end())
		x->second++;
	else
		global_clones[user->GetCIDRMask(range)] = 1;
}

void UserManager::RemoveCloneCounts(User *user)
{
	int range = 0;
	switch (user->GetProtocolFamily())
	{
#ifdef SUPPORT_IP6LINKS
		case AF_INET6:
		{
			range = ServerInstance->Config->c_ipv6_range;
		}
		break;
#endif
		case AF_INET:
		{
			range = ServerInstance->Config->c_ipv4_range;
		}
		break;
	}

	if (IS_LOCAL(user))
	{
		clonemap::iterator x = local_clones.find(user->GetCIDRMask(range));
		if (x != local_clones.end())
		{
			x->second--;
			if (!x->second)
			{
				local_clones.erase(x);
			}
		}
	}

	clonemap::iterator y = global_clones.find(user->GetCIDRMask(range));
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
	int range = 32;
	switch (user->GetProtocolFamily())
	{
#ifdef SUPPORT_IP6LINKS
		case AF_INET6:
		{
			range = ServerInstance->Config->c_ipv6_range;
		}
		break;
#endif
		case AF_INET:
		{
			range = ServerInstance->Config->c_ipv4_range;
		}
		break;
	}
	clonemap::iterator x = global_clones.find(user->GetCIDRMask(range));
	if (x != global_clones.end())
		return x->second;
	else
		return 0;
}

unsigned long UserManager::LocalCloneCount(User *user)
{
	int range = 32;
	switch (user->GetProtocolFamily())
	{
#ifdef SUPPORT_IP6LINKS
		case AF_INET6:
		{
			range = ServerInstance->Config->c_ipv6_range;
		}
		break;
#endif
		case AF_INET:
		{
			range = ServerInstance->Config->c_ipv4_range;
		}
		break;
	}
	clonemap::iterator x = local_clones.find(user->GetCIDRMask(range));
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
				t->WriteServ("NOTICE %s :%s", t->nick.c_str(), textbuffer);
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
				t->WriteServ("NOTICE %s :%s", t->nick.c_str(), textbuffer);
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
