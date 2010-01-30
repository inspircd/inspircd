/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2010 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"
#include <stdarg.h>
#include "socketengine.h"
#include "xline.h"
#include "bancache.h"
#include "commands/cmd_whowas.h"

typedef unsigned int uniq_id_t;
class sent
{
	uniq_id_t uniq_id;
	uniq_id_t* array;
	void init()
	{
		if (!array)
			array = new uniq_id_t[ServerInstance->SE->GetMaxFds()];
		memset(array, 0, ServerInstance->SE->GetMaxFds() * sizeof(uniq_id_t));
		uniq_id++;
	}
 public:
	sent() : uniq_id(static_cast<uniq_id_t>(-1)), array(NULL) {}
	inline uniq_id_t operator++()
	{
		if (++uniq_id == 0)
			init();
		return uniq_id;
	}
	inline uniq_id_t& operator[](int i)
	{
		return array[i];
	}
	~sent()
	{
		delete[] array;
	}
};

static sent already_sent;

std::string User::ProcessNoticeMasks(const char *sm)
{
	bool adding = true, oldadding = false;
	const char *c = sm;
	std::string output;

	while (c && *c)
	{
		switch (*c)
		{
			case '+':
				adding = true;
			break;
			case '-':
				adding = false;
			break;
			case '*':
				for (unsigned char d = 'A'; d <= 'z'; d++)
				{
					if (ServerInstance->SNO->IsEnabled(d))
					{
						if ((!IsNoticeMaskSet(d) && adding) || (IsNoticeMaskSet(d) && !adding))
						{
							if ((oldadding != adding) || (!output.length()))
								output += (adding ? '+' : '-');

							this->SetNoticeMask(d, adding);

							output += d;
						}
					}
					oldadding = adding;
				}
			break;
			default:
				if ((*c >= 'A') && (*c <= 'z') && (ServerInstance->SNO->IsEnabled(*c)))
				{
					if ((!IsNoticeMaskSet(*c) && adding) || (IsNoticeMaskSet(*c) && !adding))
					{
						if ((oldadding != adding) || (!output.length()))
							output += (adding ? '+' : '-');

						this->SetNoticeMask(*c, adding);

						output += *c;
					}
				}
				else
					this->WriteNumeric(ERR_UNKNOWNSNOMASK, "%s %c :is unknown snomask char to me", this->nick.c_str(), *c);

				oldadding = adding;
			break;
		}

		c++;
	}

	std::string s = this->FormatNoticeMasks();
	if (s.length() == 0)
	{
		this->modes[UM_SNOMASK] = false;
	}

	return output;
}

void LocalUser::StartDNSLookup()
{
	try
	{
		bool cached = false;
		const char* sip = this->GetIPString();
		UserResolver *res_reverse;

		QueryType resolvtype = this->client_sa.sa.sa_family == AF_INET6 ? DNS_QUERY_PTR6 : DNS_QUERY_PTR4;
		res_reverse = new UserResolver(this, sip, resolvtype, cached);

		ServerInstance->AddResolver(res_reverse, cached);
	}
	catch (CoreException& e)
	{
		ServerInstance->Logs->Log("USERS", DEBUG,"Error in resolver: %s",e.GetReason());
	}
}

bool User::IsNoticeMaskSet(unsigned char sm)
{
	if (!isalpha(sm))
		return false;
	return (snomasks[sm-65]);
}

void User::SetNoticeMask(unsigned char sm, bool value)
{
	if (!isalpha(sm))
		return;
	snomasks[sm-65] = value;
}

const char* User::FormatNoticeMasks()
{
	static char data[MAXBUF];
	int offset = 0;

	for (int n = 0; n < 64; n++)
	{
		if (snomasks[n])
			data[offset++] = n+65;
	}

	data[offset] = 0;
	return data;
}

bool User::IsModeSet(unsigned char m)
{
	if (!isalpha(m))
		return false;
	return (modes[m-65]);
}

void User::SetMode(unsigned char m, bool value)
{
	if (!isalpha(m))
		return;
	modes[m-65] = value;
}

const char* User::FormatModes(bool showparameters)
{
	static char data[MAXBUF];
	std::string params;
	int offset = 0;

	for (unsigned char n = 0; n < 64; n++)
	{
		if (modes[n])
		{
			data[offset++] = n + 65;
			ModeHandler* mh = ServerInstance->Modes->FindMode(n + 65, MODETYPE_USER);
			if (showparameters && mh && mh->GetNumParams(true))
			{
				std::string p = mh->GetUserParameter(this);
				if (p.length())
					params.append(" ").append(p);
			}
		}
	}
	data[offset] = 0;
	strlcat(data, params.c_str(), MAXBUF);
	return data;
}

User::User(const std::string &uid, const std::string& sid, int type)
	: uuid(uid), server(sid), usertype(type)
{
	age = ServerInstance->Time();
	signon = idle_lastmsg = 0;
	registered = 0;
	quietquit = quitting = exempt = dns_done = false;
	client_sa.sa.sa_family = AF_UNSPEC;

	ServerInstance->Logs->Log("USERS", DEBUG, "New UUID for user: %s", uuid.c_str());

	user_hash::iterator finduuid = ServerInstance->Users->uuidlist->find(uuid);
	if (finduuid == ServerInstance->Users->uuidlist->end())
		(*ServerInstance->Users->uuidlist)[uuid] = this;
	else
		throw CoreException("Duplicate UUID "+std::string(uuid)+" in User constructor");
}

LocalUser::LocalUser(int myfd, irc::sockets::sockaddrs* client, irc::sockets::sockaddrs* servaddr)
	: User(ServerInstance->GetUID(), ServerInstance->Config->ServerName, USERTYPE_LOCAL), eh(this)
{
	bytes_in = bytes_out = cmds_in = cmds_out = 0;
	server_sa.sa.sa_family = AF_UNSPEC;
	CommandFloodPenalty = 0;
	lastping = nping = 0;
	eh.SetFd(myfd);
	memcpy(&client_sa, client, sizeof(irc::sockets::sockaddrs));
	memcpy(&server_sa, servaddr, sizeof(irc::sockets::sockaddrs));
}

User::~User()
{
	if (ServerInstance->Users->uuidlist->find(uuid) != ServerInstance->Users->uuidlist->end())
		ServerInstance->Logs->Log("USERS", DEFAULT, "User destructor for %s called without cull", uuid.c_str());
}

const std::string& User::MakeHost()
{
	if (!this->cached_makehost.empty())
		return this->cached_makehost;

	char nhost[MAXBUF];
	/* This is much faster than snprintf */
	char* t = nhost;
	for(const char* n = ident.c_str(); *n; n++)
		*t++ = *n;
	*t++ = '@';
	for(const char* n = host.c_str(); *n; n++)
		*t++ = *n;
	*t = 0;

	this->cached_makehost.assign(nhost);

	return this->cached_makehost;
}

const std::string& User::MakeHostIP()
{
	if (!this->cached_hostip.empty())
		return this->cached_hostip;

	char ihost[MAXBUF];
	/* This is much faster than snprintf */
	char* t = ihost;
	for(const char* n = ident.c_str(); *n; n++)
		*t++ = *n;
	*t++ = '@';
	for(const char* n = this->GetIPString(); *n; n++)
		*t++ = *n;
	*t = 0;

	this->cached_hostip = ihost;

	return this->cached_hostip;
}

const std::string& User::GetFullHost()
{
	if (!this->cached_fullhost.empty())
		return this->cached_fullhost;

	char result[MAXBUF];
	char* t = result;
	for(const char* n = nick.c_str(); *n; n++)
		*t++ = *n;
	*t++ = '!';
	for(const char* n = ident.c_str(); *n; n++)
		*t++ = *n;
	*t++ = '@';
	for(const char* n = dhost.c_str(); *n; n++)
		*t++ = *n;
	*t = 0;

	this->cached_fullhost = result;

	return this->cached_fullhost;
}

char* User::MakeWildHost()
{
	static char nresult[MAXBUF];
	char* t = nresult;
	*t++ = '*';	*t++ = '!';
	*t++ = '*';	*t++ = '@';
	for(const char* n = dhost.c_str(); *n; n++)
		*t++ = *n;
	*t = 0;
	return nresult;
}

const std::string& User::GetFullRealHost()
{
	if (!this->cached_fullrealhost.empty())
		return this->cached_fullrealhost;

	char fresult[MAXBUF];
	char* t = fresult;
	for(const char* n = nick.c_str(); *n; n++)
		*t++ = *n;
	*t++ = '!';
	for(const char* n = ident.c_str(); *n; n++)
		*t++ = *n;
	*t++ = '@';
	for(const char* n = host.c_str(); *n; n++)
		*t++ = *n;
	*t = 0;

	this->cached_fullrealhost = fresult;

	return this->cached_fullrealhost;
}

bool LocalUser::IsInvited(const irc::string &channel)
{
	time_t now = ServerInstance->Time();
	InvitedList::iterator safei;
	for (InvitedList::iterator i = invites.begin(); i != invites.end(); ++i)
	{
		if (channel == i->first)
		{
			if (i->second != 0 && now > i->second)
			{
				/* Expired invite, remove it. */
				safei = i;
				--i;
				invites.erase(safei);
				continue;
			}
			return true;
		}
	}
	return false;
}

InvitedList* LocalUser::GetInviteList()
{
	time_t now = ServerInstance->Time();
	/* Weed out expired invites here. */
	InvitedList::iterator safei;
	for (InvitedList::iterator i = invites.begin(); i != invites.end(); ++i)
	{
		if (i->second != 0 && now > i->second)
		{
			/* Expired invite, remove it. */
			safei = i;
			--i;
			invites.erase(safei);
		}
	}
	return &invites;
}

void LocalUser::InviteTo(const irc::string &channel, time_t invtimeout)
{
	time_t now = ServerInstance->Time();
	if (invtimeout != 0 && now > invtimeout) return; /* Don't add invites that are expired from the get-go. */
	for (InvitedList::iterator i = invites.begin(); i != invites.end(); ++i)
	{
		if (channel == i->first)
		{
			if (i->second != 0 && invtimeout > i->second)
			{
				i->second = invtimeout;
			}

			return;
		}
	}
	invites.push_back(std::make_pair(channel, invtimeout));
}

void LocalUser::RemoveInvite(const irc::string &channel)
{
	for (InvitedList::iterator i = invites.begin(); i != invites.end(); i++)
	{
		if (channel == i->first)
		{
			invites.erase(i);
			return;
	 	}
	}
}

bool User::HasModePermission(unsigned char, ModeType)
{
	return true;
}

bool LocalUser::HasModePermission(unsigned char mode, ModeType type)
{
	if (!IS_OPER(this))
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
	if (!IS_OPER(this))
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
	if (!IS_OPER(this))
	{
		if (noisy)
			this->WriteServ("NOTICE %s :You are not an oper", this->nick.c_str());
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
		this->WriteServ("NOTICE %s :Oper type %s does not have access to priv %s", this->nick.c_str(), oper->NameStr(), privstr.c_str());
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
		line.reserve(MAXBUF);
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
			if (line.length() < MAXBUF - 2)
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
	// Add pseudo-penalty so that we continue processing after sendq recedes
	if (user->CommandFloodPenalty == 0 && getSendQSize() >= sendqmax)
		user->CommandFloodPenalty++;
	if (user->CommandFloodPenalty >= penaltymax && !user->MyClass->fakelag)
		ServerInstance->Users->QuitUser(user, "Excess Flood");
}

void UserIOHandler::AddWriteBuf(const std::string &data)
{
	if (!user->quitting && getSendQSize() + data.length() > user->MyClass->GetSendqHardMax() &&
		!user->HasPrivPermission("users/flood/increased-buffers"))
	{
		/*
		 * Quit the user FIRST, because otherwise we could recurse
		 * here and hit the same limit.
		 */
		ServerInstance->Users->QuitUser(user, "SendQ exceeded");
		ServerInstance->SNO->WriteToSnoMask('a', "User %s SendQ exceeds connect class maximum of %lu",
			user->nick.c_str(), user->MyClass->GetSendqHardMax());
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

	this->InvalidateCache();

	if (client_sa.sa.sa_family != AF_UNSPEC)
		ServerInstance->Users->RemoveCloneCounts(this);

	return Extensible::cull();
}

CullResult LocalUser::cull()
{
	std::vector<LocalUser*>::iterator x = find(ServerInstance->Users->local_users.begin(),ServerInstance->Users->local_users.end(),this);
	if (x != ServerInstance->Users->local_users.end())
		ServerInstance->Users->local_users.erase(x);
	else
		ServerInstance->Logs->Log("USERS", DEBUG, "Failed to remove user from vector");

	eh.cull();
	return User::cull();
}

CullResult FakeUser::cull()
{
	// Fake users don't quit, they just get culled.
	quitting = true;
	ServerInstance->Users->clientlist->erase(user->nick);
	ServerInstance->Users->uuidlist->erase(user->uuid);
	return User::cull();
}

void User::Oper(OperInfo* info)
{
	if (this->IsModeSet('o'))
		this->UnOper();

	this->modes[UM_OPERATOR] = 1;
	this->oper = info;
	this->WriteServ("MODE %s :+o", this->nick.c_str());
	FOREACH_MOD(I_OnOper, OnOper(this, info->name));

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
		{
			l->SetClass(opClass);
			l->CheckClass();
		}
	}

	ServerInstance->SNO->WriteToSnoMask('o',"%s (%s@%s) is now an IRC operator of type %s (using oper '%s')",
		nick.c_str(), ident.c_str(), host.c_str(), oper->NameStr(), opername.c_str());
	this->WriteNumeric(381, "%s :You are now %s %s", nick.c_str(), strchr("aeiouAEIOU", oper->name[0]) ? "an" : "a", oper->NameStr());

	ServerInstance->Logs->Log("OPER", DEFAULT, "%s!%s@%s opered as type: %s", this->nick.c_str(), this->ident.c_str(), this->host.c_str(), oper->NameStr());
	ServerInstance->Users->all_opers.push_back(this);

	// Expand permissions from config for faster lookup
	if (IS_LOCAL(this))
		oper->init();

	FOREACH_MOD(I_OnPostOper,OnPostOper(this, oper->name, opername));
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

		for (unsigned char* c = (unsigned char*)tag->getString("usermodes").c_str(); *c; ++c)
		{
			if (*c == '*')
			{
				this->AllowedUserModes.set();
			}
			else
			{
				this->AllowedUserModes[*c - 'A'] = true;
			}
		}

		for (unsigned char* c = (unsigned char*)tag->getString("chanmodes").c_str(); *c; ++c)
		{
			if (*c == '*')
			{
				this->AllowedChanModes.set();
			}
			else
			{
				this->AllowedChanModes[*c - 'A'] = true;
			}
		}
	}
}

void User::UnOper()
{
	if (!IS_OPER(this))
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

	ServerInstance->Parser->CallHandler("MODE", parameters, this);

	/* remove the user from the oper list. Will remove multiple entries as a safeguard against bug #404 */
	ServerInstance->Users->all_opers.remove(this);

	this->modes[UM_OPERATOR] = 0;
}

/* adds or updates an entry in the whowas list */
void User::AddToWhoWas()
{
	Module* whowas = ServerInstance->Modules->Find("cmd_whowas.so");
	if (whowas)
	{
		WhowasRequest req(NULL, whowas, WhowasRequest::WHOWAS_ADD);
		req.user = this;
		req.Send();
	}
}

/*
 * Check class restrictions
 */
void LocalUser::CheckClass()
{
	ConnectClass* a = this->MyClass;

	if (!a)
	{
		ServerInstance->Users->QuitUser(this, "Access denied by configuration");
	}
	else if (a->type == CC_DENY)
	{
		ServerInstance->Users->QuitUser(this, "Unauthorised connection");
		return;
	}
	else if ((a->GetMaxLocal()) && (ServerInstance->Users->LocalCloneCount(this) > a->GetMaxLocal()))
	{
		ServerInstance->Users->QuitUser(this, "No more connections allowed from your host via this connect class (local)");
		ServerInstance->SNO->WriteToSnoMask('a', "WARNING: maximum LOCAL connections (%ld) exceeded for IP %s", a->GetMaxLocal(), this->GetIPString());
		return;
	}
	else if ((a->GetMaxGlobal()) && (ServerInstance->Users->GlobalCloneCount(this) > a->GetMaxGlobal()))
	{
		ServerInstance->Users->QuitUser(this, "No more connections allowed from your host via this connect class (global)");
		ServerInstance->SNO->WriteToSnoMask('a', "WARNING: maximum GLOBAL connections (%ld) exceeded for IP %s", a->GetMaxGlobal(), this->GetIPString());
		return;
	}

	this->nping = ServerInstance->Time() + a->GetPingTime() + ServerInstance->Config->dns_timeout;
}

bool User::CheckLines(bool doZline)
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
	 * may put the user into a totally seperate class with different restrictions! so we *must* check again.
	 * Don't remove this! -- w00t
	 */
	this->SetClass();

	/* Check the password, if one is required by the user's connect class.
	 * This CANNOT be in CheckClass(), because that is called prior to PASS as well!
	 */
	if (!MyClass->pass.empty())
	{
		if (ServerInstance->PassCompare(this, MyClass->pass.c_str(), password.c_str(), MyClass->hash.c_str()))
		{
			ServerInstance->Users->QuitUser(this, "Invalid password");
			return;
		}
	}

	if (this->CheckLines())
		return;

	this->WriteServ("NOTICE Auth :Welcome to \002%s\002!",ServerInstance->Config->Network.c_str());
	this->WriteNumeric(RPL_WELCOME, "%s :Welcome to the %s IRC Network %s!%s@%s",this->nick.c_str(), ServerInstance->Config->Network.c_str(), this->nick.c_str(), this->ident.c_str(), this->host.c_str());
	this->WriteNumeric(RPL_YOURHOSTIS, "%s :Your host is %s, running version InspIRCd-2.0",this->nick.c_str(),ServerInstance->Config->ServerName.c_str());
	this->WriteNumeric(RPL_SERVERCREATED, "%s :This server was created %s %s", this->nick.c_str(), __TIME__, __DATE__);
	this->WriteNumeric(RPL_SERVERVERSION, "%s %s InspIRCd-2.0 %s %s %s", this->nick.c_str(), ServerInstance->Config->ServerName.c_str(), ServerInstance->Modes->UserModeList().c_str(), ServerInstance->Modes->ChannelModeList().c_str(), ServerInstance->Modes->ParaModeList().c_str());

	ServerInstance->Config->Send005(this);
	this->WriteNumeric(RPL_YOURUUID, "%s %s :your unique ID", this->nick.c_str(), this->uuid.c_str());

	/* Now registered */
	if (ServerInstance->Users->unregistered_count)
		ServerInstance->Users->unregistered_count--;

	/* Trigger MOTD and LUSERS output, give modules a chance too */
	ModResult MOD_RESULT;
	std::string command("MOTD");
	std::vector<std::string> parameters;
	FIRST_MOD_RESULT(OnPreCommand, MOD_RESULT, (command, parameters, this, true, command));
	if (!MOD_RESULT)
		ServerInstance->CallCommandHandler(command, parameters, this);

	MOD_RESULT = MOD_RES_PASSTHRU;
	command = "LUSERS";
	FIRST_MOD_RESULT(OnPreCommand, MOD_RESULT, (command, parameters, this, true, command));
	if (!MOD_RESULT)
		ServerInstance->CallCommandHandler(command, parameters, this);

	/*
	 * We don't set REG_ALL until triggering OnUserConnect, so some module events don't spew out stuff
	 * for a user that doesn't exist yet.
	 */
	FOREACH_MOD(I_OnUserConnect,OnUserConnect(this));

	this->registered = REG_ALL;

	FOREACH_MOD(I_OnPostConnect,OnPostConnect(this));

	ServerInstance->SNO->WriteToSnoMask('c',"Client connecting on port %d: %s!%s@%s [%s] [%s]",
		this->GetServerPort(), this->nick.c_str(), this->ident.c_str(), this->host.c_str(), this->GetIPString(), this->fullname.c_str());
	ServerInstance->Logs->Log("BANCACHE", DEBUG, "BanCache: Adding NEGATIVE hit for %s", this->GetIPString());
	ServerInstance->BanCache->AddHit(this->GetIPString(), "", "");
}

/** User::UpdateNick()
 * re-allocates a nick in the user_hash after they change nicknames,
 * returns a pointer to the new user as it may have moved
 */
User* User::UpdateNickHash(const char* New)
{
	//user_hash::iterator newnick;
	user_hash::iterator oldnick = ServerInstance->Users->clientlist->find(this->nick);

	if (!irc::string(this->nick.c_str()).compare(New))
		return oldnick->second;

	if (oldnick == ServerInstance->Users->clientlist->end())
		return NULL; /* doesnt exist */

	User* olduser = oldnick->second;
	ServerInstance->Users->clientlist->erase(oldnick);
	(*(ServerInstance->Users->clientlist))[New] = olduser;
	return olduser;
}

void User::InvalidateCache()
{
	/* Invalidate cache */
	cached_fullhost.clear();
	cached_hostip.clear();
	cached_makehost.clear();
	cached_fullrealhost.clear();
}

bool User::ForceNickChange(const char* newnick)
{
	ModResult MOD_RESULT;

	this->InvalidateCache();

	ServerInstance->NICKForced.set(this, 1);
	FIRST_MOD_RESULT(OnUserPreNick, MOD_RESULT, (this, newnick));
	ServerInstance->NICKForced.set(this, 0);

	if (MOD_RESULT == MOD_RES_DENY)
	{
		ServerInstance->stats->statsCollisions++;
		return false;
	}

	std::deque<classbase*> dummy;
	Command* nickhandler = ServerInstance->Parser->GetHandler("NICK");
	if (nickhandler) // wtfbbq, when would this not be here
	{
		std::vector<std::string> parameters;
		parameters.push_back(newnick);
		ServerInstance->NICKForced.set(this, 1);
		bool result = (ServerInstance->Parser->CallHandler("NICK", parameters, this) == CMD_SUCCESS);
		ServerInstance->NICKForced.set(this, 0);
		return result;
	}

	// Unreachable, we hope
	return false;
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

const char* User::GetIPString()
{
	int port;
	if (cachedip.empty())
	{
		irc::sockets::satoap(client_sa, cachedip, port);
		/* IP addresses starting with a : on irc are a Bad Thing (tm) */
		if (cachedip.c_str()[0] == ':')
			cachedip.insert(0,1,'0');
	}

	return cachedip.c_str();
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

bool User::SetClientIP(const char* sip)
{
	this->cachedip = "";
	return irc::sockets::aptosa(sip, 0, client_sa);
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

	if (text.length() > MAXBUF - 2)
	{
		// this should happen rarely or never. Crop the string at 512 and try again.
		std::string try_again = text.substr(0, MAXBUF - 2);
		Write(try_again);
		return;
	}

	ServerInstance->Logs->Log("USEROUTPUT", DEBUG,"C[%s] O %s", uuid.c_str(), text.c_str());

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
	va_list argsPtr;
	char textbuffer[MAXBUF];

	va_start(argsPtr, text);
	vsnprintf(textbuffer, MAXBUF, text, argsPtr);
	va_end(argsPtr);

	this->Write(std::string(textbuffer));
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
	va_list argsPtr;
	char textbuffer[MAXBUF];

	va_start(argsPtr, text);
	vsnprintf(textbuffer, MAXBUF, text, argsPtr);
	va_end(argsPtr);

	this->WriteServ(std::string(textbuffer));
}


void User::WriteNumeric(unsigned int numeric, const char* text, ...)
{
	va_list argsPtr;
	char textbuffer[MAXBUF];

	va_start(argsPtr, text);
	vsnprintf(textbuffer, MAXBUF, text, argsPtr);
	va_end(argsPtr);

	this->WriteNumeric(numeric, std::string(textbuffer));
}

void User::WriteNumeric(unsigned int numeric, const std::string &text)
{
	char textbuffer[MAXBUF];
	ModResult MOD_RESULT;

	FIRST_MOD_RESULT(OnNumeric, MOD_RESULT, (this, numeric, text));

	if (MOD_RESULT == MOD_RES_DENY)
		return;

	snprintf(textbuffer,MAXBUF,":%s %03u %s",ServerInstance->Config->ServerName.c_str(), numeric, text.c_str());
	this->Write(std::string(textbuffer));
}

void User::WriteFrom(User *user, const std::string &text)
{
	char tb[MAXBUF];

	snprintf(tb,MAXBUF,":%s %s",user->GetFullHost().c_str(),text.c_str());

	this->Write(std::string(tb));
}


/* write text from an originating user to originating user */

void User::WriteFrom(User *user, const char* text, ...)
{
	va_list argsPtr;
	char textbuffer[MAXBUF];

	va_start(argsPtr, text);
	vsnprintf(textbuffer, MAXBUF, text, argsPtr);
	va_end(argsPtr);

	this->WriteFrom(user, std::string(textbuffer));
}


/* write text to an destination user from a source user (e.g. user privmsg) */

void User::WriteTo(User *dest, const char *data, ...)
{
	char textbuffer[MAXBUF];
	va_list argsPtr;

	va_start(argsPtr, data);
	vsnprintf(textbuffer, MAXBUF, data, argsPtr);
	va_end(argsPtr);

	this->WriteTo(dest, std::string(textbuffer));
}

void User::WriteTo(User *dest, const std::string &data)
{
	dest->WriteFrom(this, data);
}

void User::WriteCommon(const char* text, ...)
{
	char textbuffer[MAXBUF];
	va_list argsPtr;

	if (this->registered != REG_ALL || quitting)
		return;

	int len = snprintf(textbuffer,MAXBUF,":%s ",this->GetFullHost().c_str());

	va_start(argsPtr, text);
	vsnprintf(textbuffer + len, MAXBUF - len, text, argsPtr);
	va_end(argsPtr);

	this->WriteCommonRaw(std::string(textbuffer), true);
}

void User::WriteCommonExcept(const char* text, ...)
{
	char textbuffer[MAXBUF];
	va_list argsPtr;

	if (this->registered != REG_ALL || quitting)
		return;

	int len = snprintf(textbuffer,MAXBUF,":%s ",this->GetFullHost().c_str());

	va_start(argsPtr, text);
	vsnprintf(textbuffer + len, MAXBUF - len, text, argsPtr);
	va_end(argsPtr);

	this->WriteCommonRaw(std::string(textbuffer), false);
}

void User::WriteCommonRaw(const std::string &line, bool include_self)
{
	if (this->registered != REG_ALL || quitting)
		return;

	uniq_id_t uniq_id = ++already_sent;

	UserChanList include_c(chans);
	std::map<User*,bool> exceptions;

	exceptions[this] = include_self;

	FOREACH_MOD(I_OnBuildNeighborList,OnBuildNeighborList(this, include_c, exceptions));

	for (std::map<User*,bool>::iterator i = exceptions.begin(); i != exceptions.end(); ++i)
	{
		LocalUser* u = IS_LOCAL(i->first);
		if (u && !u->quitting)
		{
			already_sent[u->GetFd()] = uniq_id;
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
			if (u && !u->quitting && already_sent[u->GetFd()] != uniq_id)
			{
				already_sent[u->GetFd()] = uniq_id;
				u->Write(line);
			}
		}
	}
}

void User::WriteCommonQuit(const std::string &normal_text, const std::string &oper_text)
{
	char tb1[MAXBUF];
	char tb2[MAXBUF];

	if (this->registered != REG_ALL)
		return;

	uniq_id_t uniq_id = ++already_sent;

	snprintf(tb1,MAXBUF,":%s QUIT :%s",this->GetFullHost().c_str(),normal_text.c_str());
	snprintf(tb2,MAXBUF,":%s QUIT :%s",this->GetFullHost().c_str(),oper_text.c_str());
	std::string out1 = tb1;
	std::string out2 = tb2;

	UserChanList include_c(chans);
	std::map<User*,bool> exceptions;

	FOREACH_MOD(I_OnBuildNeighborList,OnBuildNeighborList(this, include_c, exceptions));

	for (std::map<User*,bool>::iterator i = exceptions.begin(); i != exceptions.end(); ++i)
	{
		LocalUser* u = IS_LOCAL(i->first);
		if (u && !u->quitting)
		{
			already_sent[u->GetFd()] = uniq_id;
			if (i->second)
				u->Write(IS_OPER(u) ? out2 : out1);
		}
	}
	for (UCListIter v = include_c.begin(); v != include_c.end(); ++v)
	{
		const UserMembList* ulist = (*v)->GetUsers();
		for (UserMembList::const_iterator i = ulist->begin(); i != ulist->end(); i++)
		{
			LocalUser* u = IS_LOCAL(i->first);
			if (u && !u->quitting && (already_sent[u->GetFd()] != uniq_id))
			{
				already_sent[u->GetFd()] = uniq_id;
				u->Write(IS_OPER(u) ? out2 : out1);
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
	va_list argsPtr;
	char line[MAXBUF];

	va_start(argsPtr, text);
	vsnprintf(line, MAXBUF, text, argsPtr);
	va_end(argsPtr);

	SendText(std::string(line));
}

void User::SendText(const std::string &LinePrefix, std::stringstream &TextStream)
{
	char line[MAXBUF];
	int start_pos = LinePrefix.length();
	int pos = start_pos;
	memcpy(line, LinePrefix.data(), pos);
	std::string Word;
	while (TextStream >> Word)
	{
		int len = Word.length();
		if (pos + len + 12 > MAXBUF)
		{
			line[pos] = '\0';
			SendText(std::string(line));
			pos = start_pos;
		}
		line[pos] = ' ';
		memcpy(line + pos + 1, Word.data(), len);
		pos += len + 1;
	}
	line[pos] = '\0';
	SendText(std::string(line));
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

bool User::ChangeName(const char* gecos)
{
	if (!this->fullname.compare(gecos))
		return true;

	if (IS_LOCAL(this))
	{
		ModResult MOD_RESULT;
		FIRST_MOD_RESULT(OnChangeLocalUserGECOS, MOD_RESULT, (IS_LOCAL(this),gecos));
		if (MOD_RESULT == MOD_RES_DENY)
			return false;
		FOREACH_MOD(I_OnChangeName,OnChangeName(this,gecos));
	}
	this->fullname.assign(gecos, 0, ServerInstance->Config->Limits.MaxGecos);

	return true;
}

void User::DoHostCycle(const std::string &quitline)
{
	char buffer[MAXBUF];

	if (!ServerInstance->Config->CycleHosts)
		return;

	uniq_id_t silent_id = ++already_sent;
	uniq_id_t seen_id = ++already_sent;

	UserChanList include_c(chans);
	std::map<User*,bool> exceptions;

	FOREACH_MOD(I_OnBuildNeighborList,OnBuildNeighborList(this, include_c, exceptions));

	for (std::map<User*,bool>::iterator i = exceptions.begin(); i != exceptions.end(); ++i)
	{
		LocalUser* u = IS_LOCAL(i->first);
		if (u && !u->quitting)
		{
			if (i->second)
			{
				already_sent[u->GetFd()] = seen_id;
				u->Write(quitline);
			}
			else
			{
				already_sent[u->GetFd()] = silent_id;
			}
		}
	}
	for (UCListIter v = include_c.begin(); v != include_c.end(); ++v)
	{
		Channel* c = *v;
		snprintf(buffer, MAXBUF, ":%s JOIN %s", GetFullHost().c_str(), c->name.c_str());
		std::string joinline(buffer);
		Membership* memb = c->GetUser(this);
		std::string modeline = memb->modes;
		if (modeline.length() > 0)
		{
			for(unsigned int i=0; i < memb->modes.length(); i++)
				modeline.append(" ").append(nick);
			snprintf(buffer, MAXBUF, ":%s MODE %s +%s", GetFullHost().c_str(), c->name.c_str(), modeline.c_str());
			modeline = buffer;
		}

		const UserMembList *ulist = c->GetUsers();
		for (UserMembList::const_iterator i = ulist->begin(); i != ulist->end(); i++)
		{
			LocalUser* u = IS_LOCAL(i->first);
			if (u == NULL || u == this)
				continue;
			if (already_sent[u->GetFd()] == silent_id)
				continue;

			if (already_sent[u->GetFd()] != seen_id)
			{
				u->Write(quitline);
				already_sent[u->GetFd()] = seen_id;
			}
			u->Write(joinline);
			if (modeline.length() > 0)
				u->Write(modeline);
		}
	}
}

bool User::ChangeDisplayedHost(const char* shost)
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

	FOREACH_MOD(I_OnChangeHost, OnChangeHost(this,shost));

	std::string quitstr = ":" + GetFullHost() + " QUIT :Changing host";

	/* Fix by Om: User::dhost is 65 long, this was truncating some long hosts */
	this->dhost.assign(shost, 0, 64);

	this->InvalidateCache();

	this->DoHostCycle(quitstr);

	if (IS_LOCAL(this))
		this->WriteNumeric(RPL_YOURDISPLAYEDHOST, "%s %s :is now your displayed host",this->nick.c_str(),this->dhost.c_str());

	return true;
}

bool User::ChangeIdent(const char* newident)
{
	if (this->ident == newident)
		return true;

	FOREACH_MOD(I_OnChangeIdent, OnChangeIdent(this,newident));

	std::string quitstr = ":" + GetFullHost() + " QUIT :Changing ident";

	this->ident.assign(newident, 0, ServerInstance->Config->Limits.IdentMax + 1);

	this->InvalidateCache();

	this->DoHostCycle(quitstr);

	return true;
}

void User::SendAll(const char* command, const char* text, ...)
{
	char textbuffer[MAXBUF];
	char formatbuffer[MAXBUF];
	va_list argsPtr;

	va_start(argsPtr, text);
	vsnprintf(textbuffer, MAXBUF, text, argsPtr);
	va_end(argsPtr);

	snprintf(formatbuffer,MAXBUF,":%s %s $* :%s", this->GetFullHost().c_str(), command, textbuffer);
	std::string fmt = formatbuffer;

	for (std::vector<LocalUser*>::const_iterator i = ServerInstance->Users->local_users.begin(); i != ServerInstance->Users->local_users.end(); i++)
	{
		(*i)->Write(fmt);
	}
}


std::string User::ChannelList(User* source, bool spy)
{
	std::string list;

	for (UCListIter i = this->chans.begin(); i != this->chans.end(); i++)
	{
		Channel* c = *i;
		/* If the target is the sender, neither +p nor +s is set, or
		 * the channel contains the user, it is not a spy channel
		 */
		if (spy != (source == this || !(c->IsModeSet('p') || c->IsModeSet('s')) || c->HasUser(source)))
			list.append(c->GetPrefixChar(this)).append(c->name).append(" ");
	}

	return list;
}

void User::SplitChanList(User* dest, const std::string &cl)
{
	std::string line;
	std::ostringstream prefix;
	std::string::size_type start, pos, length;

	prefix << this->nick << " " << dest->nick << " :";
	line = prefix.str();
	int namelen = ServerInstance->Config->ServerName.length() + 6;

	for (start = 0; (pos = cl.find(' ', start)) != std::string::npos; start = pos+1)
	{
		length = (pos == std::string::npos) ? cl.length() : pos;

		if (line.length() + namelen + length - start > 510)
		{
			ServerInstance->SendWhoisLine(this, dest, 319, "%s", line.c_str());
			line = prefix.str();
		}

		if(pos == std::string::npos)
		{
			line.append(cl.substr(start, length - start));
			break;
		}
		else
		{
			line.append(cl.substr(start, length - start + 1));
		}
	}

	if (line.length())
	{
		ServerInstance->SendWhoisLine(this, dest, 319, "%s", line.c_str());
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

	ServerInstance->Logs->Log("CONNECTCLASS", DEBUG, "Setting connect class for UID %s", this->uuid.c_str());

	if (!explicit_name.empty())
	{
		for (ClassVector::iterator i = ServerInstance->Config->Classes.begin(); i != ServerInstance->Config->Classes.end(); i++)
		{
			ConnectClass* c = *i;

			if (explicit_name == c->name)
			{
				ServerInstance->Logs->Log("CONNECTCLASS", DEBUG, "Explicitly set to %s", explicit_name.c_str());
				found = c;
			}
		}
	}
	else
	{
		for (ClassVector::iterator i = ServerInstance->Config->Classes.begin(); i != ServerInstance->Config->Classes.end(); i++)
		{
			ConnectClass* c = *i;

			if (c->type == CC_ALLOW)
			{
				ServerInstance->Logs->Log("CONNECTCLASS", DEBUG, "ALLOW %s %d %s", c->host.c_str(), c->GetPort(), c->GetName().c_str());
			}
			else
			{
				ServerInstance->Logs->Log("CONNECTCLASS", DEBUG, "DENY %s %d %s", c->GetHost().c_str(), c->GetPort(), c->GetName().c_str());
			}

			/* check if host matches.. */
			if (c->GetHost().length() && !InspIRCd::MatchCIDR(this->GetIPString(), c->GetHost(), NULL) &&
			    !InspIRCd::MatchCIDR(this->host, c->GetHost(), NULL))
			{
				ServerInstance->Logs->Log("CONNECTCLASS", DEBUG, "No host match (for %s)", c->GetHost().c_str());
				continue;
			}

			/*
			 * deny change if change will take class over the limit check it HERE, not after we found a matching class,
			 * because we should attempt to find another class if this one doesn't match us. -- w00t
			 */
			if (c->limit && (c->GetReferenceCount() >= c->limit))
			{
				ServerInstance->Logs->Log("CONNECTCLASS", DEBUG, "OOPS: Connect class limit (%lu) hit, denying", c->limit);
				continue;
			}

			/* if it requires a port ... */
			if (c->GetPort())
			{
				ServerInstance->Logs->Log("CONNECTCLASS", DEBUG, "Requires port (%d)", c->GetPort());

				/* and our port doesn't match, fail. */
				if (this->GetServerPort() != c->GetPort())
				{
					ServerInstance->Logs->Log("CONNECTCLASS", DEBUG, "Port match failed (%d)", this->GetServerPort());
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

/* looks up a users password for their connection class (<ALLOW>/<DENY> tags)
 * NOTE: If the <ALLOW> or <DENY> tag specifies an ip, and this user resolves,
 * then their ip will be taken as 'priority' anyway, so for example,
 * <connect allow="127.0.0.1"> will match joe!bloggs@localhost
 */
ConnectClass* LocalUser::GetClass()
{
	return MyClass;
}

ConnectClass* User::GetClass()
{
	return NULL;
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
	pingtime(0), pass(""), hash(""), softsendqmax(0), hardsendqmax(0), recvqmax(0),
	penaltythreshold(0), commandrate(0), maxlocal(0), maxglobal(0), maxchans(0), port(0), limit(0)
{
}

ConnectClass::ConnectClass(ConfigTag* tag, char t, const std::string& mask, const ConnectClass& parent)
	: config(tag), type(t), fakelag(parent.fakelag), name("unnamed"),
	registration_timeout(parent.registration_timeout), host(mask), pingtime(parent.pingtime),
	pass(parent.pass), hash(parent.hash), softsendqmax(parent.softsendqmax),
	hardsendqmax(parent.hardsendqmax), recvqmax(parent.recvqmax),
	penaltythreshold(parent.penaltythreshold), commandrate(parent.commandrate),
	maxlocal(parent.maxlocal), maxglobal(parent.maxglobal), maxchans(parent.maxchans),
	port(parent.port), limit(parent.limit)
{
}

void ConnectClass::Update(const ConnectClass* src)
{
	name = src->name;
	registration_timeout = src->registration_timeout;
	host = src->host;
	pingtime = src->pingtime;
	pass = src->pass;
	hash = src->hash;
	softsendqmax = src->softsendqmax;
	hardsendqmax = src->hardsendqmax;
	recvqmax = src->recvqmax;
	penaltythreshold = src->penaltythreshold;
	maxlocal = src->maxlocal;
	maxglobal = src->maxglobal;
	limit = src->limit;
}
