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

static std::map<irc::string, std::string> helpop_map;

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
 public:
	CommandHelpop(Module* Creator) : Command(Creator, "HELPOP", 0)
	{
		syntax = "<any-text>";
	}

	CmdResult Handle (const std::vector<std::string> &parameters, User *user)
	{
		irc::string parameter("start");
		if (parameters.size() > 0)
			parameter = parameters[0].c_str();

		if (parameter == "index")
		{
			/* iterate over all helpop items */
			user->WriteNumeric(290, ":HELPOP topic index");
			for (std::map<irc::string, std::string>::iterator iter = helpop_map.begin(); iter != helpop_map.end(); iter++)
				user->WriteNumeric(292, ":  %s", iter->first.c_str());
			user->WriteNumeric(292, ":*** End of HELPOP topic index");
		}
		else
		{
			user->WriteNumeric(290, ":*** HELPOP for %s", parameter.c_str());
			user->WriteNumeric(292, ": -");

			std::map<irc::string, std::string>::iterator iter = helpop_map.find(parameter);

			if (iter == helpop_map.end())
			{
				iter = helpop_map.find("nohelp");
			}

			std::string value = iter->second;
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

class ModuleHelpop : public Module
{
		std::string  h_file;
		CommandHelpop cmd;
		Helpop ho;

	public:
		ModuleHelpop()
			: cmd(this), ho(this)
		{
		}

		void ReadConfig(ConfigStatus& status) CXX11_OVERRIDE
		{
			helpop_map.clear();

			ConfigTagList tags = ServerInstance->Config->ConfTags("helpop");
			for(ConfigIter i = tags.first; i != tags.second; ++i)
			{
				ConfigTag* tag = i->second;
				irc::string key = assign(tag->getString("key"));
				std::string value;
				tag->readString("value", value, true); /* Linefeeds allowed */

				if (key == "index")
				{
					throw ModuleException("m_helpop: The key 'index' is reserved for internal purposes. Please remove it.");
				}

				helpop_map[key] = value;
			}

			if (helpop_map.find("start") == helpop_map.end())
			{
				// error!
				throw ModuleException("m_helpop: Helpop file is missing important entry 'start'. Please check the example conf.");
			}
			else if (helpop_map.find("nohelp") == helpop_map.end())
			{
				// error!
				throw ModuleException("m_helpop: Helpop file is missing important entry 'nohelp'. Please check the example conf.");
			}

		}

		void OnWhois(User* src, User* dst) CXX11_OVERRIDE
		{
			if (dst->IsModeSet(ho))
			{
				ServerInstance->SendWhoisLine(src, dst, 310, dst->nick+" :is available for help.");
			}
		}

		Version GetVersion() CXX11_OVERRIDE
		{
			return Version("Provides the /HELPOP command for useful information", VF_VENDOR);
		}
};

MODULE_INIT(ModuleHelpop)
