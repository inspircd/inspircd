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

/** Handle /LIST.
 */
class CommandList : public Command
{
	ChanModeReference secretmode;
	ChanModeReference privatemode;

 public:
	/** Constructor for list.
	 */
	CommandList(Module* parent)
		: Command(parent,"LIST", 0, 0)
		, secretmode(creator, "secret")
		, privatemode(creator, "private")
	{
		Penalty = 5;
	}

	/** Handle command.
	 * @param parameters The parameters to the command
	 * @param user The user issuing the command
	 * @return A value from CmdResult to indicate command success or failure.
	 */
	CmdResult Handle(const std::vector<std::string>& parameters, User *user);
};


/** Handle /LIST
 */
CmdResult CommandList::Handle (const std::vector<std::string>& parameters, User *user)
{
	int minusers = 0, maxusers = 0;

	user->WriteNumeric(RPL_LISTSTART, "Channel", "Users Name");

	if ((parameters.size() == 1) && (!parameters[0].empty()))
	{
		if (parameters[0][0] == '<')
		{
			maxusers = atoi((parameters[0].c_str())+1);
		}
		else if (parameters[0][0] == '>')
		{
			minusers = atoi((parameters[0].c_str())+1);
		}
	}

	const bool has_privs = user->HasPrivPermission("channels/auspex");
	const bool match_name_topic = ((!parameters.empty()) && (!parameters[0].empty()) && (parameters[0][0] != '<') && (parameters[0][0] != '>'));

	const chan_hash& chans = ServerInstance->GetChans();
	for (chan_hash::const_iterator i = chans.begin(); i != chans.end(); ++i)
	{
		Channel* const chan = i->second;

		// attempt to match a glob pattern
		long users = chan->GetUserCounter();

		bool too_few = (minusers && (users <= minusers));
		bool too_many = (maxusers && (users >= maxusers));

		if (too_many || too_few)
			continue;

		if (match_name_topic)
		{
			if (!InspIRCd::Match(chan->name, parameters[0]) && !InspIRCd::Match(chan->topic, parameters[0]))
				continue;
		}

		// if the channel is not private/secret, OR the user is on the channel anyway
		bool n = (has_privs || chan->HasUser(user));

		// If we're not in the channel and +s is set on it, we want to ignore it
		if ((n) || (!chan->IsModeSet(secretmode)))
		{
			if ((!n) && (chan->IsModeSet(privatemode)))
			{
				// Channel is private (+p) and user is outside/not privileged
				user->WriteNumeric(RPL_LIST, '*', users, "");
			}
			else
			{
				/* User is in the channel/privileged, channel is not +s */
				user->WriteNumeric(RPL_LIST, chan->name, users, InspIRCd::Format("[+%s] %s", chan->ChanModes(n), chan->topic.c_str()));
			}
		}
	}
	user->WriteNumeric(RPL_LISTEND, "End of channel list.");

	return CMD_SUCCESS;
}


COMMAND_INIT(CommandList)
