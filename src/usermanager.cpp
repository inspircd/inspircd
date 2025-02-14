/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2019 iwalkalone <iwalkalone69@gmail.com>
 *   Copyright (C) 2013-2016, 2018 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2013, 2018-2024 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2013, 2015 Adam <Adam@anope.org>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2008-2009 Craig Edwards <brain@inspircd.org>
 *   Copyright (C) 2008 Robin Burchell <robin+git@viroteck.net>
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
#include "clientprotocolmsg.h"
#include "iohook.h"
#include "xline.h"

namespace
{
	class WriteCommonQuit final
		: public User::ForEachNeighborHandler
	{
		ClientProtocol::Messages::Quit quitmsg;
		ClientProtocol::Event quitevent;
		ClientProtocol::Messages::Quit operquitmsg;
		ClientProtocol::Event operquitevent;

		void Execute(LocalUser* user) override
		{
			user->Send(user->IsOper() ? operquitevent : quitevent);
		}

	public:
		WriteCommonQuit(User* user, const std::string& msg, const std::string& opermsg)
			: quitmsg(user, msg)
			, quitevent(ServerInstance->GetRFCEvents().quit, quitmsg)
			, operquitmsg(user, opermsg)
			, operquitevent(ServerInstance->GetRFCEvents().quit, operquitmsg)
		{
			user->ForEachNeighbor(*this, false);
		}
	};

	void CheckPingTimeout(LocalUser* user)
	{
		// Check if it is time to ping the user yet.
		if (ServerInstance->Time() < user->nextping)
			return;

		// This user didn't answer the last ping, remove them.
		if (!user->lastping)
		{
			time_t secs = ServerInstance->Time() - (user->nextping - user->GetClass()->pingtime);
			const std::string message = "Ping timeout: " + ConvToStr(secs) + (secs != 1 ? " seconds" : " second");
			ServerInstance->Users.QuitUser(user, message);
			return;
		}

		user->lastping = 0;
		user->nextping = ServerInstance->Time() + user->GetClass()->pingtime;

		// If the user has an I/O hook that can handle pinging use that instead.
		IOHook* hook = user->eh.GetIOHook();
		while (hook)
		{
			if (hook->Ping())
				return; // Client has been pinged.

			IOHookMiddle* middlehook = IOHookMiddle::ToMiddleHook(hook);
			hook = middlehook ? middlehook->GetNextHook() : nullptr;
		}


		// Send a ping to the client using an IRC message.
		ClientProtocol::Messages::Ping ping;
		user->Send(ServerInstance->GetRFCEvents().ping, ping);
	}

	void CheckConnectionTimeout(LocalUser* user)
	{
		if (user->GetClass() && (ServerInstance->Time() > static_cast<time_t>(user->signon + user->GetClass()->connection_timeout)))
		{
			// Either the user did not send NICK/USER or a module blocked connection in
			// OnCheckReady until the client timed out.
			ServerInstance->Users.QuitUser(user, "Connection timeout");
		}
	}

	void CheckModulesReady(LocalUser* user)
	{
		ModResult res;
		FIRST_MOD_RESULT(OnCheckReady, res, (user));
		if (res == MOD_RES_PASSTHRU)
		{
			// User has sent NICK/USER and modules are ready.
			user->FullConnect();
			return;
		}

		// If the user has been quit in OnCheckReady then we shouldn't quit
		// them again for having a registration timeout.
		if (!user->quitting)
			CheckConnectionTimeout(user);
	}
}

UserManager::UserManager()
{
	// We need to define a constructor here to work around a Clang bug.
}

UserManager::~UserManager()
{
	for (const auto& [_, client] : clientlist)
		delete client;
}

void UserManager::AddUser(int socket, ListenSocket* via, const irc::sockets::sockaddrs& client, const irc::sockets::sockaddrs& server)
{
	// User constructor allocates a new UUID for the user and inserts it into the uuidlist
	LocalUser* const New = new LocalUser(socket, client, server);
	UserIOHandler* eh = &New->eh;

	ServerInstance->Logs.Debug("USERS", "New user fd: {}", socket);

	this->unknown_count++;
	this->clientlist[New->nick] = New;
	this->AddClone(New);
	this->local_users.push_front(New);
	FOREACH_MOD(OnUserInit, (New));

	if (!SocketEngine::AddFd(eh, FD_WANT_FAST_READ | FD_WANT_EDGE_WRITE))
	{
		ServerInstance->Logs.Debug("USERS", "Internal error on new connection");
		this->QuitUser(New, "Internal error handling connection");
		return;
	}

	// If this listener has an IO hook provider set then tell it about the connection
	for (ListenSocket::IOHookProvList::iterator i = via->iohookprovs.begin(); i != via->iohookprovs.end(); ++i)
	{
		ListenSocket::IOHookProvRef& iohookprovref = *i;
		if (!iohookprovref)
		{
			if (iohookprovref.GetProvider().empty())
				continue;

			const char* hooktype = i == via->iohookprovs.begin() ? "hook" : "sslprofile";
			ServerInstance->Logs.Warning("USERS", "Non-existent I/O hook '{}' in <bind:{}> tag at {}",
				iohookprovref.GetProvider(), hooktype, via->bind_tag->source.str());
			this->QuitUser(New, FMT::format("Internal error handling connection (misconfigured {})", hooktype));
			return;
		}

		iohookprovref->OnAccept(eh, client, server);

		// IOHook could have encountered a fatal error, e.g. if the TLS ClientHello
		// was already in the queue and there was no common TLS version.
		if (!eh->GetError().empty())
		{
			QuitUser(New, eh->GetError());
			return;
		}
	}

	if (this->local_users.size() > ServerInstance->Config->SoftLimit)
	{
		ServerInstance->SNO.WriteToSnoMask('a', "Warning: softlimit value has been reached: {} clients", ServerInstance->Config->SoftLimit);
		this->QuitUser(New, "No more connections allowed");
		return;
	}

	if (!New->FindConnectClass())
		return; // User does not match any connect classes.

	/*
	 * even with bancache, we still have to keep User::exempt current.
	 * besides that, if we get a positive bancache hit, we still won't fuck
	 * them over if they are exempt. -- w00t
	 */
	New->exempt = (ServerInstance->XLines->MatchesLine("E", New) != nullptr);

	BanCacheHit* const b = ServerInstance->BanCache.GetHit(New->GetAddress());
	if (b)
	{
		if (!b->Type.empty() && !New->exempt)
		{
			/* user banned */
			ServerInstance->Logs.Debug("BANCACHE", "Positive hit for {}", New->GetAddress());

			if (!ServerInstance->Config->XLineMessage.empty())
				New->WriteNumeric(ERR_YOUREBANNEDCREEP, ServerInstance->Config->XLineMessage);

			// IMPORTANT: we don't check XLineQuitPublic here because the only
			// person who might see the ban at this point is the affected user.
			this->QuitUser(New, b->Reason);
			return;
		}
		else
		{
			ServerInstance->Logs.Debug("BANCACHE", "Negative hit for {}", New->GetAddress());
		}
	}
	else
	{
		if (!New->exempt)
		{
			XLine* r = ServerInstance->XLines->MatchesLine("Z", New);

			if (r)
			{
				r->Apply(New);
				return;
			}
		}
	}

	if (ServerInstance->Config->RawLog)
		Log::NotifyRawIO(New, MessageType::NOTICE);

	FOREACH_MOD(OnChangeRemoteAddress, (New));
	if (!New->quitting)
		FOREACH_MOD(OnUserPostInit, (New));
}

void UserManager::QuitUser(User* user, const std::string& quitmessage, const std::string* operquitmessage)
{
	if (user->quitting)
	{
		ServerInstance->Logs.Debug("USERS", "BUG: Tried to quit quitting user: {}", user->nick);
		return;
	}

	if (IS_SERVER(user))
	{
		ServerInstance->Logs.Debug("USERS", "BUG: Tried to quit server user: {}", user->nick);
		return;
	}

	std::string quitmsg(quitmessage);
	std::string operquitmsg;
	if (operquitmessage)
		operquitmsg.assign(*operquitmessage);

	LocalUser* const localuser = IS_LOCAL(user);
	if (localuser)
	{
		ModResult modres;
		FIRST_MOD_RESULT(OnUserPreQuit, modres, (localuser, quitmsg, operquitmsg));
		if (modres == MOD_RES_DENY)
			return;
	}

	if (quitmsg.length() > ServerInstance->Config->Limits.MaxQuit)
		quitmsg.erase(ServerInstance->Config->Limits.MaxQuit + 1);

	if (operquitmsg.empty())
		operquitmsg.assign(quitmsg);
	else if (operquitmsg.length() > ServerInstance->Config->Limits.MaxQuit)
		operquitmsg.erase(ServerInstance->Config->Limits.MaxQuit + 1);

	user->quitting = true;
	ServerInstance->Logs.Debug("USERS", "QuitUser: {}={} '{}'", user->uuid, user->nick, quitmessage);
	if (localuser)
	{
		ClientProtocol::Messages::Error errormsg(FMT::format("Closing link: ({}) [{}]", user->GetRealUserHost(), operquitmsg));
		localuser->Send(ServerInstance->GetRFCEvents().error, errormsg);
	}

	ServerInstance->GlobalCulls.AddItem(user);

	if (user->IsFullyConnected())
	{
		FOREACH_MOD(OnUserQuit, (user, quitmsg, operquitmsg));
		WriteCommonQuit(user, quitmsg, operquitmsg);
	}
	else
		unknown_count--;

	if (IS_LOCAL(user))
	{
		LocalUser* lu = IS_LOCAL(user);
		FOREACH_MOD(OnUserDisconnect, (lu));
		lu->eh.Close();

		if (lu->IsFullyConnected())
		{
			ServerInstance->SNO.WriteToSnoMask('q', "Client exiting: {} ({}) [{}]", user->GetRealMask(),
				user->GetAddress(), operquitmsg);
		}
		local_users.erase(lu);
		if (lu->GetClass())
			lu->GetClass()->use_count--;
	}

	if (!clientlist.erase(user->nick))
		ServerInstance->Logs.Debug("USERS", "BUG: Nick not found in clientlist, cannot remove: {}", user->nick);

	uuidlist.erase(user->uuid);
	user->PurgeEmptyChannels();
	user->OperLogout();
}

void UserManager::AddClone(User* user)
{
	CloneCounts& counts = clonemap[user->GetCIDRMask()];
	counts.global++;
	if (IS_LOCAL(user))
		counts.local++;
}

void UserManager::RemoveCloneCounts(User* user)
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

	for (const auto& [_, u] : ServerInstance->Users.GetUsers())
		AddClone(u);
}

void UserManager::RehashServices()
{
	UserManager::ServiceList newservices;
	for (const auto& [_, user] : ServerInstance->Users.GetUsers())
	{
		if (user->server->IsService())
			newservices.push_back(user);
	}
	std::swap(ServerInstance->Users.all_services, newservices);
}

const UserManager::CloneCounts& UserManager::GetCloneCounts(User* user) const
{
	CloneMap::const_iterator it = clonemap.find(user->GetCIDRMask());
	if (it != clonemap.end())
		return it->second;
	else
		return zeroclonecounts;
}

/**
 * This function is called once a second from the mainloop.
 * It is intended to do background checking on all the users, e.g. do
 * ping checks, connection timeouts, etc.
 */
void UserManager::DoBackgroundUserStuff()
{
	for (LocalList::iterator i = local_users.begin(); i != local_users.end(); )
	{
		// It's possible that we quit the user below due to ping timeout etc. and QuitUser() removes it from the list
		LocalUser* curr = *i;
		++i;

		if (curr->CommandFloodPenalty || curr->eh.GetSendQSize())
		{
			unsigned long rate = curr->GetClass()->commandrate;
			if (curr->CommandFloodPenalty > rate)
				curr->CommandFloodPenalty -= rate;
			else
				curr->CommandFloodPenalty = 0;
			curr->eh.OnDataReady();
		}

		switch (curr->connected)
		{
			case User::CONN_FULL:
				CheckPingTimeout(curr);
				break;

			case User::CONN_NICKUSER:
				CheckModulesReady(curr);
				break;

			default:
				CheckConnectionTimeout(curr);
				break;
		}
	}
}

uint64_t UserManager::NextAlreadySentId()
{
	if (++already_sent_id == 0)
	{
		// Wrapped around, reset the already_sent ids of all users
		already_sent_id = 1;
		for (auto* user : GetLocalUsers())
			user->already_sent = 0;
	}
	return already_sent_id;
}

User* UserManager::Find(const std::string& nickuuid, bool fullyconnected)
{
	if (nickuuid.empty())
		return nullptr;

	if (isdigit(nickuuid[0]))
		return FindUUID(nickuuid, fullyconnected);

	return FindNick(nickuuid, fullyconnected);
}

User* UserManager::FindNick(const std::string& nick, bool fullyconnected)
{
	if (nick.empty())
		return nullptr;

	UserMap::iterator uiter = this->clientlist.find(nick);
	if (uiter == this->clientlist.end())
		return nullptr;

	User* user = uiter->second;
	if (fullyconnected && !user->IsFullyConnected())
		return nullptr;

	return user;
}

User* UserManager::FindUUID(const std::string& uuid, bool fullyconnected)
{
	if (uuid.empty())
		return nullptr;

	UserMap::iterator uiter = this->uuidlist.find(uuid);
	if (uiter == this->uuidlist.end())
		return nullptr;

	User* user = uiter->second;
	if (fullyconnected && !user->IsFullyConnected())
		return nullptr;

	return user;
}
