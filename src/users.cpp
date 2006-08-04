/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd is copyright (C) 2002-2006 ChatSpike-Dev.
 *                       E-mail:
 *                <brain@chatspike.net>
 *           	  <Craig@chatspike.net>
 *     
 * Written by Craig Edwards, Craig McLure, and others.
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd_config.h"
#include "configreader.h"
#include "channels.h"
#include "connection.h"
#include "users.h"
#include "inspircd.h"
#include <stdio.h>
#include "inspstring.h"
#include "commands.h"
#include "helperfuncs.h"
#include "typedefs.h"
#include "socketengine.h"
#include "hashcomp.h"
#include "message.h"
#include "wildcard.h"
#include "xline.h"
#include "cull_list.h"

extern InspIRCd* ServerInstance;
extern int WHOWAS_STALE;
extern int WHOWAS_MAX;
extern std::vector<Module*> modules;
extern std::vector<ircd_module*> factory;
extern std::vector<InspSocket*> module_sockets;
extern int MODCOUNT;
extern InspSocket* socket_ref[MAX_DESCRIPTORS];
extern time_t TIME;
extern userrec* fd_ref_table[MAX_DESCRIPTORS];
extern ServerConfig *Config;
extern user_hash clientlist;
extern Server* MyServer;

whowas_users whowas;

extern std::vector<userrec*> local_users;

std::vector<userrec*> all_opers;

typedef std::map<irc::string,char*> opertype_t;
typedef opertype_t operclass_t;

opertype_t opertypes;
operclass_t operclass;

bool InitTypes(const char* tag)
{
	for (opertype_t::iterator n = opertypes.begin(); n != opertypes.end(); n++)
	{
		if (n->second)
			delete[] n->second;
	}
	
	opertypes.clear();
	return true;
}

bool InitClasses(const char* tag)
{
	for (operclass_t::iterator n = operclass.begin(); n != operclass.end(); n++)
	{
		if (n->second)
			delete[] n->second;
	}
	
	operclass.clear();
	return true;
}

bool DoType(const char* tag, char** entries, void** values, int* types)
{
	char* TypeName = (char*)values[0];
	char* Classes = (char*)values[1];
	
	opertypes[TypeName] = strdup(Classes);
	log(DEBUG,"Read oper TYPE '%s' with classes '%s'",TypeName,Classes);
	return true;
}

bool DoClass(const char* tag, char** entries, void** values, int* types)
{
	char* ClassName = (char*)values[0];
	char* CommandList = (char*)values[1];
	
	operclass[ClassName] = strdup(CommandList);
	log(DEBUG,"Read oper CLASS '%s' with commands '%s'",ClassName,CommandList);
	return true;
}

bool DoneClassesAndTypes(const char* tag)
{
	return true;
}

bool userrec::ProcessNoticeMasks(const char *sm)
{
	bool adding = true;
	const char *c = sm;

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
			default:
				if ((*c >= 'A') && (*c <= 'z'))
					this->SetNoticeMask(*c, adding);
				break;
		}

		*c++;
	}

	return true;
}

void userrec::StartDNSLookup()
{
	log(DEBUG,"Commencing reverse lookup");
	try
	{
		res_reverse = new UserResolver(this, this->GetIPString(), false);
		MyServer->AddResolver(res_reverse);
	}
	catch (ModuleException& e)
	{
		log(DEBUG,"Error in resolver: %s",e.GetReason());
	}
}

UserResolver::UserResolver(userrec* user, std::string to_resolve, bool forward) : Resolver(to_resolve, forward ? DNS_QUERY_FORWARD : DNS_QUERY_REVERSE), bound_user(user)
{
	this->fwd = forward;
	this->bound_fd = user->fd;
}

void UserResolver::OnLookupComplete(const std::string &result)
{
	if ((!this->fwd) && (fd_ref_table[this->bound_fd] == this->bound_user))
	{
		log(DEBUG,"Commencing forward lookup");
		this->bound_user->stored_host = result;
		try
		{
			bound_user->res_forward = new UserResolver(this->bound_user, result, true);
			MyServer->AddResolver(bound_user->res_forward);
		}
		catch (ModuleException& e)
		{
			log(DEBUG,"Error in resolver: %s",e.GetReason());
		}
	}
	else if ((this->fwd) && (fd_ref_table[this->bound_fd] == this->bound_user))
	{
		/* Both lookups completed */
		if (this->bound_user->GetIPString() == result)
		{
			std::string hostname = this->bound_user->stored_host;
			if (hostname.length() < 65)
			{
				WriteServ(this->bound_fd, "NOTICE Auth :*** Found your hostname (%s)", this->bound_user->stored_host.c_str());
				this->bound_user->dns_done = true;
				strlcpy(this->bound_user->dhost, hostname.c_str(),64);
				strlcpy(this->bound_user->host, hostname.c_str(),64);
			}
			else
			{
				WriteServ(this->bound_fd, "NOTICE Auth :*** Your hostname is longer than the maximum of 64 characters, using your IP address (%s) instead.", this->bound_user->GetIPString());
			}
		}
		else
		{
			WriteServ(this->bound_fd, "NOTICE Auth :*** Your hostname does not match up with your IP address. Sorry, using your IP address (%s) instead.", this->bound_user->GetIPString());
		}
	}
}

void UserResolver::OnError(ResolverError e, const std::string &errormessage)
{
	if (fd_ref_table[this->bound_fd] == this->bound_user)
	{
		/* Error message here */
		WriteServ(this->bound_fd, "NOTICE Auth :*** Could not resolve your hostname, using your IP address (%s) instead.", this->bound_user->GetIPString());
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

userrec::userrec()
{
	// the PROPER way to do it, AVOID bzero at *ALL* costs
	*password = *nick = *ident = *host = *dhost = *fullname = *awaymsg = *oper = 0;
	server = (char*)FindServerNamePtr(Config->ServerName);
	reset_due = TIME;
	lines_in = fd = lastping = signon = idle_lastmsg = nping = registered = 0;
	timeout = flood = bytes_in = bytes_out = cmds_in = cmds_out = 0;
	haspassed = dns_done = false;
	recvq = "";
	sendq = "";
	res_forward = res_reverse = NULL;
	chans.clear();
	invites.clear();
	chans.resize(MAXCHANS);
	memset(modes,0,sizeof(modes));
	
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
	if (this->fd > -1)
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
	log(DEBUG,"Removing invites");
	
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


bool userrec::AddBuffer(const std::string &a)
{
	std::string b = "";

	/* NB: std::string is arsey about \r and \n and tries to translate them
	 * somehow, so we CANNOT use std::string::find() here :(
	 */
	for (std::string::const_iterator i = a.begin(); i != a.end(); i++)
	{
		if (*i != '\r')
			b += *i;
	}

	if (b.length())
		recvq.append(b);

	if (recvq.length() > (unsigned)this->recvqmax)
	{
		this->SetWriteError("RecvQ exceeded");
		WriteOpers("*** User %s RecvQ of %d exceeds connect class maximum of %d",this->nick,recvq.length(),this->recvqmax);
		return false;
	}

	return true;
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
		WriteOpers("*** User %s SendQ of %d exceeds connect class maximum of %d",this->nick,sendq.length() + data.length(),this->sendqmax);
		return;
	}
	
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

// send AS MUCH OF THE USERS SENDQ as we are able to (might not be all of it)
void userrec::FlushWriteBuf()
{
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

void userrec::SetWriteError(const std::string &error)
{
	// don't try to set the error twice, its already set take the first string.
	if (!this->WriteError.length())
	{
		log(DEBUG,"Setting error string for %s to '%s'",this->nick,error.c_str());
		this->WriteError = error;
	}
}

const char* userrec::GetWriteError()
{
	return this->WriteError.c_str();
}

void AddOper(userrec* user)
{
	log(DEBUG,"Oper added to optimization list");
	all_opers.push_back(user);
}

void DeleteOper(userrec* user)
{
	for (std::vector<userrec*>::iterator a = all_opers.begin(); a < all_opers.end(); a++)
	{
		if (*a == user)
		{
			log(DEBUG,"Oper removed from optimization list");
			all_opers.erase(a);
			return;
		}
	}
}

void kill_link(userrec *user,const char* r)
{
	user_hash::iterator iter = clientlist.find(user->nick);

/*
 * I'm pretty sure returning here is causing a desync when part of the net thinks a user is gone,
 * and another part doesn't. We want to broadcast the quit/kill before bailing so the net stays in sync.
 *
 * I can't imagine this blowing up, so I'm commenting it out. We still check
 * before playing with a bad iterator below in our if(). DISCUSS THIS BEFORE YOU DO ANYTHING. --w00t
 *
 *	if (iter == clientlist.end())
 *		return;
 */

	char reason[MAXBUF];

	strlcpy(reason,r,MAXQUIT-1);
	log(DEBUG,"kill_link: %s %d '%s'",user->nick,user->fd,reason);
	
	if (IS_LOCAL(user))
		Write(user->fd,"ERROR :Closing link (%s@%s) [%s]",user->ident,user->host,reason);

	if (user->registered == REG_ALL)
	{
		purge_empty_chans(user);
		FOREACH_MOD(I_OnUserQuit,OnUserQuit(user,reason));
		WriteCommonExcept(user,"QUIT :%s",reason);
	}

	if (IS_LOCAL(user))
		user->FlushWriteBuf();

	FOREACH_MOD(I_OnUserDisconnect,OnUserDisconnect(user));

	if (IS_LOCAL(user))
	{
		if (Config->GetIOHook(user->GetPort()))
		{
			try
			{
				Config->GetIOHook(user->GetPort())->OnRawSocketClose(user->fd);
			}
			catch (ModuleException& modexcept)
			{
				log(DEBUG,"Module exception cought: %s",modexcept.GetReason());
			}
		}
		
		ServerInstance->SE->DelFd(user->fd);
		user->CloseSocket();
	}

	/*
	 * this must come before the WriteOpers so that it doesnt try to fill their buffer with anything
	 * if they were an oper with +s.
	 *
	 * XXX -
	 * In the current implementation, we only show local quits, as we only show local connects. With 
	 * the proposed implmentation of snomasks however, this will likely change in the (near?) future.
	 */
	if (user->registered == REG_ALL)
	{
		if (IS_LOCAL(user))
			WriteOpers("*** Client exiting: %s!%s@%s [%s]",user->nick,user->ident,user->host,reason);
		AddWhoWas(user);
	}

	if (iter != clientlist.end())
	{
		log(DEBUG,"deleting user hash value %lx",(unsigned long)user);
		if (IS_LOCAL(user))
		{
			fd_ref_table[user->fd] = NULL;
			if (find(local_users.begin(),local_users.end(),user) != local_users.end())
			{
				local_users.erase(find(local_users.begin(),local_users.end(),user));
				log(DEBUG,"Delete local user");
			}
		}
		clientlist.erase(iter);
		DELETE(user);
	}
}

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

/* adds or updates an entry in the whowas list */
void AddWhoWas(userrec* u)
{
	whowas_users::iterator iter = whowas.find(u->nick);
	
	if (iter == whowas.end())
	{
		whowas_set* n = new whowas_set;
		WhoWasGroup *a = new WhoWasGroup(u);
		n->push_back(a);
		whowas[u->nick] = n;
	}
	else
	{
		whowas_set* group = (whowas_set*)iter->second;
		
		if (group->size() > 10)
		{
			WhoWasGroup *a = (WhoWasGroup*)*(group->begin());
			DELETE(a);
			group->pop_front();
		}
		
		WhoWasGroup *a = new WhoWasGroup(u);
		group->push_back(a);
	}
}

/* every hour, run this function which removes all entries over 3 days */
void MaintainWhoWas(time_t TIME)
{
	for (whowas_users::iterator iter = whowas.begin(); iter != whowas.end(); iter++)
	{
		whowas_set* n = (whowas_set*)iter->second;
		if (n->size())
		{
			while ((n->begin() != n->end()) && ((*n->begin())->signon < TIME - 259200)) // 3 days
			{
				WhoWasGroup *a = *(n->begin());
				DELETE(a);
				n->erase(n->begin());
			}
		}
	}
}

/* add a client connection to the sockets list */
void AddClient(int socket, int port, bool iscached, insp_inaddr ip)
{
	std::string tempnick = ConvToStr(socket) + "-unknown";
	user_hash::iterator iter = clientlist.find(tempnick);
	const char *ipaddr = insp_ntoa(ip);
	userrec* _new;
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
	if (iter != clientlist.end())
	{
		userrec* goner = iter->second;
		DELETE(goner);
		clientlist.erase(iter);
	}

	log(DEBUG,"AddClient: %d %d %s",socket,port,ipaddr);
	
	_new = new userrec();
	clientlist[tempnick] = _new;
	_new->fd = socket;
	strlcpy(_new->nick,tempnick.c_str(),NICKMAX-1);

	/* Smarter than your average bear^H^H^H^Hset of strlcpys. */
	for (const char* temp = ipaddr; *temp && j < 64; temp++, j++)
		_new->dhost[j] = _new->host[j] = *temp;
	_new->dhost[j] = _new->host[j] = 0;

	_new->server = FindServerNamePtr(Config->ServerName);
	/* We don't need range checking here, we KNOW 'unknown\0' will fit into the ident field. */
	strcpy(_new->ident, "unknown");

	_new->registered = REG_NONE;
	_new->signon = TIME + Config->dns_timeout;
	_new->lastping = 1;
	_new->SetSockAddr(AF_FAMILY, inet_ntoa(ip), port);

	// set the registration timeout for this user
	unsigned long class_regtimeout = 90;
	int class_flood = 0;
	long class_threshold = 5;
	long class_sqmax = 262144;      // 256kb
	long class_rqmax = 4096;	// 4k

	for (ClassVector::iterator i = Config->Classes.begin(); i != Config->Classes.end(); i++)
	{
		if ((i->type == CC_ALLOW) && (match(ipaddr,i->host.c_str())))
		{
			class_regtimeout = (unsigned long)i->registration_timeout;
			class_flood = i->flood;
			_new->pingmax = i->pingtime;
			class_threshold = i->threshold;
			class_sqmax = i->sendqmax;
			class_rqmax = i->recvqmax;
			break;
		}
	}

	_new->nping = TIME + _new->pingmax + Config->dns_timeout;
	_new->timeout = TIME+class_regtimeout;
	_new->flood = class_flood;
	_new->threshold = class_threshold;
	_new->sendqmax = class_sqmax;
	_new->recvqmax = class_rqmax;

	fd_ref_table[socket] = _new;
	local_users.push_back(_new);

	if (local_users.size() > Config->SoftLimit)
	{
		kill_link(_new,"No more connections allowed");
		return;
	}

	if (local_users.size() >= MAXCLIENTS)
	{
		kill_link(_new,"No more connections allowed");
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
		kill_link(_new,"Server is full");
		return;
	}
	char* e = matches_exception(ipaddr);
	if (!e)
	{
		char* r = matches_zline(ipaddr);
		if (r)
		{
			char reason[MAXBUF];
			snprintf(reason,MAXBUF,"Z-Lined: %s",r);
			kill_link(_new,reason);
			return;
		}
	}

	if (socket > -1)
	{
		ServerInstance->SE->AddFd(socket,true,X_ESTAB_CLIENT);
	}

	WriteServ(_new->fd,"NOTICE Auth :*** Looking up your hostname...");
}

long FindMatchingGlobal(userrec* user)
{
	long x = 0;
	for (user_hash::const_iterator a = clientlist.begin(); a != clientlist.end(); a++)
	{
#ifdef IPV6
		/* I dont think theres any faster way of matching two ipv6 addresses than memcmp
		 * Let me know if you think of one.
		  */
		in6_addr* s1 = &(((sockaddr_in*)&a->second->ip)->sin6_addr);
		in6_addr* s2 = &(((sockaddr_in*)&user->ip)->sin6_addr);
		if (!memcmp(s1->s6_addr, s2->s6_addr, sizeof(in6_addr)))
			x++;
#else
		in_addr* s1 = &((sockaddr_in*)&a->second->ip)->sin_addr;
		in_addr* s2 = &((sockaddr_in*)&user->ip)->sin_addr;
		if (s1->s_addr == s2->s_addr)
			x++;
#endif
	}
	return x;
}

long FindMatchingLocal(userrec* user)
{
	long x = 0;
	for (std::vector<userrec*>::const_iterator a = local_users.begin(); a != local_users.end(); a++)
	{
		userrec* comp = *a;
#ifdef IPV6
		/* I dont think theres any faster way of matching two ipv6 addresses than memcmp */
		in6_addr* s1 = &(((sockaddr_in*)&comp->ip)->sin6_addr);
		in6_addr* s2 = &(((sockaddr_in*)&user->ip)->sin6_addr);
		if (!memcmp(s1->s6_addr, s2->s6_addr, sizeof(in6_addr)))
			x++;
#else
		in_addr* s1 = &((sockaddr_in*)&comp->ip)->sin_addr;
		in_addr* s2 = &((sockaddr_in*)&user->ip)->sin_addr;
		if (s1->s_addr == s2->s_addr)
			x++;
#endif
	}
	return x;
}

void FullConnectUser(userrec* user, CullList* Goners)
{
	ServerInstance->stats->statsConnects++;
	user->idle_lastmsg = TIME;
	log(DEBUG,"ConnectUser: %s",user->nick);

	ConnectClass a = GetClass(user);
	
	if (a.type == CC_DENY)
	{
		Goners->AddItem(user,"Unauthorised connection");
		return;
	}
	
	if ((*(a.pass.c_str())) && (!user->haspassed))
	{
		Goners->AddItem(user,"Invalid password");
		return;
	}
	
	if (FindMatchingLocal(user) > a.maxlocal)
	{
		Goners->AddItem(user,"No more connections allowed from your host via this connect class (local)");
		WriteOpers("*** WARNING: maximum LOCAL connections (%ld) exceeded for IP %s",a.maxlocal,user->GetIPString());
		return;
	}
	else if (FindMatchingGlobal(user) > a.maxglobal)
	{
		Goners->AddItem(user,"No more connections allowed from your host via this connect class (global)");
		WriteOpers("*** WARNING: maximum GLOBAL connections (%ld) exceeded for IP %s",a.maxglobal,user->GetIPString());
		return;
	}

	char match_against[MAXBUF];
	snprintf(match_against,MAXBUF,"%s@%s",user->ident,user->host);
	char* e = matches_exception(match_against);
	
	if (!e)
	{
		char* r = matches_gline(match_against);
		
		if (r)
		{
			char reason[MAXBUF];
			snprintf(reason,MAXBUF,"G-Lined: %s",r);
			Goners->AddItem(user,reason);
			return;
		}
		
		r = matches_kline(match_against);
		
		if (r)
		{
			char reason[MAXBUF];
			snprintf(reason,MAXBUF,"K-Lined: %s",r);
			Goners->AddItem(user,reason);
			return;
		}
	}


	WriteServ(user->fd,"NOTICE Auth :Welcome to \002%s\002!",Config->Network);
	WriteServ(user->fd,"001 %s :Welcome to the %s IRC Network %s!%s@%s",user->nick,Config->Network,user->nick,user->ident,user->host);
	WriteServ(user->fd,"002 %s :Your host is %s, running version %s",user->nick,Config->ServerName,VERSION);
	WriteServ(user->fd,"003 %s :This server was created %s %s",user->nick,__TIME__,__DATE__);
	WriteServ(user->fd,"004 %s %s %s %s %s %s",user->nick,Config->ServerName,VERSION,ServerInstance->ModeGrok->UserModeList().c_str(),ServerInstance->ModeGrok->ChannelModeList().c_str(),+ServerInstance->ModeGrok->ParaModeList().c_str());
	
	// anfl @ #ratbox, efnet reminded me that according to the RFC this cant contain more than 13 tokens per line...
	// so i'd better split it :)
	std::stringstream out(Config->data005);
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
			WriteServ(user->fd,"005 %s %s:are supported by this server",user->nick,line5.c_str());
			line5 = "";
			token_counter = 0;
		}
	}
	
	ShowMOTD(user);

	/*
	 * fix 3 by brain, move registered = 7 below these so that spurious modes and host
	 * changes dont go out onto the network and produce 'fake direction'.
	 */
	FOREACH_MOD(I_OnUserConnect,OnUserConnect(user));
	FOREACH_MOD(I_OnGlobalConnect,OnGlobalConnect(user));
	user->registered = REG_ALL;
	WriteOpers("*** Client connecting on port %d: %s!%s@%s [%s]",user->GetPort(),user->nick,user->ident,user->host,user->GetIPString());
}

/** ReHashNick()
 * re-allocates a nick in the user_hash after they change nicknames,
 * returns a pointer to the new user as it may have moved
 */
userrec* ReHashNick(const char* Old, const char* New)
{
	//user_hash::iterator newnick;
	user_hash::iterator oldnick = clientlist.find(Old);

	log(DEBUG,"ReHashNick: %s %s",Old,New);

	if (!strcasecmp(Old,New))
	{
		log(DEBUG,"old nick is new nick, skipping");
		return oldnick->second;
	}

	if (oldnick == clientlist.end())
		return NULL; /* doesnt exist */

	log(DEBUG,"ReHashNick: Found hashed nick %s",Old);

	userrec* olduser = oldnick->second;
	clientlist[New] = olduser;
	clientlist.erase(oldnick);

	log(DEBUG,"ReHashNick: Nick rehashed as %s",New);

	return clientlist[New];
}

void force_nickchange(userrec* user,const char* newnick)
{
	char nick[MAXBUF];
	int MOD_RESULT = 0;

	*nick = 0;

	FOREACH_RESULT(I_OnUserPreNick,OnUserPreNick(user,newnick));
	
	if (MOD_RESULT)
	{
		ServerInstance->stats->statsCollisions++;
		kill_link(user,"Nickname collision");
		return;
	}
	
	if (matches_qline(newnick))
	{
		ServerInstance->stats->statsCollisions++;
		kill_link(user,"Nickname collision");
		return;
	}

	if (user)
	{
		if (newnick)
		{
			strlcpy(nick,newnick,MAXBUF-1);
		}

		if (user->registered == REG_ALL)
		{
			const char* pars[1];
			pars[0] = nick;
			std::string cmd = "NICK";

			ServerInstance->Parser->CallHandler(cmd,pars,1,user);
		}
	}
}

void userrec::SetSockAddr(int protocol_family, const char* ip, int port)
{
	switch (protocol_family)
	{
		case AF_INET6:
		{
			sockaddr_in6* sin = (sockaddr_in6*)&this->ip;
			sin->sin6_family = AF_INET6;
			sin->sin6_port = port;
			inet_pton(AF_INET6, ip, &sin->sin6_addr);
		}
		break;
		case AF_INET:
		{
			sockaddr_in* sin = (sockaddr_in*)&this->ip;
			sin->sin_family = AF_INET;
			sin->sin_port = port;
			inet_pton(AF_INET, ip, &sin->sin_addr);
		}
		break;
		default:
			log(DEBUG,"Ut oh, I dont know protocol %d to be set on '%s'!", protocol_family, this->nick);
		break;
	}
}

int userrec::GetPort()
{
	switch (this->GetProtocolFamily())
	{
		case AF_INET6:
		{
			sockaddr_in6* sin = (sockaddr_in6*)&this->ip;
			return sin->sin6_port;
		}
		break;
		case AF_INET:
		{
			sockaddr_in* sin = (sockaddr_in*)&this->ip;
			return sin->sin_port;
		}
		break;
		default:
			log(DEBUG,"Ut oh, '%s' has an unknown protocol family!",this->nick);
		break;
	}
	return 0;
}

int userrec::GetProtocolFamily()
{
	sockaddr_in* sin = (sockaddr_in*)&this->ip;
	return sin->sin_family;
}

const char* userrec::GetIPString()
{
	static char buf[1024];

	switch (this->GetProtocolFamily())
	{
		case AF_INET6:
		{
			sockaddr_in6* sin = (sockaddr_in6*)&this->ip;
			inet_ntop(sin->sin6_family, &sin->sin6_addr, buf, sizeof(buf));
			return buf;
		}
		break;
		case AF_INET:
		{
			sockaddr_in* sin = (sockaddr_in*)&this->ip;
			inet_ntop(sin->sin_family, &sin->sin_addr, buf, sizeof(buf));
			return buf;
		}
		break;
		default:
			log(DEBUG,"Ut oh, '%s' has an unknown protocol family!",this->nick);
		break;
	}
	return "";
}

