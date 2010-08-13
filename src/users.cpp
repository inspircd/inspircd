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
#include "xline.h"
#include "bancache.h"
#include "commands/cmd_whowas.h"

already_sent_t LocalUser::already_sent_id = 0;

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
				for (unsigned char d = 'a'; d <= 'z'; d++)
				{
					if (!ServerInstance->SNO->masks[d - 'a'].Description.empty())
					{
						if ((!IsNoticeMaskSet(d) && adding) || (IsNoticeMaskSet(d) && !adding))
						{
							if ((oldadding != adding) || (!output.length()))
								output += (adding ? '+' : '-');

							this->SetNoticeMask(d, adding);

							output += d;
						}
						oldadding = adding;
						char u = toupper(d);
						if ((!IsNoticeMaskSet(u) && adding) || (IsNoticeMaskSet(u) && !adding))
						{
							if ((oldadding != adding) || (!output.length()))
								output += (adding ? '+' : '-');

							this->SetNoticeMask(u, adding);

							output += u;
						}
						oldadding = adding;
					}
				}
			break;
			default:
				if (isalpha(*c))
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
		dns_done = true;
		ServerInstance->stats->statsDnsBad++;
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
	: User(ServerInstance->GetUID(), ServerInstance->Config->ServerName, USERTYPE_LOCAL), eh(this),
	bytes_in(0), bytes_out(0), cmds_in(0), cmds_out(0), nping(0), CommandFloodPenalty(0),
	already_sent(0)
{
	lastping = 0;
	eh.SetFd(myfd);
	memcpy(&client_sa, client, sizeof(irc::sockets::sockaddrs));
	memcpy(&server_sa, servaddr, sizeof(irc::sockets::sockaddrs));
}

User::~User()
{
	if (ServerInstance->Users->uuidlist->find(uuid) != ServerInstance->Users->uuidlist->end())
		ServerInstance->Logs->Log("USERS", DEFAULT, "User destructor for %s called without cull", uuid.c_str());
}

std::string User::MakeHost()
{
	char nhost[MAXBUF];
	/* This is much faster than snprintf */
	char* t = nhost;
	for(const char* n = ident.c_str(); *n; n++)
		*t++ = *n;
	*t++ = '@';
	for(const char* n = host.c_str(); *n; n++)
		*t++ = *n;
	*t = 0;

	return nhost;
}

std::string User::MakeHostIP()
{
	char ihost[MAXBUF];
	/* This is much faster than snprintf */
	char* t = ihost;
	for(const char* n = ident.c_str(); *n; n++)
		*t++ = *n;
	*t++ = '@';
	for(const char* n = this->GetIPString(); *n; n++)
		*t++ = *n;
	*t = 0;

	return ihost;
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

std::string User::GetFullRealHost()
{
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

	return fresult;
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

bool User::HasModePermission(ModeID)
{
	return true;
}

bool LocalUser::HasModePermission(ModeID id)
{
	if (!IS_OPER(this))
		return false;

	ModeHandler* mh = ServerInstance->Modes->FindMode(id);
	if (!mh)
		return false;

	if (oper->AllowedPrivs.find("mode/" + mh->name) != oper->AllowedPrivs.end())
		return true;
	else if (oper->AllowedPrivs.find("mode/*") != oper->AllowedPrivs.end())
		return true;
	else if (oper->AllowedPrivs.find("*") != oper->AllowedPrivs.end())
		return true;

	return false;
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
		return true;

	// if we are checking "override/mode/op", also try checking for
	// "override/mode/*", "override/*", and "*"
	std::string path = privstr;
	while (1)
	{
		std::string::size_type slash = path.rfind('/');
		if (slash == std::string::npos)
			break;
		path = path.substr(0, slash);
		if (oper->AllowedPrivs.find(path + "/*") != oper->AllowedPrivs.end())
			return true;
	}
	if (oper->AllowedPrivs.find("*") != oper->AllowedPrivs.end())
		return true;

	if (noisy)
		this->WriteServ("NOTICE %s :Oper type %s does not have access to priv %s", this->nick.c_str(), oper->NameStr(), privstr.c_str());
	return false;
}

void UserIOHandler::OnDataReady()
{
	if (user->quitting)
		return;

	if (recvq.length() > user->MyClass->recvqmax)
	{
		ServerInstance->Users->QuitUser(user, "RecvQ exceeded");
		ServerInstance->SNO->WriteToSnoMask('a', "User %s RecvQ exceeds maximum of %lu (class %s)",
			user->nick.c_str(), user->MyClass->recvqmax, user->MyClass->name.c_str());
	}
	unsigned long sendqmax = user->MyClass->softsendqmax;
	unsigned long penaltymax = user->MyClass->penaltythreshold * 1000;

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
	if (!user->quitting && getSendQSize() + data.length() > user->MyClass->hardsendqmax)
	{
		/*
		 * Quit the user FIRST, because otherwise we could recurse
		 * here and hit the same limit.
		 */
		ServerInstance->Users->QuitUser(user, "SendQ exceeded");
		ServerInstance->SNO->WriteToSnoMask('a', "User %s SendQ exceeds maximum of %lu (class %s)",
			user->nick.c_str(), user->MyClass->hardsendqmax, user->MyClass->name.c_str());
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
	ServerInstance->Users->clientlist->erase(nick);
	ServerInstance->Users->uuidlist->erase(uuid);
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
	if (!info->config_blocks.empty() && info->config_blocks[0]->tag == "oper")
		opername = info->config_blocks[0]->getString("name");

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
		nick.c_str(), ident.c_str(), host.c_str(), oper->NameStr(), opername.c_str());
	this->WriteNumeric(381, "%s :You are now %s %s", nick.c_str(), strchr("aeiouAEIOU", oper->name[0]) ? "an" : "a", oper->NameStr());

	ServerInstance->Logs->Log("OPER", DEFAULT, "%s!%s@%s opered as type: %s", this->nick.c_str(), this->ident.c_str(), this->host.c_str(), oper->NameStr());
	ServerInstance->Users->all_opers.push_back(this);

	// Expand permissions from config for faster lookup
	if (IS_LOCAL(this))
		oper->init();

	FOREACH_MOD(I_OnPostOper,OnPostOper(this, oper->name, opername));
}

OperInfo::OperInfo(ConfigTag* tag)
{
	config_blocks.push_back(tag);
	if (tag->tag == "oper")
	{
		std::string type = tag->getString("type");
		OperIndex::iterator tblk = ServerInstance->Config->oper_blocks.find(" " + type);
		if (tblk == ServerInstance->Config->oper_blocks.end())
			throw CoreException("Oper block at " + tag->getTagLocation() + " has missing type " + type);
		tag = tblk->second->config_blocks[0];
		config_blocks.insert(config_blocks.end(), tblk->second->config_blocks.begin(), tblk->second->config_blocks.end());
	}
	name = tag->getString("name");
	std::string classname;
	irc::spacesepstream str(tag->getString("classes"));
	while (str.GetToken(classname))
	{
		ConfigTagIndex::iterator cls = ServerInstance->Config->oper_classes.find(classname);
		if (cls == ServerInstance->Config->oper_classes.end())
			throw CoreException("Oper type " + name + " has missing class " + classname);
		config_blocks.push_back(cls->second);
	}
}

void OperInfo::init()
{
	AllowedOperCommands.clear();
	AllowedPrivs.clear();
	AllowedPrivs.insert("mode/oper"); // Call me paranoid if you want.

	for(std::vector<reference<ConfigTag> >::iterator iter = config_blocks.begin(); iter != config_blocks.end(); ++iter)
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

		// backwards compatability for by-letter configuration
		for (unsigned char* c = (unsigned char*)tag->getString("usermodes").c_str(); *c; ++c)
		{
			if (*c == '*')
				AllowedPrivs.insert("mode/*");
			ModeHandler* mh = ServerInstance->Modes->FindMode(*c, MODETYPE_USER);
			if (mh)
				AllowedPrivs.insert("mode/" + mh->name);
		}

		for (unsigned char* c = (unsigned char*)tag->getString("chanmodes").c_str(); *c; ++c)
		{
			if (*c == '*')
				AllowedPrivs.insert("mode/*");
			ModeHandler* mh = ServerInstance->Modes->FindMode(*c, MODETYPE_CHANNEL);
			if (mh)
				AllowedPrivs.insert("mode/" + mh->name);
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
		ServerInstance->Users->QuitUser(this, "Unauthorized connection: no matching <connect> block found");
		return;
	}
	else if (a->type == CC_DENY)
	{
		ServerInstance->Users->QuitUser(this, a->config->getString("reason", "Unauthorized connection: <connect> block " + a->name));
		return;
	}
	else if (a->maxlocal && ServerInstance->Users->LocalCloneCount(this) > a->maxlocal)
	{
		ServerInstance->SNO->WriteGlobalSno('a', "WARNING: maximum LOCAL connections (%ld) exceeded for IP %s (connect class %s)",
			a->maxlocal, GetIPString(), a->name.c_str());
		ServerInstance->Users->QuitUser(this, "No more connections allowed from your host via this connect class (local)");
		return;
	}
	else if (a->maxglobal && ServerInstance->Users->GlobalCloneCount(this) > a->maxglobal)
	{
		ServerInstance->SNO->WriteGlobalSno('a', "WARNING: maximum GLOBAL connections (%ld) exceeded for IP %s (connect class %s)",
			a->maxglobal, GetIPString(), a->name.c_str());
		ServerInstance->Users->QuitUser(this, "No more connections allowed from your host via this connect class (global)");
		return;
	}

	this->nping = ServerInstance->Time() + a->pingtime + ServerInstance->Config->dns_timeout;
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
	MyClass = NULL;
	if (ServerInstance->ForcedClass.get(this))
	{
		std::string* cls = ServerInstance->ForcedClass.get(this);
		SetClass(*cls);
		ServerInstance->ForcedClass.unset(this);
	}
	else
	{
		SetClass();
	}
	CheckClass();
	CheckLines();

	if (quitting)
		return;

	this->WriteServ("NOTICE Auth :Welcome to \002%s\002!",ServerInstance->Config->Network.c_str());
	this->WriteNumeric(RPL_WELCOME, "%s :Welcome to the %s IRC Network %s!%s@%s",this->nick.c_str(), ServerInstance->Config->Network.c_str(), this->nick.c_str(), this->ident.c_str(), this->host.c_str());
	this->WriteNumeric(RPL_YOURHOSTIS, "%s :Your host is %s, running version InspIRCd-2.1",this->nick.c_str(),ServerInstance->Config->ServerName.c_str());
	this->WriteNumeric(RPL_SERVERCREATED, "%s :This server was created %s %s", this->nick.c_str(), __TIME__, __DATE__);
	this->WriteNumeric(RPL_SERVERVERSION, "%s %s InspIRCd-2.1 %s %s %s", this->nick.c_str(), ServerInstance->Config->ServerName.c_str(), ServerInstance->Modes->UserModeList().c_str(), ServerInstance->Modes->ChannelModeList().c_str(), ServerInstance->Modes->ParaModeList().c_str());

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

	if (ServerInstance->Config->RawLog)
		WriteServ("PRIVMSG %s :*** Raw I/O logging is enabled on this server. All messages, passwords, and commands are being recorded.", nick.c_str());

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
	// reset the flood penalty (which could have been raised due to things like auto +x)
	CommandFloodPenalty = 0;
}

void User::InvalidateCache()
{
	/* Invalidate cache */
	cached_fullhost.clear();
}

bool User::ChangeNick(const std::string& newnick, bool force)
{
	ModResult MOD_RESULT;

	if (force)
		ServerInstance->NICKForced.set(this, 1);
	FIRST_MOD_RESULT(OnUserPreNick, MOD_RESULT, (this, newnick));
	ServerInstance->NICKForced.set(this, 0);

	if (MOD_RESULT == MOD_RES_DENY)
	{
		ServerInstance->stats->statsCollisions++;
		return false;
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
					ServerInstance->SNO->WriteGlobalSno('a', "Q-Lined nickname %s from %s!%s@%s: %s",
						newnick.c_str(), this->nick.c_str(), this->ident.c_str(), this->host.c_str(), mq->reason.c_str());
				}
				this->WriteNumeric(432, "%s %s :Invalid nickname: %s",this->nick.c_str(), newnick.c_str(), mq->reason.c_str());
				return false;
			}

			if (ServerInstance->Config->RestrictBannedUsers)
			{
				for (UCListIter i = this->chans.begin(); i != this->chans.end(); i++)
				{
					if (i->GetAccessRank() < VOICE_VALUE && i->chan->IsBanned(this))
					{
						this->WriteNumeric(404, "%s %s :Cannot send to channel (you're banned)", this->nick.c_str(), i->chan->name.c_str());
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
		FOREACH_MOD(I_OnUserPostNick,OnUserPostNick(this,oldnick));

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

	ServerInstance->Logs->Log("USEROUTPUT", RAWIO, "C[%s] O %s", uuid.c_str(), text.c_str());

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

	LocalUser::already_sent_id++;

	std::vector<Channel*> include_c;
	include_c.reserve(chans.size());
	for (UCListIter v = chans.begin(); v != chans.end(); ++v)
		include_c.push_back(v->chan);
	std::map<User*,bool> exceptions;

	exceptions[this] = include_self;

	FOREACH_MOD(I_OnBuildNeighborList,OnBuildNeighborList(this, include_c, exceptions));

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
	for (std::vector<Channel*>::iterator v = include_c.begin(); v != include_c.end(); ++v)
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
	char tb1[MAXBUF];
	char tb2[MAXBUF];

	if (this->registered != REG_ALL)
		return;

	already_sent_t uniq_id = ++LocalUser::already_sent_id;

	snprintf(tb1,MAXBUF,":%s QUIT :%s",this->GetFullHost().c_str(),normal_text.c_str());
	snprintf(tb2,MAXBUF,":%s QUIT :%s",this->GetFullHost().c_str(),oper_text.c_str());
	std::string out1 = tb1;
	std::string out2 = tb2;

	std::vector<Channel*> include_c;
	include_c.reserve(chans.size());
	for (UCListIter v = chans.begin(); v != chans.end(); ++v)
		include_c.push_back(v->chan);
	std::map<User*,bool> exceptions;

	FOREACH_MOD(I_OnBuildNeighborList,OnBuildNeighborList(this, include_c, exceptions));

	for (std::map<User*,bool>::iterator i = exceptions.begin(); i != exceptions.end(); ++i)
	{
		LocalUser* u = IS_LOCAL(i->first);
		if (u && !u->quitting)
		{
			u->already_sent = uniq_id;
			if (i->second)
				u->Write(IS_OPER(u) ? out2 : out1);
		}
	}
	for (std::vector<Channel*>::iterator v = include_c.begin(); v != include_c.end(); ++v)
	{
		const UserMembList* ulist = (**v).GetUsers();
		for (UserMembList::const_iterator i = ulist->begin(); i != ulist->end(); i++)
		{
			LocalUser* u = IS_LOCAL(i->first);
			if (u && !u->quitting && (u->already_sent != uniq_id))
			{
				u->already_sent = uniq_id;
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
		if (i->chan->HasUser(other))
			return true;
	}
	return false;
}

bool User::ChangeName(const std::string& gecos)
{
	if (!this->fullname.compare(gecos))
		return true;

	FOREACH_MOD(I_OnChangeName,OnChangeName(this,gecos));
	fullname = gecos;

	return true;
}

void User::DoHostCycle(const std::string &quitline)
{
	char buffer[MAXBUF];

	if (!ServerInstance->Config->CycleHosts)
		return;

	already_sent_t silent_id = ++LocalUser::already_sent_id;
	already_sent_t seen_id = ++LocalUser::already_sent_id;

	std::vector<Channel*> include_c;
	include_c.reserve(chans.size());
	for (UCListIter v = chans.begin(); v != chans.end(); ++v)
		include_c.push_back(v->chan);
	std::map<User*,bool> exceptions;

	FOREACH_MOD(I_OnBuildNeighborList,OnBuildNeighborList(this, include_c, exceptions));

	for (std::map<User*,bool>::iterator i = exceptions.begin(); i != exceptions.end(); ++i)
	{
		LocalUser* u = IS_LOCAL(i->first);
		if (u && !u->quitting)
		{
			if (i->second)
			{
				u->already_sent = seen_id;
				u->Write(quitline);
			}
			else
			{
				u->already_sent = silent_id;
			}
		}
	}
	for (std::vector<Channel*>::iterator v = include_c.begin(); v != include_c.end(); ++v)
	{
		Channel* c = *v;
		snprintf(buffer, MAXBUF, ":%s JOIN %s", GetFullHost().c_str(), c->name.c_str());
		std::string joinline(buffer);
		Membership* memb = c->GetUser(this);
		std::string modeline = memb ? memb->modes : "";
		if (modeline.length() > 0)
		{
			for(unsigned int i=0; i < memb->modes.length(); i++)
				modeline.append(" ").append(nick);
			snprintf(buffer, MAXBUF, ":%s MODE %s +%s",
				ServerInstance->Config->CycleHostsFromUser ? GetFullHost().c_str() : ServerInstance->Config->ServerName.c_str(),
				c->name.c_str(), modeline.c_str());
			modeline = buffer;
		}

		const UserMembList *ulist = c->GetUsers();
		for (UserMembList::const_iterator i = ulist->begin(); i != ulist->end(); i++)
		{
			LocalUser* u = IS_LOCAL(i->first);
			if (u == NULL || u == this)
				continue;
			if (u->already_sent == silent_id)
				continue;

			if (u->already_sent != seen_id)
			{
				u->Write(quitline);
				u->already_sent = seen_id;
			}
			u->Write(joinline);
			if (modeline.length() > 0)
				u->Write(modeline);
		}
	}
}

bool User::ChangeDisplayedHost(const std::string& shost)
{
	if (dhost == shost)
		return true;

	FOREACH_MOD(I_OnChangeHost, OnChangeHost(this,shost));

	std::string quitstr = ":" + GetFullHost() + " QUIT :Changing host";
	dhost = shost;
	this->InvalidateCache();

	this->DoHostCycle(quitstr);

	if (IS_LOCAL(this))
		this->WriteNumeric(RPL_YOURDISPLAYEDHOST, "%s %s :is now your displayed host",this->nick.c_str(),this->dhost.c_str());

	return true;
}

bool User::ChangeIdent(const std::string& newident)
{
	if (this->ident == newident)
		return true;

	FOREACH_MOD(I_OnChangeIdent, OnChangeIdent(this,newident));

	std::string quitstr = ":" + GetFullHost() + " QUIT :Changing ident";

	ident = newident;

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
		Channel* c = i->chan;
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
			ServerInstance->Logs->Log("CONNECTCLASS", DEBUG, "Checking %s", c->name.c_str());

			ModResult MOD_RESULT;
			FIRST_MOD_RESULT(OnSetConnectClass, MOD_RESULT, (this,c));
			if (MOD_RESULT == MOD_RES_DENY)
				continue;
			if (MOD_RESULT == MOD_RES_ALLOW)
			{
				ServerInstance->Logs->Log("CONNECTCLASS", DEBUG, "Class forced by module to %s", c->name.c_str());
				found = c;
				break;
			}

			if (c->type == CC_NAMED)
				continue;

			bool regdone = (registered != REG_NONE);
			if (c->config->getBool("registered", regdone) != regdone)
				continue;

			/* check if host matches.. */
			if (!c->host.empty())
			{
				irc::spacesepstream HostList(c->host);
				std::string h;
				while (HostList.GetToken(h))
				{
					if (InspIRCd::MatchCIDR(this->GetIPString(), h, NULL))
						goto host_found;
					if (InspIRCd::Match(this->host, h, NULL))
						goto host_found;
				}
				ServerInstance->Logs->Log("CONNECTCLASS", DEBUG, "No host match (for %s)", c->host.c_str());
				continue;
			}
host_found:

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
			std::string porttag = c->config->getString("port");
			if (!porttag.empty())
			{
				irc::portparser portrange(porttag, false);
				int myport = GetServerPort();
				while (1)
				{
					int port = portrange.GetToken();
					if (port == myport)
						goto port_found;
					if (port == 0)
						break;
				}
				ServerInstance->Logs->Log("CONNECTCLASS", DEBUG, "Port does not match (%s)", porttag.c_str());
				continue;
			}
port_found:

			if (regdone && !c->config->getString("password").empty())
			{
				if (ServerInstance->PassCompare(this, c->config->getString("password"), password, c->config->getString("hash")))
				{
					ServerInstance->Logs->Log("CONNECTCLASS", DEBUG, "Bad password, skipping");
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
		Channel* c = f->chan;
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

ConnectClass::ConnectClass(ConfigTag* tag, ConnectClass* Parent)
	: config(tag), parent(Parent)
{
	name = tag->getString("name");

	if (tag->readString("allow", host, false))
		type = CC_ALLOW;
	else if (tag->readString("deny", host, false))
		type = CC_DENY;
	else if (!name.empty())
	{
		type = CC_NAMED;
		host = name;
	}
	else
	{
		throw CoreException("Connect class must have allow, deny, or name specified at " + tag->getTagLocation());
	}

	registration_timeout = tag->getInt("timeout", parent ? parent->registration_timeout : 90);
	pingtime = tag->getInt("pingfreq", parent ? parent->pingtime : 120);
	softsendqmax = tag->getInt("softsendq", parent ? parent->softsendqmax : 4096);
	hardsendqmax = tag->getInt("hardsendq", parent ? parent->hardsendqmax : 0x100000);
	std::string sendq;
	if (tag->readString("sendq", sendq))
	{
		// attempt to guess a good hard/soft sendq from a single value
		long value = atol(sendq.c_str());
		if (value > 16384)
			softsendqmax = value / 16;
		else
			softsendqmax = value;
		hardsendqmax = value * 8;
	}
	recvqmax = tag->getInt("recvq", parent ? parent->recvqmax : 4096);
	fakelag = tag->getBool("fakelag", parent ? parent->fakelag : true);
	penaltythreshold = tag->getInt("threshold", parent ? parent->penaltythreshold : (fakelag ? 10 : 20));
	commandrate = tag->getInt("commandrate", parent ? parent->commandrate : 1000);
	maxlocal = tag->getInt("localmax", parent ? parent->maxlocal : 0);
	maxglobal = tag->getInt("globalmax", parent ? parent->maxglobal : 0);
	maxchans = tag->getInt("maxchans", parent ? parent->maxchans : 0);
	limit = tag->getInt("limit", parent ? parent->limit : 0);
}

void ConnectClass::Update(const ConnectClass* src)
{
	config = src->config;
	parent = src->parent;
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
	maxchans = src->maxchans;
	limit = src->limit;
}

std::string ConnectClass::GetConfig(const std::string& key, const std::string& def)
{
	if (parent)
		return config->getString(key, parent->GetConfig(key, def));
	return config->getString(key, def);
}
