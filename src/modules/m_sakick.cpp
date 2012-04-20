/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2009 John Brooks <john.brooks@dereferenced.net>
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
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

/* $ModDesc: Provides a SAKICK command */

/** Handle /SAKICK
 */
class CommandSakick : public Command
{
 public:
	CommandSakick (InspIRCd* Instance) : Command(Instance,"SAKICK", "o", 2, 3, false, 0)
	{
		this->source = "m_sakick.so";
		syntax = "<channel> <nick> [reason]";
		TRANSLATE4(TR_TEXT, TR_NICK, TR_TEXT, TR_END);
	}

	CmdResult Handle (const std::vector<std::string>& parameters, User *user)
	{
		User* dest = ServerInstance->FindNick(parameters[1]);
		Channel* channel = ServerInstance->FindChan(parameters[0]);
		const char* reason = "";
		const char* servername = NULL;

		if (dest && channel)
		{
			if (parameters.size() > 2)
			{
				reason = parameters[2].c_str();
			}
			else
			{
				reason = dest->nick.c_str();
			}

			if (ServerInstance->ULine(dest->server))
			{
				user->WriteNumeric(ERR_NOPRIVILEGES, "%s :Cannot use an SA command on a u-lined client", user->nick.c_str());
				return CMD_FAILURE;
			}

			/* For local clients, directly kick them. For remote clients,
			 * just return CMD_SUCCESS knowing the protocol module will route the SAKICK to the user's
			 * local server and that will kick them instead.
			 */
			if (IS_LOCAL(dest))
			{
				if (!channel->ServerKickUser(dest, reason, servername))
					delete channel;

				Channel *n = ServerInstance->FindChan(parameters[1]);
				if (n && n->HasUser(dest))
				{
					/* Sort-of-bug: If the command was issued remotely, this message won't be sent */
					user->WriteServ("NOTICE %s :*** Unable to kick %s from %s", user->nick.c_str(), dest->nick.c_str(), parameters[0].c_str());
					return CMD_FAILURE;
				}
			}

			if (IS_LOCAL(user))
			{
				/* Locally issued command; send the snomasks */
				ServerInstance->SNO->WriteGlobalSno('a', std::string(user->nick) + " SAKICKed " + dest->nick + " on " + parameters[0]);
			}

			return CMD_SUCCESS;
		}
		else
		{
			user->WriteServ("NOTICE %s :*** Invalid nickname or channel", user->nick.c_str());
		}

		return CMD_FAILURE;
	}
};

class ModuleSakick : public Module
{
	CommandSakick*	mycommand;
 public:
	ModuleSakick(InspIRCd* Me)
		: Module(Me)
	{

		mycommand = new CommandSakick(ServerInstance);
		ServerInstance->AddCommand(mycommand);

	}

	virtual ~ModuleSakick()
	{
	}

	virtual Version GetVersion()
	{
		return Version("$Id$", VF_COMMON|VF_VENDOR, API_VERSION);
	}

};

MODULE_INIT(ModuleSakick)

