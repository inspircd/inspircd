/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2008 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2008 Craig Edwards <craigedwards@brainbox.cc>
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
#include "xline.h"
#include "listmode.h"

#include "treesocket.h"
#include "treeserver.h"
#include "main.h"

/** This function is called when we want to send a netburst to a local
 * server. There is a set order we must do this, because for example
 * users require their servers to exist, and channels require their
 * users to exist. You get the idea.
 */
void TreeSocket::DoBurst(TreeServer* s)
{
	std::string servername = s->GetName();
	ServerInstance->SNO->WriteToSnoMask('l',"Bursting to \2%s\2 (Authentication: %s%s).",
		servername.c_str(),
		capab->auth_fingerprint ? "SSL Fingerprint and " : "",
		capab->auth_challenge ? "challenge-response" : "plaintext password");
	this->CleanNegotiationInfo();
	this->WriteLine(":" + ServerInstance->Config->GetSID() + " BURST " + ConvToStr(ServerInstance->Time()));
	/* send our version string */
	this->WriteLine(":" + ServerInstance->Config->GetSID() + " VERSION :"+ServerInstance->GetVersionString());
	/* Send server tree */
	this->SendServers(Utils->TreeRoot,s,1);
	/* Send users and their oper status */
	this->SendUsers();

	for (chan_hash::const_iterator i = ServerInstance->chanlist->begin(); i != ServerInstance->chanlist->end(); ++i)
		SyncChannel(i->second);

	this->SendXLines();
	FOREACH_MOD(I_OnSyncNetwork,OnSyncNetwork(Utils->Creator,(void*)this));
	this->WriteLine(":" + ServerInstance->Config->GetSID() + " ENDBURST");
	ServerInstance->SNO->WriteToSnoMask('l',"Finished bursting to \2"+ s->GetName()+"\2.");
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
	char command[MAXBUF];
	for (unsigned int q = 0; q < Current->ChildCount(); q++)
	{
		TreeServer* recursive_server = Current->GetChild(q);
		if (recursive_server != s)
		{
			std::string recursive_servername = recursive_server->GetName();
			snprintf(command, MAXBUF, ":%s SERVER %s * %d %s :%s", Current->GetID().c_str(), recursive_servername.c_str(), hops,
					recursive_server->GetID().c_str(),
					recursive_server->GetDesc().c_str());
			this->WriteLine(command);
			this->WriteLine(":"+recursive_server->GetID()+" VERSION :"+recursive_server->GetVersion());
			/* down to next level */
			this->SendServers(recursive_server, s, hops+1);
		}
	}
}

/** Send one or more FJOINs for a channel of users.
 * If the length of a single line is more than 480-NICKMAX
 * in length, it is split over multiple lines.
 * Send one or more FMODEs for a channel with the
 * channel bans, if there's any.
 */
void TreeSocket::SendFJoins(Channel* c)
{
	std::string line(":");
	line.append(ServerInstance->Config->GetSID()).append(" FJOIN ").append(c->name).append(1, ' ').append(ConvToStr(c->age)).append(" +");
	std::string::size_type erase_from = line.length();
	line.append(c->ChanModes(true)).append(" :");

	const UserMembList *ulist = c->GetUsers();

	for (UserMembCIter i = ulist->begin(); i != ulist->end(); ++i)
	{
		const std::string& modestr = i->second->modes;
		if ((line.length() + modestr.length() + (UUID_LENGTH-1) + 2) > 480)
		{
			this->WriteLine(line);
			line.erase(erase_from);
			line.append(" :");
		}
		line.append(modestr).append(1, ',').append(i->first->uuid).push_back(' ');
	}
	this->WriteLine(line);

	ModeReference ban(NULL, "ban");
	static_cast<ListModeBase*>(*ban)->DoSyncChannel(c, Utils->Creator, this);
}

/** Send all XLines we know about */
void TreeSocket::SendXLines()
{
	char data[MAXBUF];
	const char* sn = ServerInstance->Config->GetSID().c_str();

	std::vector<std::string> types = ServerInstance->XLines->GetAllTypes();

	for (std::vector<std::string>::const_iterator it = types.begin(); it != types.end(); ++it)
	{
		/* Expired lines are removed in XLineManager::GetAll() */
		XLineLookup* lookup = ServerInstance->XLines->GetAll(*it);

		/* lookup cannot be NULL in this case but a check won't hurt */
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
						i->second->source.c_str(),
						(unsigned long)i->second->set_time,
						(unsigned long)i->second->duration,
						i->second->reason.c_str());
				this->WriteLine(data);
			}
		}
	}
}

/** Send channel topic, modes and metadata */
void TreeSocket::SyncChannel(Channel* chan)
{
	char data[MAXBUF];

	SendFJoins(chan);
	if (!chan->topic.empty())
	{
		snprintf(data,MAXBUF,":%s FTOPIC %s %lu %lu %s :%s", ServerInstance->Config->GetSID().c_str(), chan->name.c_str(), (unsigned long) chan->age, (unsigned long)chan->topicset, chan->setby.c_str(), chan->topic.c_str());
		this->WriteLine(data);
	}

	for (Extensible::ExtensibleStore::const_iterator i = chan->GetExtList().begin(); i != chan->GetExtList().end(); i++)
	{
		ExtensionItem* item = i->first;
		std::string value = item->serialize(FORMAT_NETWORK, chan, i->second);
		if (!value.empty())
			Utils->Creator->ProtoSendMetaData(this, chan, item->name, value);
	}

	FOREACH_MOD(I_OnSyncChannel,OnSyncChannel(chan, Utils->Creator, this));
}

/** send all users and their oper state/modes */
void TreeSocket::SendUsers()
{
	char data[MAXBUF];
	std::string dataline;
	for (user_hash::iterator u = ServerInstance->Users->clientlist->begin(); u != ServerInstance->Users->clientlist->end(); u++)
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
						u->second->GetIPString().c_str(),	/* 6: IP string */
						(unsigned long)u->second->signon, /* 7: Signon time for WHOWAS */
						u->second->FormatModes(true),	/* 8...n: Modes and params */
						u->second->fullname.c_str());	/* size-1: GECOS */
				this->WriteLine(data);
				if (u->second->IsOper())
				{
					snprintf(data,MAXBUF,":%s OPERTYPE %s", u->second->uuid.c_str(), u->second->oper->name.c_str());
					this->WriteLine(data);
				}
				if (u->second->IsAway())
				{
					snprintf(data,MAXBUF,":%s AWAY %ld :%s", u->second->uuid.c_str(), (long)u->second->awaytime, u->second->awaymsg.c_str());
					this->WriteLine(data);
				}
			}

			for(Extensible::ExtensibleStore::const_iterator i = u->second->GetExtList().begin(); i != u->second->GetExtList().end(); i++)
			{
				ExtensionItem* item = i->first;
				std::string value = item->serialize(FORMAT_NETWORK, u->second, i->second);
				if (!value.empty())
					Utils->Creator->ProtoSendMetaData(this, u->second, item->name, value);
			}

			FOREACH_MOD(I_OnSyncUser,OnSyncUser(u->second,Utils->Creator,this));
		}
	}
}

