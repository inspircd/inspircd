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


enum
{
	// From UnrealIRCd
	RPL_WHOISHELPOP = 310,

	// From IRCD-Hybrid
	ERR_HELPNOTFOUND = 524,
	RPL_HELPSTART = 704,
	RPL_HELPTXT = 705,
	RPL_ENDOFHELP = 706
};

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
			user->WriteNumeric(RPL_HELPSTART, "index :HELPOP topic index");
			for (HelpopMap::const_iterator iter = helpop_map.begin(); iter != helpop_map.end(); iter++)
				user->WriteNumeric(RPL_HELPTXT, "index :  %s", iter->first.c_str());
			user->WriteNumeric(RPL_ENDOFHELP, "index :*** End of HELPOP topic index");
		}
		else
		{
			HelpopMap::const_iterator iter = helpop_map.find(parameter);

			if (iter == helpop_map.end())
			{
				iter = helpop_map.find("nohelp");
				user->WriteNumeric(ERR_HELPNOTFOUND, "%s :%s", parameter.c_str(), iter->second.c_str());
				return CMD_FAILURE;
			}

			user->WriteNumeric(RPL_HELPSTART, "%s :*** HELPOP for %s", parameter.c_str(), parameter.c_str());
			user->WriteNumeric(RPL_HELPTXT, "%s : -", parameter.c_str());

			const std::string& value = iter->second;
			irc::sepstream stream(value, '\n');
			std::string token = "*";

			while (stream.GetToken(token))
			{
				// Writing a blank line will not work with some clients
				if (token.empty())
					user->WriteNumeric(RPL_HELPTXT, "%s : ", parameter.c_str());
				else
					user->WriteNumeric(RPL_HELPTXT, "%s :%s", parameter.c_str(), token.c_str());
			}

			user->WriteNumeric(RPL_HELPTXT, "%s : -", parameter.c_str());
			user->WriteNumeric(RPL_ENDOFHELP, "%s :*** End of HELPOP", parameter.c_str());
		}
		return CMD_SUCCESS;
	}
};

class ModuleHelpop : public Module
{
		CommandHelpop cmd;
		Helpop ho;

	public:
		ModuleHelpop()
			: cmd(this), ho(this)
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

		void OnWhois(User* src, User* dst) CXX11_OVERRIDE
		{
			if (dst->IsModeSet(ho))
			{
				ServerInstance->SendWhoisLine(src, dst, RPL_WHOISHELPOP, dst->nick+" :is available for help.");
			}
		}

		Version GetVersion() CXX11_OVERRIDE
		{
			return Version("Provides the /HELPOP command for useful information", VF_VENDOR);
		}
};

MODULE_INIT(ModuleHelpop)
