/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2008 Pippijn van Steenhoven <pip88nl@gmail.com>
 *   Copyright (C) 2007 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2006-2007 John Brooks <john.brooks@dereferenced.net>
 *   Copyright (C) 2005-2006 Craig Edwards <craigedwards@brainbox.cc>
 *   Copyright (C) 2006 Oliver Lupton <oliverlupton@gmail.com>
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
#include "protocol.h"

/* $ModDesc: Provides the SWHOIS command which allows setting of arbitrary WHOIS lines */

/** Handle /SWHOIS
 */
class CommandSwhois : public Command
{
 public:
	StringExtItem swhois;
	CommandSwhois(Module* Creator) : Command(Creator,"SWHOIS", 2,2), swhois(EXTENSIBLE_USER, "swhois", Creator)
	{
		flags_needed = 'o'; syntax = "<nick> :<swhois>";
		ServerInstance->Extensions.Register(&swhois);
		TRANSLATE3(TR_NICK, TR_TEXT, TR_END);
	}

	CmdResult Handle(const std::vector<std::string> &parameters, User* user)
	{
		User* dest = ServerInstance->FindNick(parameters[0]);

		if (!dest)
		{
			user->WriteNumeric(ERR_NOSUCHNICK, "%s %s :No such nick/channel", user->nick.c_str(), parameters[0].c_str());
			return CMD_FAILURE;
		}

		std::string* text = swhois.get(dest);
		if (text)
		{
			// We already had it set...
			if (!ServerInstance->ULine(user->server))
				// Ulines set SWHOISes silently
				ServerInstance->SNO->WriteGlobalSno('a', "%s used SWHOIS to set %s's extra whois from '%s' to '%s'", user->nick.c_str(), dest->nick.c_str(), text->c_str(), parameters[1].c_str());
		}
		else if (!ServerInstance->ULine(user->server))
		{
			// Ulines set SWHOISes silently
			ServerInstance->SNO->WriteGlobalSno('a', "%s used SWHOIS to set %s's extra whois to '%s'", user->nick.c_str(), dest->nick.c_str(), parameters[1].c_str());
		}

		if (parameters[1].empty())
			swhois.unset(dest);
		else
			swhois.set(dest, parameters[1]);

		/* Bug #376 - feature request -
		 * To cut down on the amount of commands services etc have to recognise, this only sends METADATA across the network now
		 * not an actual SWHOIS command. Any SWHOIS command sent from services will be automatically translated to METADATA by this.
		 * Sorry w00t i know this was your fix, but i got bored and wanted to clear down the tracker :)
		 * -- Brain
		 */
		ServerInstance->PI->SendMetaData(dest, "swhois", parameters[1]);

		return CMD_SUCCESS;
	}

};

class ModuleSWhois : public Module
{
	CommandSwhois cmd;

 public:
	ModuleSWhois() : cmd(this) {}

	void init()
	{
		ServerInstance->AddCommand(&cmd);
		Implementation eventlist[] = { I_OnWhoisLine, I_OnOper };
		ServerInstance->Modules->Attach(eventlist, this, sizeof(eventlist)/sizeof(Implementation));
	}

	// :kenny.chatspike.net 320 Brain Azhrarn :is getting paid to play games.
	ModResult OnWhoisLine(User* user, User* dest, int &numeric, std::string &text)
	{
		/* We use this and not OnWhois because this triggers for remote, too */
		if (numeric == 312)
		{
			/* Insert our numeric before 312 */
			std::string* swhois = cmd.swhois.get(dest);
			if (swhois)
			{
				ServerInstance->SendWhoisLine(user, dest, 320, "%s %s :%s",user->nick.c_str(), dest->nick.c_str(), swhois->c_str());
			}
		}

		/* Dont block anything */
		return MOD_RES_PASSTHRU;
	}

	void OnOper(User* user, const std::string&)
	{
		if (!IS_LOCAL(user))
			return;

		std::string swhois = user->oper->getConfig("swhois");

		if (!swhois.length())
			return;

		cmd.swhois.set(user, swhois);
		ServerInstance->PI->SendMetaData(user, "swhois", swhois);
	}

	~ModuleSWhois()
	{
	}

	Version GetVersion()
	{
		return Version("Provides the SWHOIS command which allows setting of arbitrary WHOIS lines", VF_OPTCOMMON | VF_VENDOR);
	}
};

MODULE_INIT(ModuleSWhois)
