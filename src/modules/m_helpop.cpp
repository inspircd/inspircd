/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
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

/* $ModDesc: /helpop Command, Works like Unreal helpop */
static std::map<irc::string, std::string> helpop_map;


/** Handles user mode +h
 */
class Helpop : public ModeHandler
{
 public:
	Helpop(InspIRCd* Instance) : ModeHandler(Instance, 'h', 0, 0, false, MODETYPE_USER, true) { }

	ModeAction OnModeChange(User* source, User* dest, Channel* channel, std::string &parameter, bool adding, bool)
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
	CommandHelpop (InspIRCd* Instance) : Command(Instance, "HELPOP", 0, 0)
	{
		this->source = "m_helpop.so";
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
			irc::sepstream stream(value, '\n');
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

		/* We dont want these going out over the network, return CMD_FAILURE
		 * to make sure the protocol module thinks theyre not worth sending.
		 */
		return CMD_FAILURE;
	}
};

class ModuleHelpop : public Module
{
	private:
		std::string  h_file;
		CommandHelpop* mycommand;
		Helpop* ho;

	public:
		ModuleHelpop(InspIRCd* Me)
			: Module(Me)
		{
			ReadConfig();
			ho = new Helpop(ServerInstance);
			if (!ServerInstance->Modes->AddMode(ho))
				throw ModuleException("Could not add new modes!");
			mycommand = new CommandHelpop(ServerInstance);
			ServerInstance->AddCommand(mycommand);
		Implementation eventlist[] = { I_OnRehash, I_OnWhois };
		ServerInstance->Modules->Attach(eventlist, this, 2);
		}

		virtual void ReadConfig()
		{
			ConfigReader MyConf(ServerInstance);

			helpop_map.clear();

			for (int i = 0; i < MyConf.Enumerate("helpop"); i++)
			{
				irc::string key = assign(MyConf.ReadValue("helpop", "key", i));
				std::string value = MyConf.ReadValue("helpop", "value", i, true); /* Linefeeds allowed! */

				if (key == "index")
				{
					throw ModuleException("m_helpop: The key 'index' is reserved for internal purposes. Please remove it.");
				}

				helpop_map[key] = value;
			}

			if (helpop_map.find("start") == helpop_map.end())
			{
				// error!
				throw ModuleException("m_helpop: Helpop file is missing important entries. Please check the example conf.");
			}
			else if (helpop_map.find("nohelp") == helpop_map.end())
			{
				// error!
				throw ModuleException("m_helpop: Helpop file is missing important entries. Please check the example conf.");
			}

		}


		virtual void OnRehash(User* user)
		{
			ReadConfig();
		}

		virtual void OnWhois(User* src, User* dst)
		{
			if (dst->IsModeSet('h'))
			{
				ServerInstance->SendWhoisLine(src, dst, 310, std::string(src->nick)+" "+std::string(dst->nick)+" :is available for help.");
			}
		}

		virtual ~ModuleHelpop()
		{
			ServerInstance->Modes->DelMode(ho);
			delete ho;
		}

		virtual Version GetVersion()
		{
			return Version("$Id$", VF_COMMON | VF_VENDOR, API_VERSION);
		}
};

MODULE_INIT(ModuleHelpop)
