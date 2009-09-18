/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2009 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

/* $Core */

#include "inspircd.h"
#include <stdarg.h>
#include "socketengine.h"
#include "xline.h"
#include "bancache.h"
#include "commands/cmd_whowas.h"

/* XXX: Used for speeding up WriteCommon operations */
unsigned long uniq_id = 1;

static unsigned long* already_sent = NULL;

LocalIntExt User::NICKForced("NICKForced", NULL);
LocalStringExt User::OperQuit("OperQuit", NULL);

void InitializeAlreadySent(SocketEngine* SE)
{
	already_sent = new unsigned long[SE->GetMaxFds()];
	memset(already_sent, 0, SE->GetMaxFds() * sizeof(unsigned long));
}

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

		*c++;
	}

	std::string s = this->FormatNoticeMasks();
	if (s.length() == 0)
	{
		this->modes[UM_SNOMASK] = false;
	}

	return output;
}

void User::StartDNSLookup()
{
	try
	{
		bool cached = false;
		const char* sip = this->GetIPString();
		UserResolver *res_reverse;

		QueryType resolvtype = this->client_sa.sa.sa_family == AF_INET6 ? DNS_QUERY_PTR6 : DNS_QUERY_PTR4;
		res_reverse = new UserResolver(ServerInstance, this, sip, resolvtype, cached);

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

void User::DecrementModes()
{
	ServerInstance->Logs->Log("USERS", DEBUG, "DecrementModes()");
	for (unsigned char n = 'A'; n <= 'z'; n++)
	{
		if (modes[n-65])
		{
			ServerInstance->Logs->Log("USERS", DEBUG,"DecrementModes() found mode %c", n);
			ModeHandler* mh = ServerInstance->Modes->FindMode(n, MODETYPE_USER);
			if (mh)
			{
				ServerInstance->Logs->Log("USERS", DEBUG,"Found handler %c and call ChangeCount", n);
				mh->ChangeCount(-1);
			}
		}
	}
}

User::User(InspIRCd* Instance, const std::string &uid)
{
	server = Instance->FindServerNamePtr(Instance->Config->ServerName);
	age = ServerInstance->Time();
	Penalty = 0;
	lastping = signon = idle_lastmsg = nping = registered = 0;
	bytes_in = bytes_out = cmds_in = cmds_out = 0;
	quietquit = quitting = exempt = haspassed = dns_done = false;
	fd = -1;
	server_sa.sa.sa_family = AF_UNSPEC;
	client_sa.sa.sa_family = AF_UNSPEC;
	recvq.clear();
	sendq.clear();
	MyClass = NULL;
	AllowedPrivs = AllowedOperCommands = NULL;
	chans.clear();
	invites.clear();

	if (uid.empty())
		uuid.assign(Instance->GetUID(), 0, UUID_LENGTH - 1);
	else
		uuid.assign(uid, 0, UUID_LENGTH - 1);

	ServerInstance->Logs->Log("USERS", DEBUG,"New UUID for user: %s (%s)", uuid.c_str(), uid.empty() ? "allocated new" : "used remote");

	user_hash::iterator finduuid = Instance->Users->uuidlist->find(uuid);
	if (finduuid == Instance->Users->uuidlist->end())
		(*Instance->Users->uuidlist)[uuid] = this;
	else
		throw CoreException("Duplicate UUID "+std::string(uuid)+" in User constructor");
}

User::~User()
{
	/* NULL for remote users :) */
	if (this->MyClass)
	{
		this->MyClass->RefCount--;
		ServerInstance->Logs->Log("USERS", DEBUG, "User destructor -- connect refcount now: %lu", this->MyClass->RefCount);
		if (MyClass->RefCount == 0)
			delete MyClass;
	}

	if (this->AllowedOperCommands)
	{
		delete AllowedOperCommands;
		AllowedOperCommands = NULL;
	}

	if (this->AllowedPrivs)
	{
		delete AllowedPrivs;
		AllowedPrivs = NULL;
	}

	this->InvalidateCache();
	this->DecrementModes();

	if (client_sa.sa.sa_family != AF_UNSPEC)
		ServerInstance->Users->RemoveCloneCounts(this);

	ServerInstance->Users->uuidlist->erase(uuid);
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

void User::CloseSocket()
{
	if (this->fd > -1)
	{
		ServerInstance->SE->Shutdown(this, 2);
		ServerInstance->SE->Close(this);
	}
}

const std::string User::GetFullHost()
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

int User::ReadData(void* buffer, size_t size)
{
	if (IS_LOCAL(this))
	{
#ifndef WIN32
		return read(this->fd, buffer, size);
#else
		return recv(this->fd, (char*)buffer, size, 0);
#endif
	}
	else
		return 0;
}


const std::string User::GetFullRealHost()
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

bool User::IsInvited(const irc::string &channel)
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

InvitedList* User::GetInviteList()
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

void User::InviteTo(const irc::string &channel, time_t invtimeout)
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

void User::RemoveInvite(const irc::string &channel)
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

bool User::HasModePermission(unsigned char mode, ModeType type)
{
	if (!IS_LOCAL(this))
		return true;

	if (!IS_OPER(this))
		return false;

	if (mode < 'A' || mode > ('A' + 64)) return false;

	return ((type == MODETYPE_USER ? AllowedUserModes : AllowedChanModes))[(mode - 'A')];

}

bool User::HasPermission(const std::string &command)
{
	/*
	 * users on remote servers can completely bypass all permissions based checks.
	 * This prevents desyncs when one server has different type/class tags to another.
	 * That having been said, this does open things up to the possibility of source changes
	 * allowing remote kills, etc - but if they have access to the src, they most likely have
	 * access to the conf - so it's an end to a means either way.
	 */
	if (!IS_LOCAL(this))
		return true;

	// are they even an oper at all?
	if (!IS_OPER(this))
	{
		return false;
	}

	if (!AllowedOperCommands)
		return false;

	if (AllowedOperCommands->find(command) != AllowedOperCommands->end())
		return true;
	else if (AllowedOperCommands->find("*") != AllowedOperCommands->end())
		return true;

	return false;
}


bool User::HasPrivPermission(const std::string &privstr, bool noisy)
{
	if (!IS_LOCAL(this))
	{
		ServerInstance->Logs->Log("PRIVS", DEBUG, "Remote (yes)");
		return true;
	}

	if (!IS_OPER(this))
	{
		if (noisy)
			this->WriteServ("NOTICE %s :You are not an oper", this->nick.c_str());
		return false;
	}

	if (!AllowedPrivs)
	{
		if (noisy)
			this->WriteServ("NOTICE %s :Privset empty(!?)", this->nick.c_str());
		return false;
	}

	if (AllowedPrivs->find(privstr) != AllowedPrivs->end())
	{
		return true;
	}
	else if (AllowedPrivs->find("*") != AllowedPrivs->end())
	{
		return true;
	}

	if (noisy)
		this->WriteServ("NOTICE %s :Oper type %s does not have access to priv %s", this->nick.c_str(), this->oper.c_str(), privstr.c_str());
	return false;
}

bool User::AddBuffer(const std::string &a)
{
	std::string::size_type start = 0;
	std::string::size_type i = a.find('\r');

	/*
	 * The old implementation here took a copy, and rfind() on \r, removing as it found them, before
	 * copying a second time onto the recvq. That's ok, but involves three copies minimum (recv() to buffer,
	 * buffer to here, here to recvq) - The new method now copies twice (recv() to buffer, buffer to recvq).
	 *
	 * We use find() instead of rfind() for clarity, however unlike the old code, our scanning of the string is
	 * contiguous: as we specify a startpoint, we never see characters we have scanned previously, making this
	 * marginally faster in cases with a number of \r hidden early on in the buffer.
	 *
	 * How it works:
	 * Start at first pos of string, find first \r, append everything in the chunk (excluding \r) to recvq. Set
	 * i ahead of the \r, search for next \r, add next chunk to buffer... repeat.
	 *		-- w00t (7 may, 2008)
	 */
	if (i == std::string::npos)
	{
		// no \r that we need to dance around, just add to buffer
		recvq.append(a);
	}
	else
	{
		// While we can find the end of a chunk to add
		while (i != std::string::npos)
		{
			// Append the chunk that we have
			recvq.append(a, start, (i - start));

			// Start looking for the next one
			start = i + 1;
			i = a.find('\r', start);
		}

		if (start != a.length())
		{
			/*
			 * This is here to catch a corner case when we get something like:
			 * NICK w0
			 * 0t\r\nU
			 * SER ...
			 * in successive calls to us.
			 *
			 * Without this conditional, the 'U' on the second case will be dropped,
			 * which is most *certainly* not the behaviour we want!
			 *		-- w00t
			 */
			recvq.append(a, start, (a.length() - start));
		}
	}

	if (this->MyClass && !this->HasPrivPermission("users/flood/increased-buffers") && recvq.length() > this->MyClass->GetRecvqMax())
	{
		ServerInstance->Users->QuitUser(this, "RecvQ exceeded");
		ServerInstance->SNO->WriteToSnoMask('a', "User %s RecvQ of %lu exceeds connect class maximum of %lu",this->nick.c_str(),(unsigned long int)recvq.length(),this->MyClass->GetRecvqMax());
		return false;
	}

	return true;
}

bool User::BufferIsReady()
{
	return (recvq.find('\n') != std::string::npos);
}

void User::ClearBuffer()
{
	recvq.clear();
}

std::string User::GetBuffer()
{
	try
	{
		if (recvq.empty())
			return "";

		/* Strip any leading \r or \n off the string.
		 * Usually there are only one or two of these,
		 * so its is computationally cheap to do.
		 */
		std::string::iterator t = recvq.begin();
		while (t != recvq.end() && (*t == '\r' || *t == '\n'))
		{
			recvq.erase(t);
			t = recvq.begin();
		}

		for (std::string::iterator x = recvq.begin(); x != recvq.end(); x++)
		{
			/* Find the first complete line, return it as the
			 * result, and leave the recvq as whats left
			 */
			if (*x == '\n')
			{
				std::string ret = std::string(recvq.begin(), x);
				recvq.erase(recvq.begin(), x + 1);
				return ret;
			}
		}
		return "";
	}

	catch (...)
	{
		ServerInstance->Logs->Log("USERS", DEBUG,"Exception in User::GetBuffer()");
		return "";
	}
}

void User::AddWriteBuf(const std::string &data)
{
	if (!this->quitting && this->MyClass && !this->HasPrivPermission("users/flood/increased-buffers") && sendq.length() + data.length() > this->MyClass->GetSendqMax())
	{
		/*
		 * Fix by brain - Set the error text BEFORE calling, because
		 * if we dont it'll recursively  call here over and over again trying
		 * to repeatedly add the text to the sendq!
		 */
		ServerInstance->Users->QuitUser(this, "SendQ exceeded");
		ServerInstance->SNO->WriteToSnoMask('a', "User %s SendQ of %lu exceeds connect class maximum of %lu",this->nick.c_str(),(unsigned long int)sendq.length() + data.length(),this->MyClass->GetSendqMax());
		return;
	}

	// We still want to append data to the sendq of a quitting user,
	// e.g. their ERROR message that says 'closing link'

	if (data.length() > MAXBUF - 2) /* MAXBUF has a value of 514, to account for line terminators */
		sendq.append(data.substr(0,MAXBUF - 4)).append("\r\n"); /* MAXBUF-4 = 510 */
	else
		sendq.append(data);
}

// send AS MUCH OF THE USERS SENDQ as we are able to (might not be all of it)
void User::FlushWriteBuf()
{
	if (this->fd == FD_MAGIC_NUMBER)
	{
		sendq.clear();
		return;
	}

	if ((sendq.length()) && (this->fd != FD_MAGIC_NUMBER))
	{
		int old_sendq_length = sendq.length();
		int n_sent = ServerInstance->SE->Send(this, this->sendq.data(), this->sendq.length(), 0);

		if (n_sent == -1)
		{
			if (errno == EAGAIN)
			{
				/* The socket buffer is full. This isnt fatal,
				 * try again later.
				 */
				ServerInstance->SE->WantWrite(this);
			}
			else
			{
				/* Fatal error, set write error and bail */
				ServerInstance->Users->QuitUser(this, errno ? strerror(errno) : "Write error");
				return;
			}
		}
		else
		{
			/* advance the queue */
			if (n_sent)
				this->sendq = this->sendq.substr(n_sent);
			/* update the user's stats counters */
			this->bytes_out += n_sent;
			this->cmds_out++;
			if (n_sent != old_sendq_length)
				ServerInstance->SE->WantWrite(this);
		}
	}

	/* note: NOT else if! */
	if (this->sendq.empty())
	{
		FOREACH_MOD(I_OnBufferFlushed,OnBufferFlushed(this));
	}
}

void User::Oper(const std::string &opertype, const std::string &opername)
{
	if (this->IsModeSet('o'))
		this->UnOper();

	this->modes[UM_OPERATOR] = 1;
	this->WriteServ("MODE %s :+o", this->nick.c_str());
	FOREACH_MOD(I_OnOper, OnOper(this, opertype));

	ServerInstance->SNO->WriteToSnoMask('o',"%s (%s@%s) is now an IRC operator of type %s (using oper '%s')", this->nick.c_str(), this->ident.c_str(), this->host.c_str(), irc::Spacify(opertype.c_str()), opername.c_str());
	this->WriteNumeric(381, "%s :You are now %s %s", this->nick.c_str(), strchr("aeiouAEIOU", *opertype.c_str()) ? "an" : "a", irc::Spacify(opertype.c_str()));

	ServerInstance->Logs->Log("OPER", DEFAULT, "%s!%s@%s opered as type: %s", this->nick.c_str(), this->ident.c_str(), this->host.c_str(), opertype.c_str());
	this->oper.assign(opertype, 0, 512);
	ServerInstance->Users->all_opers.push_back(this);

	/*
	 * This might look like it's in the wrong place.
	 * It is *not*!
	 *
	 * For multi-network servers, we may not have the opertypes of the remote server, but we still want to mark the user as an oper of that type.
	 * -- w00t
	 */
	opertype_t::iterator iter_opertype = ServerInstance->Config->opertypes.find(this->oper.c_str());
	if (iter_opertype != ServerInstance->Config->opertypes.end())
	{
		if (AllowedOperCommands)
			AllowedOperCommands->clear();
		else
			AllowedOperCommands = new std::set<std::string>;

		if (AllowedPrivs)
			AllowedPrivs->clear();
		else
			AllowedPrivs = new std::set<std::string>;

		AllowedUserModes.reset();
		AllowedChanModes.reset();
		this->AllowedUserModes['o' - 'A'] = true; // Call me paranoid if you want.

		std::string myclass, mycmd, mypriv;
		irc::spacesepstream Classes(iter_opertype->second.c_str());
		while (Classes.GetToken(myclass))
		{
			operclass_t::iterator iter_operclass = ServerInstance->Config->operclass.find(myclass.c_str());
			if (iter_operclass != ServerInstance->Config->operclass.end())
			{
				/* Process commands */
				irc::spacesepstream CommandList(iter_operclass->second.commandlist);
				while (CommandList.GetToken(mycmd))
				{
					this->AllowedOperCommands->insert(mycmd);
				}

				irc::spacesepstream PrivList(iter_operclass->second.privs);
				while (PrivList.GetToken(mypriv))
				{
					this->AllowedPrivs->insert(mypriv);
				}

				for (unsigned char* c = (unsigned char*)iter_operclass->second.umodelist.c_str(); *c; ++c)
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

				for (unsigned char* c = (unsigned char*)iter_operclass->second.cmodelist.c_str(); *c; ++c)
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
	}

	FOREACH_MOD(I_OnPostOper,OnPostOper(this, opertype, opername));
}

void User::UnOper()
{
	if (IS_OPER(this))
	{
		/*
		 * unset their oper type (what IS_OPER checks).
		 * note, order is important - this must come before modes as -o attempts
		 * to call UnOper. -- w00t
		 */
		this->oper.clear();


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

		if (AllowedOperCommands)
		{
			delete AllowedOperCommands;
			AllowedOperCommands = NULL;
		}

		if (AllowedPrivs)
		{
			delete AllowedPrivs;
			AllowedPrivs = NULL;
		}

		AllowedUserModes.reset();
		AllowedChanModes.reset();
		this->modes[UM_OPERATOR] = 0;
	}
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
void User::CheckClass()
{
	ConnectClass* a = this->MyClass;

	if ((!a) || (a->type == CC_DENY))
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

void User::FullConnect()
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
	if (this->MyClass && !this->MyClass->GetPass().empty() && !this->haspassed)
	{
		ServerInstance->Users->QuitUser(this, "Invalid password");
		return;
	}

	if (this->CheckLines())
		return;

	this->WriteServ("NOTICE Auth :Welcome to \002%s\002!",ServerInstance->Config->Network);
	this->WriteNumeric(RPL_WELCOME, "%s :Welcome to the %s IRC Network %s!%s@%s",this->nick.c_str(), ServerInstance->Config->Network, this->nick.c_str(), this->ident.c_str(), this->host.c_str());
	this->WriteNumeric(RPL_YOURHOSTIS, "%s :Your host is %s, running version InspIRCd-2.0",this->nick.c_str(),ServerInstance->Config->ServerName);
	this->WriteNumeric(RPL_SERVERCREATED, "%s :This server was created %s %s", this->nick.c_str(), __TIME__, __DATE__);
	this->WriteNumeric(RPL_SERVERVERSION, "%s %s InspIRCd-2.0 %s %s %s", this->nick.c_str(), ServerInstance->Config->ServerName, ServerInstance->Modes->UserModeList().c_str(), ServerInstance->Modes->ChannelModeList().c_str(), ServerInstance->Modes->ParaModeList().c_str());

	ServerInstance->Config->Send005(this);
	this->WriteNumeric(RPL_YOURUUID, "%s %s :your unique ID", this->nick.c_str(), this->uuid.c_str());


	this->ShowMOTD();

	/* Now registered */
	if (ServerInstance->Users->unregistered_count)
		ServerInstance->Users->unregistered_count--;

	/* Trigger LUSERS output, give modules a chance too */
	ModResult MOD_RESULT;
	std::string command("LUSERS");
	std::vector<std::string> parameters;
	FIRST_MOD_RESULT(ServerInstance, OnPreCommand, MOD_RESULT, (command, parameters, this, true, "LUSERS"));
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

	NICKForced.set(this, 1);
	FIRST_MOD_RESULT(ServerInstance, OnUserPreNick, MOD_RESULT, (this, newnick));
	NICKForced.set(this, 0);

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
		NICKForced.set(this, 1);
		bool result = (ServerInstance->Parser->CallHandler("NICK", parameters, this) == CMD_SUCCESS);
		NICKForced.set(this, 0);
		return result;
	}

	// Unreachable, we hope
	return false;
}

int User::GetServerPort()
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

const char* User::GetCIDRMask(int range)
{
	static char buf[44];

	if (range < 0)
		throw "Negative range, sorry, no.";

	/*
	 * Original code written by Oliver Lupton (Om).
	 * Integrated by me. Thanks. :) -- w00t
	 */
	switch (this->client_sa.sa.sa_family)
	{
		case AF_INET6:
		{
			/* unsigned char s6_addr[16]; */
			struct in6_addr v6;
			int i, bytestozero, extrabits;
			char buffer[40];

			if(range > 128)
				throw "CIDR mask width greater than address width (IPv6, 128 bit)";

			/* To create the CIDR mask we want to set all the bits after 'range' bits of the address
			 * to zero. This means the last (128 - range) bits of the address must be set to zero.
			 * Hence this number divided by 8 is the number of whole bytes from the end of the address
			 * which must be set to zero.
			 */
			bytestozero = (128 - range) / 8;

			/* Some of the least significant bits of the next most significant byte may also have to
			 * be zeroed. The number of bits is the remainder of the above division.
			 */
			extrabits = (128 - range) % 8;

			/* Populate our working struct with the parts of the user's IP which are required in the
			 * final CIDR mask. Set all the subsequent bytes to zero.
			 * (16 - bytestozero) is the number of bytes which must be populated with actual IP data.
			 */
			for(i = 0; i < (16 - bytestozero); i++)
			{
				v6.s6_addr[i] = client_sa.in6.sin6_addr.s6_addr[i];
			}

			/* And zero all the remaining bytes in the IP. */
			for(; i < 16; i++)
			{
				v6.s6_addr[i] = 0;
			}

			/* And finally, zero the extra bits required. */
			v6.s6_addr[15 - bytestozero] = (v6.s6_addr[15 - bytestozero] >> extrabits) << extrabits;

			snprintf(buf, 44, "%s/%d", inet_ntop(AF_INET6, &v6, buffer, 40), range);
			return buf;
		}
		break;
		case AF_INET:
		{
			struct in_addr v4;
			char buffer[16];

			if (range > 32)
				throw "CIDR mask width greater than address width (IPv4, 32 bit)";

			/* Users already have a sockaddr* pointer (User::ip) which contains either a v4 or v6 structure */
			v4.s_addr = client_sa.in4.sin_addr.s_addr;

			/* To create the CIDR mask we want to set all the bits after 'range' bits of the address
			 * to zero. This means the last (32 - range) bits of the address must be set to zero.
			 * This is done by shifting the value right and then back left by (32 - range) bits.
			 */
			if(range > 0)
			{
				v4.s_addr = ntohl(v4.s_addr);
				v4.s_addr = (v4.s_addr >> (32 - range)) << (32 - range);
				v4.s_addr = htonl(v4.s_addr);
			}
			else
			{
				/* a range of zero would cause a 32 bit value to be shifted by 32 bits.
				 * this has undefined behaviour, but for CIDR purposes the resulting mask
				 * from a.b.c.d/0 is 0.0.0.0/0
				 */
				v4.s_addr = 0;
			}

			snprintf(buf, 44, "%s/%d", inet_ntop(AF_INET, &v4, buffer, 16), range);
			return buf;
		}
		break;
	}

	return ""; // unused, but oh well
}

std::string User::GetServerIP()
{
	int port;
	std::string ip;
	irc::sockets::satoap(&server_sa, ip, port);
	return ip;
}

const char* User::GetIPString()
{
	int port;
	if (cachedip.empty())
	{
		irc::sockets::satoap(&client_sa, cachedip, port);
		/* IP addresses starting with a : on irc are a Bad Thing (tm) */
		if (cachedip.c_str()[0] == ':')
			cachedip.insert(0,1,'0');
	}

	return cachedip.c_str();
}

bool User::SetClientIP(const char* sip)
{
	this->cachedip = "";
	return irc::sockets::aptosa(sip, 0, &client_sa);
}

void User::Write(const std::string& text)
{
	if (!ServerInstance->SE->BoundsCheckFd(this))
		return;

	ServerInstance->Logs->Log("USEROUTPUT", DEBUG,"C[%d] O %s", this->GetFd(), text.c_str());

	if (this->GetIOHook())
	{
		/* XXX: The lack of buffering here is NOT a bug, modules implementing this interface have to
		 * implement their own buffering mechanisms
		 */
		try
		{
			this->GetIOHook()->OnRawSocketWrite(this->fd, text.data(), text.length());
			this->GetIOHook()->OnRawSocketWrite(this->fd, "\r\n", 2);
		}
		catch (CoreException& modexcept)
		{
			ServerInstance->Logs->Log("USEROUTPUT", DEBUG, "%s threw an exception: %s", modexcept.GetSource(), modexcept.GetReason());
		}
	}
	else
	{
		this->AddWriteBuf(text);
		this->AddWriteBuf("\r\n");
	}
	ServerInstance->stats->statsSent += text.length() + 2;
	ServerInstance->SE->WantWrite(this);
}

/** Write()
 */
void User::Write(const char *text, ...)
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
	this->Write(":%s %s",ServerInstance->Config->ServerName,text.c_str());
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

	FIRST_MOD_RESULT(ServerInstance, OnNumeric, MOD_RESULT, (this, numeric, text));

	if (MOD_RESULT == MOD_RES_DENY)
		return;

	snprintf(textbuffer,MAXBUF,":%s %03u %s",ServerInstance->Config->ServerName, numeric, text.c_str());
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

	if (this->registered != REG_ALL)
		return;

	va_start(argsPtr, text);
	vsnprintf(textbuffer, MAXBUF, text, argsPtr);
	va_end(argsPtr);

	this->WriteCommon(std::string(textbuffer));
}

void User::WriteCommon(const std::string &text)
{
	bool sent_to_at_least_one = false;
	char tb[MAXBUF];

	if (this->registered != REG_ALL)
		return;

	uniq_id++;

	if (!already_sent)
		InitializeAlreadySent(ServerInstance->SE);

	/* We dont want to be doing this n times, just once */
	snprintf(tb,MAXBUF,":%s %s",this->GetFullHost().c_str(),text.c_str());
	std::string out = tb;

	for (UCListIter v = this->chans.begin(); v != this->chans.end(); v++)
	{
		const UserMembList* ulist = (*v)->GetUsers();
		for (UserMembList::const_iterator i = ulist->begin(); i != ulist->end(); i++)
		{
			if ((IS_LOCAL(i->first)) && (already_sent[i->first->fd] != uniq_id))
			{
				already_sent[i->first->fd] = uniq_id;
				i->first->Write(out);
				sent_to_at_least_one = true;
			}
		}
	}

	/*
	 * if the user was not in any channels, no users will receive the text. Make sure the user
	 * receives their OWN message for WriteCommon
	 */
	if (!sent_to_at_least_one)
	{
		this->Write(std::string(tb));
	}
}


/* write a formatted string to all users who share at least one common
 * channel, NOT including the source user e.g. for use in QUIT
 */

void User::WriteCommonExcept(const char* text, ...)
{
	char textbuffer[MAXBUF];
	va_list argsPtr;

	va_start(argsPtr, text);
	vsnprintf(textbuffer, MAXBUF, text, argsPtr);
	va_end(argsPtr);

	this->WriteCommonExcept(std::string(textbuffer));
}

void User::WriteCommonQuit(const std::string &normal_text, const std::string &oper_text)
{
	char tb1[MAXBUF];
	char tb2[MAXBUF];

	if (this->registered != REG_ALL)
		return;

	uniq_id++;

	if (!already_sent)
		InitializeAlreadySent(ServerInstance->SE);

	snprintf(tb1,MAXBUF,":%s QUIT :%s",this->GetFullHost().c_str(),normal_text.c_str());
	snprintf(tb2,MAXBUF,":%s QUIT :%s",this->GetFullHost().c_str(),oper_text.c_str());
	std::string out1 = tb1;
	std::string out2 = tb2;

	for (UCListIter v = this->chans.begin(); v != this->chans.end(); v++)
	{
		const UserMembList* ulist = (*v)->GetUsers();
		for (UserMembList::const_iterator i = ulist->begin(); i != ulist->end(); i++)
		{
			if (this != i->first)
			{
				if ((IS_LOCAL(i->first)) && (already_sent[i->first->fd] != uniq_id))
				{
					already_sent[i->first->fd] = uniq_id;
					i->first->Write(IS_OPER(i->first) ? out2 : out1);
				}
			}
		}
	}
}

void User::WriteCommonExcept(const std::string &text)
{
	char tb1[MAXBUF];
	std::string out1;

	if (this->registered != REG_ALL)
		return;

	uniq_id++;

	if (!already_sent)
		InitializeAlreadySent(ServerInstance->SE);

	snprintf(tb1,MAXBUF,":%s %s",this->GetFullHost().c_str(),text.c_str());
	out1 = tb1;

	for (UCListIter v = this->chans.begin(); v != this->chans.end(); v++)
	{
		const UserMembList* ulist = (*v)->GetUsers();
		for (UserMembList::const_iterator i = ulist->begin(); i != ulist->end(); i++)
		{
			if (this != i->first)
			{
				if ((IS_LOCAL(i->first)) && (already_sent[i->first->fd] != uniq_id))
				{
					already_sent[i->first->fd] = uniq_id;
					i->first->Write(out1);
				}
			}
		}
	}

}

void User::WriteWallOps(const std::string &text)
{
	std::string wallop("WALLOPS :");
	wallop.append(text);

	for (std::vector<User*>::const_iterator i = ServerInstance->Users->local_users.begin(); i != ServerInstance->Users->local_users.end(); i++)
	{
		User* t = *i;
		if (t->IsModeSet('w'))
			this->WriteTo(t,wallop);
	}
}

void User::WriteWallOps(const char* text, ...)
{
	if (!IS_LOCAL(this))
		return;

	char textbuffer[MAXBUF];
	va_list argsPtr;

	va_start(argsPtr, text);
	vsnprintf(textbuffer, MAXBUF, text, argsPtr);
	va_end(argsPtr);

	this->WriteWallOps(std::string(textbuffer));
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
		FIRST_MOD_RESULT(ServerInstance, OnChangeLocalUserGECOS, MOD_RESULT, (this,gecos));
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

	ModResult result = MOD_RES_PASSTHRU;
	FIRST_MOD_RESULT(ServerInstance, OnHostCycle, result, (this));

	if (result == MOD_RES_DENY)
		return;
	if (result == MOD_RES_PASSTHRU && !ServerInstance->Config->CycleHosts)
		return;

	uniq_id++;

	if (!already_sent)
		InitializeAlreadySent(ServerInstance->SE);

	for (UCListIter v = this->chans.begin(); v != this->chans.end(); v++)
	{
		Channel* c = *v;
		snprintf(buffer, MAXBUF, ":%s JOIN %s", GetFullHost().c_str(), c->name.c_str());
		std::string joinline(buffer);
		std::string modeline = ServerInstance->Modes->ModeString(this, c);
		if (modeline.length() > 0)
		{
			snprintf(buffer, MAXBUF, ":%s MODE %s +%s", GetFullHost().c_str(), c->name.c_str(), modeline.c_str());
			modeline = buffer;
		}

		const UserMembList *ulist = c->GetUsers();
		for (UserMembList::const_iterator i = ulist->begin(); i != ulist->end(); i++)
		{
			User* u = i->first;
			if (u == this || !IS_LOCAL(u))
				continue;

			if (already_sent[i->first->fd] != uniq_id)
			{
				u->Write(quitline);
				already_sent[i->first->fd] = uniq_id;
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
		FIRST_MOD_RESULT(ServerInstance, OnChangeLocalUserHost, MOD_RESULT, (this,shost));
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

	for (std::vector<User*>::const_iterator i = ServerInstance->Users->local_users.begin(); i != ServerInstance->Users->local_users.end(); i++)
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
	int namelen = strlen(ServerInstance->Config->ServerName) + 6;

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
ConnectClass* User::SetClass(const std::string &explicit_name)
{
	ConnectClass *found = NULL;

	if (!IS_LOCAL(this))
		return NULL;

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
			if (c->limit && (c->RefCount >= c->limit))
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
		/* only fiddle with refcounts if they are already in a class .. */
		if (this->MyClass)
		{
			if (found == this->MyClass) // no point changing this shit :P
				return this->MyClass;
			this->MyClass->RefCount--;
			ServerInstance->Logs->Log("USERS", DEBUG, "Untying user from connect class -- refcount: %lu", this->MyClass->RefCount);
			if (MyClass->RefCount == 0)
				delete MyClass;
		}

		this->MyClass = found;
		this->MyClass->RefCount++;
		ServerInstance->Logs->Log("USERS", DEBUG, "User tied to new class -- connect refcount now: %lu", this->MyClass->RefCount);
	}

	return this->MyClass;
}

/* looks up a users password for their connection class (<ALLOW>/<DENY> tags)
 * NOTE: If the <ALLOW> or <DENY> tag specifies an ip, and this user resolves,
 * then their ip will be taken as 'priority' anyway, so for example,
 * <connect allow="127.0.0.1"> will match joe!bloggs@localhost
 */
ConnectClass* User::GetClass()
{
	return this->MyClass;
}

void User::PurgeEmptyChannels()
{
	std::vector<Channel*> to_delete;

	// firstly decrement the count on each channel
	for (UCListIter f = this->chans.begin(); f != this->chans.end(); f++)
	{
		Channel* c = *f;
		c->RemoveAllPrefixes(this);
		if (c->DelUser(this) == 0)
		{
			/* No users left in here, mark it for deletion */
			try
			{
				to_delete.push_back(c);
			}
			catch (...)
			{
				ServerInstance->Logs->Log("USERS", DEBUG,"Exception in User::PurgeEmptyChannels to_delete.push_back()");
			}
		}
	}

	for (std::vector<Channel*>::iterator n = to_delete.begin(); n != to_delete.end(); n++)
	{
		Channel* thischan = *n;
		chan_hash::iterator i2 = ServerInstance->chanlist->find(thischan->name);
		if (i2 != ServerInstance->chanlist->end())
		{
			ModResult MOD_RESULT;
			FIRST_MOD_RESULT(ServerInstance, OnChannelPreDelete, MOD_RESULT, (i2->second));
			if (MOD_RESULT == MOD_RES_DENY)
				continue; // delete halted by module
			FOREACH_MOD(I_OnChannelDelete,OnChannelDelete(i2->second));
			delete i2->second;
			ServerInstance->chanlist->erase(i2);
			this->chans.erase(*n);
		}
	}

	this->UnOper();
}

void User::ShowMOTD()
{
	if (!ServerInstance->Config->MOTD.size())
	{
		this->WriteNumeric(ERR_NOMOTD, "%s :Message of the day file is missing.",this->nick.c_str());
		return;
	}
	this->WriteNumeric(RPL_MOTDSTART, "%s :%s message of the day", this->nick.c_str(), ServerInstance->Config->ServerName);

	for (file_cache::iterator i = ServerInstance->Config->MOTD.begin(); i != ServerInstance->Config->MOTD.end(); i++)
		this->WriteNumeric(RPL_MOTD, "%s :- %s",this->nick.c_str(),i->c_str());

	this->WriteNumeric(RPL_ENDOFMOTD, "%s :End of message of the day.", this->nick.c_str());
}

void User::ShowRULES()
{
	if (!ServerInstance->Config->RULES.size())
	{
		this->WriteNumeric(ERR_NORULES, "%s :RULES File is missing",this->nick.c_str());
		return;
	}

	this->WriteNumeric(RPL_RULESTART, "%s :- %s Server Rules -",this->nick.c_str(),ServerInstance->Config->ServerName);

	for (file_cache::iterator i = ServerInstance->Config->RULES.begin(); i != ServerInstance->Config->RULES.end(); i++)
		this->WriteNumeric(RPL_RULES, "%s :- %s",this->nick.c_str(),i->c_str());

	this->WriteNumeric(RPL_RULESEND, "%s :End of RULES command.",this->nick.c_str());
}

void User::HandleEvent(EventType et, int errornum)
{
	if (this->quitting) // drop everything, user is due to be quit
		return;

	switch (et)
	{
		case EVENT_READ:
			ServerInstance->ProcessUser(this);
		break;
		case EVENT_WRITE:
			this->FlushWriteBuf();
		break;
		case EVENT_ERROR:
			ServerInstance->Users->QuitUser(this, errornum ? strerror(errornum) : "Client closed the connection");
		break;
	}
}

void User::IncreasePenalty(int increase)
{
	this->Penalty += increase;
}

void User::DecreasePenalty(int decrease)
{
	this->Penalty -= decrease;
}

void FakeUser::SetFakeServer(std::string name)
{
	this->nick = name;
	this->server = nick.c_str();
}

const std::string FakeUser::GetFullHost()
{
	if (*ServerInstance->Config->HideWhoisServer)
		return ServerInstance->Config->HideWhoisServer;
	return nick;
}

const std::string FakeUser::GetFullRealHost()
{
	if (*ServerInstance->Config->HideWhoisServer)
		return ServerInstance->Config->HideWhoisServer;
	return nick;
}

ConnectClass::ConnectClass(char t, const std::string& mask)
	: type(t), name("unnamed"), registration_timeout(0), host(mask), pingtime(0), pass(""), hash(""), sendqmax(0), recvqmax(0), maxlocal(0), maxglobal(0), maxchans(0), port(0), limit(0), RefCount(1)
{
}

ConnectClass::ConnectClass(char t, const std::string& mask, const ConnectClass& parent)
	: type(t), name("unnamed"), registration_timeout(parent.registration_timeout), host(mask), pingtime(parent.pingtime), pass(parent.pass), hash(parent.hash), sendqmax(parent.sendqmax), recvqmax(parent.recvqmax), maxlocal(parent.maxlocal), maxglobal(parent.maxglobal), maxchans(parent.maxchans), port(parent.port), limit(parent.limit), RefCount(1)
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
	sendqmax = src->sendqmax;
	recvqmax = src->recvqmax;
	maxlocal = src->maxlocal;
	maxglobal = src->maxglobal;
	limit = src->limit;
}
