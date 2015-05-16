/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2007 Robin Burchell <robin+git@viroteck.net>
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
#include "core_channel.h"

CommandNames::CommandNames(Module* parent)
	: Command(parent, "NAMES", 0, 0)
	, secretmode(parent, "secret")
	, privatemode(parent, "private")
	, invisiblemode(parent, "invisible")
{
	syntax = "{<channel>{,<channel>}}";
}

/** Handle /NAMES
 */
CmdResult CommandNames::Handle (const std::vector<std::string>& parameters, User *user)
{
	Channel* c;

	if (!parameters.size())
	{
		user->WriteNumeric(RPL_ENDOFNAMES, "* :End of /NAMES list.");
		return CMD_SUCCESS;
	}

	if (CommandParser::LoopCall(user, this, parameters, 0))
		return CMD_SUCCESS;

	c = ServerInstance->FindChan(parameters[0]);
	if (c)
	{
		// Show the NAMES list if one of the following is true:
		// - the channel is not secret
		// - the user doing the /NAMES is inside the channel
		// - the user doing the /NAMES has the channels/auspex privilege

		// If the user is inside the channel or has privs, instruct SendNames() to show invisible (+i) members
		bool show_invisible = ((c->HasUser(user)) || (user->HasPrivPermission("channels/auspex")));
		if ((show_invisible) || (!c->IsModeSet(secretmode)))
		{
			SendNames(user, c, show_invisible);
			return CMD_SUCCESS;
		}
	}

	user->WriteNumeric(ERR_NOSUCHNICK, "%s :No such nick/channel", parameters[0].c_str());
	return CMD_FAILURE;
}

void CommandNames::SendNames(User* user, Channel* chan, bool show_invisible)
{
	std::string list;
	if (chan->IsModeSet(secretmode))
		list.push_back('@');
	else if (chan->IsModeSet(privatemode))
		list.push_back('*');
	else
		list.push_back('=');

	list.push_back(' ');
	list.append(chan->name).append(" :");
	std::string::size_type pos = list.size();

	const size_t maxlen = ServerInstance->Config->Limits.MaxLine - 10 - ServerInstance->Config->ServerName.size() - user->nick.size();
	std::string prefixlist;
	std::string nick;
	const Channel::MemberMap& members = chan->GetUsers();
	for (Channel::MemberMap::const_iterator i = members.begin(); i != members.end(); ++i)
	{
		if ((!show_invisible) && (i->first->IsModeSet(invisiblemode)))
		{
			// Member is invisible and we are not supposed to show them
			continue;
		}

		Membership* const memb = i->second;

		prefixlist.clear();
		char prefix = memb->GetPrefixChar();
		if (prefix)
			prefixlist.push_back(prefix);
		nick = i->first->nick;

		ModResult res;
		FIRST_MOD_RESULT(OnNamesListItem, res, (user, memb, prefixlist, nick));

		// See if a module wants us to exclude this user from NAMES
		if (res == MOD_RES_DENY)
			continue;

		if (list.size() + prefixlist.length() + nick.length() + 1 > maxlen)
		{
			// List overflowed into multiple numerics
			user->WriteNumeric(RPL_NAMREPLY, list);

			// Erase all nicks, keep the constant part
			list.erase(pos);
		}

		list.append(prefixlist).append(nick).push_back(' ');
	}

	// Only send the user list numeric if there is at least one user in it
	if (list.size() != pos)
		user->WriteNumeric(RPL_NAMREPLY, list);

	user->WriteNumeric(RPL_ENDOFNAMES, "%s :End of /NAMES list.", chan->name.c_str());
}
