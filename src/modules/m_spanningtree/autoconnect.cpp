/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2010 Jackmcbarn <jackmcbarn@jackmcbarn.no-ip.org>
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

#include "utils.h"
#include "link.h"
#include "commands.h"

CommandAutoconnect::CommandAutoconnect (Module* Creator, SpanningTreeUtilities* Util)
	: Command(Creator, "AUTOCONNECT", 0, 2), Utils(Util)
{
	flags_needed = 'o';
	syntax = "[pattern] [ON|OFF]";
}

CmdResult CommandAutoconnect::Handle (const std::vector<std::string>& parameters, User *user)
{
	if(parameters.size() == 2)
	{
		bool newsetting;
		if(irc::string(parameters[1]) == "OFF")
			newsetting = false;
		else if(irc::string(parameters[1]) == "ON")
			newsetting = true;
		else
		{
			user->WriteServ("NOTICE %s :*** AUTOCONNECT: Invalid autoconnect setting.", user->nick.c_str());
			return CMD_FAILURE;
		}
		bool matchAll = parameters[0] == "*";
		for (std::vector<reference<Autoconnect> >::iterator i = Utils->AutoconnectBlocks.begin(); i < Utils->AutoconnectBlocks.end(); ++i)
		{
			Autoconnect* x = *i;
			if(!matchAll && !InspIRCd::Match(*x->servers.begin(),parameters[0]))
				continue;
			x->Enabled = newsetting;
			ServerInstance->SNO->WriteGlobalSno('l', "%s %s autoconnect beginning with server \002%s\002", user->nick.c_str(), newsetting ? "enabled" : "disabled", x->servers.begin()->c_str());
		}
	}
	else if(parameters.size() == 1 && (irc::string(parameters[0]) == "OFF" || irc::string(parameters[0]) == "ON"))
	{
		bool newsetting = irc::string(parameters[0]) == "ON";
		for (std::vector<reference<Autoconnect> >::iterator i = Utils->AutoconnectBlocks.begin(); i < Utils->AutoconnectBlocks.end(); ++i)
			(*i)->Enabled = newsetting;
		ServerInstance->SNO->WriteGlobalSno('l', "%s %s all autoconnects", user->nick.c_str(), newsetting ? "enabled" : "disabled");
	}
	else
	{
		bool matchAll = parameters.empty() || parameters[0] == "*";
		for (std::vector<reference<Autoconnect> >::iterator i = Utils->AutoconnectBlocks.begin(); i < Utils->AutoconnectBlocks.end(); ++i)
		{
			Autoconnect* x = *i;
			if(matchAll || InspIRCd::Match(*x->servers.begin(),parameters[0]))
				user->WriteServ("NOTICE %s :*** AUTOCONNECT: Autoconnect beginning with server \002%s\002 is \002%s\002.", user->nick.c_str(), x->servers.begin()->c_str(), x->Enabled ? "enabled" : "disabled");
		}
	}
	return CMD_SUCCESS;
}

RouteDescriptor CommandAutoconnect::GetRouting(User* user, const std::vector<std::string>& parameters)
{
	return ROUTE_LOCALONLY;
}
