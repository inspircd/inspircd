/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2017-2021, 2023, 2025 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2013, 2015 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2006 Craig Edwards <brain@inspircd.org>
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

enum
{
	// From ircd-ratbox.
	ERR_HELPNOTFOUND = 524,
	RPL_HELPSTART = 704,
	RPL_HELPTXT = 705,
	RPL_ENDOFHELP = 706
};

typedef std::vector<std::string> HelpMessage;

struct HelpTopic final
{
	// The body of the help topic.
	const HelpMessage body;

	// The title of the help topic.
	const std::string title;

	HelpTopic(const HelpMessage& Body, const std::string& Title)
		: body(Body)
		, title(Title)
	{
	}
};

typedef std::map<std::string, HelpTopic, irc::insensitive_swo> HelpMap;

class CommandHelp final
	: public Command
{
private:
	const std::string startkey;

public:
	HelpMap help;
	std::string nohelp;

	CommandHelp(Module* Creator)
		: Command(Creator, "HELP")
		, startkey("start")
	{
		syntax = { "<any-text>" };
	}

	CmdResult Handle(User* user, const Params& parameters) override
	{
		const std::string& topic = parameters.empty() ? startkey : parameters[0];
		HelpMap::const_iterator titer = help.find(topic);
		if (titer == help.end())
		{
			user->WriteNumeric(ERR_HELPNOTFOUND, topic, nohelp);
			return CmdResult::FAILURE;
		}

		const HelpTopic& entry = titer->second;
		user->WriteNumeric(RPL_HELPSTART, topic, entry.title);
		for (const auto& line : entry.body)
			user->WriteNumeric(RPL_HELPTXT, topic, line);
		user->WriteNumeric(RPL_ENDOFHELP, topic, "End of /HELP.");
		return CmdResult::SUCCESS;
	}
};

class ModuleHelp final
	: public Module
{
private:
	CommandHelp cmd;

public:
	ModuleHelp()
		: Module(VF_VENDOR, "Adds the /HELP command which allows users to view help on various topics.")
		, cmd(this)
	{
	}

	void ReadConfig(ConfigStatus& status) override
	{
		size_t longestkey = 0;

		HelpMap newhelp;
		auto tags = ServerInstance->Config->ConfTags("helptopic", ServerInstance->Config->ConfTags("helpop"));
		if (tags.empty())
			throw ModuleException(this, "You have loaded the help module but not configured any help topics!");

		for (const auto& [_, tag] : tags)
		{
			// Attempt to read the help key.
			const std::string key = tag->getString("key");
			if (key.empty())
				throw ModuleException(this, "<{}:key> is empty at {}", tag->name, tag->source.str());
			else if (irc::equals(key, "index"))
				throw ModuleException(this, "<{}:key> is set to \"index\" which is reserved at {}", tag->name, tag->source.str());
			else if (key.length() > longestkey)
				longestkey = key.length();

			// Attempt to read the help value.
			std::string value;
			if (!tag->readString("value", value, true) || value.empty())
				throw ModuleException(this, "<{}:value> is empty at {}", tag->name, tag->source.str());

			// Parse the help body. Empty lines are replaced with a single
			// space because some clients are unable to show blank lines.
			HelpMessage helpmsg;
			irc::sepstream linestream(value, '\n', true);
			for (std::string line; linestream.GetToken(line); )
				helpmsg.push_back(line.empty() ? " " : line);

			// Read the help title and store the topic.
			const std::string title = tag->getString("title", INSP_FORMAT("*** Help for {}", key), 1);
			if (!newhelp.emplace(key, HelpTopic(helpmsg, title)).second)
			{
				throw ModuleException(this, "<{}> tag with duplicate key '{}' at {}",
					tag->name, key, tag->source.str());
			}
		}

		// The number of items we can fit on a page.
		HelpMessage indexmsg;
		size_t maxcolumns = 80 / (longestkey + 2);
		for (HelpMap::iterator iter = newhelp.begin(); iter != newhelp.end(); )
		{
			std::string indexline;
			for (size_t column = 0; column != maxcolumns; )
			{
				if (iter == newhelp.end())
					break;

				indexline.append(iter->first);
				if (++column != maxcolumns)
					indexline.append(longestkey - iter->first.length() + 2, ' ');
				iter++;
			}
			indexmsg.push_back(indexline);
		}
		newhelp.emplace("index", HelpTopic(indexmsg, "List of help topics"));
		cmd.help.swap(newhelp);

		const auto& tag = ServerInstance->Config->ConfValue("helpmsg");
		cmd.nohelp = tag->getString("nohelp", "There is no help for the topic you searched for. Please try again.", 1);
	}
};

MODULE_INIT(ModuleHelp)
