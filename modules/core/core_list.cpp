/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2017-2023 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2015 Daniel Vassdal <shutter@canternet.org>
 *   Copyright (C) 2013-2016 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2008 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2005-2007 Craig Edwards <brain@inspircd.org>
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
#include "modules/isupport.h"

enum class ShowModes
	: uint8_t
{
	NOBODY,
	OPERS,
	ALL,
};

class CommandList final
	: public Command
{
private:
	ChanModeReference secretmode;
	ChanModeReference privatemode;

	/** Parses the creation time or topic set time out of a LIST parameter.
	 * @param value The parameter containing a minute count.
	 * @return The UNIX time at \p value minutes ago.
	 */
	static time_t ParseMinutes(const std::string& value)
	{
		time_t minutes = ConvToNum<time_t>(value.c_str() + 2);
		if (!minutes)
			return 0;
		return ServerInstance->Time() - (minutes * 60);
	}

public:
	// Whether to show modes in the LIST response.
	ShowModes showmodes;

	CommandList(Module* parent)
		: Command(parent, "LIST")
		, secretmode(creator, "secret")
		, privatemode(creator, "private")
	{
		penalty = 5000;
	}

	CmdResult Handle(User* user, const Params& parameters) override;
};

CmdResult CommandList::Handle(User* user, const Params& parameters)
{
	// C: Searching based on creation time, via the "C<val" and "C>val" modifiers
	// to search for a channel creation time that is lower or higher than val
	// respectively.
	time_t mincreationtime = 0;
	time_t maxcreationtime = 0;

	// M: Searching based on mask.
	std::string match;

	// N: Searching based on !mask.
	std::string notmatch;

	// T: Searching based on topic time, via the "T<val" and "T>val" modifiers to
	// search for a topic time that is lower or higher than val respectively.
	time_t mintopictime = 0;
	time_t maxtopictime = 0;

	// U: Searching based on user count within the channel, via the "<val" and
	// ">val" modifiers to search for a channel that has less than or more than
	// val users respectively.
	size_t minusers = 0;
	size_t maxusers = 0;

	if (!parameters.empty())
	{
		irc::commasepstream constraints(parameters[0]);
		for (std::string constraint; constraints.GetToken(constraint); )
		{
			if (constraint[0] == '<')
			{
				maxusers = ConvToNum<size_t>(constraint.c_str() + 1);
			}
			else if (constraint[0] == '>')
			{
				minusers = ConvToNum<size_t>(constraint.c_str() + 1);
			}
			else if (!constraint.compare(0, 2, "C<", 2) || !constraint.compare(0, 2, "c<", 2))
			{
				mincreationtime = ParseMinutes(constraint);
			}
			else if (!constraint.compare(0, 2, "C>", 2) || !constraint.compare(0, 2, "c>", 2))
			{
				maxcreationtime = ParseMinutes(constraint);
			}
			else if (!constraint.compare(0, 2, "T<", 2) || !constraint.compare(0, 2, "t<", 2))
			{
				mintopictime = ParseMinutes(constraint);
			}
			else if (!constraint.compare(0, 2, "T>", 2) || !constraint.compare(0, 2, "t>", 2))
			{
				maxtopictime = ParseMinutes(constraint);
			}
			else if (constraint[0] == '!')
			{
				// Ensure that the user didn't just run "LIST !".
				if (constraint.length() > 2)
					notmatch = constraint.substr(1);
			}
			else
			{
				match = constraint;
			}
		}
	}

	const bool has_privs = user->HasPrivPermission("channels/auspex");
	const bool show_modes = (showmodes == ShowModes::ALL) || (showmodes == ShowModes::OPERS && has_privs);

	user->WriteNumeric(RPL_LISTSTART, "Channel", "Users Name");

	for (const auto& [_, chan] : ServerInstance->Channels.GetChans())
	{
		// Check the user count if a search has been specified.
		const size_t users = chan->GetUsers().size();
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
		if (!match.empty() && !InspIRCd::Match(chan->name, match) && !InspIRCd::Match(chan->topic, match))
			continue;

		// Attempt to match an inverted glob pattern.
		if (!notmatch.empty() && (InspIRCd::Match(chan->name, notmatch) || InspIRCd::Match(chan->topic, notmatch)))
			continue;

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
			else if (show_modes)
			{
				// Show the list response with the modes and topic.
				user->WriteNumeric(RPL_LIST, chan->name, users, FMT::format("[+{}] {}", chan->ChanModes(n), chan->topic));
			}
			else
			{
				// Show the list response with just the modes.
				user->WriteNumeric(RPL_LIST, chan->name, users, chan->topic);
			}
		}
	}
	user->WriteNumeric(RPL_LISTEND, "End of channel list.");

	return CmdResult::SUCCESS;
}

class CoreModList final
	: public Module
	, public ISupport::EventListener
{
private:
	CommandList cmd;

public:
	CoreModList()
		: Module(VF_CORE | VF_VENDOR, "Provides the LIST command")
		, ISupport::EventListener(this)
		, cmd(this)
	{
	}

	void ReadConfig(ConfigStatus& status) override
	{
		const auto& tag = ServerInstance->Config->ConfValue("options");
		cmd.showmodes = tag->getEnum("showmodes", ShowModes::OPERS, {
			{ "no",    ShowModes::NOBODY },
			{ "opers", ShowModes::OPERS  },
			{ "yes",   ShowModes::ALL    },
		});
	}

	void OnBuildISupport(ISupport::TokenMap& tokens) override
	{
		tokens["ELIST"] = "CMNTU";
		tokens["SAFELIST"];
	}
};

MODULE_INIT(CoreModList)
