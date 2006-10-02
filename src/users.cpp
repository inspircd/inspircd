/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd is copyright (C) 2002-2006 ChatSpike-Dev.
 *		       E-mail:
 *		<brain@chatspike.net>
 *	   	  <Craig@chatspike.net>
 *     
 * Written by Craig Edwards, Craig McLure, and others.
 * This program is free but copyrighted software; see
 *	    the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "configreader.h"
#include "channels.h"
#include "users.h"
#include "inspircd.h"
#include <stdarg.h>
#include "socketengine.h"
#include "wildcard.h"
#include "xline.h"
#include "cull_list.h"

irc::whowas::whowas_users whowas;
static unsigned long already_sent[MAX_DESCRIPTORS] = {0};

typedef std::map<irc::string,char*> opertype_t;
typedef opertype_t operclass_t;

opertype_t opertypes;
operclass_t operclass;

/* XXX: Used for speeding up WriteCommon operations */
unsigned long uniq_id = 0;

bool InitTypes(ServerConfig* conf, const char* tag)
{
	for (opertype_t::iterator n = opertypes.begin(); n != opertypes.end(); n++)
	{
		if (n->second)
			delete[] n->second;
	}
	
	opertypes.clear();
	return true;
}

bool InitClasses(ServerConfig* conf, const char* tag)
{
	for (operclass_t::iterator n = operclass.begin(); n != operclass.end(); n++)
	{
		if (n->second)
			delete[] n->second;
	}
	
	operclass.clear();
	return true;
}

bool DoType(ServerConfig* conf, const char* tag, char** entries, void** values, int* types)
{
	char* TypeName = (char*)values[0];
	char* Classes = (char*)values[1];
	
	opertypes[TypeName] = strdup(Classes);
	conf->GetInstance()->Log(DEBUG,"Read oper TYPE '%s' with classes '%s'",TypeName,Classes);
	return true;
}

bool DoClass(ServerConfig* conf, const char* tag, char** entries, void** values, int* types)
{
	char* ClassName = (char*)values[0];
	char* CommandList = (char*)values[1];
	
	operclass[ClassName] = strdup(CommandList);
	conf->GetInstance()->Log(DEBUG,"Read oper CLASS '%s' with commands '%s'",ClassName,CommandList);
	return true;
}

bool DoneClassesAndTypes(ServerConfig* conf, const char* tag)
{
	return true;
}

std::string userrec::ProcessNoticeMasks(const char *sm)
{
	bool adding = true, oldadding = false;
	const char *c = sm;
	std::string output;

	ServerInstance->Log(DEBUG,"Process notice masks");

	while (c && *c)
	{
		ServerInstance->Log(DEBUG,"Process notice mask %c",*c);
		
		switch (*c)
		{
			case '+':
				adding = true;
			break;
			case '-':
				adding = false;
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

void userrec::StartDNSLookup()
{
	ServerInstance->Log(DEBUG,"Commencing reverse lookup");
	try
	{
		ServerInstance->Log(DEBUG,"Passing instance: %08x",this->ServerInstance);
		res_reverse = new UserResolver(this->ServerInstance, this, this->GetIPString(), false);
		this->ServerInstance->AddResolver(res_reverse);
	}
	catch (ModuleException& e)
	{
		ServerInstance->Log(DEBUG,"Error in resolver: %s",e.GetReason());
	}
}

UserResolver::UserResolver(InspIRCd* Instance, userrec* user, std::string to_resolve, bool forward) :
	Resolver(Instance, to_resolve, forward ? DNS_QUERY_FORWARD : DNS_QUERY_REVERSE), bound_user(user)
{
	this->fwd = forward;
	this->bound_fd = user->GetFd();
}

void UserResolver::OnLookupComplete(const std::string &result)
{
	if ((!this->fwd) && (ServerInstance->SE->GetRef(this->bound_fd) == this->bound_user))
	{
		ServerInstance->Log(DEBUG,"Commencing forward lookup");
		this->bound_user->stored_host = result;
		try
		{
			/* Check we didnt time out */
			if (this->bound_user->registered != REG_ALL)
			{
				bound_user->res_forward = new UserResolver(this->ServerInstance, this->bound_user, result, true);
				this->ServerInstance->AddResolver(bound_user->res_forward);
			}
		}
		catch (ModuleException& e)
		{
			ServerInstance->Log(DEBUG,"Error in resolver: %s",e.GetReason());
		}
	}
	else if ((this->fwd) && (ServerInstance->SE->GetRef(this->bound_fd) == this->bound_user))
	{
		/* Both lookups completed */
		if (this->bound_user->GetIPString() == result)
		{
			std::string hostname = this->bound_user->stored_host;
			if (hostname.length() < 65)
			{
				/* Check we didnt time out */
				if (this->bound_user->registered != REG_ALL)
				{
					/* Hostnames starting with : are not a good thing (tm) */
					if (*(hostname.c_str()) == ':')
						hostname = "0" + hostname;

					this->bound_user->WriteServ("NOTICE Auth :*** Found your hostname (%s)", hostname.c_str());
					this->bound_user->dns_done = true;
					strlcpy(this->bound_user->dhost, hostname.c_str(),64);
					strlcpy(this->bound_user->host, hostname.c_str(),64);
				}
			}
			else
			{
				this->bound_user->WriteServ("NOTICE Auth :*** Your hostname is longer than the maximum of 64 characters, using your IP address (%s) instead.", this->bound_user->GetIPString());
			}
		}
		else
		{
			this->bound_user->WriteServ("NOTICE Auth :*** Your hostname does not match up with your IP address. Sorry, using your IP address (%s) instead.", this->bound_user->GetIPString());
		}
	}
}

void UserResolver::OnError(ResolverError e, const std::string &errormessage)
{
	if (ServerInstance->SE->GetRef(this->bound_fd) == this->bound_user)
	{
		/* Error message here */
		this->bound_user->WriteServ("NOTICE Auth :*** Could not resolve your hostname, using your IP address (%s) instead.", this->bound_user->GetIPString());
		this->bound_user->dns_done = true;
	}
}


bool userrec::IsNoticeMaskSet(unsigned char sm)
{
	return (snomasks[sm-65]);
}

void userrec::SetNoticeMask(unsigned char sm, bool value)
{
	snomasks[sm-65] = value;
}

const char* userrec::FormatNoticeMasks()
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



bool userrec::IsModeSet(unsigned char m)
{
	return (modes[m-65]);
}

void userrec::SetMode(unsigned char m, bool value)
{
	modes[m-65] = value;
}

const char* userrec::FormatModes()
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

userrec::userrec(InspIRCd* Instance) : ServerInstance(Instance)
{
	ServerInstance->Log(DEBUG,"userrec::userrec(): Instance: %08x",ServerInstance);
	// the PROPER way to do it, AVOID bzero at *ALL* costs
	*password = *nick = *ident = *host = *dhost = *fullname = *awaymsg = *oper = 0;
	server = (char*)Instance->FindServerNamePtr(Instance->Config->ServerName);
	reset_due = ServerInstance->Time();
	lines_in = lastping = signon = idle_lastmsg = nping = registered = 0;
	ChannelCount = timeout = flood = bytes_in = bytes_out = cmds_in = cmds_out = 0;
	haspassed = dns_done = false;
	fd = -1;
	recvq = "";
	sendq = "";
	WriteError = "";
	res_forward = res_reverse = NULL;
	ip = NULL;
	chans.clear();
	invites.clear();
	chans.resize(MAXCHANS);
	memset(modes,0,sizeof(modes));
	memset(snomasks,0,sizeof(snomasks));
	
	for (unsigned int n = 0; n < MAXCHANS; n++)
	{
		ucrec* x = new ucrec();
		chans[n] = x;
		x->channel = NULL;
		x->uc_modes = 0;
	}
}

userrec::~userrec()
{
	for (std::vector<ucrec*>::iterator n = chans.begin(); n != chans.end(); n++)
	{
		ucrec* x = (ucrec*)*n;
		delete x;
	}

	if (ip)
	{
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
}

/* XXX - minor point, other *Host functions return a char *, this one creates it. Might be nice to be consistant? */
void userrec::MakeHost(char* nhost)
{
	/* This is much faster than snprintf */
	char* t = nhost;
	for(char* n = ident; *n; n++)
		*t++ = *n;
	*t++ = '@';
	for(char* n = host; *n; n++)
		*t++ = *n;
	*t = 0;
}

void userrec::CloseSocket()
{
	shutdown(this->fd,2);
	close(this->fd);
}
 
char* userrec::GetFullHost()
{
	static char result[MAXBUF];
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
	return result;
}

char* userrec::MakeWildHost()
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

int userrec::ReadData(void* buffer, size_t size)
{
	if (IS_LOCAL(this))
	{
		return read(this->fd, buffer, size);
	}
	else
		return 0;
}


char* userrec::GetFullRealHost()
{
	static char fresult[MAXBUF];
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
	return fresult;
}

bool userrec::IsInvited(irc::string &channel)
{
	for (InvitedList::iterator i = invites.begin(); i != invites.end(); i++)
	{
		irc::string compare = i->channel;
		
		if (compare == channel)
		{
			return true;
		}
	}
	return false;
}

InvitedList* userrec::GetInviteList()
{
	return &invites;
}

void userrec::InviteTo(irc::string &channel)
{
	Invited i;
	i.channel = channel;
	invites.push_back(i);
}

void userrec::RemoveInvite(irc::string &channel)
{
	ServerInstance->Log(DEBUG,"Removing invites");
	
	if (invites.size())
	{
		for (InvitedList::iterator i = invites.begin(); i != invites.end(); i++)
		{
			irc::string compare = i->channel;
			
			if (compare == channel)
			{
				invites.erase(i);
				return;
       		 	}
       		}
       	}
}

bool userrec::HasPermission(const std::string &command)
{
	char* mycmd;
	char* savept;
	char* savept2;
	
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
	if (*this->oper)
	{
		opertype_t::iterator iter_opertype = opertypes.find(this->oper);
		if (iter_opertype != opertypes.end())
		{
			char* Classes = strdup(iter_opertype->second);
			char* myclass = strtok_r(Classes," ",&savept);
			while (myclass)
			{
				operclass_t::iterator iter_operclass = operclass.find(myclass);
				if (iter_operclass != operclass.end())
				{
					char* CommandList = strdup(iter_operclass->second);
					mycmd = strtok_r(CommandList," ",&savept2);
					while (mycmd)
					{
						if ((!strcasecmp(mycmd,command.c_str())) || (*mycmd == '*'))
						{
							free(Classes);
							free(CommandList);
							return true;
						}
						mycmd = strtok_r(NULL," ",&savept2);
					}
					free(CommandList);
				}
				myclass = strtok_r(NULL," ",&savept);
			}
			free(Classes);
		}
	}
	return false;
}

/** NOTE: We cannot pass a const reference to this method.
 * The string is changed by the workings of the method,
 * so that if we pass const ref, we end up copying it to
 * something we can change anyway. Makes sense to just let
 * the compiler do that copy for us.
 */
bool userrec::AddBuffer(std::string a)
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
		
		if (recvq.length() > (unsigned)this->recvqmax)
		{
			this->SetWriteError("RecvQ exceeded");
			ServerInstance->WriteOpers("*** User %s RecvQ of %d exceeds connect class maximum of %d",this->nick,recvq.length(),this->recvqmax);
			return false;
		}
	
		return true;
	}

	catch (...)
	{
		ServerInstance->Log(DEBUG,"Exception in userrec::AddBuffer()");
		return false;
	}
}

bool userrec::BufferIsReady()
{
	return (recvq.find('\n') != std::string::npos);
}

void userrec::ClearBuffer()
{
	recvq = "";
}

std::string userrec::GetBuffer()
{
	try
	{
		if (!recvq.length())
			return "";
	
		/* Strip any leading \r or \n off the string.
		 * Usually there are only one or two of these,
		 * so its is computationally cheap to do.
		 */
		while ((*recvq.begin() == '\r') || (*recvq.begin() == '\n'))
			recvq.erase(recvq.begin());
	
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
		ServerInstance->Log(DEBUG,"Exception in userrec::GetBuffer()");
		return "";
	}
}

void userrec::AddWriteBuf(const std::string &data)
{
	if (*this->GetWriteError())
		return;
	
	if (sendq.length() + data.length() > (unsigned)this->sendqmax)
	{
		/*
		 * Fix by brain - Set the error text BEFORE calling writeopers, because
		 * if we dont it'll recursively  call here over and over again trying
		 * to repeatedly add the text to the sendq!
		 */
		this->SetWriteError("SendQ exceeded");
		ServerInstance->WriteOpers("*** User %s SendQ of %d exceeds connect class maximum of %d",this->nick,sendq.length() + data.length(),this->sendqmax);
		return;
	}

	try 
	{
		if (data.length() > 512)
		{
			std::string newdata(data);
			newdata.resize(510);
			newdata.append("\r\n");
			sendq.append(newdata);
		}
		else
		{
			sendq.append(data);
		}
	}
	catch (...)
	{
		this->SetWriteError("SendQ exceeded");
		ServerInstance->WriteOpers("*** User %s SendQ got an exception",this->nick);
	}
}

// send AS MUCH OF THE USERS SENDQ as we are able to (might not be all of it)
void userrec::FlushWriteBuf()
{
	try
	{
		if (this->fd == FD_MAGIC_NUMBER)
		{
			sendq = "";
		}
		if ((sendq.length()) && (this->fd != FD_MAGIC_NUMBER))
		{
			const char* tb = this->sendq.c_str();
			int n_sent = write(this->fd,tb,this->sendq.length());
			if (n_sent == -1)
			{
				if (errno != EAGAIN)
					this->SetWriteError(strerror(errno));
			}
			else
			{
				// advance the queue
				tb += n_sent;
				this->sendq = tb;
				// update the user's stats counters
				this->bytes_out += n_sent;
				this->cmds_out++;
			}
		}
	}

	catch (...)
	{
		ServerInstance->Log(DEBUG,"Exception in userrec::FlushWriteBuf()");
	}
}

void userrec::SetWriteError(const std::string &error)
{
	try
	{
		ServerInstance->Log(DEBUG,"SetWriteError: %s",error.c_str());
		// don't try to set the error twice, its already set take the first string.
		if (!this->WriteError.length())
		{
			ServerInstance->Log(DEBUG,"Setting error string for %s to '%s'",this->nick,error.c_str());
			this->WriteError = error;
		}
	}

	catch (...)
	{
		ServerInstance->Log(DEBUG,"Exception in userrec::SetWriteError()");
	}
}

const char* userrec::GetWriteError()
{
	return this->WriteError.c_str();
}

void userrec::Oper(const std::string &opertype)
{
	try
	{
		this->modes[UM_OPERATOR] = 1;
		this->WriteServ("MODE %s :+o", this->nick);
		FOREACH_MOD(I_OnOper, OnOper(this, opertype));
		ServerInstance->Log(DEFAULT,"OPER: %s!%s@%s opered as type: %s", this->nick, this->ident, this->host, opertype.c_str());
		strlcpy(this->oper, opertype.c_str(), NICKMAX - 1);
		ServerInstance->all_opers.push_back(this);
		FOREACH_MOD(I_OnPostOper,OnPostOper(this, opertype));
	}

	catch (...)
	{
		ServerInstance->Log(DEBUG,"Exception in userrec::Oper()");
	}
}

void userrec::UnOper()
{
	try
	{
		if (*this->oper)
		{
			*this->oper = 0;
			this->modes[UM_OPERATOR] = 0;
			for (std::vector<userrec*>::iterator a = ServerInstance->all_opers.begin(); a < ServerInstance->all_opers.end(); a++)
			{
				if (*a == this)
				{
					ServerInstance->Log(DEBUG,"Oper removed from optimization list");
					ServerInstance->all_opers.erase(a);
					return;
				}
			}
		}
	}

	catch (...)
	{
		ServerInstance->Log(DEBUG,"Exception in userrec::UnOper()");
	}
}

void userrec::QuitUser(InspIRCd* Instance, userrec *user, const std::string &quitreason)
{
	user_hash::iterator iter = Instance->clientlist.find(user->nick);
	std::string reason = quitreason;

	if (reason.length() > MAXQUIT - 1)
		reason.resize(MAXQUIT - 1);
	
	if (IS_LOCAL(user))
		user->Write("ERROR :Closing link (%s@%s) [%s]",user->ident,user->host,reason.c_str());

	if (user->registered == REG_ALL)
	{
		user->PurgeEmptyChannels();
		FOREACH_MOD_I(Instance,I_OnUserQuit,OnUserQuit(user,reason));
		user->WriteCommonExcept("QUIT :%s",reason.c_str());
	}

	if (IS_LOCAL(user))
		user->FlushWriteBuf();

	FOREACH_MOD_I(Instance,I_OnUserDisconnect,OnUserDisconnect(user));

	if (IS_LOCAL(user))
	{
		if (Instance->Config->GetIOHook(user->GetPort()))
		{
			try
			{
				Instance->Config->GetIOHook(user->GetPort())->OnRawSocketClose(user->fd);
			}
			catch (ModuleException& modexcept)
			{
				Instance->Log(DEBUG,"Module exception cought: %s",modexcept.GetReason());
			}
		}
		
		Instance->SE->DelFd(user);
		user->CloseSocket();
	}

	/*
	 * this must come before the ServerInstance->SNO->WriteToSnoMaskso that it doesnt try to fill their buffer with anything
	 * if they were an oper with +sn +qQ.
	 */
	if (user->registered == REG_ALL)
	{
		if (IS_LOCAL(user))
			Instance->SNO->WriteToSnoMask('q',"Client exiting: %s!%s@%s [%s]",user->nick,user->ident,user->host,reason.c_str());
		else
			Instance->SNO->WriteToSnoMask('Q',"Client exiting on server %s: %s!%s@%s [%s]",user->server,user->nick,user->ident,user->host,reason.c_str());
		user->AddToWhoWas();
	}

	if (iter != Instance->clientlist.end())
	{
		Instance->Log(DEBUG,"deleting user hash value %lx",(unsigned long)user);
		if (IS_LOCAL(user))
		{
			if (find(Instance->local_users.begin(),Instance->local_users.end(),user) != Instance->local_users.end())
				Instance->local_users.erase(find(Instance->local_users.begin(),Instance->local_users.end(),user));
		}
		Instance->clientlist.erase(iter);
		DELETE(user);
	}
}

namespace irc
{
	namespace whowas
	{

		WhoWasGroup::WhoWasGroup(userrec* user) : host(NULL), dhost(NULL), ident(NULL), server(NULL), gecos(NULL), signon(user->signon)
		{
			this->host = strdup(user->host);
			this->dhost = strdup(user->dhost);
			this->ident = strdup(user->ident);
			this->server = user->server;
			this->gecos = strdup(user->fullname);
		}

		WhoWasGroup::~WhoWasGroup()
		{
			if (host)
				free(host);
			if (dhost)
				free(dhost);
			if (ident)
				free(ident);
			if (gecos)
				free(gecos);
		}

		/* every hour, run this function which removes all entries over 3 days */
		void MaintainWhoWas(time_t t)
		{
			for (whowas_users::iterator iter = ::whowas.begin(); iter != ::whowas.end(); iter++)
			{
				whowas_set* n = (whowas_set*)iter->second;
				if (n->size())
				{
					while ((n->begin() != n->end()) && ((*n->begin())->signon < t - 259200)) // 3 days
					{
						WhoWasGroup *a = *(n->begin());
						DELETE(a);
						n->erase(n->begin());
					}
				}
			}
		}
	};
};

/* adds or updates an entry in the whowas list */
void userrec::AddToWhoWas()
{
	irc::whowas::whowas_users::iterator iter = whowas.find(this->nick);

	if (iter == whowas.end())
	{
		irc::whowas::whowas_set* n = new irc::whowas::whowas_set;
		irc::whowas::WhoWasGroup *a = new irc::whowas::WhoWasGroup(this);
		n->push_back(a);
		whowas[this->nick] = n;
	}
	else
	{
		irc::whowas::whowas_set* group = (irc::whowas::whowas_set*)iter->second;

		if (group->size() > 10)
		{
			irc::whowas::WhoWasGroup *a = (irc::whowas::WhoWasGroup*)*(group->begin());
			DELETE(a);
			group->pop_front();
		}

		irc::whowas::WhoWasGroup *a = new irc::whowas::WhoWasGroup(this);
		group->push_back(a);
	}
}

/* add a client connection to the sockets list */
void userrec::AddClient(InspIRCd* Instance, int socket, int port, bool iscached, insp_inaddr ip)
{
	std::string tempnick = ConvToStr(socket) + "-unknown";
	user_hash::iterator iter = Instance->clientlist.find(tempnick);
	const char *ipaddr = insp_ntoa(ip);
	userrec* New;
	int j = 0;

	/*
	 * fix by brain.
	 * as these nicknames are 'RFC impossible', we can be sure nobody is going to be
	 * using one as a registered connection. As they are per fd, we can also safely assume
	 * that we wont have collisions. Therefore, if the nick exists in the list, its only
	 * used by a dead socket, erase the iterator so that the new client may reclaim it.
	 * this was probably the cause of 'server ignores me when i hammer it with reconnects'
	 * issue in earlier alphas/betas
	 */
	if (iter != Instance->clientlist.end())
	{
		userrec* goner = iter->second;
		DELETE(goner);
		Instance->clientlist.erase(iter);
	}

	Instance->Log(DEBUG,"AddClient: %d %d %s",socket,port,ipaddr);
	
	New = new userrec(Instance);
	Instance->clientlist[tempnick] = New;
	New->fd = socket;
	strlcpy(New->nick,tempnick.c_str(),NICKMAX-1);

	New->server = Instance->FindServerNamePtr(Instance->Config->ServerName);
	/* We don't need range checking here, we KNOW 'unknown\0' will fit into the ident field. */
	strcpy(New->ident, "unknown");

	New->registered = REG_NONE;
	New->signon = Instance->Time() + Instance->Config->dns_timeout;
	New->lastping = 1;

	Instance->Log(DEBUG,"Setting socket addresses");
	New->SetSockAddr(AF_FAMILY, ipaddr, port);
	Instance->Log(DEBUG,"Socket addresses set.");

	/* Smarter than your average bear^H^H^H^Hset of strlcpys. */
	for (const char* temp = New->GetIPString(); *temp && j < 64; temp++, j++)
		New->dhost[j] = New->host[j] = *temp;
	New->dhost[j] = New->host[j] = 0;
			
	// set the registration timeout for this user
	unsigned long class_regtimeout = 90;
	int class_flood = 0;
	long class_threshold = 5;
	long class_sqmax = 262144;      // 256kb
	long class_rqmax = 4096;	// 4k

	for (ClassVector::iterator i = Instance->Config->Classes.begin(); i != Instance->Config->Classes.end(); i++)
	{
		if ((i->type == CC_ALLOW) && (match(ipaddr,i->host.c_str(),true)))
		{
			class_regtimeout = (unsigned long)i->registration_timeout;
			class_flood = i->flood;
			New->pingmax = i->pingtime;
			class_threshold = i->threshold;
			class_sqmax = i->sendqmax;
			class_rqmax = i->recvqmax;
			break;
		}
	}

	New->nping = Instance->Time() + New->pingmax + Instance->Config->dns_timeout;
	New->timeout = Instance->Time() + class_regtimeout;
	New->flood = class_flood;
	New->threshold = class_threshold;
	New->sendqmax = class_sqmax;
	New->recvqmax = class_rqmax;

	Instance->local_users.push_back(New);

	if ((Instance->local_users.size() > Instance->Config->SoftLimit) || (Instance->local_users.size() >= MAXCLIENTS))
	{
		userrec::QuitUser(Instance, New,"No more connections allowed");
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
	if ((unsigned)socket >= MAX_DESCRIPTORS)
	{
		userrec::QuitUser(Instance, New, "Server is full");
		return;
	}

	/*
	 * XXX really really fixme! QuitUser doesn't like having a null entry in the ref table it seems, moving this up so 
	 * zlines dont crash ircd. we need a better solution, as this is obviously inefficient (and probably wrong) -- w00t
	 */
	if (socket > -1)
	{
		if (!Instance->SE->AddFd(New))
		{
			userrec::QuitUser(Instance, New, "Internal error handling connection");
			return;
		}
	}

	ELine* e = Instance->XLines->matches_exception(New);
	if (!e)
	{
		ZLine* r = Instance->XLines->matches_zline(ipaddr);
		if (r)
		{
			char reason[MAXBUF];
			snprintf(reason,MAXBUF,"Z-Lined: %s",r->reason);
			userrec::QuitUser(Instance, New, reason);
			return;
		}
	}

	New->WriteServ("NOTICE Auth :*** Looking up your hostname...");
}

long userrec::GlobalCloneCount()
{
	char u1[128];
	char u2[128];
	long x = 0;

	for (user_hash::const_iterator a = ServerInstance->clientlist.begin(); a != ServerInstance->clientlist.end(); a++)
	{
		/* We have to match ip's as strings - we don't know what protocol
		 * a remote user may be using
		 */
		if (strcmp(a->second->GetIPString(u1), this->GetIPString(u2)) == 0)
			x++;
	}
	
	return x;
}

long userrec::LocalCloneCount()
{
	long x = 0;
	for (std::vector<userrec*>::const_iterator a = ServerInstance->local_users.begin(); a != ServerInstance->local_users.end(); a++)
	{
		userrec* comp = *a;
#ifdef IPV6
		/* I dont think theres any faster way of matching two ipv6 addresses than memcmp */
		in6_addr* s1 = &(((sockaddr_in6*)comp->ip)->sin6_addr);
		in6_addr* s2 = &(((sockaddr_in6*)this->ip)->sin6_addr);
		if (!memcmp(s1->s6_addr, s2->s6_addr, sizeof(in6_addr)))
			x++;
#else
		in_addr* s1 = &((sockaddr_in*)comp->ip)->sin_addr;
		in_addr* s2 = &((sockaddr_in*)this->ip)->sin_addr;
		if (s1->s_addr == s2->s_addr)
			x++;
#endif
	}
	return x;
}

void userrec::FullConnect(CullList* Goners)
{
	ServerInstance->stats->statsConnects++;
	this->idle_lastmsg = ServerInstance->Time();

	ConnectClass a = this->GetClass();

	if (a.type == CC_DENY)
	{
		Goners->AddItem(this,"Unauthorised connection");
		return;
	}
	
	if ((*(a.pass.c_str())) && (!this->haspassed))
	{
		Goners->AddItem(this,"Invalid password");
		return;
	}
	
	if (this->LocalCloneCount() > a.maxlocal)
	{
		Goners->AddItem(this, "No more connections allowed from your host via this connect class (local)");
		ServerInstance->WriteOpers("*** WARNING: maximum LOCAL connections (%ld) exceeded for IP %s", a.maxlocal, this->GetIPString());
		return;
	}
	else if (this->GlobalCloneCount() > a.maxglobal)
	{
		Goners->AddItem(this, "No more connections allowed from your host via this connect class (global)");
		ServerInstance->WriteOpers("*** WARNING: maximum GLOBAL connections (%ld) exceeded for IP %s",a.maxglobal, this->GetIPString());
		return;
	}

	ELine* e = ServerInstance->XLines->matches_exception(this);

	if (!e)
	{
		GLine* r = ServerInstance->XLines->matches_gline(this);
		
		if (r)
		{
			char reason[MAXBUF];
			snprintf(reason,MAXBUF,"G-Lined: %s",r->reason);
			Goners->AddItem(this, reason);
			return;
		}
		
		KLine* n = ServerInstance->XLines->matches_kline(this);
		
		if (n)
		{
			char reason[MAXBUF];
			snprintf(reason,MAXBUF,"K-Lined: %s",n->reason);
			Goners->AddItem(this, reason);
			return;
		}
	}


	this->WriteServ("NOTICE Auth :Welcome to \002%s\002!",ServerInstance->Config->Network);
	this->WriteServ("001 %s :Welcome to the %s IRC Network %s!%s@%s",this->nick, ServerInstance->Config->Network, this->nick, this->ident, this->host);
	this->WriteServ("002 %s :Your host is %s, running version %s",this->nick,ServerInstance->Config->ServerName,VERSION);
	this->WriteServ("003 %s :This server was created %s %s", this->nick, __TIME__, __DATE__);
	this->WriteServ("004 %s %s %s %s %s %s", this->nick, ServerInstance->Config->ServerName, VERSION, ServerInstance->Modes->UserModeList().c_str(), ServerInstance->Modes->ChannelModeList().c_str(), ServerInstance->Modes->ParaModeList().c_str());

	// anfl @ #ratbox, efnet reminded me that according to the RFC this cant contain more than 13 tokens per line...
	// so i'd better split it :)
	std::stringstream out(ServerInstance->Config->data005);
	std::string token = "";
	std::string line5 = "";
	int token_counter = 0;
	
	while (!out.eof())
	{
		out >> token;
		line5 = line5 + token + " ";
		token_counter++;
		
		if ((token_counter >= 13) || (out.eof() == true))
		{
			this->WriteServ("005 %s %s:are supported by this server", this->nick, line5.c_str());
			line5 = "";
			token_counter = 0;
		}
	}
	
	this->ShowMOTD();

	/*
	 * fix 3 by brain, move registered = 7 below these so that spurious modes and host
	 * changes dont go out onto the network and produce 'fake direction'.
	 */
	FOREACH_MOD(I_OnUserConnect,OnUserConnect(this));
	FOREACH_MOD(I_OnPostConnect,OnPostConnect(this));
	this->registered = REG_ALL;
	ServerInstance->SNO->WriteToSnoMask('c',"Client connecting on port %d: %s!%s@%s [%s]", this->GetPort(), this->nick, this->ident, this->host, this->GetIPString());
}

/** userrec::UpdateNick()
 * re-allocates a nick in the user_hash after they change nicknames,
 * returns a pointer to the new user as it may have moved
 */
userrec* userrec::UpdateNickHash(const char* New)
{
	try
	{
		//user_hash::iterator newnick;
		user_hash::iterator oldnick = ServerInstance->clientlist.find(this->nick);

		if (!strcasecmp(this->nick,New))
			return oldnick->second;

		if (oldnick == ServerInstance->clientlist.end())
			return NULL; /* doesnt exist */

		userrec* olduser = oldnick->second;
		ServerInstance->clientlist[New] = olduser;
		ServerInstance->clientlist.erase(oldnick);
		return ServerInstance->clientlist[New];
	}

	catch (...)
	{
		ServerInstance->Log(DEBUG,"Exception in userrec::UpdateNickHash()");
		return NULL;
	}
}

bool userrec::ForceNickChange(const char* newnick)
{
	try
	{
		int MOD_RESULT = 0;
	
		FOREACH_RESULT(I_OnUserPreNick,OnUserPreNick(this, newnick));

		if (MOD_RESULT)
		{
			ServerInstance->stats->statsCollisions++;
			return false;
		}
	
		if (ServerInstance->XLines->matches_qline(newnick))
		{
			ServerInstance->stats->statsCollisions++;
			return false;
		}

		if (this->registered == REG_ALL)
		{
			const char* pars[1];
			pars[0] = newnick;
			std::string cmd = "NICK";
			return (ServerInstance->Parser->CallHandler(cmd, pars, 1, this) == CMD_SUCCESS);
		}
		return false;
	}

	catch (...)
	{
		ServerInstance->Log(DEBUG,"Exception in userrec::ForceNickChange()");
		return false;
	}
}

void userrec::SetSockAddr(int protocol_family, const char* ip, int port)
{
	switch (protocol_family)
	{
#ifdef SUPPORT_IP6LINKS
		case AF_INET6:
		{
			ServerInstance->Log(DEBUG,"Set inet6 protocol address");
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
			ServerInstance->Log(DEBUG,"Set inet4 protocol address");
			sockaddr_in* sin = new sockaddr_in;
			sin->sin_family = AF_INET;
			sin->sin_port = port;
			inet_pton(AF_INET, ip, &sin->sin_addr);
			this->ip = (sockaddr*)sin;
		}
		break;
		default:
			ServerInstance->Log(DEBUG,"Ut oh, I dont know protocol %d to be set on '%s'!", protocol_family, this->nick);
		break;
	}
}

int userrec::GetPort()
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
			ServerInstance->Log(DEBUG,"Ut oh, '%s' has an unknown protocol family!",this->nick);
		break;
	}
	return 0;
}

int userrec::GetProtocolFamily()
{
	if (this->ip == NULL)
		return 0;

	sockaddr_in* sin = (sockaddr_in*)this->ip;
	return sin->sin_family;
}

const char* userrec::GetIPString()
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
				strlcpy(&temp[1], buf, sizeof(temp));
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
			ServerInstance->Log(DEBUG,"Ut oh, '%s' has an unknown protocol family!",this->nick);
		break;
	}
	return "";
}

const char* userrec::GetIPString(char* buf)
{
	if (this->ip == NULL)
	{
		*buf = 0;
		return buf;
	}

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
				strlcpy(&temp[1], buf, sizeof(temp));
				*temp = '0';
				strlcpy(buf, temp, sizeof(temp));
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
			ServerInstance->Log(DEBUG,"Ut oh, '%s' has an unknown protocol family!",this->nick);
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
void userrec::Write(std::string text)
{
	if ((this->fd < 0) || (this->fd > MAX_DESCRIPTORS))
		return;

	try
	{
		text.append("\r\n");
	}
	catch (...)
	{
		ServerInstance->Log(DEBUG,"Exception in userrec::Write() std::string::append");
		return;
	}

	if (ServerInstance->Config->GetIOHook(this->GetPort()))
	{
		try
		{
			ServerInstance->Config->GetIOHook(this->GetPort())->OnRawSocketWrite(this->fd, text.data(), text.length());
		}
		catch (ModuleException& modexcept)
		{
			ServerInstance->Log(DEBUG,"Module exception caught: %s",modexcept.GetReason());
		}
	}
	else
	{
		this->AddWriteBuf(text);
	}
	ServerInstance->stats->statsSent += text.length();
}

/** Write()
 */
void userrec::Write(const char *text, ...)
{
	va_list argsPtr;
	char textbuffer[MAXBUF];

	va_start(argsPtr, text);
	vsnprintf(textbuffer, MAXBUF, text, argsPtr);
	va_end(argsPtr);

	this->Write(std::string(textbuffer));
}

void userrec::WriteServ(const std::string& text)
{
	char textbuffer[MAXBUF];

	snprintf(textbuffer,MAXBUF,":%s %s",ServerInstance->Config->ServerName,text.c_str());
	this->Write(std::string(textbuffer));
}

/** WriteServ()
 *  Same as Write(), except `text' is prefixed with `:server.name '.
 */
void userrec::WriteServ(const char* text, ...)
{
	va_list argsPtr;
	char textbuffer[MAXBUF];

	va_start(argsPtr, text);
	vsnprintf(textbuffer, MAXBUF, text, argsPtr);
	va_end(argsPtr);

	this->WriteServ(std::string(textbuffer));
}


void userrec::WriteFrom(userrec *user, const std::string &text)
{
	char tb[MAXBUF];

	snprintf(tb,MAXBUF,":%s %s",user->GetFullHost(),text.c_str());
	
	this->Write(std::string(tb));
}


/* write text from an originating user to originating user */

void userrec::WriteFrom(userrec *user, const char* text, ...)
{
	va_list argsPtr;
	char textbuffer[MAXBUF];

	va_start(argsPtr, text);
	vsnprintf(textbuffer, MAXBUF, text, argsPtr);
	va_end(argsPtr);

	this->WriteFrom(user, std::string(textbuffer));
}


/* write text to an destination user from a source user (e.g. user privmsg) */

void userrec::WriteTo(userrec *dest, const char *data, ...)
{
	char textbuffer[MAXBUF];
	va_list argsPtr;

	va_start(argsPtr, data);
	vsnprintf(textbuffer, MAXBUF, data, argsPtr);
	va_end(argsPtr);

	this->WriteTo(dest, std::string(textbuffer));
}

void userrec::WriteTo(userrec *dest, const std::string &data)
{
	dest->WriteFrom(this, data);
}


void userrec::WriteCommon(const char* text, ...)
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

void userrec::WriteCommon(const std::string &text)
{
	try
	{
		bool sent_to_at_least_one = false;
	
		if (this->registered != REG_ALL)
			return;
	
		uniq_id++;
	
		for (std::vector<ucrec*>::const_iterator v = this->chans.begin(); v != this->chans.end(); v++)
		{
			ucrec *n = *v;
			if (n->channel)
			{
				CUList *ulist= n->channel->GetUsers();
		
				for (CUList::iterator i = ulist->begin(); i != ulist->end(); i++)
				{
					if ((IS_LOCAL(i->second)) && (already_sent[i->second->fd] != uniq_id))
					{
						already_sent[i->second->fd] = uniq_id;
						i->second->WriteFrom(this, std::string(text));
						sent_to_at_least_one = true;
					}
				}
			}
		}
	
		/*
		 * if the user was not in any channels, no users will receive the text. Make sure the user
		 * receives their OWN message for WriteCommon
		 */
		if (!sent_to_at_least_one)
		{
			this->WriteFrom(this,std::string(text));
		}
	}

	catch (...)
	{
		ServerInstance->Log(DEBUG,"Exception in userrec::WriteCommon()");
	}
}


/* write a formatted string to all users who share at least one common
 * channel, NOT including the source user e.g. for use in QUIT
 */

void userrec::WriteCommonExcept(const char* text, ...)
{
	char textbuffer[MAXBUF];
	va_list argsPtr;

	va_start(argsPtr, text);
	vsnprintf(textbuffer, MAXBUF, text, argsPtr);
	va_end(argsPtr);

	this->WriteCommonExcept(std::string(textbuffer));
}

void userrec::WriteCommonExcept(const std::string &text)
{
	bool quit_munge = false;
	char oper_quit[MAXBUF];
	char textbuffer[MAXBUF];

	strlcpy(textbuffer, text.c_str(), MAXBUF);

	if (this->registered != REG_ALL)
		return;

	uniq_id++;

	/* TODO: We need some form of WriteCommonExcept that will send two lines, one line to
	 * opers and the other line to non-opers, then all this hidebans and hidesplits gunk
	 * can go byebye.
	 */
	if (ServerInstance->Config->HideSplits)
	{
		char* check = textbuffer + 6;

		if (!strncasecmp(textbuffer, "QUIT :",6))
		{
			std::stringstream split(check);
			std::string server_one;
			std::string server_two;

			split >> server_one;
			split >> server_two;

			if ((ServerInstance->FindServerName(server_one)) && (ServerInstance->FindServerName(server_two)))
			{
				strlcpy(oper_quit,textbuffer,MAXQUIT);
				strlcpy(check,"*.net *.split",MAXQUIT);
				quit_munge = true;
			}
		}
	}

	if ((ServerInstance->Config->HideBans) && (!quit_munge))
	{
		if ((!strncasecmp(textbuffer, "QUIT :G-Lined:",14)) || (!strncasecmp(textbuffer, "QUIT :K-Lined:",14))
		|| (!strncasecmp(textbuffer, "QUIT :Q-Lined:",14)) || (!strncasecmp(textbuffer, "QUIT :Z-Lined:",14)))
		{
			char* check = textbuffer + 13;
			strlcpy(oper_quit,textbuffer,MAXQUIT);
			*check = 0;  // We don't need to strlcpy, we just chop it from the :
			quit_munge = true;
		}
	}

	for (std::vector<ucrec*>::const_iterator v = this->chans.begin(); v != this->chans.end(); v++)
	{
		ucrec* n = *v;
		if (n->channel)
		{
			CUList *ulist= n->channel->GetUsers();

			for (CUList::iterator i = ulist->begin(); i != ulist->end(); i++)
			{
				if (this != i->second)
				{
					if ((IS_LOCAL(i->second)) && (already_sent[i->second->fd] != uniq_id))
					{
						already_sent[i->second->fd] = uniq_id;
						if (quit_munge)
							i->second->WriteFrom(this, *i->second->oper ? std::string(oper_quit) : std::string(textbuffer));
						else
							i->second->WriteFrom(this, std::string(textbuffer));
					}
				}
			}
		}
	}

}

void userrec::WriteWallOps(const std::string &text)
{
	/* Does nothing if theyre not opered */
	if ((!*this->oper) && (IS_LOCAL(this)))
		return;

	std::string wallop = "WALLOPS :";

	try
	{
		wallop.append(text);
	}
	catch (...)
	{
		ServerInstance->Log(DEBUG,"Exception in userrec::Write() std::string::append");
		return;
	}

	for (std::vector<userrec*>::const_iterator i = ServerInstance->local_users.begin(); i != ServerInstance->local_users.end(); i++)
	{
		userrec* t = *i;
		if ((IS_LOCAL(t)) && (t->modes[UM_WALLOPS]))
			this->WriteTo(t,wallop);
	}
}

void userrec::WriteWallOps(const char* text, ...)
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
 * channel's userlist in the inner loop which is a std::map<userrec*,userrec*>
 * and saves us time as we already know what pointer value we are after.
 * Don't quote me on the maths as i am not a mathematician or computer scientist,
 * but i believe this algorithm is now x+(log y) maximum iterations instead.
 */
bool userrec::SharesChannelWith(userrec *other)
{
	if ((!other) || (this->registered != REG_ALL) || (other->registered != REG_ALL))
		return false;

	/* Outer loop */
	for (std::vector<ucrec*>::const_iterator i = this->chans.begin(); i != this->chans.end(); i++)
	{
		/* Fetch the channel from the user */
		ucrec* user_channel = *i;

		if (user_channel->channel)
		{
			/* Eliminate the inner loop (which used to be ~equal in size to the outer loop)
			 * by replacing it with a map::find which *should* be more efficient
			 */
			if (user_channel->channel->HasUser(other))
				return true;
		}
	}
	return false;
}

int userrec::CountChannels()
{
	return ChannelCount;
}

void userrec::ModChannelCount(int n)
{
	ChannelCount += n;
}

bool userrec::ChangeName(const char* gecos)
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

bool userrec::ChangeDisplayedHost(const char* host)
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
		this->WriteCommonExcept("%s","QUIT :Changing hosts");

	strlcpy(this->dhost,host,63);

	if (this->ServerInstance->Config->CycleHosts)
	{
		for (std::vector<ucrec*>::const_iterator i = this->chans.begin(); i != this->chans.end(); i++)
		{
			if ((*i)->channel)
			{
				(*i)->channel->WriteAllExceptSender(this, false, 0, "JOIN %s", (*i)->channel->name);
				std::string n = this->ServerInstance->Modes->ModeString(this, (*i)->channel);
				if (n.length() > 0)
					(*i)->channel->WriteAllExceptSender(this, true, 0, "MODE %s +%s", (*i)->channel->name, n.c_str());
			}
		}
	}

	if (IS_LOCAL(this))
		this->WriteServ("396 %s %s :is now your hidden host",this->nick,this->dhost);

	return true;
}

bool userrec::ChangeIdent(const char* newident)
{
	if (!strcmp(newident, this->ident))
		return true;

	if (this->ServerInstance->Config->CycleHosts)
		this->WriteCommonExcept("%s","QUIT :Changing ident");

	strlcpy(this->ident, newident, IDENTMAX+2);

	if (this->ServerInstance->Config->CycleHosts)
	{
		for (std::vector<ucrec*>::const_iterator i = this->chans.begin(); i != this->chans.end(); i++)
		{
			if ((*i)->channel)
			{
				(*i)->channel->WriteAllExceptSender(this, false, 0, "JOIN %s", (*i)->channel->name);
				std::string n = this->ServerInstance->Modes->ModeString(this, (*i)->channel);
				if (n.length() > 0)
					(*i)->channel->WriteAllExceptSender(this, true, 0, "MODE %s +%s", (*i)->channel->name, n.c_str());
			}
		}
	}

	return true;
}

void userrec::NoticeAll(char* text, ...)
{
	char textbuffer[MAXBUF];
	char formatbuffer[MAXBUF];
	va_list argsPtr;

	va_start(argsPtr, text);
	vsnprintf(textbuffer, MAXBUF, text, argsPtr);
	va_end(argsPtr);

	snprintf(formatbuffer,MAXBUF,"NOTICE $* :%s",textbuffer);

	for (std::vector<userrec*>::const_iterator i = ServerInstance->local_users.begin(); i != ServerInstance->local_users.end(); i++)
	{
		userrec* t = *i;
		t->WriteFrom(this, std::string(formatbuffer));
	}
}


std::string userrec::ChannelList(userrec* source)
{
	try
	{
		std::string list;
		for (std::vector<ucrec*>::const_iterator i = this->chans.begin(); i != this->chans.end(); i++)
		{
			ucrec* rec = *i;
	
			if(rec->channel && rec->channel->name)
			{
				/* If the target is the same as the sender, let them see all their channels.
				 * If the channel is NOT private/secret OR the user shares a common channel
				 * If the user is an oper, and the <options:operspywhois> option is set.
				 */
				if ((source == this) || (*source->oper && ServerInstance->Config->OperSpyWhois) || (((!rec->channel->modes[CM_PRIVATE]) && (!rec->channel->modes[CM_SECRET])) || (rec->channel->HasUser(source))))
				{
					list.append(rec->channel->GetPrefixChar(this)).append(rec->channel->name).append(" ");
				}
			}
		}
		return list;
	}
	catch (...)
	{
		ServerInstance->Log(DEBUG,"Exception in userrec::ChannelList()");
		return "";
	}
}

void userrec::SplitChanList(userrec* dest, const std::string &cl)
{
	std::string line;
	std::ostringstream prefix;
	std::string::size_type start, pos, length;

	try
	{
		prefix << ":" << ServerInstance->Config->ServerName << " 319 " << this->nick << " " << dest->nick << " :";
		line = prefix.str();
	
		for (start = 0; (pos = cl.find(' ', start)) != std::string::npos; start = pos+1)
		{
			length = (pos == std::string::npos) ? cl.length() : pos;
	
			if (line.length() + length - start > 510)
			{
				this->Write(line);
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
			this->Write(line);
		}
	}

	catch (...)
	{
		ServerInstance->Log(DEBUG,"Exception in userrec::SplitChanList()");
	}
}


/* looks up a users password for their connection class (<ALLOW>/<DENY> tags)
 * NOTE: If the <ALLOW> or <DENY> tag specifies an ip, and this user resolves,
 * then their ip will be taken as 'priority' anyway, so for example,
 * <connect allow="127.0.0.1"> will match joe!bloggs@localhost
 */
ConnectClass& userrec::GetClass()
{
	for (ClassVector::iterator i = ServerInstance->Config->Classes.begin(); i != ServerInstance->Config->Classes.end(); i++)
	{
		if ((match(this->GetIPString(),i->host.c_str(),true)) || (match(this->host,i->host.c_str())))
			return *i;
	}

	return *(ServerInstance->Config->Classes.begin());
}

void userrec::PurgeEmptyChannels()
{
	std::vector<chanrec*> to_delete;

	// firstly decrement the count on each channel
	for (std::vector<ucrec*>::iterator f = this->chans.begin(); f != this->chans.end(); f++)
	{
		ucrec* uc = *f;
		if (uc->channel)
		{
			uc->channel->RemoveAllPrefixes(this);
			if (uc->channel->DelUser(this) == 0)
			{
				/* No users left in here, mark it for deletion */
				try
				{
					to_delete.push_back(uc->channel);
				}
				catch (...)
				{
					ServerInstance->Log(DEBUG,"Exception in userrec::PurgeEmptyChannels to_delete.push_back()");
				}
				uc->channel = NULL;
			}
		}
	}

	for (std::vector<chanrec*>::iterator n = to_delete.begin(); n != to_delete.end(); n++)
	{
		chanrec* thischan = *n;
		chan_hash::iterator i2 = ServerInstance->chanlist.find(thischan->name);
		if (i2 != ServerInstance->chanlist.end())
		{
			FOREACH_MOD(I_OnChannelDelete,OnChannelDelete(i2->second));
			DELETE(i2->second);
			ServerInstance->chanlist.erase(i2);
		}
	}

	this->UnOper();
}

void userrec::ShowMOTD()
{
	if (!ServerInstance->Config->MOTD.size())
	{
		this->WriteServ("422 %s :Message of the day file is missing.",this->nick);
		return;
	}
	this->WriteServ("375 %s :%s message of the day", this->nick, ServerInstance->Config->ServerName);

	for (unsigned int i = 0; i < ServerInstance->Config->MOTD.size(); i++)
		this->WriteServ("372 %s :- %s",this->nick,ServerInstance->Config->MOTD[i].c_str());

	this->WriteServ("376 %s :End of message of the day.", this->nick);
}

void userrec::ShowRULES()
{
	if (!ServerInstance->Config->RULES.size())
	{
		this->WriteServ("NOTICE %s :Rules file is missing.",this->nick);
		return;
	}
	this->WriteServ("NOTICE %s :%s rules",this->nick,ServerInstance->Config->ServerName);

	for (unsigned int i = 0; i < ServerInstance->Config->RULES.size(); i++)
		this->WriteServ("NOTICE %s :%s",this->nick,ServerInstance->Config->RULES[i].c_str());

	this->WriteServ("NOTICE %s :End of %s rules.",this->nick,ServerInstance->Config->ServerName);
}

void userrec::HandleEvent(EventType et)
{
	/* WARNING: May delete this user! */
	try
	{
		ServerInstance->ProcessUser(this);
	}
	catch (...)
	{
		ServerInstance->Log(DEBUG,"Exception in userrec::HandleEvent intercepted");
	}
}

