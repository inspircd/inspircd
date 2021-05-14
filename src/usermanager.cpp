/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2019 iwalkalone <iwalkalone69@gmail.com>
 *   Copyright (C) 2019 Matt Schatz <genius3000@g3k.solutions>
 *   Copyright (C) 2013-2016, 2018 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2013, 2018-2021 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2013, 2015 Adam <Adam@anope.org>
 *   Copyright (C) 2013 Daniel Vassdal <shutter@canternet.org>
 *   Copyright (C) 2012, 2019 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2009 Uli Schlachter <psychon@inspircd.org>
 *   Copyright (C) 2008-2010 Craig Edwards <brain@inspircd.org>
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
#include "xline.h"
#include "iohook.h"

namespace
{
	class WriteCommonQuit : public User::ForEachNeighborHandler
	{
		ClientProtocol::Messages::Quit quitmsg;
		ClientProtocol::Event quitevent;
		ClientProtocol::Messages::Quit operquitmsg;
		ClientProtocol::Event operquitevent;

		void Execute(LocalUser* user) CXX11_OVERRIDE
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
			ModResult res;
			FIRST_MOD_RESULT(OnConnectionFail, res, (user, I_ERR_TIMEOUT));
			if (res == MOD_RES_ALLOW)
			{
				// A module is preventing this user from being timed out.
				user->lastping = 1;
				user->nextping = ServerInstance->Time() + user->MyClass->GetPingTime();
				return;
			}

			time_t secs = ServerInstance->Time() - (user->nextping - user->MyClass->GetPingTime());
			const std::string message = "Ping timeout: " + ConvToStr(secs) + (secs != 1 ? " seconds" : " second");
			ServerInstance->Users.QuitUser(user, message);
			return;
		}

		// Send a ping to the client.
		ClientProtocol::Messages::Ping ping;
		user->Send(ServerInstance->GetRFCEvents().ping, ping);
		user->lastping = 0;
		user->nextping = ServerInstance->Time() + user->MyClass->GetPingTime();
	}

	void CheckRegistrationTimeout(LocalUser* user)
	{
		if (user->GetClass() && (ServerInstance->Time() > (user->signon + user->GetClass()->GetRegTimeout())))
		{
			// Either the user did not send NICK/USER or a module blocked registration in
			// OnCheckReady until the client timed out.
			ServerInstance->Users.QuitUser(user, "Registration timeout");
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
			CheckRegistrationTimeout(user);
	}
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
	FOREACH_MOD(OnUserInit, (New));

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
		{
			if (!iohookprovref.GetProvider().empty())
			{
				ServerInstance->Logs->Log("USERS", LOG_DEBUG, "Non-existent I/O hook '%s' in <bind:%s> tag at %s",
					iohookprovref.GetProvider().c_str(),
					i == via->iohookprovs.begin() ? "hook" : "sslprofile",
					via->bind_tag->getTagLocation().c_str());
				this->QuitUser(New, "Internal error handling connection");
				return;
			}
			continue;
		}

		iohookprovref->OnAccept(eh, client, server);

		// IOHook could have encountered a fatal error, e.g. if the TLS ClientHello
		// was already in the queue and there was no common TLS version.
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

			if (ServerInstance->Config->HideBans)
				this->QuitUser(New, b->Type + "-lined", &b->Reason);
			else
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
	if (!New->quitting)
		FOREACH_MOD(OnUserPostInit, (New));
}

void UserManager::QuitUser(User* user, const std::string& quitmessage, const std::string* operquitmessage)
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

	std::string quitmsg(quitmessage);
	std::string operquitmsg;
	if (operquitmessage)
		operquitmsg.assign(*operquitmessage);

	LocalUser* const localuser = IS_LOCAL(user);
	if (localuser)
	{
		ModResult MOD_RESULT;
		FIRST_MOD_RESULT(OnUserPreQuit, MOD_RESULT, (localuser, quitmsg, operquitmsg));
		if (MOD_RESULT == MOD_RES_DENY)
			return;
	}

	if (quitmsg.length() > ServerInstance->Config->Limits.MaxQuit)
		quitmsg.erase(ServerInstance->Config->Limits.MaxQuit + 1);

	if (operquitmsg.empty())
		operquitmsg.assign(quitmsg);
	else if (operquitmsg.length() > ServerInstance->Config->Limits.MaxQuit)
		operquitmsg.erase(ServerInstance->Config->Limits.MaxQuit + 1);

	user->quitting = true;
	ServerInstance->Logs->Log("USERS", LOG_DEBUG, "QuitUser: %s=%s '%s'", user->uuid.c_str(), user->nick.c_str(), quitmessage.c_str());
	if (localuser)
	{
		ClientProtocol::Messages::Error errormsg(InspIRCd::Format("Closing link: (%s@%s) [%s]", user->ident.c_str(), user->GetRealHost().c_str(), operquitmsg.c_str()));
		localuser->Send(ServerInstance->GetRFCEvents().error, errormsg);
	}

	ServerInstance->GlobalCulls.AddItem(user);

	if (user->registered == REG_ALL)
	{
		FOREACH_MOD(OnUserQuit, (user, quitmsg, operquitmsg));
		WriteCommonQuit(user, quitmsg, operquitmsg);
	}
	else
		unregistered_count--;

	if (IS_LOCAL(user))
	{
		LocalUser* lu = IS_LOCAL(user);
		FOREACH_MOD(OnUserDisconnect, (lu));
		lu->eh.Close();

		if (lu->registered == REG_ALL)
			ServerInstance->SNO->WriteToSnoMask('q',"Client exiting: %s (%s) [%s]", user->GetFullRealHost().c_str(), user->GetIPString().c_str(), operquitmsg.c_str());
		local_users.erase(lu);
	}

	if (!clientlist.erase(user->nick))
		ServerInstance->Logs->Log("USERS", LOG_DEFAULT, "ERROR: Nick not found in clientlist, cannot remove: " + user->nick);

	uuidlist.erase(user->uuid);
	user->PurgeEmptyChannels();
	user->UnOper();
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
	ClientProtocol::Messages::Privmsg msg(ClientProtocol::Messages::Privmsg::nocopy, ServerInstance->FakeClient, ServerInstance->Config->GetServerName(), message, MSG_NOTICE);
	ClientProtocol::Event msgevent(ServerInstance->GetRFCEvents().privmsg, msg);

	for (LocalList::const_iterator i = local_users.begin(); i != local_users.end(); ++i)
	{
		LocalUser* user = *i;
		user->Send(msgevent);
	}
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
				CheckPingTimeout(curr);
				break;

			case REG_NICKUSER:
				CheckModulesReady(curr);
				break;

			default:
				CheckRegistrationTimeout(curr);
				break;
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
