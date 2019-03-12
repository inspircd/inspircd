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
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */


#include "inspircd.h"
#include "modules/whois.h"

enum
{
	// From UnrealIRCd.
	RPL_WHOISHELPOP = 310,

	// From ircd-ratbox.
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
 private:
	const std::string startkey;

 public:
	std::string nohelp;

	CommandHelpop(Module* Creator)
		: Command(Creator, "HELPOP", 0)
		, startkey("start")
	{
		syntax = "<any-text>";
	}

	CmdResult Handle(User* user, const Params& parameters) CXX11_OVERRIDE
	{
		const std::string& parameter = (!parameters.empty() ? parameters[0] : startkey);

		if (parameter == "index")
		{
			/* iterate over all helpop items */
			user->WriteNumeric(RPL_HELPSTART, parameter, "HELPOP topic index");
			for (HelpopMap::const_iterator iter = helpop_map.begin(); iter != helpop_map.end(); iter++)
				user->WriteNumeric(RPL_HELPTXT, parameter, InspIRCd::Format("  %s", iter->first.c_str()));
			user->WriteNumeric(RPL_ENDOFHELP, parameter, "*** End of HELPOP topic index");
		}
		else
		{
			HelpopMap::const_iterator iter = helpop_map.find(parameter);
			if (iter == helpop_map.end())
			{
				user->WriteNumeric(ERR_HELPNOTFOUND, parameter, nohelp);
				return CMD_FAILURE;
			}

			const std::string& value = iter->second;
			irc::sepstream stream(value, '\n', true);
			std::string token = "*";

			user->WriteNumeric(RPL_HELPSTART, parameter, InspIRCd::Format("*** HELPOP for %s", parameter.c_str()));
			while (stream.GetToken(token))
			{
				// Writing a blank line will not work with some clients
				if (token.empty())
					user->WriteNumeric(RPL_HELPTXT, parameter, ' ');
				else
					user->WriteNumeric(RPL_HELPTXT, parameter, token);
			}
			user->WriteNumeric(RPL_ENDOFHELP, parameter, "*** End of HELPOP");
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

			helpop_map.swap(help);

			ConfigTag* tag = ServerInstance->Config->ConfValue("helpmsg");
			cmd.nohelp = tag->getString("nohelp", "There is no help for the topic you searched for. Please try again.", 1);
		}

		void OnWhois(Whois::Context& whois) CXX11_OVERRIDE
		{
			if (whois.GetTarget()->IsModeSet(ho))
			{
				whois.SendLine(RPL_WHOISHELPOP, "is available for help.");
			}
		}

		Version GetVersion() CXX11_OVERRIDE
		{
			return Version("Provides the /HELPOP command for useful information", VF_VENDOR);
		}
};

MODULE_INIT(ModuleHelpop)
