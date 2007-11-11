/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2007 InspIRCd Development Team
 * See: http://www.inspircd.org/wiki/index.php/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

/* $Core: libIRCDusers */

#include "inspircd.h"
#include <stdarg.h>
#include "socketengine.h"
#include "wildcard.h"
#include "xline.h"
#include "bancache.h"
#include "commands/cmd_whowas.h"

static unsigned long already_sent[MAX_DESCRIPTORS] = {0};

/* XXX: Used for speeding up WriteCommon operations */
unsigned long uniq_id = 0;

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
				oldadding = adding;
			break;
		}

		*c++;
	}

	return output;
}

void User::StartDNSLookup()
{
	try
	{
		bool cached;
		const char* ip = this->GetIPString();

		/* Special case for 4in6 (Have i mentioned i HATE 4in6?) */
		if (!strncmp(ip, "0::ffff:", 8))
			res_reverse = new UserResolver(this->ServerInstance, this, ip + 8, DNS_QUERY_PTR4, cached);
		else
			res_reverse = new UserResolver(this->ServerInstance, this, ip, this->GetProtocolFamily() == AF_INET ? DNS_QUERY_PTR4 : DNS_QUERY_PTR6, cached);

		this->ServerInstance->AddResolver(res_reverse, cached);
	}
	catch (CoreException& e)
	{
		ServerInstance->Log(DEBUG,"Error in resolver: %s",e.GetReason());
	}
}

bool User::IsNoticeMaskSet(unsigned char sm)
{
	return (snomasks[sm-65]);
}

void User::SetNoticeMask(unsigned char sm, bool value)
{
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
	return (modes[m-65]);
}

void User::SetMode(unsigned char m, bool value)
{
	modes[m-65] = value;
}

const char* User::FormatModes()
{
	static char data[MAXBUF];
	int offset = 0;
	for (int n = 0; n < 64; n++)
	{
		if (modes[n])
			data[offset++] = n+65;
	}
	data[offset] = 0;
	return data;
}

void User::DecrementModes()
{
	ServerInstance->Log(DEBUG,"DecrementModes()");
	for (unsigned char n = 'A'; n <= 'z'; n++)
	{
		if (modes[n-65])
		{
			ServerInstance->Log(DEBUG,"DecrementModes() found mode %c", n);
			ModeHandler* mh = ServerInstance->Modes->FindMode(n, MODETYPE_USER);
			if (mh)
			{
				ServerInstance->Log(DEBUG,"Found handler %c and call ChangeCount", n);
				mh->ChangeCount(-1);
			}
		}
	}
}

User::User(InspIRCd* Instance, const std::string &uid) : ServerInstance(Instance)
{
	*password = *nick = *ident = *host = *dhost = *fullname = *awaymsg = *oper = *uuid = 0;
	server = (char*)Instance->FindServerNamePtr(Instance->Config->ServerName);
	reset_due = ServerInstance->Time();
	age = ServerInstance->Time(true);
	Penalty = 0;
	lines_in = lastping = signon = idle_lastmsg = nping = registered = 0;
	ChannelCount = timeout = bytes_in = bytes_out = cmds_in = cmds_out = 0;
	OverPenalty = ExemptFromPenalty = muted = exempt = haspassed = dns_done = false;
	fd = -1;
	recvq.clear();
	sendq.clear();
	WriteError.clear();
	res_forward = res_reverse = NULL;
	Visibility = NULL;
	ip = NULL;
	MyClass = NULL;
	AllowedOperCommands = NULL;
	chans.clear();
	invites.clear();
	memset(modes,0,sizeof(modes));
	memset(snomasks,0,sizeof(snomasks));
	/* Invalidate cache */
	operquit = cached_fullhost = cached_hostip = cached_makehost = cached_fullrealhost = NULL;

	if (uid.empty())
		strlcpy(uuid, Instance->GetUID().c_str(), UUID_LENGTH);
	else
		strlcpy(uuid, uid.c_str(), UUID_LENGTH);

	ServerInstance->Log(DEBUG,"New UUID for user: %s (%s)", uuid, uid.empty() ? "allocated new" : "used remote");

	user_hash::iterator finduuid = Instance->uuidlist->find(uuid);
	if (finduuid == Instance->uuidlist->end())
		(*Instance->uuidlist)[uuid] = this;
	else
		throw CoreException("Duplicate UUID "+std::string(uuid)+" in User constructor");
}

void User::RemoveCloneCounts()
{
	clonemap::iterator x = ServerInstance->local_clones.find(this->GetIPString());
	if (x != ServerInstance->local_clones.end())
	{
		x->second--;
		if (!x->second)
		{
			ServerInstance->local_clones.erase(x);
		}
	}
	
	clonemap::iterator y = ServerInstance->global_clones.find(this->GetIPString());
	if (y != ServerInstance->global_clones.end())
	{
		y->second--;
		if (!y->second)
		{
			ServerInstance->global_clones.erase(y);
		}
	}
}

User::~User()
{
	/* NULL for remote users :) */
	if (this->MyClass)
	{
		this->MyClass->RefCount--;
		ServerInstance->Log(DEBUG, "User destructor -- connect refcount now: %u", this->MyClass->RefCount);
	}
	if (this->AllowedOperCommands)
	{
		delete AllowedOperCommands;
		AllowedOperCommands = NULL;
	}

	this->InvalidateCache();
	this->DecrementModes();
	if (operquit)
		free(operquit);
	if (ip)
	{
		this->RemoveCloneCounts();

		if (this->GetProtocolFamily() == AF_INET)
		{
			delete (sockaddr_in*)ip;
		}
#ifdef SUPPORT_IP6LINKS
		else
		{
			delete (sockaddr_in6*)ip;
		}
#endif
	}

	ServerInstance->uuidlist->erase(uuid);
}

char* User::MakeHost()
{
	if (this->cached_makehost)
		return this->cached_makehost;

	char nhost[MAXBUF];
	/* This is much faster than snprintf */
	char* t = nhost;
	for(char* n = ident; *n; n++)
		*t++ = *n;
	*t++ = '@';
	for(char* n = host; *n; n++)
		*t++ = *n;
	*t = 0;

	this->cached_makehost = strdup(nhost);

	return this->cached_makehost;
}

char* User::MakeHostIP()
{
	if (this->cached_hostip)
		return this->cached_hostip;

	char ihost[MAXBUF];
	/* This is much faster than snprintf */
	char* t = ihost;
	for(char* n = ident; *n; n++)
		*t++ = *n;
	*t++ = '@';
	for(const char* n = this->GetIPString(); *n; n++)
		*t++ = *n;
	*t = 0;

	this->cached_hostip = strdup(ihost);

	return this->cached_hostip;
}

void User::CloseSocket()
{
	ServerInstance->SE->Shutdown(this, 2);
	ServerInstance->SE->Close(this);
}

char* User::GetFullHost()
{
	if (this->cached_fullhost)
		return this->cached_fullhost;

	char result[MAXBUF];
	char* t = result;
	for(char* n = nick; *n; n++)
		*t++ = *n;
	*t++ = '!';
	for(char* n = ident; *n; n++)
		*t++ = *n;
	*t++ = '@';
	for(char* n = dhost; *n; n++)
		*t++ = *n;
	*t = 0;

	this->cached_fullhost = strdup(result);

	return this->cached_fullhost;
}

char* User::MakeWildHost()
{
	static char nresult[MAXBUF];
	char* t = nresult;
	*t++ = '*';	*t++ = '!';
	*t++ = '*';	*t++ = '@';
	for(char* n = dhost; *n; n++)
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


char* User::GetFullRealHost()
{
	if (this->cached_fullrealhost)
		return this->cached_fullrealhost;

	char fresult[MAXBUF];
	char* t = fresult;
	for(char* n = nick; *n; n++)
		*t++ = *n;
	*t++ = '!';
	for(char* n = ident; *n; n++)
		*t++ = *n;
	*t++ = '@';
	for(char* n = host; *n; n++)
		*t++ = *n;
	*t = 0;

	this->cached_fullrealhost = strdup(fresult);

	return this->cached_fullrealhost;
}

bool User::IsInvited(const irc::string &channel)
{
	for (InvitedList::iterator i = invites.begin(); i != invites.end(); i++)
	{
		if (channel == *i)
		{
			return true;
		}
	}
	return false;
}

InvitedList* User::GetInviteList()
{
	return &invites;
}

void User::InviteTo(const irc::string &channel)
{
	invites.push_back(channel);
}

void User::RemoveInvite(const irc::string &channel)
{
	for (InvitedList::iterator i = invites.begin(); i != invites.end(); i++)
	{
		if (channel == *i)
		{
			invites.erase(i);
			return;
	 	}
	}
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

/** NOTE: We cannot pass a const reference to this method.
 * The string is changed by the workings of the method,
 * so that if we pass const ref, we end up copying it to
 * something we can change anyway. Makes sense to just let
 * the compiler do that copy for us.
 */
bool User::AddBuffer(std::string a)
{
	try
	{
		std::string::size_type i = a.rfind('\r');

		while (i != std::string::npos)
		{
			a.erase(i, 1);
			i = a.rfind('\r');
		}

		if (a.length())
			recvq.append(a);

		if (this->MyClass && (recvq.length() > this->MyClass->GetRecvqMax()))
		{
			this->SetWriteError("RecvQ exceeded");
			ServerInstance->WriteOpers("*** User %s RecvQ of %d exceeds connect class maximum of %d",this->nick,recvq.length(),this->MyClass->GetRecvqMax());
			return false;
		}

		return true;
	}

	catch (...)
	{
		ServerInstance->Log(DEBUG,"Exception in User::AddBuffer()");
		return false;
	}
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
		ServerInstance->Log(DEBUG,"Exception in User::GetBuffer()");
		return "";
	}
}

void User::AddWriteBuf(const std::string &data)
{
	if (*this->GetWriteError())
		return;

	if (this->MyClass && (sendq.length() + data.length() > this->MyClass->GetSendqMax()))
	{
		/*
		 * Fix by brain - Set the error text BEFORE calling writeopers, because
		 * if we dont it'll recursively  call here over and over again trying
		 * to repeatedly add the text to the sendq!
		 */
		this->SetWriteError("SendQ exceeded");
		ServerInstance->WriteOpers("*** User %s SendQ of %d exceeds connect class maximum of %d",this->nick,sendq.length() + data.length(),this->MyClass->GetSendqMax());
		return;
	}

	try
	{
		if (data.length() > MAXBUF - 2) /* MAXBUF has a value of 514, to account for line terminators */
			sendq.append(data.substr(0,MAXBUF - 4)).append("\r\n"); /* MAXBUF-4 = 510 */
		else
			sendq.append(data);
	}
	catch (...)
	{
		this->SetWriteError("SendQ exceeded");
		ServerInstance->WriteOpers("*** User %s SendQ got an exception",this->nick);
	}
}

// send AS MUCH OF THE USERS SENDQ as we are able to (might not be all of it)
void User::FlushWriteBuf()
{
	try
	{
		if ((this->fd == FD_MAGIC_NUMBER) || (*this->GetWriteError()))
		{
			sendq.clear();
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
					this->ServerInstance->SE->WantWrite(this);
				}
				else
				{
					/* Fatal error, set write error and bail
					 */
					this->SetWriteError(errno ? strerror(errno) : "EOF from client");
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
					this->ServerInstance->SE->WantWrite(this);
			}
		}
	}

	catch (...)
	{
		ServerInstance->Log(DEBUG,"Exception in User::FlushWriteBuf()");
	}

	if (this->sendq.empty())
	{
		FOREACH_MOD(I_OnBufferFlushed,OnBufferFlushed(this));
	}
}

void User::SetWriteError(const std::string &error)
{
	try
	{
		// don't try to set the error twice, its already set take the first string.
		if (this->WriteError.empty())
			this->WriteError = error;
	}

	catch (...)
	{
		ServerInstance->Log(DEBUG,"Exception in User::SetWriteError()");
	}
}

const char* User::GetWriteError()
{
	return this->WriteError.c_str();
}

void User::Oper(const std::string &opertype)
{
	char* mycmd;
	char* savept;
	char* savept2;

	try
	{
		this->modes[UM_OPERATOR] = 1;
		this->WriteServ("MODE %s :+o", this->nick);
		FOREACH_MOD(I_OnOper, OnOper(this, opertype));
		ServerInstance->Log(DEFAULT,"OPER: %s!%s@%s opered as type: %s", this->nick, this->ident, this->host, opertype.c_str());
		strlcpy(this->oper, opertype.c_str(), NICKMAX - 1);
		ServerInstance->all_opers.push_back(this);

		opertype_t::iterator iter_opertype = ServerInstance->Config->opertypes.find(this->oper);
		if (iter_opertype != ServerInstance->Config->opertypes.end())
		{

			if (AllowedOperCommands)
				AllowedOperCommands->clear();
			else
				AllowedOperCommands = new std::map<std::string, bool>;

			char* Classes = strdup(iter_opertype->second);
			char* myclass = strtok_r(Classes," ",&savept);
			while (myclass)
			{
				operclass_t::iterator iter_operclass = ServerInstance->Config->operclass.find(myclass);
				if (iter_operclass != ServerInstance->Config->operclass.end())
				{
					char* CommandList = strdup(iter_operclass->second);
					mycmd = strtok_r(CommandList," ",&savept2);
					while (mycmd)
					{
						this->AllowedOperCommands->insert(std::make_pair(mycmd, true));
						mycmd = strtok_r(NULL," ",&savept2);
					}
					free(CommandList);
				}
				myclass = strtok_r(NULL," ",&savept);
			}
			free(Classes);
		}

		FOREACH_MOD(I_OnPostOper,OnPostOper(this, opertype));
	}

	catch (...)
	{
		ServerInstance->Log(DEBUG,"Exception in User::Oper()");
	}
}

void User::UnOper()
{
	try
	{
		if (IS_OPER(this))
		{
			// unset their oper type (what IS_OPER checks), and remove +o
			*this->oper = 0;
			this->modes[UM_OPERATOR] = 0;
			
			// remove the user from the oper list. Will remove multiple entries as a safeguard against bug #404
			ServerInstance->all_opers.remove(this);

			if (AllowedOperCommands)
			{
				delete AllowedOperCommands;
				AllowedOperCommands = NULL;
			}
		}
	}

	catch (...)
	{
		ServerInstance->Log(DEBUG,"Exception in User::UnOper()");
	}
}

void User::QuitUser(InspIRCd* Instance, User *user, const std::string &quitreason, const char* operreason)
{
	Instance->Log(DEBUG,"QuitUser: %s '%s'", user->nick, quitreason.c_str());
	user->Write("ERROR :Closing link (%s@%s) [%s]", user->ident, user->host, *operreason ? operreason : quitreason.c_str());
	user->muted = true;
	Instance->GlobalCulls.AddItem(user, quitreason.c_str(), operreason);
}

/* adds or updates an entry in the whowas list */
void User::AddToWhoWas()
{
	Command* whowas_command = ServerInstance->Parser->GetHandler("WHOWAS");
	if (whowas_command)
	{
		std::deque<classbase*> params;
		params.push_back(this);
		whowas_command->HandleInternal(WHOWAS_ADD, params);
	}
}

/* add a client connection to the sockets list */
void User::AddClient(InspIRCd* Instance, int socket, int port, bool iscached, int socketfamily, sockaddr* ip)
{
	/* NOTE: Calling this one parameter constructor for User automatically
	 * allocates a new UUID and places it in the hash_map.
	 */
	User* New = NULL;
	try
	{
		New = new User(Instance);
	}
	catch (...)
	{
		Instance->Log(DEFAULT,"*** WTF *** Duplicated UUID! -- Crack smoking monkies have been unleashed.");
		Instance->WriteOpers("*** WARNING *** Duplicate UUID allocated!");
		return;
	}

	Instance->Log(DEBUG,"New user fd: %d", socket);

	int j = 0;

	Instance->unregistered_count++;

	char ipaddr[MAXBUF];
#ifdef IPV6
	if (socketfamily == AF_INET6)
		inet_ntop(AF_INET6, &((const sockaddr_in6*)ip)->sin6_addr, ipaddr, sizeof(ipaddr));
	else
#endif
	inet_ntop(AF_INET, &((const sockaddr_in*)ip)->sin_addr, ipaddr, sizeof(ipaddr));

	(*(Instance->clientlist))[New->uuid] = New;
	New->SetFd(socket);

	/* The users default nick is their UUID */
	strlcpy(New->nick, New->uuid, NICKMAX - 1);

	New->server = Instance->FindServerNamePtr(Instance->Config->ServerName);
	/* We don't need range checking here, we KNOW 'unknown\0' will fit into the ident field. */
	strcpy(New->ident, "unknown");

	New->registered = REG_NONE;
	New->signon = Instance->Time() + Instance->Config->dns_timeout;
	New->lastping = 1;

	New->SetSockAddr(socketfamily, ipaddr, port);

	/* Smarter than your average bear^H^H^H^Hset of strlcpys. */
	for (const char* temp = New->GetIPString(); *temp && j < 64; temp++, j++)
		New->dhost[j] = New->host[j] = *temp;
	New->dhost[j] = New->host[j] = 0;

	Instance->AddLocalClone(New);
	Instance->AddGlobalClone(New);

	/*
	 * First class check. We do this again in FullConnect after DNS is done, and NICK/USER is recieved.
	 * See my note down there for why this is required. DO NOT REMOVE. :) -- w00t
	 */
	ConnectClass* i = New->SetClass();

	if (!i)
	{
		User::QuitUser(Instance, New, "Access denied by configuration");
		return;
	}

	/*
	 * Check connect class settings and initialise settings into User.
	 * This will be done again after DNS resolution. -- w00t
	 */
	New->CheckClass();

	Instance->local_users.push_back(New);

	if ((Instance->local_users.size() > Instance->Config->SoftLimit) || (Instance->local_users.size() >= MAXCLIENTS))
	{
		Instance->WriteOpers("*** Warning: softlimit value has been reached: %d clients", Instance->Config->SoftLimit);
		User::QuitUser(Instance, New,"No more connections allowed");
		return;
	}

	/*
	 * XXX -
	 * this is done as a safety check to keep the file descriptors within range of fd_ref_table.
	 * its a pretty big but for the moment valid assumption:
	 * file descriptors are handed out starting at 0, and are recycled as theyre freed.
	 * therefore if there is ever an fd over 65535, 65536 clients must be connected to the
	 * irc server at once (or the irc server otherwise initiating this many connections, files etc)
	 * which for the time being is a physical impossibility (even the largest networks dont have more
	 * than about 10,000 users on ONE server!)
	 */
#ifndef WINDOWS
	if ((unsigned int)socket >= MAX_DESCRIPTORS)
	{
		User::QuitUser(Instance, New, "Server is full");
		return;
	}
#endif
	/*
	 * even with bancache, we still have to keep User::exempt current.
	 * besides that, if we get a positive bancache hit, we still won't fuck
	 * them over if they are exempt. -- w00t
	 */
	New->exempt = (Instance->XLines->MatchesLine("E",New) != NULL);

	if (BanCacheHit *b = Instance->BanCache->GetHit(New->GetIPString()))
	{
		if (!b->Type.empty() && !New->exempt)
		{
			/* user banned */
			Instance->Log(DEBUG, std::string("BanCache: Positive hit for ") + New->GetIPString());
			if (*Instance->Config->MoronBanner)
				New->WriteServ("NOTICE %s :*** %s", New->nick, Instance->Config->MoronBanner);
			User::QuitUser(Instance, New, b->Reason);
			return;
		}
		else
		{
			Instance->Log(DEBUG, std::string("BanCache: Negative hit for ") + New->GetIPString());
		}
	}
	else
	{
		if (!New->exempt)
		{
			XLine* r = Instance->XLines->MatchesLine("Z",New);

			if (r)
			{
				r->Apply(New);
				return;
			}
		}
	}

        if (socket > -1)
        {
                if (!Instance->SE->AddFd(New))
                {
			Instance->Log(DEBUG,"Internal error on new connection");
			User::QuitUser(Instance, New, "Internal error handling connection");
                }
        }

	/* NOTE: even if dns lookups are *off*, we still need to display this.
	 * BOPM and other stuff requires it.
	 */
	New->WriteServ("NOTICE Auth :*** Looking up your hostname...");

	if (Instance->Config->NoUserDns)
	{
		New->dns_done = true;
	}
	else
	{
		New->StartDNSLookup();
	}
}

unsigned long User::GlobalCloneCount()
{
	clonemap::iterator x = ServerInstance->global_clones.find(this->GetIPString());
	if (x != ServerInstance->global_clones.end())
		return x->second;
	else
		return 0;
}

unsigned long User::LocalCloneCount()
{
	clonemap::iterator x = ServerInstance->local_clones.find(this->GetIPString());
	if (x != ServerInstance->local_clones.end())
		return x->second;
	else
		return 0;
}

/*
 * Check class restrictions
 */
void User::CheckClass()
{
	ConnectClass* a = this->MyClass;

	if ((!a) || (a->GetType() == CC_DENY))
	{
		User::QuitUser(ServerInstance, this, "Unauthorised connection");
		return;
	}
	else if ((a->GetMaxLocal()) && (this->LocalCloneCount() > a->GetMaxLocal()))
	{
		User::QuitUser(ServerInstance, this, "No more connections allowed from your host via this connect class (local)");
		ServerInstance->WriteOpers("*** WARNING: maximum LOCAL connections (%ld) exceeded for IP %s", a->GetMaxLocal(), this->GetIPString());
		return;
	}
	else if ((a->GetMaxGlobal()) && (this->GlobalCloneCount() > a->GetMaxGlobal()))
	{
		User::QuitUser(ServerInstance, this, "No more connections allowed from your host via this connect class (global)");
		ServerInstance->WriteOpers("*** WARNING: maximum GLOBAL connections (%ld) exceeded for IP %s", a->GetMaxGlobal(), this->GetIPString());
		return;
	}

	this->nping = ServerInstance->Time() + a->GetPingTime() + ServerInstance->Config->dns_timeout;
	this->timeout = ServerInstance->Time() + a->GetRegTimeout();
	this->MaxChans = a->GetMaxChans();
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
		User::QuitUser(ServerInstance, this, "Invalid password");
		return;
	}

	if (!this->exempt)
	{
		GLine *r = (GLine *)ServerInstance->XLines->MatchesLine("G", this);

		if (r)
		{
			this->muted = true;
			r->Apply(this);
			return;
		}

		KLine *n = (KLine *)ServerInstance->XLines->MatchesLine("K", this);

		if (n)
		{
			this->muted = true;
			n->Apply(this);
			return;
		}
	}

	this->WriteServ("NOTICE Auth :Welcome to \002%s\002!",ServerInstance->Config->Network);
	this->WriteServ("001 %s :Welcome to the %s IRC Network %s!%s@%s",this->nick, ServerInstance->Config->Network, this->nick, this->ident, this->host);
	this->WriteServ("002 %s :Your host is %s, running version %s",this->nick,ServerInstance->Config->ServerName,VERSION);
	this->WriteServ("003 %s :This server was created %s %s", this->nick, __TIME__, __DATE__);
	this->WriteServ("004 %s %s %s %s %s %s", this->nick, ServerInstance->Config->ServerName, VERSION, ServerInstance->Modes->UserModeList().c_str(), ServerInstance->Modes->ChannelModeList().c_str(), ServerInstance->Modes->ParaModeList().c_str());

	ServerInstance->Config->Send005(this);

	this->WriteServ("042 %s %s :your unique ID", this->nick, this->uuid);


	this->ShowMOTD();

	/* Now registered */
	if (ServerInstance->unregistered_count)
		ServerInstance->unregistered_count--;

	/* Trigger LUSERS output, give modules a chance too */
	int MOD_RESULT = 0;
	FOREACH_RESULT(I_OnPreCommand, OnPreCommand("LUSERS", NULL, 0, this, true, "LUSERS"));
	if (!MOD_RESULT)
		ServerInstance->CallCommandHandler("LUSERS", NULL, 0, this);

	/*
	 * We don't set REG_ALL until triggering OnUserConnect, so some module events don't spew out stuff
	 * for a user that doesn't exist yet.
	 */
	FOREACH_MOD(I_OnUserConnect,OnUserConnect(this));

	this->registered = REG_ALL;

	FOREACH_MOD(I_OnPostConnect,OnPostConnect(this));

	ServerInstance->SNO->WriteToSnoMask('c',"Client connecting on port %d: %s!%s@%s [%s] [%s]", this->GetPort(), this->nick, this->ident, this->host, this->GetIPString(), this->fullname);

	ServerInstance->Log(DEBUG, "BanCache: Adding NEGATIVE hit for %s", this->GetIPString());
	ServerInstance->BanCache->AddHit(this->GetIPString(), "", "");
}

/** User::UpdateNick()
 * re-allocates a nick in the user_hash after they change nicknames,
 * returns a pointer to the new user as it may have moved
 */
User* User::UpdateNickHash(const char* New)
{
	try
	{
		//user_hash::iterator newnick;
		user_hash::iterator oldnick = ServerInstance->clientlist->find(this->nick);

		if (!strcasecmp(this->nick,New))
			return oldnick->second;

		if (oldnick == ServerInstance->clientlist->end())
			return NULL; /* doesnt exist */

		User* olduser = oldnick->second;
		(*(ServerInstance->clientlist))[New] = olduser;
		ServerInstance->clientlist->erase(oldnick);
		return olduser;
	}

	catch (...)
	{
		ServerInstance->Log(DEBUG,"Exception in User::UpdateNickHash()");
		return NULL;
	}
}

void User::InvalidateCache()
{
	/* Invalidate cache */
	if (cached_fullhost)
		free(cached_fullhost);
	if (cached_hostip)
		free(cached_hostip);
	if (cached_makehost)
		free(cached_makehost);
	if (cached_fullrealhost)
		free(cached_fullrealhost);
	cached_fullhost = cached_hostip = cached_makehost = cached_fullrealhost = NULL;
}

bool User::ForceNickChange(const char* newnick)
{
	try
	{
		int MOD_RESULT = 0;

		this->InvalidateCache();

		FOREACH_RESULT(I_OnUserPreNick,OnUserPreNick(this, newnick));

		if (MOD_RESULT)
		{
			ServerInstance->stats->statsCollisions++;
			return false;
		}

		if (ServerInstance->XLines->MatchesLine("Q",newnick))
		{
			ServerInstance->stats->statsCollisions++;
			return false;
		}

		if (this->registered == REG_ALL)
		{
			std::deque<classbase*> dummy;
			Command* nickhandler = ServerInstance->Parser->GetHandler("NICK");
			if (nickhandler)
			{
				nickhandler->HandleInternal(1, dummy);
				bool result = (ServerInstance->Parser->CallHandler("NICK", &newnick, 1, this) == CMD_SUCCESS);
				nickhandler->HandleInternal(0, dummy);
				return result;
			}
		}
		return false;
	}

	catch (...)
	{
		ServerInstance->Log(DEBUG,"Exception in User::ForceNickChange()");
		return false;
	}
}

void User::SetSockAddr(int protocol_family, const char* ip, int port)
{
	switch (protocol_family)
	{
#ifdef SUPPORT_IP6LINKS
		case AF_INET6:
		{
			sockaddr_in6* sin = new sockaddr_in6;
			sin->sin6_family = AF_INET6;
			sin->sin6_port = port;
			inet_pton(AF_INET6, ip, &sin->sin6_addr);
			this->ip = (sockaddr*)sin;
		}
		break;
#endif
		case AF_INET:
		{
			sockaddr_in* sin = new sockaddr_in;
			sin->sin_family = AF_INET;
			sin->sin_port = port;
			inet_pton(AF_INET, ip, &sin->sin_addr);
			this->ip = (sockaddr*)sin;
		}
		break;
		default:
			ServerInstance->Log(DEBUG,"Uh oh, I dont know protocol %d to be set on '%s'!", protocol_family, this->nick);
		break;
	}
}

int User::GetPort()
{
	if (this->ip == NULL)
		return 0;

	switch (this->GetProtocolFamily())
	{
#ifdef SUPPORT_IP6LINKS
		case AF_INET6:
		{
			sockaddr_in6* sin = (sockaddr_in6*)this->ip;
			return sin->sin6_port;
		}
		break;
#endif
		case AF_INET:
		{
			sockaddr_in* sin = (sockaddr_in*)this->ip;
			return sin->sin_port;
		}
		break;
		default:
		break;
	}
	return 0;
}

int User::GetProtocolFamily()
{
	if (this->ip == NULL)
		return 0;

	sockaddr_in* sin = (sockaddr_in*)this->ip;
	return sin->sin_family;
}

/*
 * XXX the duplication here is horrid..
 * do we really need two methods doing essentially the same thing?
 */
const char* User::GetIPString()
{
	static char buf[1024];

	if (this->ip == NULL)
		return "";

	switch (this->GetProtocolFamily())
	{
#ifdef SUPPORT_IP6LINKS
		case AF_INET6:
		{
			static char temp[1024];

			sockaddr_in6* sin = (sockaddr_in6*)this->ip;
			inet_ntop(sin->sin6_family, &sin->sin6_addr, buf, sizeof(buf));
			/* IP addresses starting with a : on irc are a Bad Thing (tm) */
			if (*buf == ':')
			{
				strlcpy(&temp[1], buf, sizeof(temp) - 1);
				*temp = '0';
				return temp;
			}
			return buf;
		}
		break;
#endif
		case AF_INET:
		{
			sockaddr_in* sin = (sockaddr_in*)this->ip;
			inet_ntop(sin->sin_family, &sin->sin_addr, buf, sizeof(buf));
			return buf;
		}
		break;
		default:
		break;
	}
	return "";
}

/** NOTE: We cannot pass a const reference to this method.
 * The string is changed by the workings of the method,
 * so that if we pass const ref, we end up copying it to
 * something we can change anyway. Makes sense to just let
 * the compiler do that copy for us.
 */
void User::Write(std::string text)
{
	if (!ServerInstance->SE->BoundsCheckFd(this))
		return;

	try
	{
		/* ServerInstance->Log(DEBUG,"C[%d] O %s", this->GetFd(), text.c_str());
		 * WARNING: The above debug line is VERY loud, do NOT
		 * enable it till we have a good way of filtering it
		 * out of the logs (e.g. 1.2 would be good).
		 */
		text.append("\r\n");
	}
	catch (...)
	{
		ServerInstance->Log(DEBUG,"Exception in User::Write() std::string::append");
		return;
	}

	if (ServerInstance->Config->GetIOHook(this->GetPort()))
	{
		try
		{
			/* XXX: The lack of buffering here is NOT a bug, modules implementing this interface have to
			 * implement their own buffering mechanisms
			 */
			ServerInstance->Config->GetIOHook(this->GetPort())->OnRawSocketWrite(this->fd, text.data(), text.length());
		}
		catch (CoreException& modexcept)
		{
			ServerInstance->Log(DEBUG, "%s threw an exception: %s", modexcept.GetSource(), modexcept.GetReason());
		}
	}
	else
	{
		this->AddWriteBuf(text);
	}
	ServerInstance->stats->statsSent += text.length();
	this->ServerInstance->SE->WantWrite(this);
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
	char textbuffer[MAXBUF];

	snprintf(textbuffer,MAXBUF,":%s %s",ServerInstance->Config->ServerName,text.c_str());
	this->Write(std::string(textbuffer));
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


void User::WriteFrom(User *user, const std::string &text)
{
	char tb[MAXBUF];

	snprintf(tb,MAXBUF,":%s %s",user->GetFullHost(),text.c_str());

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
	try
	{
		bool sent_to_at_least_one = false;
		char tb[MAXBUF];

		if (this->registered != REG_ALL)
			return;

		uniq_id++;

		/* We dont want to be doing this n times, just once */
		snprintf(tb,MAXBUF,":%s %s",this->GetFullHost(),text.c_str());
		std::string out = tb;

		for (UCListIter v = this->chans.begin(); v != this->chans.end(); v++)
		{
			CUList* ulist = v->first->GetUsers();
			for (CUList::iterator i = ulist->begin(); i != ulist->end(); i++)
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

	catch (...)
	{
		ServerInstance->Log(DEBUG,"Exception in User::WriteCommon()");
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
	snprintf(tb1,MAXBUF,":%s QUIT :%s",this->GetFullHost(),normal_text.c_str());
	snprintf(tb2,MAXBUF,":%s QUIT :%s",this->GetFullHost(),oper_text.c_str());
	std::string out1 = tb1;
	std::string out2 = tb2;

	for (UCListIter v = this->chans.begin(); v != this->chans.end(); v++)
	{
		CUList *ulist = v->first->GetUsers();
		for (CUList::iterator i = ulist->begin(); i != ulist->end(); i++)
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
	snprintf(tb1,MAXBUF,":%s %s",this->GetFullHost(),text.c_str());
	out1 = tb1;

	for (UCListIter v = this->chans.begin(); v != this->chans.end(); v++)
	{
		CUList *ulist = v->first->GetUsers();
		for (CUList::iterator i = ulist->begin(); i != ulist->end(); i++)
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
	if (!IS_OPER(this) && IS_LOCAL(this))
		return;

	std::string wallop("WALLOPS :");
	wallop.append(text);

	for (std::vector<User*>::const_iterator i = ServerInstance->local_users.begin(); i != ServerInstance->local_users.end(); i++)
	{
		User* t = *i;
		if (t->IsModeSet('w'))
			this->WriteTo(t,wallop);
	}
}

void User::WriteWallOps(const char* text, ...)
{
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
		if (i->first->HasUser(other))
			return true;
	}
	return false;
}

bool User::ChangeName(const char* gecos)
{
	if (!strcmp(gecos, this->fullname))
		return true;

	if (IS_LOCAL(this))
	{
		int MOD_RESULT = 0;
		FOREACH_RESULT(I_OnChangeLocalUserGECOS,OnChangeLocalUserGECOS(this,gecos));
		if (MOD_RESULT)
			return false;
		FOREACH_MOD(I_OnChangeName,OnChangeName(this,gecos));
	}
	strlcpy(this->fullname,gecos,MAXGECOS+1);

	return true;
}

bool User::ChangeDisplayedHost(const char* host)
{
	if (!strcmp(host, this->dhost))
		return true;

	if (IS_LOCAL(this))
	{
		int MOD_RESULT = 0;
		FOREACH_RESULT(I_OnChangeLocalUserHost,OnChangeLocalUserHost(this,host));
		if (MOD_RESULT)
			return false;
		FOREACH_MOD(I_OnChangeHost,OnChangeHost(this,host));
	}
	if (this->ServerInstance->Config->CycleHosts)
		this->WriteCommonExcept("QUIT :Changing hosts");

	/* Fix by Om: User::dhost is 65 long, this was truncating some long hosts */
	strlcpy(this->dhost,host,64);

	this->InvalidateCache();

	if (this->ServerInstance->Config->CycleHosts)
	{
		for (UCListIter i = this->chans.begin(); i != this->chans.end(); i++)
		{
			i->first->WriteAllExceptSender(this, false, 0, "JOIN %s", i->first->name);
			std::string n = this->ServerInstance->Modes->ModeString(this, i->first);
			if (n.length() > 0)
				i->first->WriteAllExceptSender(this, true, 0, "MODE %s +%s", i->first->name, n.c_str());
		}
	}

	if (IS_LOCAL(this))
		this->WriteServ("396 %s %s :is now your displayed host",this->nick,this->dhost);

	return true;
}

bool User::ChangeIdent(const char* newident)
{
	if (!strcmp(newident, this->ident))
		return true;

	if (this->ServerInstance->Config->CycleHosts)
		this->WriteCommonExcept("%s","QUIT :Changing ident");

	strlcpy(this->ident, newident, IDENTMAX+2);

	this->InvalidateCache();

	if (this->ServerInstance->Config->CycleHosts)
	{
		for (UCListIter i = this->chans.begin(); i != this->chans.end(); i++)
		{
			i->first->WriteAllExceptSender(this, false, 0, "JOIN %s", i->first->name);
			std::string n = this->ServerInstance->Modes->ModeString(this, i->first);
			if (n.length() > 0)
				i->first->WriteAllExceptSender(this, true, 0, "MODE %s +%s", i->first->name, n.c_str());
		}
	}

	return true;
}

void User::SendAll(const char* command, char* text, ...)
{
	char textbuffer[MAXBUF];
	char formatbuffer[MAXBUF];
	va_list argsPtr;

	va_start(argsPtr, text);
	vsnprintf(textbuffer, MAXBUF, text, argsPtr);
	va_end(argsPtr);

	snprintf(formatbuffer,MAXBUF,":%s %s $* :%s", this->GetFullHost(), command, textbuffer);
	std::string fmt = formatbuffer;

	for (std::vector<User*>::const_iterator i = ServerInstance->local_users.begin(); i != ServerInstance->local_users.end(); i++)
	{
		(*i)->Write(fmt);
	}
}


std::string User::ChannelList(User* source)
{
	try
	{
		std::string list;
		for (UCListIter i = this->chans.begin(); i != this->chans.end(); i++)
		{
			/* If the target is the same as the sender, let them see all their channels.
			 * If the channel is NOT private/secret OR the user shares a common channel
			 * If the user is an oper, and the <options:operspywhois> option is set.
			 */
			if ((source == this) || (IS_OPER(source) && ServerInstance->Config->OperSpyWhois) || (((!i->first->IsModeSet('p')) && (!i->first->IsModeSet('s'))) || (i->first->HasUser(source))))
			{
				list.append(i->first->GetPrefixChar(this)).append(i->first->name).append(" ");
			}
		}
		return list;
	}
	catch (...)
	{
		ServerInstance->Log(DEBUG,"Exception in User::ChannelList()");
		return "";
	}
}

void User::SplitChanList(User* dest, const std::string &cl)
{
	std::string line;
	std::ostringstream prefix;
	std::string::size_type start, pos, length;

	try
	{
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

	catch (...)
	{
		ServerInstance->Log(DEBUG,"Exception in User::SplitChanList()");
	}
}

unsigned int User::GetMaxChans()
{
	return this->MaxChans;
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

	if (!explicit_name.empty())
	{
		for (ClassVector::iterator i = ServerInstance->Config->Classes.begin(); i != ServerInstance->Config->Classes.end(); i++)
		{
			ConnectClass* c = *i;

			if (explicit_name == c->GetName() && !c->GetDisabled())
			{
				found = c;
			}
		}
	}
	else
	{
		for (ClassVector::iterator i = ServerInstance->Config->Classes.begin(); i != ServerInstance->Config->Classes.end(); i++)
		{
			ConnectClass* c = *i;

			if (((match(this->GetIPString(),c->GetHost().c_str(),true)) || (match(this->host,c->GetHost().c_str()))))
			{
				if (c->GetPort())
				{
					if (this->GetPort() == c->GetPort() && !c->GetDisabled())
					{
						found = c;
					}
					else
						continue;
				}
				else
				{
					if (!c->GetDisabled())
						found = c;
				}
			}
		}
	}

	/* ensure we don't fuck things up refcount wise, only remove them from a class if we find a new one :P */
	if (found)
	{
		/* deny change if change will take class over the limit */
		if (found->limit && (found->RefCount + 1 >= found->limit))
		{
			ServerInstance->Log(DEBUG, "OOPS: Connect class limit (%u) hit, denying", found->limit);
			return this->MyClass;
		}

		/* should always be valid, but just in case .. */
		if (this->MyClass)
		{
			if (found == this->MyClass) // no point changing this shit :P
				return this->MyClass;
			this->MyClass->RefCount--;
			ServerInstance->Log(DEBUG, "Untying user from connect class -- refcount: %u", this->MyClass->RefCount);
		}

		this->MyClass = found;
		this->MyClass->RefCount++;
		ServerInstance->Log(DEBUG, "User tied to new class -- connect refcount now: %u", this->MyClass->RefCount);
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
		f->first->RemoveAllPrefixes(this);
		if (f->first->DelUser(this) == 0)
		{
			/* No users left in here, mark it for deletion */
			try
			{
				to_delete.push_back(f->first);
			}
			catch (...)
			{
				ServerInstance->Log(DEBUG,"Exception in User::PurgeEmptyChannels to_delete.push_back()");
			}
		}
	}

	for (std::vector<Channel*>::iterator n = to_delete.begin(); n != to_delete.end(); n++)
	{
		Channel* thischan = *n;
		chan_hash::iterator i2 = ServerInstance->chanlist->find(thischan->name);
		if (i2 != ServerInstance->chanlist->end())
		{
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
		this->WriteServ("422 %s :Message of the day file is missing.",this->nick);
		return;
	}
	this->WriteServ("375 %s :%s message of the day", this->nick, ServerInstance->Config->ServerName);

	for (file_cache::iterator i = ServerInstance->Config->MOTD.begin(); i != ServerInstance->Config->MOTD.end(); i++)
		this->WriteServ("372 %s :- %s",this->nick,i->c_str());

	this->WriteServ("376 %s :End of message of the day.", this->nick);
}

void User::ShowRULES()
{
	if (!ServerInstance->Config->RULES.size())
	{
		this->WriteServ("434 %s :RULES File is missing",this->nick);
		return;
	}

	this->WriteServ("308 %s :- %s Server Rules -",this->nick,ServerInstance->Config->ServerName);

	for (file_cache::iterator i = ServerInstance->Config->RULES.begin(); i != ServerInstance->Config->RULES.end(); i++)
		this->WriteServ("232 %s :- %s",this->nick,i->c_str());

	this->WriteServ("309 %s :End of RULES command.",this->nick);
}

void User::HandleEvent(EventType et, int errornum)
{
	/* WARNING: May delete this user! */
	int thisfd = this->GetFd();

	try
	{
		switch (et)
		{
			case EVENT_READ:
				ServerInstance->ProcessUser(this);
			break;
			case EVENT_WRITE:
				this->FlushWriteBuf();
			break;
			case EVENT_ERROR:
				/** This should be safe, but dont DARE do anything after it -- Brain */
				this->SetWriteError(errornum ? strerror(errornum) : "EOF from client");
			break;
		}
	}
	catch (...)
	{
		ServerInstance->Log(DEBUG,"Exception in User::HandleEvent intercepted");
	}

	/* If the user has raised an error whilst being processed, quit them now we're safe to */
	if ((ServerInstance->SE->GetRef(thisfd) == this))
	{
		if (!WriteError.empty())
		{
			User::QuitUser(ServerInstance, this, GetWriteError());
		}
	}
}

void User::SetOperQuit(const std::string &oquit)
{
	if (operquit)
		return;

	operquit = strdup(oquit.c_str());
}

const char* User::GetOperQuit()
{
	return operquit ? operquit : "";
}

void User::IncreasePenalty(int increase)
{
	this->Penalty += increase;
}

void User::DecreasePenalty(int decrease)
{
	this->Penalty -= decrease;
}

VisData::VisData()
{
}

VisData::~VisData()
{
}

bool VisData::VisibleTo(User* user)
{
	return true;
}

