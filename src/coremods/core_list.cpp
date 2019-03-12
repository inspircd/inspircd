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
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */


#include "inspircd.h"

/** Handle /LIST.
 */
class CommandList : public Command
{
 private:
	ChanModeReference secretmode;
	ChanModeReference privatemode;

	/** Parses the creation time or topic set time out of a LIST parameter.
	 * @param value The parameter containing a minute count.
	 * @return The UNIX time at \p value minutes ago.
	 */
	time_t ParseMinutes(const std::string& value)
	{
		time_t minutes = ConvToNum<time_t>(value.c_str() + 2);
		if (!minutes)
			return 0;
		return ServerInstance->Time() - (minutes * 60);
	}

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
	CmdResult Handle(User* user, const Params& parameters) CXX11_OVERRIDE;
};


/** Handle /LIST
 */
CmdResult CommandList::Handle(User* user, const Params& parameters)
{
	// C: Searching based on creation time, via the "C<val" and "C>val" modifiers
	// to search for a channel creation time that is lower or higher than val
	// respectively.
	time_t mincreationtime = 0;
	time_t maxcreationtime = 0;

	// M: Searching based on mask.
	// N: Searching based on !mask.
	bool match_name_topic = false;
	bool match_inverted = false;
	const char* match = NULL;

	// T: Searching based on topic time, via the "T<val" and "T>val" modifiers to
	// search for a topic time that is lower or higher than val respectively.
	time_t mintopictime = 0;
	time_t maxtopictime = 0;

	// U: Searching based on user count within the channel, via the "<val" and
	// ">val" modifiers to search for a channel that has less than or more than
	// val users respectively.
	size_t minusers = 0;
	size_t maxusers = 0;

	if ((parameters.size() == 1) && (!parameters[0].empty()))
	{
		if (parameters[0][0] == '<')
		{
			maxusers = ConvToNum<size_t>(parameters[0].c_str() + 1);
		}
		else if (parameters[0][0] == '>')
		{
			minusers = ConvToNum<size_t>(parameters[0].c_str() + 1);
		}
		else if (!parameters[0].compare(0, 2, "C<", 2))
		{
			mincreationtime = ParseMinutes(parameters[0]);
		}
		else if (!parameters[0].compare(0, 2, "C>", 2))
		{
			maxcreationtime = ParseMinutes(parameters[0]);
		}
		else if (!parameters[0].compare(0, 2, "T<", 2))
		{
			mintopictime = ParseMinutes(parameters[0]);
		}
		else if (!parameters[0].compare(0, 2, "T>", 2))
		{
			maxtopictime = ParseMinutes(parameters[0]);
		}
		else
		{
			// If the glob is prefixed with ! it is inverted.
			match = parameters[0].c_str();
			if (match[0] == '!')
			{
				match_inverted = true;
				match += 1;
			}

			// Ensure that the user didn't just run "LIST !".
			if (match[0])
				match_name_topic = true;
		}
	}

	const bool has_privs = user->HasPrivPermission("channels/auspex");

	user->WriteNumeric(RPL_LISTSTART, "Channel", "Users Name");
	const chan_hash& chans = ServerInstance->GetChans();
	for (chan_hash::const_iterator i = chans.begin(); i != chans.end(); ++i)
	{
		Channel* const chan = i->second;

		// Check the user count if a search has been specified.
		const size_t users = chan->GetUserCounter();
		if ((minusers && users <= minusers) || (maxusers && users >= maxusers))
			continue;

		// Check the creation ts if a search has been specified.
		const time_t creationtime = chan->age;
		if ((mincreationtime && creationtime <= mincreationtime) || (maxcreationtime && creationtime >= maxcreationtime))
			continue;

		// Check the topic ts if a search has been specified.
		const time_t topictime = chan->topicset;
		if ((mintopictime && (!topictime || topictime <= mintopictime)) || (maxtopictime && (!topictime || topictime >= maxtopictime)))
			continue;

		// Attempt to match a glob pattern.
		if (match_name_topic)
		{
			bool matches = InspIRCd::Match(chan->name, match) || InspIRCd::Match(chan->topic, match);

			// The user specified an match that we did not match.
			if (!matches && !match_inverted)
				continue;

			// The user specified an inverted match that we did match.
			if (matches && match_inverted)
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

class CoreModList : public Module
{
 private:
	CommandList cmd;

 public:
	CoreModList()
		: cmd(this)
	{
	}

	void On005Numeric(std::map<std::string, std::string>& tokens) CXX11_OVERRIDE
	{
		tokens["ELIST"] = "CMNTU";
		tokens["SAFELIST"];
	}

	Version GetVersion() CXX11_OVERRIDE
	{
		return Version("Provides the LIST command", VF_VENDOR|VF_CORE);
	}
};

MODULE_INIT(CoreModList)
