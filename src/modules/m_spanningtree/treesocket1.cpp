/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2008 InspIRCd Development Team
 * See: http://www.inspircd.org/wiki/index.php/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"
#include "commands/cmd_whois.h"
#include "commands/cmd_stats.h"
#include "socket.h"
#include "wildcard.h"
#include "xline.h"
#include "transport.h"
#include "m_hash.h"
#include "socketengine.h"

#include "m_spanningtree/main.h"
#include "m_spanningtree/utils.h"
#include "m_spanningtree/treeserver.h"
#include "m_spanningtree/link.h"
#include "m_spanningtree/treesocket.h"
#include "m_spanningtree/resolvers.h"
#include "m_spanningtree/handshaketimer.h"

/* $ModDep: m_spanningtree/timesynctimer.h m_spanningtree/resolvers.h m_spanningtree/main.h m_spanningtree/utils.h m_spanningtree/treeserver.h m_spanningtree/link.h m_spanningtree/treesocket.h m_hash.h */


/** Because most of the I/O gubbins are encapsulated within
 * BufferedSocket, we just call the superclass constructor for
 * most of the action, and append a few of our own values
 * to it.
 */
TreeSocket::TreeSocket(SpanningTreeUtilities* Util, InspIRCd* SI, std::string host, int port, bool listening, unsigned long maxtime, Module* HookMod)
	: BufferedSocket(SI, host, port, listening, maxtime), Utils(Util), Hook(HookMod)
{
	myhost = host;
	this->LinkState = LISTENER;
	theirchallenge.clear();
	ourchallenge.clear();
	if (listening && Hook)
		BufferedSocketHookRequest(this, (Module*)Utils->Creator, Hook).Send();
}

TreeSocket::TreeSocket(SpanningTreeUtilities* Util, InspIRCd* SI, std::string host, int port, bool listening, unsigned long maxtime, const std::string &ServerName, const std::string &bindto, Module* HookMod)
	: BufferedSocket(SI, host, port, listening, maxtime, bindto), Utils(Util), Hook(HookMod)
{
	myhost = ServerName;
	theirchallenge.clear();
	ourchallenge.clear();
	this->LinkState = CONNECTING;
	if (Hook)
		BufferedSocketHookRequest(this, (Module*)Utils->Creator, Hook).Send();
}

/** When a listening socket gives us a new file descriptor,
 * we must associate it with a socket without creating a new
 * connection. This constructor is used for this purpose.
 */
TreeSocket::TreeSocket(SpanningTreeUtilities* Util, InspIRCd* SI, int newfd, char* ip, Module* HookMod)
	: BufferedSocket(SI, newfd, ip), Utils(Util), Hook(HookMod)
{
	this->LinkState = WAIT_AUTH_1;
	theirchallenge.clear();
	ourchallenge.clear();
	sentcapab = false;
	/* If we have a transport module hooked to the parent, hook the same module to this
	 * socket, and set a timer waiting for handshake before we send CAPAB etc.
	 */
	if (Hook)
		BufferedSocketHookRequest(this, (Module*)Utils->Creator, Hook).Send();

	Instance->Timers->AddTimer(new HandshakeTimer(Instance, this, &(Utils->LinkBlocks[0]), this->Utils, 1));
}

ServerState TreeSocket::GetLinkState()
{
	return this->LinkState;
}

Module* TreeSocket::GetHook()
{
	return this->Hook;
}

TreeSocket::~TreeSocket()
{
	if (Hook)
		BufferedSocketUnhookRequest(this, (Module*)Utils->Creator, Hook).Send();
}

const std::string& TreeSocket::GetOurChallenge()
{
	return this->ourchallenge;
}

void TreeSocket::SetOurChallenge(const std::string &c)
{
	this->ourchallenge = c;
}

const std::string& TreeSocket::GetTheirChallenge()
{
	return this->theirchallenge;
}

void TreeSocket::SetTheirChallenge(const std::string &c)
{
	this->theirchallenge = c;
}

std::string TreeSocket::MakePass(const std::string &password, const std::string &challenge)
{
	/* This is a simple (maybe a bit hacky?) HMAC algorithm, thanks to jilles for
	 * suggesting the use of HMAC to secure the password against various attacks.
	 *
	 * Note: If m_sha256.so is not loaded, we MUST fall back to plaintext with no
	 *       HMAC challenge/response.
	 */
	Module* sha256 = Instance->Modules->Find("m_sha256.so");
	if (Utils->ChallengeResponse && sha256 && !challenge.empty())
	{
		/* XXX: This is how HMAC is supposed to be done:
		 *
		 * sha256( (pass xor 0x5c) + sha256((pass xor 0x36) + m) )
		 *
		 * Note that we are encoding the hex hash, not the binary
		 * output of the hash which is slightly different to standard.
		 *
		 * Don't ask me why its always 0x5c and 0x36... it just is.
		 */
		std::string hmac1, hmac2;

		for (size_t n = 0; n < password.length(); n++)
		{
			hmac1 += static_cast<char>(password[n] ^ 0x5C);
			hmac2 += static_cast<char>(password[n] ^ 0x36);
		}

		hmac2 += challenge;
		HashResetRequest(Utils->Creator, sha256).Send();
		hmac2 = HashSumRequest(Utils->Creator, sha256, hmac2).Send();

		HashResetRequest(Utils->Creator, sha256).Send();
		std::string hmac = hmac1 + hmac2;
		hmac = HashSumRequest(Utils->Creator, sha256, hmac).Send();

		return "HMAC-SHA256:"+ hmac;
	}
	else if (!challenge.empty() && !sha256)
		Instance->Log(DEFAULT,"Not authenticating to server using SHA256/HMAC because we don't have m_sha256 loaded!");

	return password;
}

/** When an outbound connection finishes connecting, we receive
 * this event, and must send our SERVER string to the other
 * side. If the other side is happy, as outlined in the server
 * to server docs on the inspircd.org site, the other side
 * will then send back its own server string.
 */
bool TreeSocket::OnConnected()
{
	if (this->LinkState == CONNECTING)
	{
		/* we do not need to change state here. */
		for (std::vector<Link>::iterator x = Utils->LinkBlocks.begin(); x < Utils->LinkBlocks.end(); x++)
		{
			if (x->Name == this->myhost)
			{
				Utils->Creator->RemoteMessage(NULL,"Connection to \2%s\2[%s] started.", myhost.c_str(), (x->HiddenFromStats ? "<hidden>" : this->GetIP().c_str()));
				if (Hook)
				{
					BufferedSocketHookRequest(this, (Module*)Utils->Creator, Hook).Send();
					Utils->Creator->RemoteMessage(NULL,"Connection to \2%s\2[%s] using transport \2%s\2", myhost.c_str(), (x->HiddenFromStats ? "<hidden>" : this->GetIP().c_str()),
							x->Hook.c_str());
				}
				this->OutboundPass = x->SendPass;
				sentcapab = false;

				/* found who we're supposed to be connecting to, send the neccessary gubbins. */
				if (this->GetHook())
					Instance->Timers->AddTimer(new HandshakeTimer(Instance, this, &(*x), this->Utils, 1));
				else
					this->SendCapabilities();

				return true;
			}
		}
	}
	/* There is a (remote) chance that between the /CONNECT and the connection
	 * being accepted, some muppet has removed the <link> block and rehashed.
	 * If that happens the connection hangs here until it's closed. Unlikely
	 * and rather harmless.
	 */
	this->Utils->Creator->RemoteMessage(NULL,"Connection to \2%s\2 lost link tag(!)", myhost.c_str());
	return true;
}

void TreeSocket::OnError(BufferedSocketError e)
{
	Link* MyLink;

	if (this->LinkState == LISTENER)
		return;

	switch (e)
	{
		case I_ERR_CONNECT:
			Utils->Creator->RemoteMessage(NULL,"Connection failed: Connection to \002%s\002 refused", myhost.c_str());
			MyLink = Utils->FindLink(myhost);
			if (MyLink)
				Utils->DoFailOver(MyLink);
		break;
		case I_ERR_SOCKET:
			Utils->Creator->RemoteMessage(NULL,"Connection failed: Could not create socket");
		break;
		case I_ERR_BIND:
			Utils->Creator->RemoteMessage(NULL,"Connection failed: Error binding socket to address or port");
		break;
		case I_ERR_WRITE:
			Utils->Creator->RemoteMessage(NULL,"Connection failed: I/O error on connection");
		break;
		case I_ERR_NOMOREFDS:
			Utils->Creator->RemoteMessage(NULL,"Connection failed: Operating system is out of file descriptors!");
		break;
		default:
			if ((errno) && (errno != EINPROGRESS) && (errno != EAGAIN))
				Utils->Creator->RemoteMessage(NULL,"Connection to \002%s\002 failed with OS error: %s", myhost.c_str(), strerror(errno));
		break;
	}
}

int TreeSocket::OnDisconnect()
{
	/* For the same reason as above, we don't
	 * handle OnDisconnect()
	 */
	return true;
}

/** Recursively send the server tree with distances as hops.
 * This is used during network burst to inform the other server
 * (and any of ITS servers too) of what servers we know about.
 * If at any point any of these servers already exist on the other
 * end, our connection may be terminated. The hopcounts given
 * by this function are relative, this doesn't matter so long as
 * they are all >1, as all the remote servers re-calculate them
 * to be relative too, with themselves as hop 0.
 */
void TreeSocket::SendServers(TreeServer* Current, TreeServer* s, int hops)
{
	char command[1024];
	for (unsigned int q = 0; q < Current->ChildCount(); q++)
	{
		TreeServer* recursive_server = Current->GetChild(q);
		if (recursive_server != s)
		{
			snprintf(command,1024,":%s SERVER %s * %d %s :%s",Current->GetName().c_str(),recursive_server->GetName().c_str(),hops,
					recursive_server->GetID().c_str(),
					recursive_server->GetDesc().c_str());
			this->WriteLine(command);
			this->WriteLine(":"+recursive_server->GetName()+" VERSION :"+recursive_server->GetVersion());
			/* down to next level */
			this->SendServers(recursive_server, s, hops+1);
		}
	}
}

std::string TreeSocket::RandString(unsigned int length)
{
	char* randombuf = new char[length+1];
	std::string out;
#ifdef WINDOWS
	int fd = -1;
#else
	int fd = open("/dev/urandom", O_RDONLY, 0);
#endif

	if (fd >= 0)
	{
#ifndef WINDOWS
		read(fd, randombuf, length);
		close(fd);
#endif
	}
	else
	{
		for (unsigned int i = 0; i < length; i++)
			randombuf[i] = rand();
	}

	for (unsigned int i = 0; i < length; i++)
	{
		char randchar = static_cast<char>((randombuf[i] & 0x7F) | 0x21);
		out += (randchar == '=' ? '_' : randchar);
	}

	delete[] randombuf;
	return out;
}

void TreeSocket::SendError(const std::string &errormessage)
{
	/* Display the error locally as well as sending it remotely */
	Utils->Creator->RemoteMessage(NULL, "Sent \2ERROR\2 to %s: %s", (this->InboundServerName.empty() ? "<unknown>" : this->InboundServerName.c_str()), errormessage.c_str());
	this->WriteLine("ERROR :"+errormessage);
	/* One last attempt to make sure the error reaches its target */
	this->FlushWriteBuffer();
}

/** This function forces this server to quit, removing this server
 * and any users on it (and servers and users below that, etc etc).
 * It's very slow and pretty clunky, but luckily unless your network
 * is having a REAL bad hair day, this function shouldnt be called
 * too many times a month ;-)
 */
void TreeSocket::SquitServer(std::string &from, TreeServer* Current)
{
	/* recursively squit the servers attached to 'Current'.
	 * We're going backwards so we don't remove users
	 * while we still need them ;)
	 */
	for (unsigned int q = 0; q < Current->ChildCount(); q++)
	{
		TreeServer* recursive_server = Current->GetChild(q);
		this->SquitServer(from,recursive_server);
	}
	/* Now we've whacked the kids, whack self */
	num_lost_servers++;
	num_lost_users += Current->QuitUsers(from);
}

/** This is a wrapper function for SquitServer above, which
 * does some validation first and passes on the SQUIT to all
 * other remaining servers.
 */
void TreeSocket::Squit(TreeServer* Current, const std::string &reason)
{
	if ((Current) && (Current != Utils->TreeRoot))
	{
		Event rmode((char*)Current->GetName().c_str(), (Module*)Utils->Creator, "lost_server");
		rmode.Send(Instance);

		std::deque<std::string> params;
		params.push_back(Current->GetName());
		params.push_back(":"+reason);
		Utils->DoOneToAllButSender(Current->GetParent()->GetName(),"SQUIT",params,Current->GetName());
		if (Current->GetParent() == Utils->TreeRoot)
		{
			this->Instance->SNO->WriteToSnoMask('l',"Server \002"+Current->GetName()+"\002 split: "+reason);
		}
		else
		{
			this->Instance->SNO->WriteToSnoMask('l',"Server \002"+Current->GetName()+"\002 split from server \002"+Current->GetParent()->GetName()+"\002 with reason: "+reason);
		}
		num_lost_servers = 0;
		num_lost_users = 0;
		std::string from = Current->GetParent()->GetName()+" "+Current->GetName();
		SquitServer(from, Current);
		Current->Tidy();
		Current->GetParent()->DelChild(Current);
		delete Current;
		this->Instance->SNO->WriteToSnoMask('l',"Netsplit complete, lost \002%d\002 users on \002%d\002 servers.", num_lost_users, num_lost_servers);
	}
	else
		Instance->Log(DEFAULT,"Squit from unknown server");
}

/** Send one or more FJOINs for a channel of users.
 * If the length of a single line is more than 480-NICKMAX
 * in length, it is split over multiple lines.
 */
void TreeSocket::SendFJoins(TreeServer* Current, Channel* c)
{
	std::string buffer;
	char list[MAXBUF];
	std::string individual_halfops = std::string(":")+this->Instance->Config->GetSID()+" FMODE "+c->name+" "+ConvToStr(c->age);

	size_t dlen, curlen;
	dlen = curlen = snprintf(list,MAXBUF,":%s FJOIN %s %lu",this->Instance->Config->GetSID().c_str(),c->name,(unsigned long)c->age);
	int numusers = 1;
	char* ptr = list + dlen;

	CUList *ulist = c->GetUsers();
	std::string modes;
	std::string params;

	for (CUList::iterator i = ulist->begin(); i != ulist->end(); i++)
	{
		// The first parameter gets a : before it
		size_t ptrlen = snprintf(ptr, MAXBUF, " %s%s,%s", !numusers ? ":" : "", c->GetAllPrefixChars(i->first), i->first->uuid);

		curlen += ptrlen;
		ptr += ptrlen;

		numusers++;

		if (curlen > (480-NICKMAX))
		{
			buffer.append(list).append("\r\n");
			dlen = curlen = snprintf(list,MAXBUF,":%s FJOIN %s %lu",this->Instance->Config->GetSID().c_str(),c->name,(unsigned long)c->age);
			ptr = list + dlen;
			ptrlen = 0;
			numusers = 0;
		}
	}

	if (numusers)
		buffer.append(list).append("\r\n");

	buffer.append(":").append(this->Instance->Config->GetSID()).append(" FMODE ").append(c->name).append(" ").append(ConvToStr(c->age)).append(" +").append(c->ChanModes(true)).append("\r\n");

	int linesize = 1;
	for (BanList::iterator b = c->bans.begin(); b != c->bans.end(); b++)
	{
		int size = strlen(b->data) + 2;
		int currsize = linesize + size;
		if (currsize <= 350)
		{
			modes.append("b");
			params.append(" ").append(b->data);
			linesize += size; 
		}
		if ((params.length() >= MAXMODES) || (currsize > 350))
		{
			/* Wrap at MAXMODES */
			buffer.append(":").append(this->Instance->Config->GetSID()).append(" FMODE ").append(c->name).append(" ").append(ConvToStr(c->age)).append(" +").append(modes).append(params).append("\r\n");
			modes.clear();
			params.clear();
			linesize = 1;
		}
	}

	/* Only send these if there are any */
	if (!modes.empty())
		buffer.append(":").append(this->Instance->Config->GetSID()).append(" FMODE ").append(c->name).append(" ").append(ConvToStr(c->age)).append(" +").append(modes).append(params);

	this->WriteLine(buffer);
}

/** Send G, Q, Z and E lines */
void TreeSocket::SendXLines(TreeServer* Current)
{
	char data[MAXBUF];
	std::string buffer;
	std::string n = this->Instance->Config->GetSID();
	const char* sn = n.c_str();

	std::vector<std::string> types = Instance->XLines->GetAllTypes();

	for (std::vector<std::string>::iterator it = types.begin(); it != types.end(); ++it)
	{
		XLineLookup* lookup = Instance->XLines->GetAll(*it);

		if (lookup)
		{
			for (LookupIter i = lookup->begin(); i != lookup->end(); ++i)
			{
				snprintf(data,MAXBUF,":%s ADDLINE %s %s %s %lu %lu :%s\r\n",sn, it->c_str(), i->second->Displayable(),
						i->second->source,
						(unsigned long)i->second->set_time,
						(unsigned long)i->second->duration,
						i->second->reason);
				buffer.append(data);
			}
		}
	}

	if (!buffer.empty())
		this->WriteLine(buffer);
}

/** Send channel modes and topics */
void TreeSocket::SendChannelModes(TreeServer* Current)
{
	char data[MAXBUF];
	std::deque<std::string> list;
	std::string n = this->Instance->Config->GetSID();
	const char* sn = n.c_str();
	Instance->Log(DEBUG,"Sending channels and modes, %d to send", this->Instance->chanlist->size());
	for (chan_hash::iterator c = this->Instance->chanlist->begin(); c != this->Instance->chanlist->end(); c++)
	{
		SendFJoins(Current, c->second);
		if (*c->second->topic)
		{
			snprintf(data,MAXBUF,":%s FTOPIC %s %lu %s :%s",sn,c->second->name,(unsigned long)c->second->topicset,c->second->setby,c->second->topic);
			this->WriteLine(data);
		}
		FOREACH_MOD_I(this->Instance,I_OnSyncChannel,OnSyncChannel(c->second,(Module*)Utils->Creator,(void*)this));
		list.clear();
		c->second->GetExtList(list);
		for (unsigned int j = 0; j < list.size(); j++)
		{
			FOREACH_MOD_I(this->Instance,I_OnSyncChannelMetaData,OnSyncChannelMetaData(c->second,(Module*)Utils->Creator,(void*)this,list[j]));
		}
	}
}

/** send all users and their oper state/modes */
void TreeSocket::SendUsers(TreeServer* Current)
{
	char data[MAXBUF];
	std::deque<std::string> list;
	std::string dataline;
	for (user_hash::iterator u = this->Instance->Users->clientlist->begin(); u != this->Instance->Users->clientlist->end(); u++)
	{
		if (u->second->registered == REG_ALL)
		{
			TreeServer* theirserver = Utils->FindServer(u->second->server);
			if (theirserver)
			{
				snprintf(data,MAXBUF,":%s UID %s %lu %s %s %s %s +%s %s %lu :%s", theirserver->GetID().c_str(), u->second->uuid,
						(unsigned long)u->second->age, u->second->nick, u->second->host, u->second->dhost,
						u->second->ident, u->second->FormatModes(), u->second->GetIPString(),
						(unsigned long)u->second->signon, u->second->fullname);
				this->WriteLine(data);
				if (*u->second->oper)
				{
					snprintf(data,MAXBUF,":%s OPERTYPE %s", u->second->uuid, u->second->oper);
					this->WriteLine(data);
				}
				if (*u->second->awaymsg)
				{
					snprintf(data,MAXBUF,":%s AWAY :%s", u->second->uuid, u->second->awaymsg);
					this->WriteLine(data);
				}
			}

			FOREACH_MOD_I(this->Instance,I_OnSyncUser,OnSyncUser(u->second,(Module*)Utils->Creator,(void*)this));
			list.clear();
			u->second->GetExtList(list);
			for (unsigned int j = 0; j < list.size(); j++)
			{
				FOREACH_MOD_I(this->Instance,I_OnSyncUserMetaData,OnSyncUserMetaData(u->second,(Module*)Utils->Creator,(void*)this,list[j]));
			}
		}
	}
}

/** This function is called when we want to send a netburst to a local
 * server. There is a set order we must do this, because for example
 * users require their servers to exist, and channels require their
 * users to exist. You get the idea.
 */
void TreeSocket::DoBurst(TreeServer* s)
{
	std::string name = s->GetName();
	std::string burst = ":" + this->Instance->Config->GetSID() + " BURST " +ConvToStr(Instance->Time(true));
	std::string endburst = ":" + this->Instance->Config->GetSID() + " ENDBURST";
	this->Instance->SNO->WriteToSnoMask('l',"Bursting to \2%s\2 (Authentication: %s).", name.c_str(), this->GetTheirChallenge().empty() ? "plaintext password" : "SHA256-HMAC challenge-response");
	this->WriteLine(burst);
	/* send our version string */
	this->WriteLine(std::string(":")+this->Instance->Config->GetSID()+" VERSION :"+this->Instance->GetVersionString());
	/* Send server tree */
	this->SendServers(Utils->TreeRoot,s,1);
	/* Send users and their oper status */
	this->SendUsers(s);
	/* Send everything else (channel modes, xlines etc) */
	this->SendChannelModes(s);
	this->SendXLines(s);
	FOREACH_MOD_I(this->Instance,I_OnSyncOtherMetaData,OnSyncOtherMetaData((Module*)Utils->Creator,(void*)this));
	this->WriteLine(endburst);
	this->Instance->SNO->WriteToSnoMask('l',"Finished bursting to \2"+name+"\2.");
}

/** This function is called when we receive data from a remote
 * server. We buffer the data in a std::string (it doesnt stay
 * there for long), reading using BufferedSocket::Read() which can
 * read up to 16 kilobytes in one operation.
 *
 * IF THIS FUNCTION RETURNS FALSE, THE CORE CLOSES AND DELETES
 * THE SOCKET OBJECT FOR US.
 */
bool TreeSocket::OnDataReady()
{
	char* data = this->Read();
	/* Check that the data read is a valid pointer and it has some content */
	if (data && *data)
	{
		this->in_buffer.append(data);
		/* While there is at least one new line in the buffer,
		 * do something useful (we hope!) with it.
		 */
		while (in_buffer.find("\n") != std::string::npos)
		{
			std::string ret = in_buffer.substr(0,in_buffer.find("\n")-1);
			in_buffer = in_buffer.substr(in_buffer.find("\n")+1,in_buffer.length()-in_buffer.find("\n"));
			/* Use rfind here not find, as theres more
			 * chance of the \r being near the end of the
			 * string, not the start.
			 */
			if (ret.find("\r") != std::string::npos)
				ret = in_buffer.substr(0,in_buffer.find("\r")-1);
			/* Process this one, abort if it
			 * didnt return true.
			 */
			if (!this->ProcessLine(ret))
			{
				return false;
			}
		}
		return true;
	}
	/* EAGAIN returns an empty but non-NULL string, so this
	 * evaluates to TRUE for EAGAIN but to FALSE for EOF.
	 */
	return (data && !*data);
}
