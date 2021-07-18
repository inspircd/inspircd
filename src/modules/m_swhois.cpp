/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2012-2016 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012-2013, 2017-2018, 2020 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2007-2008 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2006 Oliver Lupton <om@inspircd.org>
 *   Copyright (C) 2005, 2007, 2010 Craig Edwards <brain@inspircd.org>
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
#include "modules/whois.h"

class CommandSwhois : public Command
{
 public:
	BoolExtItem operblock;
	StringExtItem swhois;
	CommandSwhois(Module* Creator)
		: Command(Creator, "SWHOIS", 2, 2)
		, operblock(Creator, "swhois_operblock", ExtensionItem::EXT_USER)
		, swhois(Creator, "swhois", ExtensionItem::EXT_USER, true)
	{
		access_needed = CmdAccess::OPERATOR;
		syntax = { "<nick> :<swhois>" };
		translation = { TR_NICK, TR_TEXT };
	}

	CmdResult Handle(User* user, const Params& parameters) override
	{
		User* dest = ServerInstance->Users.Find(parameters[0]);

		if (!dest) // allow setting swhois using SWHOIS before reg
		{
			user->WriteNumeric(Numerics::NoSuchNick(parameters[0]));
			return CmdResult::FAILURE;
		}

		std::string* text = swhois.Get(dest);
		if (text)
		{
			// We already had it set...
			if (!user->server->IsService())
				// Ulines set SWHOISes silently
				ServerInstance->SNO.WriteGlobalSno('a', "%s used SWHOIS to set %s's extra whois from '%s' to '%s'", user->nick.c_str(), dest->nick.c_str(), text->c_str(), parameters[1].c_str());
		}
		else if (!user->server->IsService())
		{
			// Ulines set SWHOISes silently
			ServerInstance->SNO.WriteGlobalSno('a', "%s used SWHOIS to set %s's extra whois to '%s'", user->nick.c_str(), dest->nick.c_str(), parameters[1].c_str());
		}

		operblock.Unset(user);
		if (parameters[1].empty())
			swhois.Unset(dest);
		else
			swhois.Set(dest, parameters[1]);

		return CmdResult::SUCCESS;
	}

};

class ModuleSWhois final
	: public Module
	, public Whois::LineEventListener
{
 private:
	CommandSwhois cmd;
	UserModeReference hideopermode;

 public:
	ModuleSWhois()
		: Module(VF_VENDOR | VF_OPTCOMMON, "Adds the /SWHOIS command which adds custom lines to a user's WHOIS response.")
		, Whois::LineEventListener(this)
		, cmd(this)
		, hideopermode(this, "hideoper")
	{
	}

	// :kenny.chatspike.net 320 Brain Azhrarn :is getting paid to play games.
	ModResult OnWhoisLine(Whois::Context& whois, Numeric::Numeric& numeric) override
	{
		// We use this and not OnWhois because this triggers for remote users too.
		if (numeric.GetNumeric() != RPL_WHOISSERVER)
			return MOD_RES_PASSTHRU;

		// Don't send soper swhois if hideoper is set.
		if (cmd.operblock.Get(whois.GetTarget()) && whois.GetTarget()->IsModeSet(hideopermode))
			return MOD_RES_PASSTHRU;

		// Insert our numeric before RPL_WHOISSERVER.
		const std::string* swhois = cmd.swhois.Get(whois.GetTarget());
		if (swhois && !swhois->empty())
			whois.SendLine(RPL_WHOISSPECIAL, *swhois);

		return MOD_RES_PASSTHRU;
	}

	void OnPostOper(User* user, const std::string &opertype, const std::string &opername) override
	{
		if (!IS_LOCAL(user))
			return;

		std::string swhois = user->oper->getConfig("swhois");

		if (!swhois.length())
			return;

		cmd.operblock.Set(user);
		cmd.swhois.Set(user, swhois);
	}

	void OnPostDeoper(User* user) override
	{
		std::string* swhois = cmd.swhois.Get(user);
		if (!swhois)
			return;

		if (!cmd.operblock.Get(user))
			return;

		cmd.operblock.Unset(user);
		cmd.swhois.Unset(user);
	}

	void OnDecodeMetaData(Extensible* target, const std::string& extname, const std::string&) override
	{
		User* dest = static_cast<User*>(target);
		if (dest && (extname == "swhois"))
			cmd.operblock.Unset(dest);
	}
};

MODULE_INIT(ModuleSWhois)
