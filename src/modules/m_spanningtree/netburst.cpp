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
#include "commands.h"

/**
 * Creates FMODE messages, used only when syncing channels
 */
class FModeBuilder : public CmdBuilder
{
	static const size_t maxline = 480;
	std::string params;
	unsigned int modes;
	std::string::size_type startpos;

 public:
	FModeBuilder(Channel* chan)
		: CmdBuilder("FMODE"), modes(0)
	{
		push(chan->name).push_int(chan->age).push_raw(" +");
		startpos = str().size();
	}

	/** Add a mode to the message
	 */
	void push_mode(const char modeletter, const std::string& mask)
	{
		push_raw(modeletter);
		params.push_back(' ');
		params.append(mask);
		modes++;
	}

	/** Remove all modes from the message
	 */
	void clear()
	{
		content.erase(startpos);
		params.clear();
		modes = 0;
	}

	/** Prepare the message for sending, next mode can only be added after clear()
	 */
	const std::string& finalize()
	{
		return push_raw(params);
	}

	/** Returns true if the given mask can be added to the message, false if the message
	 * has no room for the mask
	 */
	bool has_room(const std::string& mask) const
	{
		return ((str().size() + params.size() + mask.size() + 2 <= maxline) &&
				(modes < ServerInstance->Config->Limits.MaxModes));
	}

	/** Returns true if this message is empty (has no modes)
	 */
	bool empty() const
	{
		return (modes == 0);
	}
};

/** This function is called when we want to send a netburst to a local
 * server. There is a set order we must do this, because for example
 * users require their servers to exist, and channels require their
 * users to exist. You get the idea.
 */
void TreeSocket::DoBurst(TreeServer* s)
{
	ServerInstance->SNO->WriteToSnoMask('l',"Bursting to \2%s\2 (Authentication: %s%s).",
		s->GetName().c_str(),
		capab->auth_fingerprint ? "SSL Fingerprint and " : "",
		capab->auth_challenge ? "challenge-response" : "plaintext password");
	this->CleanNegotiationInfo();
	this->WriteLine(":" + ServerInstance->Config->GetSID() + " BURST " + ConvToStr(ServerInstance->Time()));
	/* send our version string */
	this->WriteLine(":" + ServerInstance->Config->GetSID() + " VERSION :"+ServerInstance->GetVersionString());
	/* Send server tree */
	this->SendServers(Utils->TreeRoot, s);
	/* Send users and their oper status */
	this->SendUsers();

	for (chan_hash::const_iterator i = ServerInstance->chanlist->begin(); i != ServerInstance->chanlist->end(); ++i)
		SyncChannel(i->second);

	this->SendXLines();
	FOREACH_MOD(OnSyncNetwork, (Utils->Creator,(void*)this));
	this->WriteLine(":" + ServerInstance->Config->GetSID() + " ENDBURST");
	ServerInstance->SNO->WriteToSnoMask('l',"Finished bursting to \2"+ s->GetName()+"\2.");
}

/** Recursively send the server tree.
 * This is used during network burst to inform the other server
 * (and any of ITS servers too) of what servers we know about.
 * If at any point any of these servers already exist on the other
 * end, our connection may be terminated.
 * The hopcount parameter (3rd) is deprecated, and is always 0.
 */
void TreeSocket::SendServers(TreeServer* Current, TreeServer* s)
{
	const TreeServer::ChildServers& children = Current->GetChildren();
	for (TreeServer::ChildServers::const_iterator i = children.begin(); i != children.end(); ++i)
	{
		TreeServer* recursive_server = *i;
		if (recursive_server != s)
		{
			this->WriteLine(CommandServer::Builder(recursive_server));
			this->WriteLine(":" + recursive_server->GetID() + " VERSION :" + recursive_server->GetVersion());
			/* down to next level */
			this->SendServers(recursive_server, s);
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
		if ((line.length() + modestr.length() + UIDGenerator::UUID_LENGTH + 2) > 480)
		{
			this->WriteLine(line);
			line.erase(erase_from);
			line.append(" :");
		}
		line.append(modestr).append(1, ',').append(i->first->uuid).push_back(' ');
	}
	this->WriteLine(line);
}

/** Send all XLines we know about */
void TreeSocket::SendXLines()
{
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

				this->WriteLine(CommandAddLine::Builder(i->second));
			}
		}
	}
}

void TreeSocket::SendListModes(Channel* chan)
{
	FModeBuilder fmode(chan);
	const ModeParser::ListModeList& listmodes = ServerInstance->Modes->GetListModes();
	for (ModeParser::ListModeList::const_iterator i = listmodes.begin(); i != listmodes.end(); ++i)
	{
		ListModeBase* mh = *i;
		ListModeBase::ModeList* list = mh->GetList(chan);
		if (!list)
			continue;

		// Add all items on the list to the FMODE, send it whenever it becomes too long
		const char modeletter = mh->GetModeChar();
		for (ListModeBase::ModeList::const_iterator j = list->begin(); j != list->end(); ++j)
		{
			const std::string& mask = j->mask;
			if (!fmode.has_room(mask))
			{
				// No room for this mask, send the current line as-is then add the mask to a
				// new, empty FMODE message
				this->WriteLine(fmode.finalize());
				fmode.clear();
			}
			fmode.push_mode(modeletter, mask);
		}
	}

	if (!fmode.empty())
		this->WriteLine(fmode.finalize());
}

/** Send channel topic, modes and metadata */
void TreeSocket::SyncChannel(Channel* chan)
{
	SendFJoins(chan);

	// If the topic was ever set, send it, even if it's empty now
	// because a new empty topic should override an old non-empty topic
	if (chan->topicset != 0)
		this->WriteLine(CommandFTopic::Builder(chan));

	SendListModes(chan);

	for (Extensible::ExtensibleStore::const_iterator i = chan->GetExtList().begin(); i != chan->GetExtList().end(); i++)
	{
		ExtensionItem* item = i->first;
		std::string value = item->serialize(FORMAT_NETWORK, chan, i->second);
		if (!value.empty())
			Utils->Creator->ProtoSendMetaData(this, chan, item->name, value);
	}

	FOREACH_MOD(OnSyncChannel, (chan, Utils->Creator, this));
}

/** send all users and their oper state/modes */
void TreeSocket::SendUsers()
{
	for (user_hash::iterator u = ServerInstance->Users->clientlist->begin(); u != ServerInstance->Users->clientlist->end(); u++)
	{
		User* user = u->second;
		if (user->registered != REG_ALL)
			continue;

		this->WriteLine(CommandUID::Builder(user));

		if (user->IsOper())
			this->WriteLine(CommandOpertype::Builder(user));

		if (user->IsAway())
			this->WriteLine(CommandAway::Builder(user));

		const Extensible::ExtensibleStore& exts = user->GetExtList();
		for (Extensible::ExtensibleStore::const_iterator i = exts.begin(); i != exts.end(); ++i)
		{
			ExtensionItem* item = i->first;
			std::string value = item->serialize(FORMAT_NETWORK, u->second, i->second);
			if (!value.empty())
				Utils->Creator->ProtoSendMetaData(this, u->second, item->name, value);
		}

		FOREACH_MOD(OnSyncUser, (user, Utils->Creator, this));
	}
}

