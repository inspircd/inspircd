/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2012-2016 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012-2013, 2017-2018, 2020-2021 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2007-2008 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2006 Oliver Lupton <om@inspircd.org>
 *   Copyright (C) 2005, 2007 Craig Edwards <brain@inspircd.org>
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
#include "extension.h"
#include "modules/whois.h"
#include "numerichelper.h"

class CommandSwhois final
	: public Command
{
public:
	BoolExtItem operblock;
	StringExtItem swhois;
	CommandSwhois(Module* Creator)
		: Command(Creator, "SWHOIS", 2, 2)
		, operblock(Creator, "swhois-operblock", ExtensionType::USER)
		, swhois(Creator, "swhois", ExtensionType::USER, true)
	{
		access_needed = CmdAccess::OPERATOR;
		allow_empty_last_param = true;
		syntax = { "<nick> :<swhois>" };
		translation = { TR_NICK, TR_TEXT };
	}

	CmdResult Handle(User* user, const Params& parameters) override
	{
		auto* dest = ServerInstance->Users.Find(parameters[0]);

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
				ServerInstance->SNO.WriteGlobalSno('a', "{} used SWHOIS to set {}'s extra whois from '{}' to '{}'", user->nick, dest->nick, *text, parameters[1]);
		}
		else if (!user->server->IsService())
		{
			// Ulines set SWHOISes silently
			ServerInstance->SNO.WriteGlobalSno('a', "{} used SWHOIS to set {}'s extra whois to '{}'", user->nick, dest->nick, parameters[1]);
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

	void OnPostOperLogin(User* user, bool automatic) override
	{
		if (!IS_LOCAL(user))
			return;

		const std::string swhois = user->oper->GetConfig()->getString("swhois");
		if (!swhois.length())
			return;

		cmd.operblock.Set(user);
		cmd.swhois.Set(user, swhois);
	}

	void OnPostOperLogout(User* user, const std::shared_ptr<OperAccount>& oper) override
	{
		std::string* swhois = cmd.swhois.Get(user);
		if (!swhois)
			return;

		if (!cmd.operblock.Get(user))
			return;

		cmd.operblock.Unset(user);
		cmd.swhois.Unset(user);
	}

	void OnDecodeMetadata(Extensible* target, const std::string& extname, const std::string&) override
	{
		if (target && target->extype == ExtensionType::USER && irc::equals(extname, "swhois"))
			cmd.operblock.Unset(static_cast<User*>(target));
	}
};

MODULE_INIT(ModuleSWhois)
