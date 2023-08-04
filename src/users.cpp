/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2019 linuxdaemon <linuxdaemon.irc@gmail.com>
 *   Copyright (C) 2018 systocrat <systocrat@outlook.com>
 *   Copyright (C) 2018 Dylan Frank <b00mx0r@aureus.pw>
 *   Copyright (C) 2013, 2016-2022 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2013 Daniel Vassdal <shutter@canternet.org>
 *   Copyright (C) 2013 ChrisTX <xpipe@hotmail.de>
 *   Copyright (C) 2013 Adam <Adam@anope.org>
 *   Copyright (C) 2012-2016, 2018 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012, 2019 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2012 DjSlash <djslash@djslash.org>
 *   Copyright (C) 2011 jackmcbarn <jackmcbarn@inspircd.org>
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2008 Thomas Stagner <aquanight@inspircd.org>
 *   Copyright (C) 2008 John Brooks <special@inspircd.org>
 *   Copyright (C) 2007, 2009 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2006-2009 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2004, 2006-2009 Craig Edwards <brain@inspircd.org>
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
#include "clientprotocolevent.h"
#include "xline.h"

enum
{
	// From RFC 1459.
	ERR_NICKNAMEINUSE = 433,

	// From ircu.
	RPL_YOURDISPLAYEDHOST = 396,
};

ClientProtocol::MessageList LocalUser::sendmsglist;

bool User::IsNoticeMaskSet(unsigned char sm) const
{
	if (!SnomaskManager::IsSnomask(sm))
		return false;
	return (snomasks[sm-65]);
}

bool User::IsModeSet(unsigned char m) const
{
	ModeHandler* mh = ServerInstance->Modes.FindMode(m, MODETYPE_USER);
	return (mh && modes[mh->GetId()]);
}

std::string User::GetModeLetters(bool includeparams) const
{
	std::string ret(1, '+');
	std::string params;

	for (const auto& [_, mh] : ServerInstance->Modes.GetModes(MODETYPE_USER))
	{
		if (!IsModeSet(mh))
			continue;

		ret.push_back(mh->GetModeChar());
		if ((includeparams) && (mh->NeedsParam(true)))
		{
			const std::string val = mh->GetUserParameter(this);
			if (!val.empty())
				params.append(1, ' ').append(val);
		}
	}

	ret += params;
	return ret;
}

User::User(const std::string& uid, Server* srv, Type type)
	: Extensible(ExtensionType::USER)
	, nickchanged(ServerInstance->Time())
	, uuid(uid)
	, server(srv)
	, connected(CONN_NONE)
	, quitting(false)
	, uniqueusername(false)
	, usertype(type)
{
	ServerInstance->Logs.Debug("USERS", "New UUID for user: {}", uuid);

	if (srv->IsService())
		ServerInstance->Users.all_services.push_back(this);

	// Do not insert FakeUsers into the uuidlist so FindUUID() won't return them which is the desired behavior
	if (type != User::TYPE_SERVER)
	{
		if (!ServerInstance->Users.uuidlist.emplace(uuid, this).second)
			throw CoreException("Duplicate UUID in User constructor: " + uuid);
	}
}

const std::string& User::GetAddress()
{
	if (cached_address.empty())
	{
		cached_address = client_sa.addr();

		// If the user is connecting from an IPv4 address like ::1 we
		// need to partially expand the address to avoid issues with
		// the IRC wire format.
		if (cached_address[0] == ':')
			cached_address.insert(cached_address.begin(), 1, '0');

		cached_address.shrink_to_fit();
	}

	return cached_address;
}

const std::string& User::GetUserAddress()
{
	if (cached_useraddress.empty())
	{
		cached_useraddress = INSP_FORMAT("{}@{}", GetRealUser(), GetAddress());
		cached_useraddress.shrink_to_fit();
	}

	return cached_useraddress;
}
const std::string& User::GetUserHost()
{
	if (cached_userhost.empty())
	{
		cached_userhost = INSP_FORMAT("{}@{}", GetDisplayedUser(), GetDisplayedHost());
		cached_userhost.shrink_to_fit();
	}

	return cached_userhost;
}

const std::string& User::GetRealUserHost()
{
	if (cached_realuserhost.empty())
	{
		cached_realuserhost = INSP_FORMAT("{}@{}", GetRealUser(), GetRealHost());
		cached_realuserhost.shrink_to_fit();
	}

	return cached_realuserhost;
}

const std::string& User::GetMask()
{
	if (cached_mask.empty())
	{
		cached_mask = INSP_FORMAT("{}!{}@{}", nick, GetDisplayedUser(), GetDisplayedHost());
		cached_mask.shrink_to_fit();
	}

	return cached_mask;
}

const std::string& User::GetRealMask()
{
	if (cached_realmask.empty())
	{
		cached_realmask = INSP_FORMAT("{}!{}@{}", nick, GetRealUser(), GetRealHost());
		cached_realmask.shrink_to_fit();
	}

	return cached_realmask;
}

LocalUser::LocalUser(int myfd, const irc::sockets::sockaddrs& clientsa, const irc::sockets::sockaddrs& serversa)
	: User(ServerInstance->UIDGen.GetUID(), ServerInstance->FakeClient->server, User::TYPE_LOCAL)
	, eh(this)
	, server_sa(serversa)
	, quitting_sendq(false)
	, lastping(true)
	, exempt(false)
{
	signon = ServerInstance->Time();
	// The user's default nick is their UUID
	nick = uuid;
	eh.SetFd(myfd);
	memcpy(&client_sa, &clientsa, sizeof(irc::sockets::sockaddrs));
	ChangeRealUser(uuid, true);
	ChangeRealHost(GetAddress(), true);
}

FakeUser::FakeUser(const std::string& uid, Server* srv)
	: User(uid, srv, TYPE_SERVER)
{
	nick = srv->GetName();
}

FakeUser::FakeUser(const std::string& uid, const std::string& sname, const std::string& sdesc)
	: User(uid, new Server(uid, sname, sdesc), TYPE_SERVER)
{
	nick = sname;
}

const std::string& FakeUser::GetMask()
{
	return server->GetPublicName();
}

const std::string& FakeUser::GetRealMask()
{
	return server->GetPublicName();
}

void UserIOHandler::OnDataReady()
{
	if (user->quitting)
		return;

	if (recvq.length() > user->GetClass()->recvqmax && !user->HasPrivPermission("users/flood/increased-buffers"))
	{
		ServerInstance->Users.QuitUser(user, "RecvQ exceeded");
		ServerInstance->SNO.WriteToSnoMask('a', "User {} RecvQ of {} exceeds connect class maximum of {}",
			user->nick, recvq.length(), user->GetClass()->recvqmax);
		return;
	}

	unsigned long sendqmax = ULONG_MAX;
	if (!user->HasPrivPermission("users/flood/increased-buffers"))
		sendqmax = user->GetClass()->softsendqmax;

	unsigned long penaltymax = ULONG_MAX;
	if (!user->HasPrivPermission("users/flood/no-fakelag"))
		penaltymax = user->GetClass()->penaltythreshold * 1000;

	// The cleaned message sent by the user or empty if not found yet.
	std::string line;

	// The position of the most \n character or npos if not found yet.
	std::string::size_type eolpos;

	// The position within the recvq of the current character.
	std::string::size_type qpos;

	while (user->CommandFloodPenalty < penaltymax && GetSendQSize() < sendqmax)
	{
		// Check the newly received data for an EOL.
		eolpos = recvq.find('\n', checked_until);
		if (eolpos == std::string::npos)
		{
			checked_until = recvq.length();
			return;
		}

		// We've found a line! Clean it up and move it to the line buffer.
		line.reserve(eolpos);
		for (qpos = 0; qpos < eolpos; ++qpos)
		{
			char c = recvq[qpos];
			switch (c)
			{
				case '\0':
					c = ' ';
					break;
				case '\r':
					continue;
			}

			line.push_back(c);
		}

		// just found a newline. Terminate the string, and pull it out of recvq
		recvq.erase(0, eolpos + 1);
		checked_until = 0;

		// TODO should this be moved to when it was inserted in recvq?
		ServerInstance->Stats.Recv += qpos;
		user->bytes_in += qpos;
		user->cmds_in++;

		ServerInstance->Parser.ProcessBuffer(user, line);
		if (user->quitting)
			return;

		// clear() does not reclaim memory associated with the string, so our .reserve() call is safe
		line.clear();
	}

	if (user->CommandFloodPenalty >= penaltymax && !user->GetClass()->fakelag)
		ServerInstance->Users.QuitUser(user, "Excess Flood");
}

void UserIOHandler::AddWriteBuf(const std::string& data)
{
	if (user->quitting_sendq)
		return;

	if (!user->quitting
		&& (user->GetClass() && GetSendQSize() + data.length() > user->GetClass()->hardsendqmax)
		&& !user->HasPrivPermission("users/flood/increased-buffers"))
	{
		user->quitting_sendq = true;
		ServerInstance->GlobalCulls.AddSQItem(user);
		return;
	}

	// We still want to append data to the sendq of a quitting user,
	// e.g. their ERROR message that says 'closing link'

	WriteData(data);
}

bool UserIOHandler::OnChangeLocalSocketAddress(const irc::sockets::sockaddrs& sa)
{
	memcpy(&user->server_sa, &sa, sizeof(irc::sockets::sockaddrs));
	return true;
}

bool UserIOHandler::OnChangeRemoteSocketAddress(const irc::sockets::sockaddrs& sa)
{
	user->ChangeRemoteAddress(sa);
	return !user->quitting;
}

void UserIOHandler::OnError(BufferedSocketError sockerr)
{
	ServerInstance->Users.QuitUser(user, GetError());
}

Cullable::Result User::Cull()
{
	if (!quitting)
	{
		ServerInstance->Logs.Debug("CULL", "BUG: User {} (@{}) was culled without being quit first!",
			uuid, fmt::ptr(this));
		ServerInstance->Users.QuitUser(this, "Culled without QuitUser");
	}

	if (client_sa.family() != AF_UNSPEC)
		ServerInstance->Users.RemoveCloneCounts(this);

	if (server->IsService())
		stdalgo::erase(ServerInstance->Users.all_services, this);

	return Extensible::Cull();
}

Cullable::Result LocalUser::Cull()
{
	eh.Cull();
	return User::Cull();
}

Cullable::Result FakeUser::Cull()
{
	// Fake users don't quit, they just get culled.
	quitting = true;
	// Fake users are not inserted into UserManager::clientlist or uuidlist, so we don't need to modify those here
	return User::Cull();
}

bool User::OperLogin(const std::shared_ptr<OperAccount>& account, bool automatic, bool force)
{
	LocalUser* luser = IS_LOCAL(this);
	if (luser && !quitting && !force)
	{
		ModResult modres;
		FIRST_MOD_RESULT(OnPreOperLogin, modres, (luser, account, automatic));
		if (modres == MOD_RES_DENY)
			return false; // Module rejected the oper attempt.
	}

	// If this user is already logged in to an oper account then log them out.
	if (IsOper())
		OperLogout();

	FOREACH_MOD(OnOperLogin, (this, account, automatic));

	// When a user logs in we need to:
	//   1. Set the operator account (this is what IsOper checks).
	//   2. Set user mode o (oper) *WITHOUT* calling the mode handler.
	//   3. Add the user to the operator list.
	oper = account;
	auto* opermh = ServerInstance->Modes.FindMode('o', MODETYPE_USER);
	if (opermh)
	{
		SetMode(opermh, true);
		if (luser)
		{
			Modes::ChangeList changelist;
			changelist.push_add(opermh);
			ClientProtocol::Events::Mode modemsg(ServerInstance->FakeClient, nullptr, luser, changelist);
			luser->Send(modemsg);
		}
	}
	ServerInstance->Users.all_opers.push_back(this);

	FOREACH_MOD(OnPostOperLogin, (this, automatic));
	return true;
}

void User::OperLogout()
{
	if (!IsOper())
		return;

	FOREACH_MOD(OnOperLogout, (this));

	// When a user logs OUT we need to:
	//   1. Unset the operator account (this is what IsOper checks).
	//   2. Unset user mode o (oper) *WITHOUT* calling the mode handler.
	//   3. Remove the user from the operator list.
	auto account = oper;
	oper = nullptr;

	// If the user is quitting we shouldn't remove any modes as it results in
	// mode messages being broadcast across the network.
	if (!quitting)
	{
		// Remove any oper-only modes from the user.
		Modes::ChangeList changelist;
		for (const auto& [_, mh] : ServerInstance->Modes.GetModes(MODETYPE_USER))
		{
			if (mh->NeedsOper() && IsModeSet(mh))
				changelist.push_remove(mh);
		}
		ServerInstance->Modes.Process(this, nullptr, this, changelist);

		auto* opermh = ServerInstance->Modes.FindMode('o', MODETYPE_USER);
		if (opermh)
			SetMode(opermh, false);
	}

	stdalgo::vector::swaperase(ServerInstance->Users.all_opers, this);
	FOREACH_MOD(OnPostOperLogout, (this, account));
}

bool LocalUser::CheckLines(bool doZline)
{
	const char* check[] = { "G" , "K", (doZline) ? "Z" : nullptr, nullptr };

	if (!this->exempt)
	{
		for (int n = 0; check[n]; ++n)
		{
			XLine* r = ServerInstance->XLines->MatchesLine(check[n], this);

			if (r)
			{
				r->Apply(this);
				return true;
			}
		}
	}

	return false;
}

void LocalUser::FullConnect()
{
	ServerInstance->Stats.Connects++;
	this->idle_lastmsg = ServerInstance->Time();

	/*
	 * You may be thinking "wtf, we checked this in User::AddClient!" - and yes, we did, BUT.
	 * At the time AddClient is called, we don't have a resolved host, by here we probably do - which
	 * may put the user into a totally separate class with different restrictions! so we *must* check again.
	 * Don't remove this! -- w00t
	 */
	if (!FindConnectClass())
		return; // User does not match any connect classes.

	CheckLines();
	if (quitting)
		return;

	/*
	 * We don't set CONN_FULL until triggering OnUserConnect, so some module events don't spew out stuff
	 * for a user that doesn't exist yet.
	 */
	FOREACH_MOD(OnUserConnect, (this));

	// The user is now fully connected.
	if (ServerInstance->Users.unknown_count)
		ServerInstance->Users.unknown_count--;
	this->connected = CONN_FULL;

	FOREACH_MOD(OnPostConnect, (this));

	ServerInstance->SNO.WriteToSnoMask('c', "Client connecting on port {} (class {}): {} ({}) [{}\x0F]",
		server_sa.port(), GetClass()->GetName(), GetRealMask(), GetAddress(), GetRealName());

	ServerInstance->Logs.Debug("BANCACHE", "Adding NEGATIVE hit for " + this->GetAddress());
	ServerInstance->BanCache.AddHit(this->GetAddress(), "", "");
	// reset the flood penalty (which could have been raised due to things like auto +x)
	CommandFloodPenalty = 0;
}

void User::InvalidateCache()
{
	/* Invalidate cache */
	cached_address.clear();
	cached_useraddress.clear();
	cached_userhost.clear();
	cached_realuserhost.clear();
	cached_mask.clear();
	cached_realmask.clear();
}

bool User::ChangeNick(const std::string& newnick, time_t newts)
{
	if (quitting)
	{
		ServerInstance->Logs.Debug("USERS", "BUG: Attempted to change nick of a quitting user: " + this->nick);
		return false;
	}

	User* const InUse = ServerInstance->Users.FindNick(newnick);
	if (InUse == this)
	{
		// case change, don't need to check campers
		// and, if it's identical including case, we can leave right now
		// We also don't update the nick TS if it's a case change, either
		if (newnick == nick)
			return true;
	}
	else
	{
		/*
		 * Uh oh.. if the nickname is in use, and it's not in use by the person using it (doh) --
		 * then we have a potential collide. Check whether someone else is camping on the nick
		 * (i.e. connect -> send NICK, don't send USER.) If they are camping, force-change the
		 * camper to their UID, and allow the incoming nick change.
		 *
		 * If the guy using the nick is already using it, tell the incoming nick change to gtfo,
		 * because the nick is already (rightfully) in use. -- w00t
		 */
		if (InUse)
		{
			if (!InUse->IsFullyConnected())
			{
				/* force the camper to their UUID, and ask them to re-send a NICK. */
				LocalUser* const localuser = static_cast<LocalUser*>(InUse);
				localuser->OverruleNick();
			}
			else
			{
				/* No camping, tell the incoming user  to stop trying to change nick ;p */
				this->WriteNumeric(ERR_NICKNAMEINUSE, newnick, "Nickname is already in use.");
				return false;
			}
		}

		nickchanged = newts ? newts : ServerInstance->Time();
	}

	if (IsFullyConnected())
	{
		ClientProtocol::Messages::Nick nickmsg(this, newnick);
		ClientProtocol::Event nickevent(ServerInstance->GetRFCEvents().nick, nickmsg);
		this->WriteCommonRaw(nickevent, true);
	}
	const std::string oldnick = nick;
	nick = newnick;
	nick.shrink_to_fit();

	InvalidateCache();
	ServerInstance->Users.clientlist.erase(oldnick);
	ServerInstance->Users.clientlist[newnick] = this;

	if (IsFullyConnected())
		FOREACH_MOD(OnUserPostNick, (this, oldnick));

	return true;
}

void LocalUser::OverruleNick()
{
	{
		ClientProtocol::Messages::Nick nickmsg(this, this->uuid);
		this->Send(ServerInstance->GetRFCEvents().nick, nickmsg);
	}
	this->WriteNumeric(ERR_NICKNAMEINUSE, this->nick, "Nickname overruled.");

	// Clear the bit before calling ChangeNick() to make it NOT run the OnUserPostNick() hook
	this->connected &= ~CONN_NICK;
	this->ChangeNick(this->uuid);
}

const std::string& User::GetBanUser(bool real) const
{
	static const std::string wildcard = "*";
	return uniqueusername ? GetUser(real) : wildcard;
}

irc::sockets::cidr_mask User::GetCIDRMask() const
{
	unsigned char range = 0;
	switch (client_sa.family())
	{
		case AF_INET6:
			range = ServerInstance->Config->IPv6Range;
			break;
		case AF_INET:
			range = ServerInstance->Config->IPv4Range;
			break;
	}
	return irc::sockets::cidr_mask(client_sa, range);
}

void User::ChangeRemoteAddress(const irc::sockets::sockaddrs& sa)
{
	const std::string oldip(client_sa.family() == AF_UNSPEC ? "" : GetAddress());
	memcpy(&client_sa, &sa, sizeof(irc::sockets::sockaddrs));
	this->InvalidateCache();

	// If the users hostname was their IP then update it.
	if (GetRealHost() == oldip)
		ChangeRealHost(GetAddress(), false);
	if (GetDisplayedHost() == oldip)
		ChangeDisplayedHost(GetAddress());
}

void LocalUser::ChangeRemoteAddress(const irc::sockets::sockaddrs& sa)
{
	if (sa == client_sa)
		return;

	ServerInstance->Users.RemoveCloneCounts(this);
	User::ChangeRemoteAddress(sa);
	ServerInstance->Users.AddClone(this);

	// Recheck the connect class.
	if (FindConnectClass())
		FOREACH_MOD(OnChangeRemoteAddress, (this));
}

bool LocalUser::FindConnectClass()
{
	ServerInstance->Logs.Debug("CONNECTCLASS", "Finding a connect class for {} ({}) ...",
		uuid, GetRealMask());

	std::optional<Numeric::Numeric> errnum;
	for (const auto& klass : ServerInstance->Config->Classes)
	{
		ServerInstance->Logs.Debug("CONNECTCLASS", "Checking the {} connect class ...",
			klass->GetName());

		// Users can not be automatically assigned to a named class.
		if (klass->type == ConnectClass::NAMED)
		{
			ServerInstance->Logs.Debug("CONNECTCLASS", "The {} connect class is not suitable as neither <connect:allow> nor <connect:deny> are set.",
				klass->GetName());
			continue;
		}

		ModResult modres;
		FIRST_MOD_RESULT(OnPreChangeConnectClass, modres, (this, klass, errnum));
		if (modres != MOD_RES_DENY)
		{
			ServerInstance->Logs.Debug("CONNECTCLASS", "The {} connect class is suitable for {} ({}).",
				klass->GetName(), uuid, GetRealMask());

			ChangeConnectClass(klass, false);
			return !quitting;
		}
	}

	// The user didn't match any connect classes.
	if (connectclass)
	{
		connectclass->use_count--;
		connectclass = nullptr;
	}

	if (errnum)
		WriteNumeric(*errnum);
	ServerInstance->Users.QuitUser(this, "You are not allowed to connect to this server");
	return false;
}

void LocalUser::ChangeConnectClass(const std::shared_ptr<ConnectClass>& klass, bool force)
{
	// Let modules know the class is about to be changed.
	FOREACH_MOD(OnChangeConnectClass, (this, klass, force));
	if (quitting)
		return; // User hit some kind of restriction.

	// Assign the new connect class.
	if (connectclass)
		connectclass->use_count--;
	connectclass = klass;
	connectclass->use_count++;

	// Update the core user data that depends on connect class.
	nextping = ServerInstance->Time() + klass->pingtime;
	uniqueusername = klass->uniqueusername;

	// Let modules know the class has been changed.
	FOREACH_MOD(OnPostChangeConnectClass, (this, force));
}

void LocalUser::Write(const ClientProtocol::SerializedMessage& text)
{
	if (!eh.HasFd())
		return;

	if (ServerInstance->Config->RawLog)
	{
		if (text.empty())
			return;

		std::string::size_type nlpos = text.find_first_of("\r\n", 0, 2);
		if (nlpos == std::string::npos)
			nlpos = text.length();

		ServerInstance->Logs.RawIO("USEROUTPUT", "C[{}] O {}", uuid, std::string_view(text.c_str(), nlpos));
	}

	eh.AddWriteBuf(text);

	const size_t bytessent = text.length() + 2;
	ServerInstance->Stats.Sent += bytessent;
	this->bytes_out += bytessent;
	this->cmds_out++;
}

void LocalUser::Send(ClientProtocol::Event& protoev)
{
	if (!serializer)
	{
		ServerInstance->Logs.Debug("USERS", "BUG: LocalUser::Send() called on {} who does not have a serializer!",
			GetRealMask());
		return;
	}

	// In the most common case a static LocalUser field, sendmsglist, is passed to the event to be
	// populated. The list is cleared before returning.
	// To handle re-enters, if sendmsglist is non-empty upon entering the method then a temporary
	// list is used instead of the static one.
	if (sendmsglist.empty())
	{
		Send(protoev, sendmsglist);
		sendmsglist.clear();
	}
	else
	{
		ClientProtocol::MessageList msglist;
		Send(protoev, msglist);
	}
}

void LocalUser::Send(ClientProtocol::Event& protoev, ClientProtocol::MessageList& msglist)
{
	// Modules can personalize the messages sent per user for the event
	protoev.GetMessagesForUser(this, msglist);
	for (auto* msg : msglist)
	{
		ClientProtocol::Message& curr = *msg;
		ModResult res;
		FIRST_MOD_RESULT(OnUserWrite, res, (this, curr));
		if (res != MOD_RES_DENY)
			Write(serializer->SerializeForUser(this, curr));
	}
}

void User::WriteNumeric(const Numeric::Numeric& numeric)
{
	LocalUser* const localuser = IS_LOCAL(this);
	if (!localuser)
		return;

	ModResult MOD_RESULT;

	FIRST_MOD_RESULT(OnNumeric, MOD_RESULT, (this, numeric));

	if (MOD_RESULT == MOD_RES_DENY)
		return;

	ClientProtocol::Messages::Numeric numericmsg(numeric, localuser);
	localuser->Send(ServerInstance->GetRFCEvents().numeric, numericmsg);
}

void User::WriteRemoteNotice(const std::string& text)
{
	ServerInstance->PI->SendMessage(this, text, MessageType::NOTICE);
}

void LocalUser::WriteRemoteNotice(const std::string& text)
{
	WriteNotice(text);
}

namespace
{
	class WriteCommonRawHandler final
		: public User::ForEachNeighborHandler
	{
		ClientProtocol::Event& ev;

		void Execute(LocalUser* user) override
		{
			user->Send(ev);
		}

	public:
		WriteCommonRawHandler(ClientProtocol::Event& protoev)
			: ev(protoev)
		{
		}
	};
}

void User::WriteCommonRaw(ClientProtocol::Event& protoev, bool include_self)
{
	WriteCommonRawHandler handler(protoev);
	ForEachNeighbor(handler, include_self);
}

uint64_t User::ForEachNeighbor(ForEachNeighborHandler& handler, bool include_self)
{
	// The basic logic for visiting the neighbors of a user is to iterate the channel list of the user
	// and visit all users on those channels. Because two users may share more than one common channel,
	// we must skip users that we have already visited.
	// To do this, we make use of a global counter and an integral 'already_sent' field in LocalUser.
	// The global counter is incremented every time we do something for each neighbor of a user. Then,
	// before visiting a member we examine user->already_sent. If it's equal to the current counter, we
	// skip the member. Otherwise, we set it to the current counter and visit the member.

	// Ask modules to build a list of exceptions.
	// Mods may also exclude entire channels by erasing them from include_chans.
	User::NeighborList include_chans(chans.begin(), chans.end());
	User::NeighborExceptions exceptions;
	exceptions[this] = include_self;
	FOREACH_MOD(OnBuildNeighborList, (this, include_chans, exceptions));

	// Get next id, guaranteed to differ from the already_sent field of all users
	const uint64_t newid = ServerInstance->Users.NextAlreadySentId();

	// Handle exceptions first
	for (NeighborExceptions::const_iterator i = exceptions.begin(); i != exceptions.end(); ++i)
	{
		LocalUser* curr = IS_LOCAL(i->first);
		if (curr)
		{
			// Mark as visited to ensure we won't visit again if there is a common channel
			curr->already_sent = newid;
			// Always treat quitting users as excluded
			if ((i->second) && (!curr->quitting))
				handler.Execute(curr);
		}
	}

	// Now consider the real neighbors
	for (const auto* memb : include_chans)
	{
		for (const auto& [user, _] : memb->chan->GetUsers())
		{
			LocalUser* curr = IS_LOCAL(user);
			// User not yet visited?
			if ((curr) && (curr->already_sent != newid))
			{
				// Mark as visited and execute function
				curr->already_sent = newid;
				handler.Execute(curr);
			}
		}
	}

	return newid;
}

void User::WriteRemoteNumeric(const Numeric::Numeric& numeric)
{
	WriteNumeric(numeric);
}

/* return 0 or 1 depending if users u and u2 share one or more common channels
 * (used by QUIT, NICK etc which arent channel specific notices)
 *
 * The old algorithm in 1.0 for this was relatively inefficient, iterating over
 * the first users channels then the second users channels within the outer loop,
 * therefore it was a maximum of x*y iterations (upon returning 0 and checking
 * all possible iterations). However this new function instead checks against the
 * channel's userlist in the inner loop which is a std::map<User*,User*>
 * and saves us time as we already know what pointer value we are after.
 * Don't quote me on the maths as i am not a mathematician or computer scientist,
 * but i believe this algorithm is now x+(log y) maximum iterations instead.
 */
bool User::SharesChannelWith(User* other) const
{
	for (const auto* memb : chans)
	{
		if (memb->chan->HasUser(other))
			return true;
	}
	return false;
}

void User::ChangeRealName(const std::string& real)
{
	if (!this->realname.compare(real))
		return;

	FOREACH_MOD(OnChangeRealName, (this, real));

	this->realname.assign(real, 0, ServerInstance->Config->Limits.MaxReal);
	this->realname.shrink_to_fit();
}

void User::ChangeDisplayedHost(const std::string& newhost)
{
	if (GetDisplayedHost() == newhost)
		return;

	FOREACH_MOD(OnChangeHost, (this, newhost));

	if (realhost == newhost)
		this->displayhost.clear();
	else
		this->displayhost.assign(newhost, 0, ServerInstance->Config->Limits.MaxHost);
	this->displayhost.shrink_to_fit();

	this->InvalidateCache();

	if (IS_LOCAL(this) && connected != User::CONN_NONE)
		this->WriteNumeric(RPL_YOURDISPLAYEDHOST, this->GetDisplayedHost(), "is now your displayed host");
}

void User::ChangeRealHost(const std::string& newhost, bool resetdisplay)
{
	// If the real host is the new host and we are not resetting the
	// display host then we have nothing to do.
	const bool changehost = (realhost != newhost);
	if (!changehost && !resetdisplay)
		return;

	// If the displayhost is not set and we are not resetting it then
	// we need to copy it to the displayhost field.
	if (displayhost.empty() && !resetdisplay)
		displayhost = realhost;

	// If the displayhost is the new host or we are resetting it then
	// we clear its contents to save memory.
	else if (displayhost == newhost || resetdisplay)
		displayhost.clear();

	// If we are just resetting the display host then we don't need to
	// do anything else.
	if (!changehost)
	{
		InvalidateCache();
		return;
	}

	// Don't call the OnChangeRealHost event when initialising a user.
	const bool initializing = realhost.empty();
	if (!initializing)
		FOREACH_MOD(OnChangeRealHost, (this, newhost));

	realhost = newhost;
	realhost.shrink_to_fit();

	this->InvalidateCache();

	// Don't call the OnPostChangeRealHost event when initialising a user.
	if (!this->quitting && !initializing)
		FOREACH_MOD(OnPostChangeRealHost, (this));
}

void User::ChangeRealUser(const std::string& newuser, bool resetdisplay)
{
	// If the real user is the new user and we are not resetting the
	// display user then we have nothing to do.
	const bool changeuser = (realuser != newuser);
	if (!changeuser && !resetdisplay)
		return;

	// If the displayuser is not set and we are not resetting it then
	// we need to copy it to the displayuser field.
	if (displayuser.empty() && !resetdisplay)
		displayuser = realuser;

	// If the displayuser is the new user or we are resetting it then
	// we clear its contents to save memory.
	else if (displayuser == newuser || resetdisplay)
		displayuser.clear();

	// If we are just resetting the display user then we don't need to
	// do anything else.
	if (!changeuser)
		return;

	// Don't call the OnChangeRealUser event when initialising a user.
	const bool initializing = realuser.empty();
	if (!initializing)
		FOREACH_MOD(OnChangeRealUser, (this, newuser));

	realuser = newuser;
	realuser.shrink_to_fit();

	this->InvalidateCache();

	// Don't call the OnPostChangeRealUser event when initialising a user.
	if (!this->quitting && !initializing)
		FOREACH_MOD(OnPostChangeRealUser, (this));
}

void User::ChangeDisplayedUser(const std::string& newuser)
{
	if (GetDisplayedUser() == newuser)
		return;

	FOREACH_MOD(OnChangeUser, (this, newuser));

	if (realuser == newuser)
		this->displayuser.clear();
	else
		this->displayuser.assign(newuser, 0, ServerInstance->Config->Limits.MaxUser);
	this->displayuser.shrink_to_fit();

	this->InvalidateCache();
}

void User::PurgeEmptyChannels()
{
	// firstly decrement the count on each channel
	for (User::ChanList::iterator i = this->chans.begin(); i != this->chans.end(); )
	{
		Channel* c = (*i)->chan;
		++i;
		c->DelUser(this);
	}
}

void User::WriteNotice(const std::string& text)
{
	LocalUser* const localuser = IS_LOCAL(this);
	if (!localuser)
		return;

	ClientProtocol::Messages::Privmsg msg(ClientProtocol::Messages::Privmsg::nocopy, ServerInstance->FakeClient, localuser, text, MessageType::NOTICE);
	localuser->Send(ServerInstance->GetRFCEvents().privmsg, msg);
}

ConnectClass::ConnectClass(const std::shared_ptr<ConfigTag>& tag, Type t, const std::vector<std::string>& masks)
	: config(tag)
	, hosts(masks)
	, name("unnamed")
	, type(t)
	, fakelag(true)
	, maxconnwarn(true)
	, resolvehostnames(true)
	, uniqueusername(false)
{
}

ConnectClass::ConnectClass(const std::shared_ptr<ConfigTag>& tag, Type t, const std::vector<std::string>& masks, const std::shared_ptr<ConnectClass>& parent)
{
	Update(parent);
	name = "unnamed";
	type = t;
	hosts = masks;

	// Connect classes can inherit from each other but this is problematic for modules which can't use
	// ConnectClass::Update so we build a hybrid tag containing all of the values set on this class as
	// well as the parent class.
	config = std::make_shared<ConfigTag>(tag->name, tag->source);
	for (const auto& [key, value] : parent->config->GetItems())
	{
		// The class name and parent name are not inherited
		if (stdalgo::string::equalsci(key, "name") || stdalgo::string::equalsci(key, "parent"))
			continue;

		// Store the item in the config tag. If this item also
		// exists in the child it will be overwritten.
		config->GetItems()[key] = value;
	}

	for (const auto& [key, value] : tag->GetItems())
	{
		// This will overwrite the parent value if present.
		config->GetItems()[key] = value;
	}
}

void ConnectClass::Configure(const std::string& classname, const std::shared_ptr<ConfigTag>& tag)
{
	name = classname;

	password = tag->getString("password", password);
	passwordhash = tag->getString("hash", passwordhash);
	if (!password.empty() && (passwordhash.empty() || stdalgo::string::equalsci(passwordhash, "plaintext")))
	{
		ServerInstance->Logs.Normal("CONNECTCLASS", "<connect> tag '{}' at {} contains an plain text password, this is insecure!",
			name, tag->source.str());
	}

	irc::portparser portrange(tag->getString("port"), false);
	while (long port = portrange.GetToken())
	{
		if (port > std::numeric_limits<in_port_t>::min() && port <= std::numeric_limits<in_port_t>::max())
			ports.insert(static_cast<in_port_t>(port));
	}

	commandrate = tag->getNum<unsigned long>("commandrate", commandrate, 1);
	fakelag = tag->getBool("fakelag", fakelag);
	hardsendqmax = tag->getNum<unsigned long>("hardsendq", hardsendqmax, ServerInstance->Config->Limits.MaxLine);
	limit = tag->getNum<unsigned long>("limit", limit, 1);
	maxchans = tag->getNum<unsigned long>("maxchans", maxchans);
	maxconnwarn = tag->getBool("maxconnwarn", maxconnwarn);
	maxlocal = tag->getNum<unsigned long>("localmax", maxlocal);
	maxglobal = tag->getNum<unsigned long>("globalmax", maxglobal, maxlocal);
	penaltythreshold = tag->getNum<unsigned long>("threshold", penaltythreshold, 1);
	pingtime = tag->getDuration("pingfreq", pingtime);
	recvqmax = tag->getNum<unsigned long>("recvq", recvqmax, ServerInstance->Config->Limits.MaxLine);
	connection_timeout = tag->getDuration("timeout", connection_timeout);
	resolvehostnames = tag->getBool("resolvehostnames", resolvehostnames);
	softsendqmax = tag->getNum<unsigned long>("softsendq", softsendqmax, ServerInstance->Config->Limits.MaxLine);
	uniqueusername = tag->getBool("uniqueusername", uniqueusername);
}

void ConnectClass::Update(const std::shared_ptr<ConnectClass>& src)
{
	ServerInstance->Logs.Debug("CONNECTCLASS", "Updating {} from {}", name, src->name);
	commandrate = src->commandrate;
	config = src->config;
	fakelag = src->fakelag;
	hardsendqmax = src->hardsendqmax;
	hosts = src->hosts;
	limit = src->limit;
	maxchans = src->maxchans;
	maxconnwarn = src->maxconnwarn;
	maxglobal = src->maxglobal;
	maxlocal = src->maxlocal;
	name = src->name;
	password = src->password;
	passwordhash = src->passwordhash;
	penaltythreshold = src->penaltythreshold;
	pingtime = src->pingtime;
	ports = src->ports;
	recvqmax = src->recvqmax;
	connection_timeout = src->connection_timeout;
	resolvehostnames = src->resolvehostnames;
	softsendqmax = src->softsendqmax;
	type = src->type;
	uniqueusername = src->uniqueusername;
}

AwayState::AwayState(const std::string& m, time_t t)
	: message(m, 0, ServerInstance->Config->Limits.MaxAway)
	, time(t ? t : ServerInstance->Time())
{
}

OperType::OperType(const std::string& n, const std::shared_ptr<ConfigTag>& t)
	: config(std::make_shared<ConfigTag>("generated", FilePosition("<generated>", 0, 0)))
	, name(n)
{
	if (t)
		Configure(t, true);
}

void OperType::Configure(const std::shared_ptr<ConfigTag>& tag, bool merge)
{
	commands.AddList(tag->getString("commands"));
	privileges.AddList(tag->getString("privs"));

	auto modefn = [&tag](ModeParser::ModeStatus& modes, const std::string& key)
	{
		bool adding = true;
		for (const auto chr : tag->getString(key))
		{
			if (chr == '+' || chr == '-')
				adding = (chr == '+');
			else if (chr == '*' && adding)
				modes.set();
			else if (chr == '*' && !adding)
				modes.reset();
			else if (ModeParser::IsModeChar(chr))
				modes.set(ModeParser::GetModeIndex(chr), adding);
		}
	};
	modefn(chanmodes, "chanmodes");
	modefn(usermodes, "usermodes");
	modefn(snomasks, "snomasks");

	if (merge)
		MergeTag(tag);
}

std::string OperType::GetCommands(bool all) const
{
	if (all)
		return commands.ToString();

	std::vector<std::string> ret;
	for (const auto& [_, cmd] : ServerInstance->Parser.GetCommands())
	{
		if (cmd->access_needed == CmdAccess::OPERATOR && CanUseCommand(cmd->name))
			ret.push_back(cmd->name);
	}
	std::sort(ret.begin(), ret.end());
	return stdalgo::string::join(ret);
}

std::string OperType::GetModes(ModeType mt, bool all) const
{
	std::string ret;
	for (unsigned char chr = '0'; chr <= 'z'; ++chr)
	{
		if (!ModeParser::IsModeChar(chr))
			continue;

		ModeHandler* mh = ServerInstance->Modes.FindMode(chr, mt);
		if ((all || (mh && mh->NeedsOper())) && CanUseMode(mt, chr))
			ret.push_back(chr);
	}
	return ret;
}

std::string OperType::GetSnomasks(bool all) const
{
	std::string ret;
	for (unsigned char sno = 'A'; sno <= 'z'; ++sno)
	{
		if (!SnomaskManager::IsSnomask(sno))
			continue;

		if ((all || ServerInstance->SNO.IsSnomaskUsable(sno)) && CanUseSnomask(sno))
			ret.push_back(sno);
	}
	return ret;
}

bool OperType::CanUseCommand(const std::string& cmd) const
{
	return commands.Contains(cmd);
}

bool OperType::CanUseMode(ModeType mt, unsigned char chr) const
{
	const size_t index = ModeParser::GetModeIndex(chr);
	if (index == ModeParser::MODEID_MAX)
		return false;

	return (mt == MODETYPE_USER ? usermodes : chanmodes)[index];
}

bool OperType::CanUseSnomask(unsigned char chr) const
{
	if (SnomaskManager::IsSnomask(chr))
		return snomasks[ModeParser::GetModeIndex(chr)];
	return false;
}

bool OperType::HasPrivilege(const std::string& priv) const
{
	return privileges.Contains(priv);
}

void OperType::MergeTag(const std::shared_ptr<ConfigTag>& tag)
{
	for (const auto& [key, value] : tag->GetItems())
	{
		if (stdalgo::string::equalsci(key, "name") || stdalgo::string::equalsci(key, "type")
			|| stdalgo::string::equalsci(key, "classes"))
		{
			// These are meta keys for linking the oper/type/class tags.
			continue;
		}

		if (stdalgo::string::equalsci(key, "commands") || stdalgo::string::equalsci(key, "privs")
			|| stdalgo::string::equalsci(key, "chanmodes") || stdalgo::string::equalsci(key, "usermodes")
			|| stdalgo::string::equalsci(key, "snomasks"))
		{
			// These have already been parsed into the object.
			continue;
		}

		config->GetItems()[key] = value;
	}
}

OperAccount::OperAccount(const std::string& n, const std::shared_ptr<OperType>& o, const std::shared_ptr<ConfigTag>& t)
	: OperType(n, nullptr)
	, password(t->getString("password"))
	, passwordhash(t->getString("hash", "plaintext", 1))
	, type(o ? o->GetName() : n)
{
	autologin = t->getEnum("autologin", AutoLogin::NEVER, {
		{ "strict",  AutoLogin::STRICT  },
		{ "relaxed", AutoLogin::RELAXED },
		{ "never",   AutoLogin::NEVER   },
	});

	if (o)
	{
		chanmodes = o->chanmodes;
		commands = o->commands;
		privileges = o->privileges;
		snomasks = o->snomasks;
		usermodes = o->usermodes;
		Configure(o->GetConfig(), true);
	}
	Configure(t, true);
}

bool OperAccount::CanAutoLogin(LocalUser* user) const
{
	switch (autologin)
	{
		case AutoLogin::STRICT:
			return user->nick == GetName();

		case AutoLogin::RELAXED:
			return true;

		case AutoLogin::NEVER:
			return false;
	}

	// Should never be reached.
	return false;
}

bool OperAccount::CheckPassword(const std::string& pw) const
{
	return InspIRCd::CheckPassword(password, passwordhash, pw);
}
