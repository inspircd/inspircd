/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2006-2009 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2006-2007, 2009 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2008 John Brooks <john.brooks@dereferenced.net>
 *   Copyright (C) 2008 Thomas Stagner <aquanight@inspircd.org>
 *   Copyright (C) 2008 Oliver Lupton <oliverlupton@gmail.com>
 *   Copyright (C) 2003-2008 Craig Edwards <craigedwards@brainbox.cc>
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
#include <stdarg.h>
#include "socketengine.h"
#include "xline.h"
#include "bancache.h"

already_sent_t LocalUser::already_sent_id = 0;

bool User::IsNoticeMaskSet(unsigned char sm)
{
	if (!isalpha(sm))
		return false;
	return (snomasks[sm-65]);
}

bool User::IsModeSet(unsigned char m)
{
	if (!isalpha(m))
		return false;
	return (modes[m-65]);
}

const char* User::FormatModes(bool showparameters)
{
	static std::string data;
	std::string params;
	data.clear();

	for (unsigned char n = 0; n < 64; n++)
	{
		if (modes[n])
		{
			data.push_back(n + 65);
			ModeHandler* mh = ServerInstance->Modes->FindMode(n + 65, MODETYPE_USER);
			if (showparameters && mh && mh->GetNumParams(true))
			{
				std::string p = mh->GetUserParameter(this);
				if (p.length())
					params.append(" ").append(p);
			}
		}
	}
	data += params;
	return data.c_str();
}

User::User(const std::string &uid, const std::string& sid, int type)
	: uuid(uid), server(sid), usertype(type)
{
	age = ServerInstance->Time();
	signon = 0;
	registered = 0;
	quietquit = quitting = false;
	client_sa.sa.sa_family = AF_UNSPEC;

	ServerInstance->Logs->Log("USERS", LOG_DEBUG, "New UUID for user: %s", uuid.c_str());

	if (!ServerInstance->Users->uuidlist->insert(std::make_pair(uuid, this)).second)
		throw CoreException("Duplicate UUID "+std::string(uuid)+" in User constructor");
}

LocalUser::LocalUser(int myfd, irc::sockets::sockaddrs* client, irc::sockets::sockaddrs* servaddr)
	: User(ServerInstance->UIDGen.GetUID(), ServerInstance->Config->ServerName, USERTYPE_LOCAL), eh(this),
	localuseriter(ServerInstance->Users->local_users.end()),
	bytes_in(0), bytes_out(0), cmds_in(0), cmds_out(0), nping(0), CommandFloodPenalty(0),
	already_sent(0)
{
	exempt = quitting_sendq = false;
	idle_lastmsg = 0;
	ident = "unknown";
	lastping = 0;
	eh.SetFd(myfd);
	memcpy(&client_sa, client, sizeof(irc::sockets::sockaddrs));
	memcpy(&server_sa, servaddr, sizeof(irc::sockets::sockaddrs));
	dhost = host = GetIPString();
}

User::~User()
{
	if (ServerInstance->Users->uuidlist->find(uuid) != ServerInstance->Users->uuidlist->end())
		ServerInstance->Logs->Log("USERS", LOG_DEFAULT, "User destructor for %s called without cull", uuid.c_str());
}

const std::string& User::MakeHost()
{
	if (!this->cached_makehost.empty())
		return this->cached_makehost;

	// XXX: Is there really a need to cache this?
	this->cached_makehost = ident + "@" + host;
	return this->cached_makehost;
}

const std::string& User::MakeHostIP()
{
	if (!this->cached_hostip.empty())
		return this->cached_hostip;

	// XXX: Is there really a need to cache this?
	this->cached_hostip = ident + "@" + this->GetIPString();
	return this->cached_hostip;
}

const std::string& User::GetFullHost()
{
	if (!this->cached_fullhost.empty())
		return this->cached_fullhost;

	// XXX: Is there really a need to cache this?
	this->cached_fullhost = nick + "!" + ident + "@" + dhost;
	return this->cached_fullhost;
}

const std::string& User::GetFullRealHost()
{
	if (!this->cached_fullrealhost.empty())
		return this->cached_fullrealhost;

	// XXX: Is there really a need to cache this?
	this->cached_fullrealhost = nick + "!" + ident + "@" + host;
	return this->cached_fullrealhost;
}

InviteList& LocalUser::GetInviteList()
{
	RemoveExpiredInvites();
	return invites;
}

bool LocalUser::RemoveInvite(Channel* chan)
{
	Invitation* inv = Invitation::Find(chan, this);
	if (inv)
	{
		inv->cull();
		delete inv;
		return true;
	}
	return false;
}

void LocalUser::RemoveExpiredInvites()
{
	Invitation::Find(NULL, this);
}

bool User::HasModePermission(unsigned char, ModeType)
{
	return true;
}

bool LocalUser::HasModePermission(unsigned char mode, ModeType type)
{
	if (!this->IsOper())
		return false;

	if (mode < 'A' || mode > ('A' + 64)) return false;

	return ((type == MODETYPE_USER ? oper->AllowedUserModes : oper->AllowedChanModes))[(mode - 'A')];

}
/*
 * users on remote servers can completely bypass all permissions based checks.
 * This prevents desyncs when one server has different type/class tags to another.
 * That having been said, this does open things up to the possibility of source changes
 * allowing remote kills, etc - but if they have access to the src, they most likely have
 * access to the conf - so it's an end to a means either way.
 */
bool User::HasPermission(const std::string&)
{
	return true;
}

bool LocalUser::HasPermission(const std::string &command)
{
	// are they even an oper at all?
	if (!this->IsOper())
	{
		return false;
	}

	if (oper->AllowedOperCommands.find(command) != oper->AllowedOperCommands.end())
		return true;
	else if (oper->AllowedOperCommands.find("*") != oper->AllowedOperCommands.end())
		return true;

	return false;
}

bool User::HasPrivPermission(const std::string &privstr, bool noisy)
{
	return true;
}

bool LocalUser::HasPrivPermission(const std::string &privstr, bool noisy)
{
	if (!this->IsOper())
	{
		if (noisy)
			this->WriteNotice("You are not an oper");
		return false;
	}

	if (oper->AllowedPrivs.find(privstr) != oper->AllowedPrivs.end())
	{
		return true;
	}
	else if (oper->AllowedPrivs.find("*") != oper->AllowedPrivs.end())
	{
		return true;
	}

	if (noisy)
		this->WriteNotice("Oper type " + oper->name + " does not have access to priv " + privstr);

	return false;
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

	while (user->CommandFloodPenalty < penaltymax && getSendQSize() < sendqmax)
	{
		std::string line;
		line.reserve(ServerInstance->Config->Limits.MaxLine);
		std::string::size_type qpos = 0;
		while (qpos < recvq.length())
		{
			char c = recvq[qpos++];
			switch (c)
			{
			case '\0':
				c = ' ';
				break;
			case '\r':
				continue;
			case '\n':
				goto eol_found;
			}
			if (line.length() < ServerInstance->Config->Limits.MaxLine - 2)
				line.push_back(c);
		}
		// if we got here, the recvq ran out before we found a newline
		return;
eol_found:
		// just found a newline. Terminate the string, and pull it out of recvq
		recvq = recvq.substr(qpos);

		// TODO should this be moved to when it was inserted in recvq?
		ServerInstance->stats->statsRecv += qpos;
		user->bytes_in += qpos;
		user->cmds_in++;

		ServerInstance->Parser->ProcessBuffer(line, user);
		if (user->quitting)
			return;
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

void UserIOHandler::OnError(BufferedSocketError)
{
	ServerInstance->Users->QuitUser(user, getError());
}

CullResult User::cull()
{
	if (!quitting)
		ServerInstance->Users->QuitUser(this, "Culled without QuitUser");
	PurgeEmptyChannels();

	if (client_sa.sa.sa_family != AF_UNSPEC)
		ServerInstance->Users->RemoveCloneCounts(this);

	return Extensible::cull();
}

CullResult LocalUser::cull()
{
	// The iterator is initialized to local_users.end() in the constructor. It is
	// overwritten in UserManager::AddUser() with the real iterator so this check
	// is only a precaution currently.
	if (localuseriter != ServerInstance->Users->local_users.end())
	{
		ServerInstance->Users->local_count--;
		ServerInstance->Users->local_users.erase(localuseriter);
	}
	else
		ServerInstance->Logs->Log("USERS", LOG_DEFAULT, "ERROR: LocalUserIter does not point to a valid entry for " + this->nick);

	ClearInvites();
	eh.cull();
	return User::cull();
}

CullResult FakeUser::cull()
{
	// Fake users don't quit, they just get culled.
	quitting = true;
	// Fake users are not inserted into UserManager::clientlist, they're only in the uuidlist
	ServerInstance->Users->uuidlist->erase(uuid);
	return User::cull();
}

void User::Oper(OperInfo* info)
{
	ModeHandler* opermh = ServerInstance->Modes->FindMode('o', MODETYPE_USER);
	if (this->IsModeSet(opermh))
		this->UnOper();

	this->SetMode(opermh, true);
	this->oper = info;
	this->WriteServ("MODE %s :+o", this->nick.c_str());
	FOREACH_MOD(OnOper, (this, info->name));

	std::string opername;
	if (info->oper_block)
		opername = info->oper_block->getString("name");

	if (IS_LOCAL(this))
	{
		LocalUser* l = IS_LOCAL(this);
		std::string vhost = oper->getConfig("vhost");
		if (!vhost.empty())
			l->ChangeDisplayedHost(vhost.c_str());
		std::string opClass = oper->getConfig("class");
		if (!opClass.empty())
			l->SetClass(opClass);
	}

	ServerInstance->SNO->WriteToSnoMask('o',"%s (%s@%s) is now an IRC operator of type %s (using oper '%s')",
		nick.c_str(), ident.c_str(), host.c_str(), oper->name.c_str(), opername.c_str());
	this->WriteNumeric(381, "%s :You are now %s %s", nick.c_str(), strchr("aeiouAEIOU", oper->name[0]) ? "an" : "a", oper->name.c_str());

	ServerInstance->Logs->Log("OPER", LOG_DEFAULT, "%s opered as type: %s", GetFullRealHost().c_str(), oper->name.c_str());
	ServerInstance->Users->all_opers.push_back(this);

	// Expand permissions from config for faster lookup
	if (IS_LOCAL(this))
		oper->init();

	FOREACH_MOD(OnPostOper, (this, oper->name, opername));
}

void OperInfo::init()
{
	AllowedOperCommands.clear();
	AllowedPrivs.clear();
	AllowedUserModes.reset();
	AllowedChanModes.reset();
	AllowedUserModes['o' - 'A'] = true; // Call me paranoid if you want.

	for(std::vector<reference<ConfigTag> >::iterator iter = class_blocks.begin(); iter != class_blocks.end(); ++iter)
	{
		ConfigTag* tag = *iter;
		std::string mycmd, mypriv;
		/* Process commands */
		irc::spacesepstream CommandList(tag->getString("commands"));
		while (CommandList.GetToken(mycmd))
		{
			AllowedOperCommands.insert(mycmd);
		}

		irc::spacesepstream PrivList(tag->getString("privs"));
		while (PrivList.GetToken(mypriv))
		{
			AllowedPrivs.insert(mypriv);
		}

		std::string modes = tag->getString("usermodes");
		for (std::string::const_iterator c = modes.begin(); c != modes.end(); ++c)
		{
			if (*c == '*')
			{
				this->AllowedUserModes.set();
			}
			else if (*c >= 'A' && *c < 'z')
			{
				this->AllowedUserModes[*c - 'A'] = true;
			}
		}

		modes = tag->getString("chanmodes");
		for (std::string::const_iterator c = modes.begin(); c != modes.end(); ++c)
		{
			if (*c == '*')
			{
				this->AllowedChanModes.set();
			}
			else if (*c >= 'A' && *c < 'z')
			{
				this->AllowedChanModes[*c - 'A'] = true;
			}
		}
	}
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


	/* Remove all oper only modes from the user when the deoper - Bug #466*/
	std::string moderemove("-");

	for (unsigned char letter = 'A'; letter <= 'z'; letter++)
	{
		ModeHandler* mh = ServerInstance->Modes->FindMode(letter, MODETYPE_USER);
		if (mh && mh->NeedsOper())
			moderemove += letter;
	}


	std::vector<std::string> parameters;
	parameters.push_back(this->nick);
	parameters.push_back(moderemove);

	ServerInstance->Modes->Process(parameters, this);

	/* remove the user from the oper list. Will remove multiple entries as a safeguard against bug #404 */
	ServerInstance->Users->all_opers.remove(this);

	ModeHandler* opermh = ServerInstance->Modes->FindMode('o', MODETYPE_USER);
	this->SetMode(opermh, false);
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
		ServerInstance->Users->QuitUser(this, a->config->getString("reason", "Unauthorised connection"));
		return;
	}
	else if (clone_count)
	{
		if ((a->GetMaxLocal()) && (ServerInstance->Users->LocalCloneCount(this) > a->GetMaxLocal()))
		{
			ServerInstance->Users->QuitUser(this, "No more connections allowed from your host via this connect class (local)");
			if (a->maxconnwarn)
				ServerInstance->SNO->WriteToSnoMask('a', "WARNING: maximum LOCAL connections (%ld) exceeded for IP %s", a->GetMaxLocal(), this->GetIPString().c_str());
			return;
		}
		else if ((a->GetMaxGlobal()) && (ServerInstance->Users->GlobalCloneCount(this) > a->GetMaxGlobal()))
		{
			ServerInstance->Users->QuitUser(this, "No more connections allowed from your host via this connect class (global)");
			if (a->maxconnwarn)
				ServerInstance->SNO->WriteToSnoMask('a', "WARNING: maximum GLOBAL connections (%ld) exceeded for IP %s", a->GetMaxGlobal(), this->GetIPString().c_str());
			return;
		}
	}

	this->nping = ServerInstance->Time() + a->GetPingTime() + ServerInstance->Config->dns_timeout;
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
	ServerInstance->stats->statsConnects++;
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

	this->WriteNumeric(RPL_WELCOME, "%s :Welcome to the %s IRC Network %s",this->nick.c_str(), ServerInstance->Config->Network.c_str(), GetFullRealHost().c_str());
	this->WriteNumeric(RPL_YOURHOSTIS, "%s :Your host is %s, running version %s",this->nick.c_str(),ServerInstance->Config->ServerName.c_str(),BRANCH);
	this->WriteNumeric(RPL_SERVERCREATED, "%s :This server was created %s %s", this->nick.c_str(), __TIME__, __DATE__);

	const std::string& modelist = ServerInstance->Modes->GetModeListFor004Numeric();
	this->WriteNumeric(RPL_SERVERVERSION, "%s %s %s %s", this->nick.c_str(), ServerInstance->Config->ServerName.c_str(), BRANCH, modelist.c_str());

	ServerInstance->ISupport.SendTo(this);
	this->WriteNumeric(RPL_YOURUUID, "%s %s :your unique ID", this->nick.c_str(), this->uuid.c_str());

	/* Now registered */
	if (ServerInstance->Users->unregistered_count)
		ServerInstance->Users->unregistered_count--;

	/* Trigger MOTD and LUSERS output, give modules a chance too */
	ModResult MOD_RESULT;
	std::string command("LUSERS");
	std::vector<std::string> parameters;
	FIRST_MOD_RESULT(OnPreCommand, MOD_RESULT, (command, parameters, this, true, command));
	if (!MOD_RESULT)
		ServerInstance->Parser->CallHandler(command, parameters, this);

	MOD_RESULT = MOD_RES_PASSTHRU;
	command = "MOTD";
	FIRST_MOD_RESULT(OnPreCommand, MOD_RESULT, (command, parameters, this, true, command));
	if (!MOD_RESULT)
		ServerInstance->Parser->CallHandler(command, parameters, this);

	if (ServerInstance->Config->RawLog)
		WriteServ("PRIVMSG %s :*** Raw I/O logging is enabled on this server. All messages, passwords, and commands are being recorded.", nick.c_str());

	/*
	 * We don't set REG_ALL until triggering OnUserConnect, so some module events don't spew out stuff
	 * for a user that doesn't exist yet.
	 */
	FOREACH_MOD(OnUserConnect, (this));

	this->registered = REG_ALL;

	FOREACH_MOD(OnPostConnect, (this));

	ServerInstance->SNO->WriteToSnoMask('c',"Client connecting on port %d (class %s): %s (%s) [%s]",
		this->GetServerPort(), this->MyClass->name.c_str(), GetFullRealHost().c_str(), this->GetIPString().c_str(), this->fullname.c_str());
	ServerInstance->Logs->Log("BANCACHE", LOG_DEBUG, "BanCache: Adding NEGATIVE hit for " + this->GetIPString());
	ServerInstance->BanCache->AddHit(this->GetIPString(), "", "");
	// reset the flood penalty (which could have been raised due to things like auto +x)
	CommandFloodPenalty = 0;
}

void User::InvalidateCache()
{
	/* Invalidate cache */
	cached_fullhost.clear();
	cached_hostip.clear();
	cached_makehost.clear();
	cached_fullrealhost.clear();
}

bool User::ChangeNick(const std::string& newnick, bool force)
{
	if (quitting)
	{
		ServerInstance->Logs->Log("USERS", LOG_DEFAULT, "ERROR: Attempted to change nick of a quitting user: " + this->nick);
		return false;
	}

	if (!force)
	{
		ModResult MOD_RESULT;
		FIRST_MOD_RESULT(OnUserPreNick, MOD_RESULT, (this, newnick));

		if (MOD_RESULT == MOD_RES_DENY)
		{
			ServerInstance->stats->statsCollisions++;
			return false;
		}
	}

	if (assign(newnick) == assign(nick))
	{
		// case change, don't need to check Q:lines and such
		// and, if it's identical including case, we can leave right now
		if (newnick == nick)
			return true;
	}
	else
	{
		/*
		 * Don't check Q:Lines if it's a server-enforced change, just on the off-chance some fucking *moron*
		 * tries to Q:Line SIDs, also, this means we just get our way period, as it really should be.
		 * Thanks Kein for finding this. -- w00t
		 *
		 * Also don't check Q:Lines for remote nickchanges, they should have our Q:Lines anyway to enforce themselves.
		 *		-- w00t
		 */
		if (IS_LOCAL(this) && !force)
		{
			XLine* mq = ServerInstance->XLines->MatchesLine("Q",newnick);
			if (mq)
			{
				if (this->registered == REG_ALL)
				{
					ServerInstance->SNO->WriteGlobalSno('a', "Q-Lined nickname %s from %s: %s",
						newnick.c_str(), GetFullRealHost().c_str(), mq->reason.c_str());
				}
				this->WriteNumeric(432, "%s %s :Invalid nickname: %s",this->nick.c_str(), newnick.c_str(), mq->reason.c_str());
				return false;
			}

			if (ServerInstance->Config->RestrictBannedUsers)
			{
				for (UCListIter i = this->chans.begin(); i != this->chans.end(); i++)
				{
					Channel *chan = *i;
					if (chan->GetPrefixValue(this) < VOICE_VALUE && chan->IsBanned(this))
					{
						this->WriteNumeric(404, "%s %s :Cannot send to channel (you're banned)", this->nick.c_str(), chan->name.c_str());
						return false;
					}
				}
			}
		}

		/*
		 * Uh oh.. if the nickname is in use, and it's not in use by the person using it (doh) --
		 * then we have a potential collide. Check whether someone else is camping on the nick
		 * (i.e. connect -> send NICK, don't send USER.) If they are camping, force-change the
		 * camper to their UID, and allow the incoming nick change.
		 *
		 * If the guy using the nick is already using it, tell the incoming nick change to gtfo,
		 * because the nick is already (rightfully) in use. -- w00t
		 */
		User* InUse = ServerInstance->FindNickOnly(newnick);
		if (InUse && (InUse != this))
		{
			if (InUse->registered != REG_ALL)
			{
				/* force the camper to their UUID, and ask them to re-send a NICK. */
				InUse->WriteTo(InUse, "NICK %s", InUse->uuid.c_str());
				InUse->WriteNumeric(433, "%s %s :Nickname overruled.", InUse->nick.c_str(), InUse->nick.c_str());

				ServerInstance->Users->clientlist->erase(InUse->nick);
				(*(ServerInstance->Users->clientlist))[InUse->uuid] = InUse;

				InUse->nick = InUse->uuid;
				InUse->InvalidateCache();
				InUse->registered &= ~REG_NICK;
			}
			else
			{
				/* No camping, tell the incoming user  to stop trying to change nick ;p */
				this->WriteNumeric(433, "%s %s :Nickname is already in use.", this->registered >= REG_NICK ? this->nick.c_str() : "*", newnick.c_str());
				return false;
			}
		}
	}

	if (this->registered == REG_ALL)
		this->WriteCommon("NICK %s",newnick.c_str());
	std::string oldnick = nick;
	nick = newnick;

	InvalidateCache();
	ServerInstance->Users->clientlist->erase(oldnick);
	(*(ServerInstance->Users->clientlist))[newnick] = this;

	if (registered == REG_ALL)
		FOREACH_MOD(OnUserPostNick, (this,oldnick));

	return true;
}

int LocalUser::GetServerPort()
{
	switch (this->server_sa.sa.sa_family)
	{
		case AF_INET6:
			return htons(this->server_sa.in6.sin6_port);
		case AF_INET:
			return htons(this->server_sa.in4.sin_port);
	}
	return 0;
}

const std::string& User::GetIPString()
{
	int port;
	if (cachedip.empty())
	{
		irc::sockets::satoap(client_sa, cachedip, port);
		/* IP addresses starting with a : on irc are a Bad Thing (tm) */
		if (cachedip[0] == ':')
			cachedip.insert(0,1,'0');
	}

	return cachedip;
}

irc::sockets::cidr_mask User::GetCIDRMask()
{
	int range = 0;
	switch (client_sa.sa.sa_family)
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

bool User::SetClientIP(const char* sip, bool recheck_eline)
{
	cachedip.clear();
	cached_hostip.clear();
	return irc::sockets::aptosa(sip, 0, client_sa);
}

void User::SetClientIP(const irc::sockets::sockaddrs& sa, bool recheck_eline)
{
	cachedip.clear();
	cached_hostip.clear();
	memcpy(&client_sa, &sa, sizeof(irc::sockets::sockaddrs));
}

bool LocalUser::SetClientIP(const char* sip, bool recheck_eline)
{
	irc::sockets::sockaddrs sa;
	if (!irc::sockets::aptosa(sip, 0, sa))
		// Invalid
		return false;

	LocalUser::SetClientIP(sa, recheck_eline);
	return true;
}

void LocalUser::SetClientIP(const irc::sockets::sockaddrs& sa, bool recheck_eline)
{
	if (sa != client_sa)
	{
		User::SetClientIP(sa);
		if (recheck_eline)
			this->exempt = (ServerInstance->XLines->MatchesLine("E", this) != NULL);

		FOREACH_MOD(OnSetUserIP, (this));
	}
}

static std::string wide_newline("\r\n");

void User::Write(const std::string& text)
{
}

void User::Write(const char *text, ...)
{
}

void LocalUser::Write(const std::string& text)
{
	if (!ServerInstance->SE->BoundsCheckFd(&eh))
		return;

	if (text.length() > ServerInstance->Config->Limits.MaxLine - 2)
	{
		// this should happen rarely or never. Crop the string at 512 and try again.
		std::string try_again = text.substr(0, ServerInstance->Config->Limits.MaxLine - 2);
		Write(try_again);
		return;
	}

	ServerInstance->Logs->Log("USEROUTPUT", LOG_RAWIO, "C[%s] O %s", uuid.c_str(), text.c_str());

	eh.AddWriteBuf(text);
	eh.AddWriteBuf(wide_newline);

	ServerInstance->stats->statsSent += text.length() + 2;
	this->bytes_out += text.length() + 2;
	this->cmds_out++;
}

/** Write()
 */
void LocalUser::Write(const char *text, ...)
{
	std::string textbuffer;
	VAFORMAT(textbuffer, text, text);
	this->Write(textbuffer);
}

void User::WriteServ(const std::string& text)
{
	this->Write(":%s %s",ServerInstance->Config->ServerName.c_str(),text.c_str());
}

/** WriteServ()
 *  Same as Write(), except `text' is prefixed with `:server.name '.
 */
void User::WriteServ(const char* text, ...)
{
	std::string textbuffer;
	VAFORMAT(textbuffer, text, text);
	this->WriteServ(textbuffer);
}

void User::WriteNotice(const std::string& text)
{
	this->WriteServ("NOTICE " + (this->registered == REG_ALL ? this->nick : "*") + " :" + text);
}

void User::WriteNumeric(unsigned int numeric, const char* text, ...)
{
	std::string textbuffer;
	VAFORMAT(textbuffer, text, text);
	this->WriteNumeric(numeric, textbuffer);
}

void User::WriteNumeric(unsigned int numeric, const std::string &text)
{
	ModResult MOD_RESULT;

	FIRST_MOD_RESULT(OnNumeric, MOD_RESULT, (this, numeric, text));

	if (MOD_RESULT == MOD_RES_DENY)
		return;
	
	const std::string message = InspIRCd::Format(":%s %03u %s", ServerInstance->Config->ServerName.c_str(),
		numeric, text.c_str());
	this->Write(message);
}

void User::WriteFrom(User *user, const std::string &text)
{
	const std::string message = ":" + user->GetFullHost() + " " + text;
	this->Write(message);
}


/* write text from an originating user to originating user */

void User::WriteFrom(User *user, const char* text, ...)
{
	std::string textbuffer;
	VAFORMAT(textbuffer, text, text);
	this->WriteFrom(user, textbuffer);
}


/* write text to an destination user from a source user (e.g. user privmsg) */

void User::WriteTo(User *dest, const char *data, ...)
{
	std::string textbuffer;
	VAFORMAT(textbuffer, data, data);
	this->WriteTo(dest, textbuffer);
}

void User::WriteTo(User *dest, const std::string &data)
{
	dest->WriteFrom(this, data);
}

void User::WriteCommon(const char* text, ...)
{
	if (this->registered != REG_ALL || quitting)
		return;

	std::string textbuffer;
	VAFORMAT(textbuffer, text, text);
	textbuffer = ":" + this->GetFullHost() + " " + textbuffer;
	this->WriteCommonRaw(textbuffer, true);
}

void User::WriteCommonExcept(const char* text, ...)
{
	if (this->registered != REG_ALL || quitting)
		return;

	std::string textbuffer;
	VAFORMAT(textbuffer, text, text);
	textbuffer = ":" + this->GetFullHost() + " " + textbuffer;
	this->WriteCommonRaw(textbuffer, false);
}

void User::WriteCommonRaw(const std::string &line, bool include_self)
{
	if (this->registered != REG_ALL || quitting)
		return;

	LocalUser::already_sent_id++;

	UserChanList include_c(chans);
	std::map<User*,bool> exceptions;

	exceptions[this] = include_self;

	FOREACH_MOD(OnBuildNeighborList, (this, include_c, exceptions));

	for (std::map<User*,bool>::iterator i = exceptions.begin(); i != exceptions.end(); ++i)
	{
		LocalUser* u = IS_LOCAL(i->first);
		if (u && !u->quitting)
		{
			u->already_sent = LocalUser::already_sent_id;
			if (i->second)
				u->Write(line);
		}
	}
	for (UCListIter v = include_c.begin(); v != include_c.end(); ++v)
	{
		Channel* c = *v;
		const UserMembList* ulist = c->GetUsers();
		for (UserMembList::const_iterator i = ulist->begin(); i != ulist->end(); i++)
		{
			LocalUser* u = IS_LOCAL(i->first);
			if (u && !u->quitting && u->already_sent != LocalUser::already_sent_id)
			{
				u->already_sent = LocalUser::already_sent_id;
				u->Write(line);
			}
		}
	}
}

void User::WriteCommonQuit(const std::string &normal_text, const std::string &oper_text)
{
	if (this->registered != REG_ALL)
		return;

	already_sent_t uniq_id = ++LocalUser::already_sent_id;

	const std::string normalMessage = ":" + this->GetFullHost() + " QUIT :" + normal_text;
	const std::string operMessage = ":" + this->GetFullHost() + " QUIT :" + oper_text;

	UserChanList include_c(chans);
	std::map<User*,bool> exceptions;

	FOREACH_MOD(OnBuildNeighborList, (this, include_c, exceptions));

	for (std::map<User*,bool>::iterator i = exceptions.begin(); i != exceptions.end(); ++i)
	{
		LocalUser* u = IS_LOCAL(i->first);
		if (u && !u->quitting)
		{
			u->already_sent = uniq_id;
			if (i->second)
				u->Write(u->IsOper() ? operMessage : normalMessage);
		}
	}
	for (UCListIter v = include_c.begin(); v != include_c.end(); ++v)
	{
		const UserMembList* ulist = (*v)->GetUsers();
		for (UserMembList::const_iterator i = ulist->begin(); i != ulist->end(); i++)
		{
			LocalUser* u = IS_LOCAL(i->first);
			if (u && !u->quitting && (u->already_sent != uniq_id))
			{
				u->already_sent = uniq_id;
				u->Write(u->IsOper() ? operMessage : normalMessage);
			}
		}
	}
}

void LocalUser::SendText(const std::string& line)
{
	Write(line);
}

void RemoteUser::SendText(const std::string& line)
{
	ServerInstance->PI->PushToClient(this, line);
}

void FakeUser::SendText(const std::string& line)
{
}

void User::SendText(const char *text, ...)
{
	std::string line;
	VAFORMAT(line, text, text);
	SendText(line);
}

void User::SendText(const std::string& linePrefix, std::stringstream& textStream)
{
	std::string line;
	std::string word;
	while (textStream >> word)
	{
		size_t lineLength = linePrefix.length() + line.length() + word.length() + 3; // "\s\n\r"
		if (lineLength > ServerInstance->Config->Limits.MaxLine)
		{
			SendText(linePrefix + line);
			line.clear();
		}
		line += " " + word;
	}
	SendText(linePrefix + line);
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
	if ((!other) || (this->registered != REG_ALL) || (other->registered != REG_ALL))
		return false;

	/* Outer loop */
	for (UCListIter i = this->chans.begin(); i != this->chans.end(); i++)
	{
		/* Eliminate the inner loop (which used to be ~equal in size to the outer loop)
		 * by replacing it with a map::find which *should* be more efficient
		 */
		if ((*i)->HasUser(other))
			return true;
	}
	return false;
}

bool User::ChangeName(const std::string& gecos)
{
	if (!this->fullname.compare(gecos))
		return true;

	if (IS_LOCAL(this))
	{
		ModResult MOD_RESULT;
		FIRST_MOD_RESULT(OnChangeLocalUserGECOS, MOD_RESULT, (IS_LOCAL(this),gecos));
		if (MOD_RESULT == MOD_RES_DENY)
			return false;
		FOREACH_MOD(OnChangeName, (this,gecos));
	}
	this->fullname.assign(gecos, 0, ServerInstance->Config->Limits.MaxGecos);

	return true;
}

bool User::ChangeDisplayedHost(const std::string& shost)
{
	if (dhost == shost)
		return true;

	if (IS_LOCAL(this))
	{
		ModResult MOD_RESULT;
		FIRST_MOD_RESULT(OnChangeLocalUserHost, MOD_RESULT, (IS_LOCAL(this),shost));
		if (MOD_RESULT == MOD_RES_DENY)
			return false;
	}

	FOREACH_MOD(OnChangeHost, (this,shost));

	this->dhost.assign(shost, 0, 64);
	this->InvalidateCache();

	if (IS_LOCAL(this))
		this->WriteNumeric(RPL_YOURDISPLAYEDHOST, "%s %s :is now your displayed host",this->nick.c_str(),this->dhost.c_str());

	return true;
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

void User::SendAll(const char* command, const char* text, ...)
{
	std::string textbuffer;
	VAFORMAT(textbuffer, text, text);
	const std::string message = ":" + this->GetFullHost() + " " + command + " $* :" + textbuffer;

	for (LocalUserList::const_iterator i = ServerInstance->Users->local_users.begin(); i != ServerInstance->Users->local_users.end(); i++)
	{
		if ((*i)->registered == REG_ALL)
			(*i)->Write(message);
	}
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
	ConnectClass *found = NULL;

	ServerInstance->Logs->Log("CONNECTCLASS", LOG_DEBUG, "Setting connect class for UID %s", this->uuid.c_str());

	if (!explicit_name.empty())
	{
		for (ClassVector::iterator i = ServerInstance->Config->Classes.begin(); i != ServerInstance->Config->Classes.end(); i++)
		{
			ConnectClass* c = *i;

			if (explicit_name == c->name)
			{
				ServerInstance->Logs->Log("CONNECTCLASS", LOG_DEBUG, "Explicitly set to %s", explicit_name.c_str());
				found = c;
			}
		}
	}
	else
	{
		for (ClassVector::iterator i = ServerInstance->Config->Classes.begin(); i != ServerInstance->Config->Classes.end(); i++)
		{
			ConnectClass* c = *i;
			ServerInstance->Logs->Log("CONNECTCLASS", LOG_DEBUG, "Checking %s", c->GetName().c_str());

			ModResult MOD_RESULT;
			FIRST_MOD_RESULT(OnSetConnectClass, MOD_RESULT, (this,c));
			if (MOD_RESULT == MOD_RES_DENY)
				continue;
			if (MOD_RESULT == MOD_RES_ALLOW)
			{
				ServerInstance->Logs->Log("CONNECTCLASS", LOG_DEBUG, "Class forced by module to %s", c->GetName().c_str());
				found = c;
				break;
			}

			if (c->type == CC_NAMED)
				continue;

			bool regdone = (registered != REG_NONE);
			if (c->config->getBool("registered", regdone) != regdone)
				continue;

			/* check if host matches.. */
			if (!InspIRCd::MatchCIDR(this->GetIPString(), c->GetHost(), NULL) &&
			    !InspIRCd::MatchCIDR(this->host, c->GetHost(), NULL))
			{
				ServerInstance->Logs->Log("CONNECTCLASS", LOG_DEBUG, "No host match (for %s)", c->GetHost().c_str());
				continue;
			}

			/*
			 * deny change if change will take class over the limit check it HERE, not after we found a matching class,
			 * because we should attempt to find another class if this one doesn't match us. -- w00t
			 */
			if (c->limit && (c->GetReferenceCount() >= c->limit))
			{
				ServerInstance->Logs->Log("CONNECTCLASS", LOG_DEBUG, "OOPS: Connect class limit (%lu) hit, denying", c->limit);
				continue;
			}

			/* if it requires a port ... */
			int port = c->config->getInt("port");
			if (port)
			{
				ServerInstance->Logs->Log("CONNECTCLASS", LOG_DEBUG, "Requires port (%d)", port);

				/* and our port doesn't match, fail. */
				if (this->GetServerPort() != port)
					continue;
			}

			if (regdone && !c->config->getString("password").empty())
			{
				if (ServerInstance->PassCompare(this, c->config->getString("password"), password, c->config->getString("hash")))
				{
					ServerInstance->Logs->Log("CONNECTCLASS", LOG_DEBUG, "Bad password, skipping");
					continue;
				}
			}

			/* we stop at the first class that meets ALL critera. */
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
	for (UCListIter f = this->chans.begin(); f != this->chans.end(); f++)
	{
		Channel* c = *f;
		c->DelUser(this);
	}

	this->UnOper();
}

const std::string& FakeUser::GetFullHost()
{
	if (!ServerInstance->Config->HideWhoisServer.empty())
		return ServerInstance->Config->HideWhoisServer;
	return server;
}

const std::string& FakeUser::GetFullRealHost()
{
	if (!ServerInstance->Config->HideWhoisServer.empty())
		return ServerInstance->Config->HideWhoisServer;
	return server;
}

ConnectClass::ConnectClass(ConfigTag* tag, char t, const std::string& mask)
	: config(tag), type(t), fakelag(true), name("unnamed"), registration_timeout(0), host(mask),
	pingtime(0), softsendqmax(0), hardsendqmax(0), recvqmax(0),
	penaltythreshold(0), commandrate(0), maxlocal(0), maxglobal(0), maxconnwarn(true), maxchans(0),
	limit(0), resolvehostnames(true)
{
}

ConnectClass::ConnectClass(ConfigTag* tag, char t, const std::string& mask, const ConnectClass& parent)
	: config(tag), type(t), fakelag(parent.fakelag), name("unnamed"),
	registration_timeout(parent.registration_timeout), host(mask), pingtime(parent.pingtime),
	softsendqmax(parent.softsendqmax), hardsendqmax(parent.hardsendqmax), recvqmax(parent.recvqmax),
	penaltythreshold(parent.penaltythreshold), commandrate(parent.commandrate),
	maxlocal(parent.maxlocal), maxglobal(parent.maxglobal), maxconnwarn(parent.maxconnwarn), maxchans(parent.maxchans),
	limit(parent.limit), resolvehostnames(parent.resolvehostnames)
{
}

void ConnectClass::Update(const ConnectClass* src)
{
	config = src->config;
	type = src->type;
	fakelag = src->fakelag;
	name = src->name;
	registration_timeout = src->registration_timeout;
	host = src->host;
	pingtime = src->pingtime;
	softsendqmax = src->softsendqmax;
	hardsendqmax = src->hardsendqmax;
	recvqmax = src->recvqmax;
	penaltythreshold = src->penaltythreshold;
	commandrate = src->commandrate;
	maxlocal = src->maxlocal;
	maxglobal = src->maxglobal;
	maxconnwarn = src->maxconnwarn;
	maxchans = src->maxchans;
	limit = src->limit;
	resolvehostnames = src->resolvehostnames;
}
