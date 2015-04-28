/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2005-2009 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2004-2006, 2008 Craig Edwards <craigedwards@brainbox.cc>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2004-2005 Craig McLure <craig@chatspike.net>
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

typedef std::map<std::string, std::string, irc::insensitive_swo> HelpopMap;
static HelpopMap helpop_map;

/** Handles user mode +h
 */
class Helpop : public SimpleUserModeHandler
{
 public:
	Helpop(Module* Creator) : SimpleUserModeHandler(Creator, "helpop", 'h')
	{
		oper = true;
	}
};

/** Handles /HELPOP
 */
class CommandHelpop : public Command
{
	const std::string startkey;
 public:
	CommandHelpop(Module* Creator)
		: Command(Creator, "HELPOP", 0)
		, startkey("start")
	{
		syntax = "<any-text>";
	}

	CmdResult Handle (const std::vector<std::string> &parameters, User *user)
	{
		const std::string& parameter = (!parameters.empty() ? parameters[0] : startkey);

		if (parameter == "index")
		{
			/* iterate over all helpop items */
			user->WriteNumeric(290, ":HELPOP topic index");
			for (HelpopMap::const_iterator iter = helpop_map.begin(); iter != helpop_map.end(); iter++)
				user->WriteNumeric(292, ":  %s", iter->first.c_str());
			user->WriteNumeric(292, ":*** End of HELPOP topic index");
		}
		else
		{
			user->WriteNumeric(290, ":*** HELPOP for %s", parameter.c_str());
			user->WriteNumeric(292, ": -");

			HelpopMap::const_iterator iter = helpop_map.find(parameter);

			if (iter == helpop_map.end())
			{
				iter = helpop_map.find("nohelp");
			}

			const std::string& value = iter->second;
			irc::sepstream stream(value, '\n');
			std::string token = "*";

			while (stream.GetToken(token))
			{
				// Writing a blank line will not work with some clients
				if (token.empty())
					user->WriteNumeric(292, ": ");
				else
					user->WriteNumeric(292, ":%s", token.c_str());
			}

			user->WriteNumeric(292, ": -");
			user->WriteNumeric(292, ":*** End of HELPOP");
		}
		return CMD_SUCCESS;
	}
};

class ModuleHelpop : public Module, public Whois::EventListener
{
		CommandHelpop cmd;
		Helpop ho;

	public:
		ModuleHelpop()
			: Whois::EventListener(this)
			, cmd(this)
			, ho(this)
		{
		}

		void ReadConfig(ConfigStatus& status) CXX11_OVERRIDE
		{
			HelpopMap help;

			ConfigTagList tags = ServerInstance->Config->ConfTags("helpop");
			for(ConfigIter i = tags.first; i != tags.second; ++i)
			{
				ConfigTag* tag = i->second;
				std::string key = tag->getString("key");
				std::string value;
				tag->readString("value", value, true); /* Linefeeds allowed */

				if (key == "index")
				{
					throw ModuleException("m_helpop: The key 'index' is reserved for internal purposes. Please remove it.");
				}

				help[key] = value;
			}

			if (help.find("start") == help.end())
			{
				// error!
				throw ModuleException("m_helpop: Helpop file is missing important entry 'start'. Please check the example conf.");
			}
			else if (help.find("nohelp") == help.end())
			{
				// error!
				throw ModuleException("m_helpop: Helpop file is missing important entry 'nohelp'. Please check the example conf.");
			}

			helpop_map.swap(help);
		}

		void OnWhois(Whois::Context& whois) CXX11_OVERRIDE
		{
			if (whois.GetTarget()->IsModeSet(ho))
			{
				whois.SendLine(310, ":is available for help.");
			}
		}

		Version GetVersion() CXX11_OVERRIDE
		{
			return Version("Provides the /HELPOP command for useful information", VF_VENDOR);
		}
};

MODULE_INIT(ModuleHelpop)
