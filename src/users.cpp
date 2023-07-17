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
#include "xline.h"

ClientProtocol::MessageList LocalUser::sendmsglist;

bool User::IsNoticeMaskSet(unsigned char sm)
{
	if (!isalpha(sm))
		return false;
	return (snomasks[sm-65]);
}

bool User::IsModeSet(unsigned char m) const
{
	ModeHandler* mh = ServerInstance->Modes->FindMode(m, MODETYPE_USER);
	return (mh && modes[mh->GetId()]);
}

std::string User::GetModeLetters(bool includeparams) const
{
	std::string ret(1, '+');
	std::string params;

	for (unsigned char i = 'A'; i <= 'z'; i++)
	{
		const ModeHandler* const mh = ServerInstance->Modes.FindMode(i, MODETYPE_USER);
		if ((!mh) || (!IsModeSet(mh)))
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

User::User(const std::string& uid, Server* srv, UserType type)
	: age(ServerInstance->Time())
	, signon(0)
	, uuid(uid)
	, server(srv)
	, registered(REG_NONE)
	, quitting(false)
	, uniqueusername(false)
	, usertype(type)
{
	client_sa.sa.sa_family = AF_UNSPEC;

	ServerInstance->Logs->Log("USERS", LOG_DEBUG, "New UUID for user: %s", uuid.c_str());

	if (srv->IsULine())
		ServerInstance->Users.all_ulines.push_back(this);

	// Do not insert FakeUsers into the uuidlist so FindUUID() won't return them which is the desired behavior
	if (type != USERTYPE_SERVER)
	{
		if (!ServerInstance->Users.uuidlist.insert(std::make_pair(uuid, this)).second)
			throw CoreException("Duplicate UUID in User constructor: " + uuid);
	}
}

LocalUser::LocalUser(int myfd, irc::sockets::sockaddrs* client, irc::sockets::sockaddrs* servaddr)
	: User(ServerInstance->UIDGen.GetUID(), ServerInstance->FakeClient->server, USERTYPE_LOCAL)
	, eh(this)
	, serializer(NULL)
	, bytes_in(0)
	, bytes_out(0)
	, cmds_in(0)
	, cmds_out(0)
	, quitting_sendq(false)
	, lastping(true)
	, exempt(false)
	, nextping(0)
	, idle_lastmsg(0)
	, CommandFloodPenalty(0)
	, already_sent(0)
{
	signon = ServerInstance->Time();
	// The user's default nick is their UUID
	nick = uuid;
	ident = uuid;
	eh.SetFd(myfd);
	memcpy(&client_sa, client, sizeof(irc::sockets::sockaddrs));
	memcpy(&server_sa, servaddr, sizeof(irc::sockets::sockaddrs));
	ChangeRealHost(GetIPString(), true);
}

LocalUser::LocalUser(int myfd, const std::string& uid, Serializable::Data& data)
	: User(uid, ServerInstance->FakeClient->server, USERTYPE_LOCAL)
	, eh(this)
	, already_sent(0)
{
	eh.SetFd(myfd);
	Deserialize(data);
}

User::~User()
{
}

const std::string& User::MakeHost()
{
	if (!this->cached_makehost.empty())
		return this->cached_makehost;

	this->cached_makehost = ident + "@" + GetRealHost();
	return this->cached_makehost;
}

const std::string& User::MakeHostIP()
{
	if (!this->cached_hostip.empty())
		return this->cached_hostip;

	this->cached_hostip = ident + "@" + this->GetIPString();
	return this->cached_hostip;
}

const std::string& User::GetFullHost()
{
	if (!this->cached_fullhost.empty())
		return this->cached_fullhost;

	this->cached_fullhost = nick + "!" + ident + "@" + GetDisplayedHost();
	return this->cached_fullhost;
}

const std::string& User::GetFullRealHost()
{
	if (!this->cached_fullrealhost.empty())
		return this->cached_fullrealhost;

	this->cached_fullrealhost = nick + "!" + ident + "@" + GetRealHost();
	return this->cached_fullrealhost;
}

bool User::HasModePermission(const ModeHandler* mh) const
{
	return true;
}

bool LocalUser::HasModePermission(const ModeHandler* mh) const
{
	if (!this->IsOper())
		return false;

	const unsigned char mode = mh->GetModeChar();
	if (!ModeParser::IsModeChar(mode))
		return false;

	return ((mh->GetModeType() == MODETYPE_USER ? oper->AllowedUserModes : oper->AllowedChanModes))[(mode - 'A')];

}
/*
 * users on remote servers can completely bypass all permissions based checks.
 * This prevents desyncs when one server has different type/class tags to another.
 * That having been said, this does open things up to the possibility of source changes
 * allowing remote kills, etc - but if they have access to the src, they most likely have
 * access to the conf - so it's an end to a means either way.
 */
bool User::HasCommandPermission(const std::string&)
{
	return true;
}

bool LocalUser::HasCommandPermission(const std::string& command)
{
	// are they even an oper at all?
	if (!this->IsOper())
	{
		return false;
	}

	return oper->AllowedOperCommands.Contains(command);
}

bool User::HasPrivPermission(const std::string& privstr)
{
	return true;
}

bool LocalUser::HasPrivPermission(const std::string& privstr)
{
	if (!this->IsOper())
		return false;

	return oper->AllowedPrivs.Contains(privstr);
}

bool User::HasSnomaskPermission(char chr) const
{
	return true;
}

bool LocalUser::HasSnomaskPermission(char chr) const
{
	if (!this->IsOper() || !ModeParser::IsModeChar(chr))
		return false;

	return this->oper->AllowedSnomasks[chr - 'A'];
}

void UserIOHandler::OnDataReady()
{
	if (user->quitting)
		return;

	if (recvq.length() > user->MyClass->GetRecvqMax() && !user->HasPrivPermission("users/flood/increased-buffers"))
	{
		ServerInstance->Users->QuitUser(user, "RecvQ exceeded");
		ServerInstance->SNO->WriteToSnoMask('a', "User %s RecvQ of %lu exceeds connect class maximum of %lu",
			user->nick.c_str(), (unsigned long)recvq.length(), user->MyClass->GetRecvqMax());
		return;
	}

	unsigned long sendqmax = ULONG_MAX;
	if (!user->HasPrivPermission("users/flood/increased-buffers"))
		sendqmax = user->MyClass->GetSendqSoftMax();

	unsigned long penaltymax = ULONG_MAX;
	if (!user->HasPrivPermission("users/flood/no-fakelag"))
		penaltymax = user->MyClass->GetPenaltyThreshold() * 1000;

	// The cleaned message sent by the user or empty if not found yet.
	std::string line;

	// The position of the most \n character or npos if not found yet.
	std::string::size_type eolpos;

	// The position within the recvq of the current character.
	std::string::size_type qpos;

	while (user->CommandFloodPenalty < penaltymax && getSendQSize() < sendqmax)
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
		ServerInstance->stats.Recv += qpos;
		user->bytes_in += qpos;
		user->cmds_in++;

		ServerInstance->Parser.ProcessBuffer(user, line);
		if (user->quitting)
			return;

		// clear() does not reclaim memory associated with the string, so our .reserve() call is safe
		line.clear();
	}

	if (user->CommandFloodPenalty >= penaltymax && !user->MyClass->fakelag)
		ServerInstance->Users->QuitUser(user, "Excess Flood");
}

void UserIOHandler::AddWriteBuf(const std::string &data)
{
	if (user->quitting_sendq)
		return;
	if (!user->quitting && getSendQSize() + data.length() > user->MyClass->GetSendqHardMax() &&
		!user->HasPrivPermission("users/flood/increased-buffers"))
	{
		user->quitting_sendq = true;
		ServerInstance->GlobalCulls.AddSQItem(user);
		return;
	}

	// We still want to append data to the sendq of a quitting user,
	// e.g. their ERROR message that says 'closing link'

	WriteData(data);
}

void UserIOHandler::SwapInternals(UserIOHandler& other)
{
	StreamSocket::SwapInternals(other);
	std::swap(checked_until, other.checked_until);
}

bool UserIOHandler::OnSetEndPoint(const irc::sockets::sockaddrs& server, const irc::sockets::sockaddrs& client)
{
	memcpy(&user->server_sa, &server, sizeof(irc::sockets::sockaddrs));
	user->SetClientIP(client);
	return !user->quitting;
}

void UserIOHandler::OnError(BufferedSocketError sockerr)
{
	ModResult res;
	FIRST_MOD_RESULT(OnConnectionFail, res, (user, sockerr));
	if (res != MOD_RES_ALLOW)
		ServerInstance->Users->QuitUser(user, getError());
}

CullResult User::cull()
{
	if (!quitting)
		ServerInstance->Users->QuitUser(this, "Culled without QuitUser");

	if (client_sa.family() != AF_UNSPEC)
		ServerInstance->Users->RemoveCloneCounts(this);

	if (server->IsULine())
		stdalgo::erase(ServerInstance->Users->all_ulines, this);

	return Extensible::cull();
}

CullResult LocalUser::cull()
{
	eh.cull();
	return User::cull();
}

CullResult FakeUser::cull()
{
	// Fake users don't quit, they just get culled.
	quitting = true;
	// Fake users are not inserted into UserManager::clientlist or uuidlist, so we don't need to modify those here
	return User::cull();
}

void User::Oper(OperInfo* info)
{
	ModeHandler* opermh = ServerInstance->Modes->FindMode('o', MODETYPE_USER);
	if (opermh)
	{
		if (this->IsModeSet(opermh))
			this->UnOper();
		this->SetMode(opermh, true);
	}
	this->oper = info;

	LocalUser* localuser = IS_LOCAL(this);
	if (localuser && opermh)
	{
		Modes::ChangeList changelist;
		changelist.push_add(opermh);
		ClientProtocol::Events::Mode modemsg(ServerInstance->FakeClient, NULL, localuser, changelist);
		localuser->Send(modemsg);
	}

	FOREACH_MOD(OnOper, (this, info->name));

	std::string opername;
	if (info->oper_block)
		opername = info->oper_block->getString("name");

	ServerInstance->SNO->WriteToSnoMask('o', "%s (%s@%s) is now a server operator of type %s (using oper '%s')",
		nick.c_str(), ident.c_str(), GetRealHost().c_str(), oper->name.c_str(), opername.c_str());
	this->WriteNumeric(RPL_YOUAREOPER, InspIRCd::Format("You are now %s %s", strchr("aeiouAEIOU", oper->name[0]) ? "an" : "a", oper->name.c_str()));

	ServerInstance->Users->all_opers.push_back(this);

	// Expand permissions from config for faster lookup
	if (localuser)
		oper->init();

	FOREACH_MOD(OnPostOper, (this, oper->name, opername));
}

namespace
{
	bool ParseModeList(std::bitset<64>& modeset, ConfigTag* tag, const std::string& field)
	{
		std::string modes;
		bool hasmodes = tag->readString(field, modes);
		for (std::string::const_iterator iter = modes.begin(); iter != modes.end(); ++iter)
		{
			const char& chr = *iter;
			if (chr == '*')
				modeset.set();
			else if (ModeParser::IsModeChar(chr))
				modeset.set(chr - 'A');
			else
				ServerInstance->Logs->Log("CONFIG", LOG_DEFAULT, "'%c' is not a valid value for <class:%s>, ignoring...", chr, field.c_str());
		}
		return hasmodes;
	}
}

void OperInfo::init()
{
	AllowedOperCommands.Clear();
	AllowedPrivs.Clear();
	AllowedUserModes.reset();
	AllowedChanModes.reset();
	AllowedSnomasks.reset();
	AllowedUserModes['o' - 'A'] = true; // Call me paranoid if you want.

	bool defaultsnomasks = true;
	for(std::vector<reference<ConfigTag> >::iterator iter = class_blocks.begin(); iter != class_blocks.end(); ++iter)
	{
		ConfigTag* tag = *iter;

		AllowedOperCommands.AddList(tag->getString("commands"));
		AllowedPrivs.AddList(tag->getString("privs"));

		ParseModeList(AllowedChanModes, tag, "chanmodes");
		ParseModeList(AllowedUserModes, tag, "usermodes");
		if (ParseModeList(AllowedSnomasks, tag, "snomasks"))
			defaultsnomasks = false;
	}

	// Compatibility for older configs that don't have the snomasks field.
	if (defaultsnomasks)
		AllowedSnomasks.set();
}

void User::UnOper()
{
	if (!this->IsOper())
		return;

	/*
	 * unset their oper type (what IS_OPER checks).
	 * note, order is important - this must come before modes as -o attempts
	 * to call UnOper. -- w00t
	 */
	oper = NULL;

	// Remove the user from the oper list
	stdalgo::vector::swaperase(ServerInstance->Users->all_opers, this);

	// If the user is quitting we shouldn't remove any modes as it results in
	// mode messages being broadcast across the network.
	if (quitting)
		return;

	/* Remove all oper only modes from the user when the deoper - Bug #466*/
	Modes::ChangeList changelist;
	const ModeParser::ModeHandlerMap& usermodes = ServerInstance->Modes->GetModes(MODETYPE_USER);
	for (ModeParser::ModeHandlerMap::const_iterator i = usermodes.begin(); i != usermodes.end(); ++i)
	{
		ModeHandler* mh = i->second;
		if (mh->NeedsOper())
			changelist.push_remove(mh);
	}

	ServerInstance->Modes->Process(this, NULL, this, changelist);

	ModeHandler* opermh = ServerInstance->Modes->FindMode('o', MODETYPE_USER);
	if (opermh)
		this->SetMode(opermh, false);
	FOREACH_MOD(OnPostDeoper, (this));
}

/*
 * Check class restrictions
 */
void LocalUser::CheckClass(bool clone_count)
{
	ConnectClass* a = this->MyClass;

	if (!a)
	{
		ServerInstance->Users->QuitUser(this, "Access denied by configuration");
		return;
	}
	else if (a->type == CC_DENY)
	{
		ServerInstance->Users->QuitUser(this, a->config->getString("reason", "Unauthorised connection", 1));
		return;
	}
	else if (clone_count)
	{
		const UserManager::CloneCounts& clonecounts = ServerInstance->Users->GetCloneCounts(this);
		if ((a->GetMaxLocal()) && (clonecounts.local > a->GetMaxLocal()))
		{
			ServerInstance->Users->QuitUser(this, "No more connections allowed from your host via this connect class (local)");
			if (a->maxconnwarn)
			{
				ServerInstance->SNO->WriteToSnoMask('a', "WARNING: maximum local connections for the %s class (%ld) exceeded by %s",
					a->name.c_str(), a->GetMaxLocal(), this->GetIPString().c_str());
			}
			return;
		}
		else if ((a->GetMaxGlobal()) && (clonecounts.global > a->GetMaxGlobal()))
		{
			ServerInstance->Users->QuitUser(this, "No more connections allowed from your host via this connect class (global)");
			if (a->maxconnwarn)
			{
				ServerInstance->SNO->WriteToSnoMask('a', "WARNING: maximum global connections for the %s class (%ld) exceeded by %s",
				a->name.c_str(), a->GetMaxGlobal(), this->GetIPString().c_str());
			}
			return;
		}
	}

	this->nextping = ServerInstance->Time() + a->GetPingTime();
	this->uniqueusername = a->uniqueusername;
}

bool LocalUser::CheckLines(bool doZline)
{
	const char* check[] = { "G" , "K", (doZline) ? "Z" : NULL, NULL };

	if (!this->exempt)
	{
		for (int n = 0; check[n]; ++n)
		{
			XLine *r = ServerInstance->XLines->MatchesLine(check[n], this);

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
	ServerInstance->stats.Connects++;
	this->idle_lastmsg = ServerInstance->Time();

	/*
	 * You may be thinking "wtf, we checked this in User::AddClient!" - and yes, we did, BUT.
	 * At the time AddClient is called, we don't have a resolved host, by here we probably do - which
	 * may put the user into a totally separate class with different restrictions! so we *must* check again.
	 * Don't remove this! -- w00t
	 */
	MyClass = NULL;
	SetClass();
	CheckClass();
	CheckLines();

	if (quitting)
		return;

	/*
	 * We don't set REG_ALL until triggering OnUserConnect, so some module events don't spew out stuff
	 * for a user that doesn't exist yet.
	 */
	FOREACH_MOD(OnUserConnect, (this));

	/* Now registered */
	if (ServerInstance->Users->unregistered_count)
		ServerInstance->Users->unregistered_count--;
	this->registered = REG_ALL;

	FOREACH_MOD(OnPostConnect, (this));

	ServerInstance->SNO->WriteToSnoMask('c',"Client connecting on port %d (class %s): %s (%s) [%s\x0F]",
		this->server_sa.port(), this->MyClass->name.c_str(), GetFullRealHost().c_str(), this->GetIPString().c_str(), this->GetRealName().c_str());
	ServerInstance->Logs->Log("BANCACHE", LOG_DEBUG, "BanCache: Adding NEGATIVE hit for " + this->GetIPString());
	ServerInstance->BanCache.AddHit(this->GetIPString(), "", "");
	// reset the flood penalty (which could have been raised due to things like auto +x)
	CommandFloodPenalty = 0;
}

void User::InvalidateCache()
{
	/* Invalidate cache */
	cachedip.clear();
	cached_fullhost.clear();
	cached_hostip.clear();
	cached_makehost.clear();
	cached_fullrealhost.clear();
}

bool User::ChangeNick(const std::string& newnick, time_t newts)
{
	if (quitting)
	{
		ServerInstance->Logs->Log("USERS", LOG_DEFAULT, "ERROR: Attempted to change nick of a quitting user: " + this->nick);
		return false;
	}

	User* const InUse = ServerInstance->FindNickOnly(newnick);
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
			if (InUse->registered != REG_ALL)
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

		age = newts ? newts : ServerInstance->Time();
	}

	if (this->registered == REG_ALL)
	{
		ClientProtocol::Messages::Nick nickmsg(this, newnick);
		ClientProtocol::Event nickevent(ServerInstance->GetRFCEvents().nick, nickmsg);
		this->WriteCommonRaw(nickevent, true);
	}
	const std::string oldnick = nick;
	nick = newnick;

	InvalidateCache();
	ServerInstance->Users->clientlist.erase(oldnick);
	ServerInstance->Users->clientlist[newnick] = this;

	if (registered == REG_ALL)
		FOREACH_MOD(OnUserPostNick, (this,oldnick));

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
	this->registered &= ~REG_NICK;
	this->ChangeNick(this->uuid);
}

const std::string& User::GetIPString()
{
	if (cachedip.empty())
	{
		cachedip = client_sa.addr();
		/* IP addresses starting with a : on irc are a Bad Thing (tm) */
		if (cachedip[0] == ':')
			cachedip.insert(cachedip.begin(),1,'0');
	}

	return cachedip;
}

const std::string& User::GetBanIdent() const
{
	static const std::string wildcard = "*";
	return uniqueusername ? ident : wildcard;
}

const std::string& User::GetHost(bool uncloak) const
{
	return uncloak ? GetRealHost() : GetDisplayedHost();
}

const std::string& User::GetDisplayedHost() const
{
	return displayhost.empty() ? realhost : displayhost;
}

const std::string& User::GetRealHost() const
{
	return realhost;
}

const std::string& User::GetRealName() const
{
	return realname;
}

irc::sockets::cidr_mask User::GetCIDRMask()
{
	unsigned char range = 0;
	switch (client_sa.family())
	{
		case AF_INET6:
			range = ServerInstance->Config->c_ipv6_range;
			break;
		case AF_INET:
			range = ServerInstance->Config->c_ipv4_range;
			break;
	}
	return irc::sockets::cidr_mask(client_sa, range);
}

bool User::SetClientIP(const std::string& address)
{
	irc::sockets::sockaddrs sa;
	if (!irc::sockets::aptosa(address, client_sa.family() == AF_UNSPEC ? 0 : client_sa.port(), sa))
		return false;

	User::SetClientIP(sa);
	return true;
}

void User::SetClientIP(const irc::sockets::sockaddrs& sa)
{
	const std::string oldip(client_sa.family() == AF_UNSPEC ? "" : GetIPString());
	memcpy(&client_sa, &sa, sizeof(irc::sockets::sockaddrs));
	this->InvalidateCache();

	// If the users hostname was their IP then update it.
	if (GetRealHost() == oldip)
		ChangeRealHost(GetIPString(), false);
	if (GetDisplayedHost() == oldip)
		ChangeDisplayedHost(GetIPString());
}

bool LocalUser::SetClientIP(const std::string& address)
{
	irc::sockets::sockaddrs sa;
	if (!irc::sockets::aptosa(address, client_sa.port(), sa))
		return false;

	LocalUser::SetClientIP(sa);
	return true;
}

void LocalUser::SetClientIP(const irc::sockets::sockaddrs& sa)
{
	if (sa == client_sa)
		return;

	ServerInstance->Users->RemoveCloneCounts(this);
	User::SetClientIP(sa);
	ServerInstance->Users->AddClone(this);

	// Recheck the connect class.
	this->MyClass = NULL;
	this->SetClass();
	this->CheckClass();

	if (!quitting)
		FOREACH_MOD(OnSetUserIP, (this));
}

void LocalUser::Write(const ClientProtocol::SerializedMessage& text)
{
	if (!SocketEngine::BoundsCheckFd(&eh))
		return;

	if (ServerInstance->Config->RawLog)
	{
		if (text.empty())
			return;

		std::string::size_type nlpos = text.find_first_of("\r\n", 0, 2);
		if (nlpos == std::string::npos)
			nlpos = text.length(); // TODO is this ok, test it

		ServerInstance->Logs->Log("USEROUTPUT", LOG_RAWIO, "C[%s] O %.*s", uuid.c_str(), (int) nlpos, text.c_str());
	}

	eh.AddWriteBuf(text);

	const size_t bytessent = text.length() + 2;
	ServerInstance->stats.Sent += bytessent;
	this->bytes_out += bytessent;
	this->cmds_out++;
}

void LocalUser::Send(ClientProtocol::Event& protoev)
{
	if (!serializer)
	{
		ServerInstance->Logs->Log("USERS", LOG_DEBUG, "BUG: LocalUser::Send() called on %s who does not have a serializer!",
			GetFullRealHost().c_str());
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
	for (ClientProtocol::MessageList::const_iterator i = msglist.begin(); i != msglist.end(); ++i)
	{
		ClientProtocol::Message& curr = **i;
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
	ServerInstance->PI->SendMessage(this, text, MSG_NOTICE);
}

void LocalUser::WriteRemoteNotice(const std::string& text)
{
	WriteNotice(text);
}

namespace
{
	class WriteCommonRawHandler : public User::ForEachNeighborHandler
	{
		ClientProtocol::Event& ev;

		void Execute(LocalUser* user) CXX11_OVERRIDE
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

already_sent_t User::ForEachNeighbor(ForEachNeighborHandler& handler, bool include_self)
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
	IncludeChanList include_chans(chans.begin(), chans.end());
	std::map<User*, bool> exceptions;
	exceptions[this] = include_self;
	FOREACH_MOD(OnBuildNeighborList, (this, include_chans, exceptions));

	// Get next id, guaranteed to differ from the already_sent field of all users
	const already_sent_t newid = ServerInstance->Users.NextAlreadySentId();

	// Handle exceptions first
	for (std::map<User*, bool>::const_iterator i = exceptions.begin(); i != exceptions.end(); ++i)
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
	for (IncludeChanList::const_iterator i = include_chans.begin(); i != include_chans.end(); ++i)
	{
		Channel* chan = (*i)->chan;
		const Channel::MemberMap& userlist = chan->GetUsers();
		for (Channel::MemberMap::const_iterator j = userlist.begin(); j != userlist.end(); ++j)
		{
			LocalUser* curr = IS_LOCAL(j->first);
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
bool User::SharesChannelWith(User *other)
{
	/* Outer loop */
	for (User::ChanList::iterator i = this->chans.begin(); i != this->chans.end(); ++i)
	{
		/* Eliminate the inner loop (which used to be ~equal in size to the outer loop)
		 * by replacing it with a map::find which *should* be more efficient
		 */
		if ((*i)->chan->HasUser(other))
			return true;
	}
	return false;
}

bool User::ChangeRealName(const std::string& real)
{
	if (!this->realname.compare(real))
		return true;

	if (IS_LOCAL(this))
	{
		ModResult MOD_RESULT;
		FIRST_MOD_RESULT(OnPreChangeRealName, MOD_RESULT, (IS_LOCAL(this), real));
		if (MOD_RESULT == MOD_RES_DENY)
			return false;
	}
	FOREACH_MOD(OnChangeRealName, (this, real));
	this->realname.assign(real, 0, ServerInstance->Config->Limits.MaxReal);

	return true;
}

bool User::ChangeDisplayedHost(const std::string& shost)
{
	if (GetDisplayedHost() == shost)
		return true;

	LocalUser* luser = IS_LOCAL(this);
	if (luser)
	{
		ModResult MOD_RESULT;
		FIRST_MOD_RESULT(OnPreChangeHost, MOD_RESULT, (luser, shost));
		if (MOD_RESULT == MOD_RES_DENY)
			return false;
	}

	FOREACH_MOD(OnChangeHost, (this,shost));

	if (realhost == shost)
		this->displayhost.clear();
	else
		this->displayhost.assign(shost, 0, ServerInstance->Config->Limits.MaxHost);

	this->InvalidateCache();

	if (IS_LOCAL(this) && this->registered != REG_NONE)
		this->WriteNumeric(RPL_YOURDISPLAYEDHOST, this->GetDisplayedHost(), "is now your displayed host");

	return true;
}

void User::ChangeRealHost(const std::string& host, bool resetdisplay)
{
	// If the real host is the new host and we are not resetting the
	// display host then we have nothing to do.
	const bool changehost = (realhost != host);
	if (!changehost && !resetdisplay)
		return;

	// If the displayhost is not set and we are not resetting it then
	// we need to copy it to the displayhost field.
	if (displayhost.empty() && !resetdisplay)
		displayhost = realhost;

	// If the displayhost is the new host or we are resetting it then
	// we clear its contents to save memory.
	else if (displayhost == host || resetdisplay)
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
		FOREACH_MOD(OnChangeRealHost, (this, host));

	realhost = host;
	this->InvalidateCache();

	// Don't call the OnPostChangeRealHost event when initialising a user.
	if (!this->quitting && !initializing)
		FOREACH_MOD(OnPostChangeRealHost, (this));
}

bool User::ChangeIdent(const std::string& newident)
{
	if (this->ident == newident)
		return true;

	FOREACH_MOD(OnChangeIdent, (this,newident));

	this->ident.assign(newident, 0, ServerInstance->Config->Limits.IdentMax);
	this->InvalidateCache();

	return true;
}

/*
 * Sets a user's connection class.
 * If the class name is provided, it will be used. Otherwise, the class will be guessed using host/ip/ident/etc.
 * NOTE: If the <ALLOW> or <DENY> tag specifies an ip, and this user resolves,
 * then their ip will be taken as 'priority' anyway, so for example,
 * <connect allow="127.0.0.1"> will match joe!bloggs@localhost
 */
void LocalUser::SetClass(const std::string &explicit_name)
{
	ServerInstance->Logs->Log("CONNECTCLASS", LOG_DEBUG, "Setting connect class for %s (%s) ...",
		this->uuid.c_str(), this->GetFullRealHost().c_str());

	ConnectClass *found = NULL;
	if (!explicit_name.empty())
	{
		for (ServerConfig::ClassVector::const_iterator i = ServerInstance->Config->Classes.begin(); i != ServerInstance->Config->Classes.end(); ++i)
		{
			ConnectClass* c = *i;

			if (explicit_name == c->name)
			{
				ServerInstance->Logs->Log("CONNECTCLASS", LOG_DEBUG, "Connect class explicitly set to %s",
					explicit_name.c_str());
				found = c;
			}
		}
	}
	else
	{
		for (ServerConfig::ClassVector::const_iterator i = ServerInstance->Config->Classes.begin(); i != ServerInstance->Config->Classes.end(); ++i)
		{
			ConnectClass* c = *i;
			ServerInstance->Logs->Log("CONNECTCLASS", LOG_DEBUG, "Checking the %s connect class ...",
					c->GetName().c_str());

			ModResult MOD_RESULT;
			FIRST_MOD_RESULT(OnSetConnectClass, MOD_RESULT, (this,c));
			if (MOD_RESULT == MOD_RES_DENY)
				continue;

			if (MOD_RESULT == MOD_RES_ALLOW)
			{
				ServerInstance->Logs->Log("CONNECTCLASS", LOG_DEBUG, "The %s connect class was explicitly chosen by a module",
					c->GetName().c_str());
				found = c;
				break;
			}

			if (c->type == CC_NAMED)
			{
				ServerInstance->Logs->Log("CONNECTCLASS", LOG_DEBUG, "The %s connect class is not suitable as neither <connect:allow> nor <connect:deny> are set",
						c->GetName().c_str());
				continue;
			}

			bool regdone = (registered != REG_NONE);
			if (c->config->getBool("registered", regdone) != regdone)
			{
				ServerInstance->Logs->Log("CONNECTCLASS", LOG_DEBUG, "The %s connect class is not suitable as it requires that the user is %s",
						c->GetName().c_str(), regdone ? "not fully connected" : "fully connected");
				continue;
			}

			bool hostmatches = false;
			for (std::vector<std::string>::const_iterator host = c->GetHosts().begin(); host != c->GetHosts().end(); ++host)
			{
				if (InspIRCd::MatchCIDR(this->GetIPString(), *host) || InspIRCd::MatchCIDR(this->GetRealHost(), *host))
				{
					hostmatches = true;
					break;
				}
			}
			if (!hostmatches)
			{
				ServerInstance->Logs->Log("CONNECTCLASS", LOG_DEBUG, "The %s connect class is not suitable as neither the host (%s) nor the IP (%s) matches %s",
					c->GetName().c_str(), this->GetRealHost().c_str(), this->GetIPString().c_str(), c->GetHost().c_str());
				continue;
			}

			/*
			 * deny change if change will take class over the limit check it HERE, not after we found a matching class,
			 * because we should attempt to find another class if this one doesn't match us. -- w00t
			 */
			if (c->limit && (c->GetReferenceCount() >= c->limit))
			{
				ServerInstance->Logs->Log("CONNECTCLASS", LOG_DEBUG, "The %s connect class is not suitable as it has reached its user limit (%lu)",
						c->GetName().c_str(), c->limit);
				continue;
			}

			/* if it requires a port and our port doesn't match, fail */
			if (!c->ports.empty() && !c->ports.count(this->server_sa.port()))
			{
				ServerInstance->Logs->Log("CONNECTCLASS", LOG_DEBUG, "The %s connect class is not suitable as the connection port (%d) is not any of %s",
					c->GetName().c_str(), this->server_sa.port(), stdalgo::string::join(c->ports).c_str());
				continue;
			}

			if (regdone && !c->password.empty() && !ServerInstance->PassCompare(this, c->password, password, c->passwordhash))
			{
				ServerInstance->Logs->Log("CONNECTCLASS", LOG_DEBUG, "The %s connect class is not suitable as requires a password and %s",
					c->GetName().c_str(), password.empty() ? "one was not provided" : "the provided password was incorrect");
				continue;
			}

			/* we stop at the first class that meets ALL criteria. */
			ServerInstance->Logs->Log("CONNECTCLASS", LOG_DEBUG, "The %s connect class is suitable for %s (%s)",
				c->GetName().c_str(), this->uuid.c_str(), this->GetFullRealHost().c_str());
			found = c;
			break;
		}
	}

	/*
	 * Okay, assuming we found a class that matches.. switch us into that class, keeping refcounts up to date.
	 */
	if (found)
	{
		MyClass = found;
	}
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

	ClientProtocol::Messages::Privmsg msg(ClientProtocol::Messages::Privmsg::nocopy, ServerInstance->FakeClient, localuser, text, MSG_NOTICE);
	localuser->Send(ServerInstance->GetRFCEvents().privmsg, msg);
}

const std::string& FakeUser::GetFullHost()
{
	return server->GetPublicName();
}

const std::string& FakeUser::GetFullRealHost()
{
	return server->GetPublicName();
}

ConnectClass::ConnectClass(ConfigTag* tag, char t, const std::string& mask)
	: config(tag)
	, host(mask)
	, name("unnamed")
	, type(t)
	, fakelag(true)
	, maxconnwarn(true)
	, resolvehostnames(true)
	, uniqueusername(false)
	, maxchans(0)
	, penaltythreshold(0)
	, pingtime(0)
	, registration_timeout(0)
	, commandrate(0)
	, hardsendqmax(0)
	, limit(0)
	, maxglobal(0)
	, maxlocal(0)
	, recvqmax(0)
	, softsendqmax(0)
{
	irc::spacesepstream hoststream(host);
	for (std::string hostentry; hoststream.GetToken(hostentry); )
		hosts.push_back(hostentry);
}

ConnectClass::ConnectClass(ConfigTag* tag, char t, const std::string& mask, const ConnectClass& parent)
{
	Update(&parent);
	name = "unnamed";
	type = t;
	host = mask;
	hosts.clear();
	irc::spacesepstream hoststream(host);
	for (std::string hostentry; hoststream.GetToken(hostentry); )
		hosts.push_back(hostentry);

	// Connect classes can inherit from each other but this is problematic for modules which can't use
	// ConnectClass::Update so we build a hybrid tag containing all of the values set on this class as
	// well as the parent class.
	ConfigItems* items = NULL;
	config = ConfigTag::create(tag->tag, tag->src_name, tag->src_line, items);

	const ConfigItems& parentkeys = parent.config->getItems();
	for (ConfigItems::const_iterator piter = parentkeys.begin(); piter != parentkeys.end(); ++piter)
	{
		// The class name and parent name are not inherited
		if (stdalgo::string::equalsci(piter->first, "name") || stdalgo::string::equalsci(piter->first, "parent"))
			continue;

		// Store the item in the config tag. If this item also
		// exists in the child it will be overwritten.
		(*items)[piter->first] = piter->second;
	}

	const ConfigItems& childkeys = tag->getItems();
	for (ConfigItems::const_iterator citer = childkeys.begin(); citer != childkeys.end(); ++citer)
	{
		// This will overwrite the parent value if present.
		(*items)[citer->first] = citer->second;
	}
}

void ConnectClass::Update(const ConnectClass* src)
{
	config = src->config;
	host = src->host;
	hosts = src->hosts;
	name = src->name;
	password = src->password;
	passwordhash = src->passwordhash;
	ports = src->ports;
	type = src->type;
	fakelag = src->fakelag;
	maxconnwarn = src->maxconnwarn;
	resolvehostnames = src->resolvehostnames;
	uniqueusername = src->uniqueusername;
	maxchans = src->maxchans;
	penaltythreshold = src->penaltythreshold;
	pingtime = src->pingtime;
	registration_timeout = src->registration_timeout;
	commandrate = src->commandrate;
	hardsendqmax = src->hardsendqmax;
	limit = src->limit;
	maxglobal = src->maxglobal;
	maxlocal = src->maxlocal;
	recvqmax = src->recvqmax;
	softsendqmax = src->softsendqmax;
}
