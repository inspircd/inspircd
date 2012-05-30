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


/* $ModDesc: Provides the /HELPOP command, works like UnrealIRCd's helpop */

#include "inspircd.h"

static std::map<irc::string, std::string> helpop_map;

/** Handles user mode +h
 */
class Helpop : public ModeHandler
{
 public:
	Helpop(Module* Creator) : ModeHandler(Creator, "helpop", 'h', PARAM_NONE, MODETYPE_USER)
	{
		oper = true;
	}

	ModeAction OnModeChange(User* source, User* dest, Channel* channel, std::string &parameter, bool adding)
	{
		if (adding)
		{
			if (!dest->IsModeSet('h'))
			{
				dest->SetMode('h',true);
				return MODEACTION_ALLOW;
			}
		}
		else
		{
			if (dest->IsModeSet('h'))
			{
				dest->SetMode('h',false);
				return MODEACTION_ALLOW;
			}
		}

		return MODEACTION_DENY;
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
			user->WriteServ("290 %s :HELPOP topic index", user->nick.c_str());
			for (std::map<irc::string, std::string>::iterator iter = helpop_map.begin(); iter != helpop_map.end(); iter++)
			{
				user->WriteServ("292 %s :  %s", user->nick.c_str(), iter->first.c_str());
			}
			user->WriteServ("292 %s :*** End of HELPOP topic index", user->nick.c_str());
		}
		else
		{
			user->WriteServ("290 %s :*** HELPOP for %s", user->nick.c_str(), parameter.c_str());
			user->WriteServ("292 %s : -", user->nick.c_str());

			std::map<irc::string, std::string>::iterator iter = helpop_map.find(parameter);

			if (iter == helpop_map.end())
			{
				iter = helpop_map.find("nohelp");
			}

			std::string value = iter->second;
			irc::sepstream stream(value, '\n', false);
			std::string token = "*";

			while (stream.GetToken(token))
			{
				// Writing a blank line will not work with some clients
				if (token.empty())
					user->WriteServ("292 %s : ", user->nick.c_str());
				else
					user->WriteServ("292 %s :%s", user->nick.c_str(), token.c_str());
			}

			user->WriteServ("292 %s : -", user->nick.c_str());
			user->WriteServ("292 %s :*** End of HELPOP", user->nick.c_str());
		}
		return CMD_SUCCESS;
	}
};

class ModuleHelpop : public Module
{
	private:
		std::string  h_file;
		CommandHelpop cmd;
		Helpop ho;

	public:
		ModuleHelpop()
			: cmd(this), ho(this)
		{
		}

		void init()
		{
			ServerInstance->Modules->AddService(ho);
			ServerInstance->Modules->AddService(cmd);
			Implementation eventlist[] = { I_OnWhois };
			ServerInstance->Modules->Attach(eventlist, this, sizeof(eventlist)/sizeof(Implementation));
		}

		void ReadConfig(ConfigReadStatus&)
		{
			helpop_map.clear();

			ConfigTagList tags = ServerInstance->Config->GetTags("helpop");
			for(ConfigIter i = tags.first; i != tags.second; ++i)
			{
				ConfigTag* tag = i->second;
				irc::string key = tag->getString("key");
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

		void OnWhois(User* src, User* dst)
		{
			if (dst->IsModeSet('h'))
			{
				ServerInstance->SendWhoisLine(src, dst, 310, std::string(src->nick)+" "+std::string(dst->nick)+" :is available for help.");
			}
		}

		Version GetVersion()
		{
			return Version("Provides the /HELPOP command, works like UnrealIRCd's helpop", VF_VENDOR);
		}
};

MODULE_INIT(ModuleHelpop)
