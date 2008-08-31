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
#include "xline.h"

#include "m_spanningtree/treesocket.h"
#include "m_spanningtree/treeserver.h"
#include "m_spanningtree/utils.h"

/* $ModDep: m_spanningtree/utils.h m_spanningtree/treeserver.h m_spanningtree/treesocket.h */


/** This function is called when we want to send a netburst to a local
 * server. There is a set order we must do this, because for example
 * users require their servers to exist, and channels require their
 * users to exist. You get the idea.
 */
void TreeSocket::DoBurst(TreeServer* s)
{
	std::string name = s->GetName();
	std::string burst = ":" + this->Instance->Config->GetSID() + " BURST " +ConvToStr(Instance->Time());
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

	size_t dlen, curlen;
	dlen = curlen = snprintf(list,MAXBUF,":%s FJOIN %s %lu +%s", this->Instance->Config->GetSID().c_str(), c->name.c_str(),(unsigned long)c->age, c->ChanModes(true));
	int numusers = 0;
	char* ptr = list + dlen;
	bool looped_once = false;

	CUList *ulist = c->GetUsers();
	std::string modes;
	std::string params;

	for (CUList::iterator i = ulist->begin(); i != ulist->end(); i++)
	{
		size_t ptrlen = 0;
		std::string modestr = this->Instance->Modes->ModeString(i->first, c, false);

		if ((curlen + modestr.length() + i->first->uuid.length() + 4) > 480)
		{
			buffer.append(list).append("\r\n");
			dlen = curlen = snprintf(list, MAXBUF, ":%s FJOIN %s %lu +%s", this->Instance->Config->GetSID().c_str(), c->name.c_str(), (unsigned long)c->age, c->ChanModes(true));
			ptr = list + dlen;
			numusers = 0;
		}

		// The first parameter gets a : before it
		ptrlen = snprintf(ptr, MAXBUF, " %s%s,%s", !numusers ? ":" : "", modestr.c_str(), i->first->uuid.c_str());

		looped_once = true;

		curlen += ptrlen;
		ptr += ptrlen;

		numusers++;
	}

	// Okay, permanent channels will (of course) need this \r\n anyway, numusers check is if there
	// actually were people in the channel (looped_once == true)
	if (!looped_once || numusers > 0)
		buffer.append(list).append("\r\n");

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
		if ((params.length() >= Instance->Config->Limits.MaxModes) || (currsize > 350))
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
				/* Is it burstable? this is better than an explicit check for type 'K'.
				 * We break the loop as NONE of the items in this group are worth iterating.
				 */
				if (!i->second->IsBurstable())
					break;

				snprintf(data,MAXBUF,":%s ADDLINE %s %s %s %lu %lu :%s",sn, it->c_str(), i->second->Displayable(),
						i->second->source,
						(unsigned long)i->second->set_time,
						(unsigned long)i->second->duration,
						i->second->reason);
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
	std::string n = this->Instance->Config->GetSID();
	const char* sn = n.c_str();
	for (chan_hash::iterator c = this->Instance->chanlist->begin(); c != this->Instance->chanlist->end(); c++)
	{
		SendFJoins(Current, c->second);
		if (!c->second->topic.empty())
		{
			snprintf(data,MAXBUF,":%s FTOPIC %s %lu %s :%s", sn, c->second->name.c_str(), (unsigned long)c->second->topicset, c->second->setby.c_str(), c->second->topic.c_str());
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
				snprintf(data,MAXBUF,":%s UID %s %lu %s %s %s %s +%s +%s %s %lu :%s", theirserver->GetID().c_str(), u->second->uuid.c_str(),
						(unsigned long)u->second->age, u->second->nick.c_str(), u->second->host.c_str(), u->second->dhost.c_str(),
						u->second->ident.c_str(), u->second->FormatModes(), u->second->FormatNoticeMasks(), u->second->GetIPString(),
						(unsigned long)u->second->signon, u->second->fullname.c_str());
				this->WriteLine(data);
				if (IS_OPER(u->second))
				{
					snprintf(data,MAXBUF,":%s OPERTYPE %s", u->second->uuid.c_str(), u->second->oper.c_str());
					this->WriteLine(data);
				}
				if (IS_AWAY(u->second))
				{
					snprintf(data,MAXBUF,":%s AWAY :%s", u->second->uuid.c_str(), u->second->awaymsg.c_str());
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

