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
#include "xline.h"
#include "bancache.h"

/* add a client connection to the sockets list */
void UserManager::AddUser(LocalUser* New, ListenSocket* via)
{
	UserIOHandler* eh = &New->eh;

	ServerInstance->Logs->Log("USERS", DEBUG,"New user %s on FD %d", New->uuid.c_str(), eh->GetFd());

	this->unregistered_count++;

	/* The users default nick is their UUID */
	New->nick.assign(New->uuid, 0, ServerInstance->Config->Limits.NickMax);
	(*(this->clientlist))[New->nick] = New;

	New->ident.assign("unknown");

	New->registered = REG_NONE;
	New->signon = ServerInstance->Time() + ServerInstance->Config->dns_timeout;
	New->lastping = 1;

	/* Smarter than your average bear^H^H^H^Hset of strlcpys. */
	New->dhost.assign(New->GetIPString(), 0, 64);
	New->host.assign(New->GetIPString(), 0, 64);

	ServerInstance->Users->AddLocalClone(New);
	ServerInstance->Users->AddGlobalClone(New);

	this->local_users.push_back(New);

	if ((this->local_users.size() > ServerInstance->Config->SoftLimit) || (this->local_users.size() >= (unsigned int)ServerInstance->SE->GetMaxFds()))
	{
		ServerInstance->SNO->WriteToSnoMask('a', "Warning: softlimit value has been reached: %d clients", ServerInstance->Config->SoftLimit);
		this->QuitUser(New,"No more connections allowed");
		return;
	}

	/*
	 * First class check. We do this again in FullConnect after DNS is done, and NICK/USER is recieved.
	 * See my note down there for why this is required. DO NOT REMOVE. :) -- w00t
	 */
	New->SetClass();

	/*
	 * Check connect class settings and initialise settings into User.
	 * This will be done again after DNS resolution. -- w00t
	 */
	New->CheckClass();
	if (New->quitting)
		return;

	/*
	 * even with bancache, we still have to keep User::exempt current.
	 * besides that, if we get a positive bancache hit, we still won't fuck
	 * them over if they are exempt. -- w00t
	 */
	New->exempt = (ServerInstance->XLines->MatchesLine("E",New) != NULL);

	if (BanCacheHit *b = ServerInstance->BanCache->GetHit(New->GetIPString()))
	{
		if (!b->Type.empty() && !New->exempt)
		{
			/* user banned */
			ServerInstance->Logs->Log("BANCACHE", DEBUG, std::string("BanCache: Positive hit for ") + New->GetIPString());
			if (!ServerInstance->Config->MoronBanner.empty())
				New->WriteServ("NOTICE %s :*** %s", New->nick.c_str(), ServerInstance->Config->MoronBanner.c_str());
			this->QuitUser(New, b->Reason);
			return;
		}
		else
		{
			ServerInstance->Logs->Log("BANCACHE", DEBUG, std::string("BanCache: Negative hit for ") + New->GetIPString());
		}
	}
	else
	{
		if (!New->exempt)
		{
			XLine* r = ServerInstance->XLines->MatchesLine("Z",New);

			if (r)
			{
				r->Apply(New);
				return;
			}
		}
	}

	if (!ServerInstance->SE->AddFd(eh, FD_WANT_FAST_READ | FD_WANT_EDGE_WRITE))
	{
		ServerInstance->Logs->Log("USERS", DEBUG,"Internal error on new connection");
		this->QuitUser(New, "Internal error handling connection");
	}

	/* NOTE: even if dns lookups are *off*, we still need to display this.
	 * BOPM and other stuff requires it.
	 */
	New->WriteServ("NOTICE Auth :*** Looking up your hostname...");
	if (ServerInstance->Config->RawLog)
		New->WriteServ("NOTICE Auth :*** Raw I/O logging is enabled on this server. All messages, passwords, and commands are being recorded.");

	FOREACH_MOD(I_OnUserInit,OnUserInit(New));

	if (ServerInstance->Config->NoUserDns)
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

	if (IS_SERVER(user))
	{
		ServerInstance->Logs->Log("CULLLIST",DEBUG, "*** Warning *** - You tried to quit a fake user (%s)", user->nick.c_str());
		return;
	}

	user->quitting = true;

	ServerInstance->Logs->Log("USERS", DEBUG, "QuitUser: %s=%s '%s'", user->uuid.c_str(), user->nick.c_str(), quitreason.c_str());
	user->Write("ERROR :Closing link: (%s@%s) [%s]", user->ident.c_str(), user->host.c_str(), *operreason ? operreason : quitreason.c_str());

	std::string reason;
	std::string oper_reason;
	reason.assign(quitreason, 0, ServerInstance->Config->Limits.MaxQuit);
	if (operreason && *operreason)
		oper_reason.assign(operreason, 0, ServerInstance->Config->Limits.MaxQuit);
	else
		oper_reason = quitreason;

	ServerInstance->GlobalCulls.AddItem(user);

	if (user->registered == REG_ALL)
	{
		FOREACH_MOD(I_OnUserQuit,OnUserQuit(user, reason, oper_reason));
		user->WriteCommonQuit(reason, oper_reason);
	}

	if (user->registered != REG_ALL)
		if (ServerInstance->Users->unregistered_count)
			ServerInstance->Users->unregistered_count--;

	if (IS_LOCAL(user))
	{
		LocalUser* lu = IS_LOCAL(user);
		FOREACH_MOD(I_OnUserDisconnect,OnUserDisconnect(lu));
		lu->eh.Close();
	}

	/*
	 * this must come before the ServerInstance->SNO->WriteToSnoMaskso that it doesnt try to fill their buffer with anything
	 * if they were an oper with +sn +qQ.
	 */
	if (user->registered == REG_ALL)
	{
		if (IS_LOCAL(user))
		{
			if (!user->quietquit)
			{
				ServerInstance->SNO->WriteToSnoMask('q',"Client exiting: %s!%s@%s [%s] (%s)",
					user->nick.c_str(), user->ident.c_str(), user->host.c_str(), oper_reason.c_str(), user->GetIPString());
			}
		}
		else
		{
			if ((!ServerInstance->SilentULine(user->server)) && (!user->quietquit))
			{
				ServerInstance->SNO->WriteToSnoMask('Q',"Client exiting on server %s: %s!%s@%s [%s] (%s)",
					user->server.c_str(), user->nick.c_str(), user->ident.c_str(), user->host.c_str(), oper_reason.c_str(), user->GetIPString());
			}
		}
		user->AddToWhoWas();
	}

	user_hash::iterator iter = this->clientlist->find(user->nick);

	if (iter != this->clientlist->end())
		this->clientlist->erase(iter);
	else
		ServerInstance->Logs->Log("USERS", DEBUG, "iter == clientlist->end, can't remove them from hash... problematic..");

	ServerInstance->Users->uuidlist->erase(user->uuid);
}


void UserManager::AddLocalClone(User *user)
{
	clonemap::iterator x;
	x = local_clones.find(user->GetCIDRMask());
	if (x != local_clones.end())
		x->second++;
	else
		local_clones[user->GetCIDRMask()] = 1;
}

void UserManager::AddGlobalClone(User *user)
{
	clonemap::iterator x;

	x = global_clones.find(user->GetCIDRMask());
	if (x != global_clones.end())
		x->second++;
	else
		global_clones[user->GetCIDRMask()] = 1;
}

void UserManager::RemoveCloneCounts(User *user)
{
	if (IS_LOCAL(user))
	{
		clonemap::iterator x = local_clones.find(user->GetCIDRMask());
		if (x != local_clones.end())
		{
			x->second--;
			if (!x->second)
			{
				local_clones.erase(x);
			}
		}
	}

	clonemap::iterator y = global_clones.find(user->GetCIDRMask());
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
	clonemap::iterator x = global_clones.find(user->GetCIDRMask());
	if (x != global_clones.end())
		return x->second;
	else
		return 0;
}

unsigned long UserManager::LocalCloneCount(User *user)
{
	clonemap::iterator x = local_clones.find(user->GetCIDRMask());
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

	snprintf(formatbuffer,MAXBUF,"NOTICE $%s :%s", ServerInstance->Config->ServerName.c_str(), textbuffer);

	for (std::vector<LocalUser*>::const_iterator i = local_users.begin(); i != local_users.end(); i++)
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

	snprintf(formatbuffer,MAXBUF,"PRIVMSG $%s :%s", ServerInstance->Config->ServerName.c_str(), textbuffer);

	for (std::vector<LocalUser*>::const_iterator i = local_users.begin(); i != local_users.end(); i++)
	{
		User* t = *i;
		t->WriteServ(std::string(formatbuffer));
	}
}


/* return how many users have a given mode e.g. 'a' */
int UserManager::ModeCount(const char mode)
{
	int c = 0;
	for(user_hash::iterator i = clientlist->begin(); i != clientlist->end(); ++i)
	{
		User* u = i->second;
		if (u->modes[mode-65])
			c++;
	}
	return c;
}
