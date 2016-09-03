/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2008 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2008 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2008 Craig Edwards <craigedwards@brainbox.cc>
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
#include "xline.h"
#include "iohook.h"

namespace
{
	class WriteCommonQuit : public User::ForEachNeighborHandler
	{
		std::string line;
		std::string operline;

		void Execute(LocalUser* user) CXX11_OVERRIDE
		{
			user->Write(user->IsOper() ? operline : line);
		}

	 public:
		WriteCommonQuit(User* user, const std::string& msg, const std::string& opermsg)
			: line(":" + user->GetFullHost() + " QUIT :")
			, operline(line)
		{
			line += msg;
			operline += opermsg;
			user->ForEachNeighbor(*this, false);
		}
	};
}

UserManager::UserManager()
	: already_sent_id(0)
	, unregistered_count(0)
{
}

UserManager::~UserManager()
{
	for (user_hash::iterator i = clientlist.begin(); i != clientlist.end(); ++i)
	{
		delete i->second;
	}
}

void UserManager::AddUser(int socket, ListenSocket* via, irc::sockets::sockaddrs* client, irc::sockets::sockaddrs* server)
{
	// User constructor allocates a new UUID for the user and inserts it into the uuidlist
	LocalUser* const New = new LocalUser(socket, client, server);
	UserIOHandler* eh = &New->eh;

	ServerInstance->Logs->Log("USERS", LOG_DEBUG, "New user fd: %d", socket);

	this->unregistered_count++;
	this->clientlist[New->nick] = New;
	this->AddClone(New);
	this->local_users.push_front(New);

	if (!SocketEngine::AddFd(eh, FD_WANT_FAST_READ | FD_WANT_EDGE_WRITE))
	{
		ServerInstance->Logs->Log("USERS", LOG_DEBUG, "Internal error on new connection");
		this->QuitUser(New, "Internal error handling connection");
		return;
	}

	// If this listener has an IO hook provider set then tell it about the connection
	for (ListenSocket::IOHookProvList::iterator i = via->iohookprovs.begin(); i != via->iohookprovs.end(); ++i)
	{
		ListenSocket::IOHookProvRef& iohookprovref = *i;
		if (!iohookprovref)
			continue;

		iohookprovref->OnAccept(eh, client, server);
		// IOHook could have encountered a fatal error, e.g. if the TLS ClientHello was already in the queue and there was no common TLS version
		if (!eh->getError().empty())
		{
			QuitUser(New, eh->getError());
			return;
		}
	}

	if (this->local_users.size() > ServerInstance->Config->SoftLimit)
	{
		ServerInstance->SNO->WriteToSnoMask('a', "Warning: softlimit value has been reached: %d clients", ServerInstance->Config->SoftLimit);
		this->QuitUser(New,"No more connections allowed");
		return;
	}

	// First class check. We do this again in LocalUser::FullConnect() after DNS is done, and NICK/USER is received.
	New->SetClass();
	// If the user doesn't have an acceptable connect class CheckClass() quits them
	New->CheckClass(ServerInstance->Config->CCOnConnect);
	if (New->quitting)
		return;

	/*
	 * even with bancache, we still have to keep User::exempt current.
	 * besides that, if we get a positive bancache hit, we still won't fuck
	 * them over if they are exempt. -- w00t
	 */
	New->exempt = (ServerInstance->XLines->MatchesLine("E",New) != NULL);

	BanCacheHit* const b = ServerInstance->BanCache.GetHit(New->GetIPString());
	if (b)
	{
		if (!b->Type.empty() && !New->exempt)
		{
			/* user banned */
			ServerInstance->Logs->Log("BANCACHE", LOG_DEBUG, "BanCache: Positive hit for " + New->GetIPString());
			if (!ServerInstance->Config->XLineMessage.empty())
				New->WriteNumeric(ERR_YOUREBANNEDCREEP, ServerInstance->Config->XLineMessage);
			this->QuitUser(New, b->Reason);
			return;
		}
		else
		{
			ServerInstance->Logs->Log("BANCACHE", LOG_DEBUG, "BanCache: Negative hit for " + New->GetIPString());
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

	if (ServerInstance->Config->RawLog)
		New->WriteNotice("*** Raw I/O logging is enabled on this server. All messages, passwords, and commands are being recorded.");

	FOREACH_MOD(OnSetUserIP, (New));
	if (New->quitting)
		return;

	FOREACH_MOD(OnUserInit, (New));
}

void UserManager::QuitUser(User* user, const std::string& quitreason, const std::string* operreason)
{
	if (user->quitting)
	{
		ServerInstance->Logs->Log("USERS", LOG_DEFAULT, "ERROR: Tried to quit quitting user: " + user->nick);
		return;
	}

	if (IS_SERVER(user))
	{
		ServerInstance->Logs->Log("USERS", LOG_DEFAULT, "ERROR: Tried to quit server user: " + user->nick);
		return;
	}

	user->quitting = true;

	ServerInstance->Logs->Log("USERS", LOG_DEBUG, "QuitUser: %s=%s '%s'", user->uuid.c_str(), user->nick.c_str(), quitreason.c_str());
	user->Write("ERROR :Closing link: (%s@%s) [%s]", user->ident.c_str(), user->host.c_str(), operreason ? operreason->c_str() : quitreason.c_str());

	std::string reason;
	reason.assign(quitreason, 0, ServerInstance->Config->Limits.MaxQuit);
	if (!operreason)
		operreason = &reason;

	ServerInstance->GlobalCulls.AddItem(user);

	if (user->registered == REG_ALL)
	{
		FOREACH_MOD(OnUserQuit, (user, reason, *operreason));
		WriteCommonQuit(user, reason, *operreason);
	}
	else
		unregistered_count--;

	if (IS_LOCAL(user))
	{
		LocalUser* lu = IS_LOCAL(user);
		FOREACH_MOD(OnUserDisconnect, (lu));
		lu->eh.Close();

		if (lu->registered == REG_ALL)
			ServerInstance->SNO->WriteToSnoMask('q',"Client exiting: %s (%s) [%s]", user->GetFullRealHost().c_str(), user->GetIPString().c_str(), operreason->c_str());
		local_users.erase(lu);
	}

	if (!clientlist.erase(user->nick))
		ServerInstance->Logs->Log("USERS", LOG_DEFAULT, "ERROR: Nick not found in clientlist, cannot remove: " + user->nick);

	uuidlist.erase(user->uuid);
	user->PurgeEmptyChannels();
}

void UserManager::AddClone(User* user)
{
	CloneCounts& counts = clonemap[user->GetCIDRMask()];
	counts.global++;
	if (IS_LOCAL(user))
		counts.local++;
}

void UserManager::RemoveCloneCounts(User *user)
{
	CloneMap::iterator it = clonemap.find(user->GetCIDRMask());
	if (it != clonemap.end())
	{
		CloneCounts& counts = it->second;
		counts.global--;
		if (counts.global == 0)
		{
			// No more users from this IP, remove entry from the map
			clonemap.erase(it);
			return;
		}

		if (IS_LOCAL(user))
			counts.local--;
	}
}

void UserManager::RehashCloneCounts()
{
	clonemap.clear();

	const user_hash& hash = ServerInstance->Users.GetUsers();
	for (user_hash::const_iterator i = hash.begin(); i != hash.end(); ++i)
	{
		User* u = i->second;
		AddClone(u);
	}
}

const UserManager::CloneCounts& UserManager::GetCloneCounts(User* user) const
{
	CloneMap::const_iterator it = clonemap.find(user->GetCIDRMask());
	if (it != clonemap.end())
		return it->second;
	else
		return zeroclonecounts;
}

void UserManager::ServerNoticeAll(const char* text, ...)
{
	std::string message;
	VAFORMAT(message, text, text);
	message = "NOTICE $" + ServerInstance->Config->ServerName + " :" + message;

	for (LocalList::const_iterator i = local_users.begin(); i != local_users.end(); ++i)
	{
		User* t = *i;
		t->WriteServ(message);
	}
}

/* this returns true when all modules are satisfied that the user should be allowed onto the irc server
 * (until this returns true, a user will block in the waiting state, waiting to connect up to the
 * registration timeout maximum seconds)
 */
bool UserManager::AllModulesReportReady(LocalUser* user)
{
	ModResult res;
	FIRST_MOD_RESULT(OnCheckReady, res, (user));
	return (res == MOD_RES_PASSTHRU);
}

/**
 * This function is called once a second from the mainloop.
 * It is intended to do background checking on all the users, e.g. do
 * ping checks, registration timeouts, etc.
 */
void UserManager::DoBackgroundUserStuff()
{
	for (LocalList::iterator i = local_users.begin(); i != local_users.end(); )
	{
		// It's possible that we quit the user below due to ping timeout etc. and QuitUser() removes it from the list
		LocalUser* curr = *i;
		++i;

		if (curr->CommandFloodPenalty || curr->eh.getSendQSize())
		{
			unsigned int rate = curr->MyClass->GetCommandRate();
			if (curr->CommandFloodPenalty > rate)
				curr->CommandFloodPenalty -= rate;
			else
				curr->CommandFloodPenalty = 0;
			curr->eh.OnDataReady();
		}

		switch (curr->registered)
		{
			case REG_ALL:
				if (ServerInstance->Time() >= curr->nping)
				{
					// This user didn't answer the last ping, remove them
					if (!curr->lastping)
					{
						time_t time = ServerInstance->Time() - (curr->nping - curr->MyClass->GetPingTime());
						const std::string message = "Ping timeout: " + ConvToStr(time) + (time != 1 ? " seconds" : " second");
						this->QuitUser(curr, message);
						continue;
					}

					curr->Write("PING :" + ServerInstance->Config->ServerName);
					curr->lastping = 0;
					curr->nping = ServerInstance->Time() + curr->MyClass->GetPingTime();
				}
				break;
			case REG_NICKUSER:
				if (AllModulesReportReady(curr))
				{
					/* User has sent NICK/USER, modules are okay, DNS finished. */
					curr->FullConnect();
					continue;
				}

				// If the user has been quit in OnCheckReady then we shouldn't
				// quit them again for having a registration timeout.
				if (curr->quitting)
					continue;
				break;
		}

		if (curr->registered != REG_ALL && curr->MyClass && (ServerInstance->Time() > (curr->signon + curr->MyClass->GetRegTimeout())))
		{
			/*
			 * registration timeout -- didnt send USER/NICK/HOST
			 * in the time specified in their connection class.
			 */
			this->QuitUser(curr, "Registration timeout");
			continue;
		}
	}
}

already_sent_t UserManager::NextAlreadySentId()
{
	if (++already_sent_id == 0)
	{
		// Wrapped around, reset the already_sent ids of all users
		already_sent_id = 1;
		for (LocalList::iterator i = local_users.begin(); i != local_users.end(); ++i)
		{
			LocalUser* user = *i;
			user->already_sent = 0;
		}
	}
	return already_sent_id;
}
