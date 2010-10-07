/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2010 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
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
