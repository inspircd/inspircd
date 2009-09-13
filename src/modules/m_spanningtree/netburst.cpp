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

#include "inspircd.h"
#include "xline.h"

#include "treesocket.h"
#include "treeserver.h"
#include "utils.h"
#include "main.h"

/* $ModDep: m_spanningtree/utils.h m_spanningtree/treeserver.h m_spanningtree/treesocket.h */


/** This function is called when we want to send a netburst to a local
 * server. There is a set order we must do this, because for example
 * users require their servers to exist, and channels require their
 * users to exist. You get the idea.
 */
void TreeSocket::DoBurst(TreeServer* s)
{
	std::string name = s->GetName();
	std::string burst = ":" + this->ServerInstance->Config->GetSID() + " BURST " +ConvToStr(ServerInstance->Time());
	std::string endburst = ":" + this->ServerInstance->Config->GetSID() + " ENDBURST";
	this->ServerInstance->SNO->WriteToSnoMask('l',"Bursting to \2%s\2 (Authentication: %s%s).",
		name.c_str(),
		this->auth_fingerprint ? "SSL Fingerprint and " : "",
		this->auth_challenge ? "challenge-response" : "plaintext password");
	this->CleanNegotiationInfo();
	this->WriteLine(burst);
	/* send our version string */
	this->WriteLine(std::string(":")+this->ServerInstance->Config->GetSID()+" VERSION :"+this->ServerInstance->GetVersionString());
	/* Send server tree */
	this->SendServers(Utils->TreeRoot,s,1);
	/* Send users and their oper status */
	this->SendUsers(s);
	/* Send everything else (channel modes, xlines etc) */
	this->SendChannelModes(s);
	this->SendXLines(s);
	FOREACH_MOD_I(this->ServerInstance,I_OnSyncNetwork,OnSyncNetwork(Utils->Creator,(void*)this));
	this->WriteLine(endburst);
	this->ServerInstance->SNO->WriteToSnoMask('l',"Finished bursting to \2"+name+"\2.");
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

/** Send one or more FJOINs for a channel of users.
 * If the length of a single line is more than 480-NICKMAX
 * in length, it is split over multiple lines.
 */
void TreeSocket::SendFJoins(TreeServer* Current, Channel* c)
{
	std::string buffer;
	char list[MAXBUF];

	size_t curlen, headlen;
	curlen = headlen = snprintf(list,MAXBUF,":%s FJOIN %s %lu +%s :",
		this->ServerInstance->Config->GetSID().c_str(), c->name.c_str(), (unsigned long)c->age, c->ChanModes(true));
	int numusers = 0;
	char* ptr = list + curlen;
	bool looped_once = false;

	CUList *ulist = c->GetUsers();
	std::string modes;
	std::string params;

	for (CUList::iterator i = ulist->begin(); i != ulist->end(); i++)
	{
		size_t ptrlen = 0;
		std::string modestr = this->ServerInstance->Modes->ModeString(i->first, c, false);

		if ((curlen + modestr.length() + i->first->uuid.length() + 4) > 480)
		{
			// remove the final space
			if (ptr[-1] == ' ')
				ptr[-1] = '\0';
			buffer.append(list).append("\r\n");
			curlen = headlen;
			ptr = list + headlen;
			numusers = 0;
		}

		ptrlen = snprintf(ptr, MAXBUF-curlen, "%s,%s ", modestr.c_str(), i->first->uuid.c_str());

		looped_once = true;

		curlen += ptrlen;
		ptr += ptrlen;

		numusers++;
	}

	// Okay, permanent channels will (of course) need this \r\n anyway, numusers check is if there
	// actually were people in the channel (looped_once == true)
	if (!looped_once || numusers > 0)
	{
		// remove the final space
		if (ptr[-1] == ' ')
			ptr[-1] = '\0';
		buffer.append(list).append("\r\n");
	}

	int linesize = 1;
	for (BanList::iterator b = c->bans.begin(); b != c->bans.end(); b++)
	{
		int size = b->data.length() + 2;
		int currsize = linesize + size;
		if (currsize <= 350)
		{
			modes.append("b");
			params.append(" ").append(b->data);
			linesize += size;
		}
		if ((params.length() >= ServerInstance->Config->Limits.MaxModes) || (currsize > 350))
		{
			/* Wrap at MAXMODES */
			buffer.append(":").append(this->ServerInstance->Config->GetSID()).append(" FMODE ").append(c->name).append(" ").append(ConvToStr(c->age)).append(" +").append(modes).append(params).append("\r\n");
			modes.clear();
			params.clear();
			linesize = 1;
		}
	}

	/* Only send these if there are any */
	if (!modes.empty())
		buffer.append(":").append(this->ServerInstance->Config->GetSID()).append(" FMODE ").append(c->name).append(" ").append(ConvToStr(c->age)).append(" +").append(modes).append(params);

	this->WriteLine(buffer);
}

/** Send G, Q, Z and E lines */
void TreeSocket::SendXLines(TreeServer* Current)
{
	char data[MAXBUF];
	std::string n = this->ServerInstance->Config->GetSID();
	const char* sn = n.c_str();

	std::vector<std::string> types = ServerInstance->XLines->GetAllTypes();
	time_t current = ServerInstance->Time();

	for (std::vector<std::string>::iterator it = types.begin(); it != types.end(); ++it)
	{
		XLineLookup* lookup = ServerInstance->XLines->GetAll(*it);

		if (lookup)
		{
			for (LookupIter i = lookup->begin(); i != lookup->end(); ++i)
			{
				/* Is it burstable? this is better than an explicit check for type 'K'.
				 * We break the loop as NONE of the items in this group are worth iterating.
				 */
				if (!i->second->IsBurstable())
					break;

				/* If it's expired, don't bother to burst it
				 */
				if (i->second->duration && current > i->second->expiry)
					continue;

				snprintf(data,MAXBUF,":%s ADDLINE %s %s %s %lu %lu :%s",sn, it->c_str(), i->second->Displayable(),
						i->second->source.c_str(),
						(unsigned long)i->second->set_time,
						(unsigned long)i->second->duration,
						i->second->reason.c_str());
				this->WriteLine(data);
			}
		}
	}
}

/** Send channel modes and topics */
void TreeSocket::SendChannelModes(TreeServer* Current)
{
	char data[MAXBUF];
	std::deque<std::string> list;
	std::string n = this->ServerInstance->Config->GetSID();
	const char* sn = n.c_str();
	for (chan_hash::iterator c = this->ServerInstance->chanlist->begin(); c != this->ServerInstance->chanlist->end(); c++)
	{
		SendFJoins(Current, c->second);
		if (!c->second->topic.empty())
		{
			snprintf(data,MAXBUF,":%s FTOPIC %s %lu %s :%s", sn, c->second->name.c_str(), (unsigned long)c->second->topicset, c->second->setby.c_str(), c->second->topic.c_str());
			this->WriteLine(data);
		}

		for(ExtensibleStore::const_iterator i = c->second->GetExtList().begin(); i != c->second->GetExtList().end(); i++)
		{
			ExtensionItem* item = Extensible::GetItem(i->first);
			std::string value;
			if (item)
				value = item->serialize(Utils->Creator, c->second, i->second);
			if (!value.empty())
				Utils->Creator->ProtoSendMetaData(this, c->second, i->first, value);
		}

		FOREACH_MOD_I(this->ServerInstance,I_OnSyncChannel,OnSyncChannel(c->second,Utils->Creator,this));
	}
}

/** send all users and their oper state/modes */
void TreeSocket::SendUsers(TreeServer* Current)
{
	char data[MAXBUF];
	std::string dataline;
	for (user_hash::iterator u = this->ServerInstance->Users->clientlist->begin(); u != this->ServerInstance->Users->clientlist->end(); u++)
	{
		if (u->second->registered == REG_ALL)
		{
			TreeServer* theirserver = Utils->FindServer(u->second->server);
			if (theirserver)
			{
				snprintf(data,MAXBUF,":%s UID %s %lu %s %s %s %s %s %lu +%s :%s",
						theirserver->GetID().c_str(),	/* Prefix: SID */
						u->second->uuid.c_str(),	/* 0: UUID */
						(unsigned long)u->second->age,	/* 1: TS */
						u->second->nick.c_str(),	/* 2: Nick */
						u->second->host.c_str(),	/* 3: Displayed Host */
						u->second->dhost.c_str(),	/* 4: Real host */
						u->second->ident.c_str(),	/* 5: Ident */
						u->second->GetIPString(),	/* 6: IP string */
						(unsigned long)u->second->signon, /* 7: Signon time for WHOWAS */
						u->second->FormatModes(true),	/* 8...n: Modes and params */
						u->second->fullname.c_str());	/* size-1: GECOS */
				this->WriteLine(data);
				if (IS_OPER(u->second))
				{
					snprintf(data,MAXBUF,":%s OPERTYPE %s", u->second->uuid.c_str(), u->second->oper.c_str());
					this->WriteLine(data);
				}
				if (IS_AWAY(u->second))
				{
					snprintf(data,MAXBUF,":%s AWAY %ld :%s", u->second->uuid.c_str(), (long)u->second->awaytime, u->second->awaymsg.c_str());
					this->WriteLine(data);
				}
			}

			for(ExtensibleStore::const_iterator i = u->second->GetExtList().begin(); i != u->second->GetExtList().end(); i++)
			{
				ExtensionItem* item = Extensible::GetItem(i->first);
				std::string value;
				if (item)
					value = item->serialize(Utils->Creator, u->second, i->second);
				if (!value.empty())
					Utils->Creator->ProtoSendMetaData(this, u->second, i->first, value);
			}

			FOREACH_MOD_I(this->ServerInstance,I_OnSyncUser,OnSyncUser(u->second,Utils->Creator,this));
		}
	}
}

