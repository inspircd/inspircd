/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2019 Matt Schatz <genius3000@g3k.solutions>
 *   Copyright (C) 2018 linuxdaemon <linuxdaemon.irc@gmail.com>
 *   Copyright (C) 2017-2025 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2017 Dylan Frank <b00mx0r@aureus.pw>
 *   Copyright (C) 2012-2013, 2016 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012, 2019 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2012 Jens Voss <DukePyrolator@anope.org>
 *   Copyright (C) 2009 John Brooks <john@jbrooks.io>
 *   Copyright (C) 2009 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2008-2009 Craig Edwards <brain@inspircd.org>
 *   Copyright (C) 2008 Thomas Stagner <aquanight@gmail.com>
 *   Copyright (C) 2008 Robin Burchell <robin+git@viroteck.net>
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
#include "modules/shun.h"
#include "modules/stats.h"
#include "timeutils.h"
#include "xline.h"

/** An XLineFactory specialized to generate shun pointers
 */
class ShunFactory final
	: public XLineFactory
{
public:
	ShunFactory()
		: XLineFactory("SHUN") { }

	XLine* Generate(time_t set_time, unsigned long duration, const std::string& source, const std::string& reason, const std::string& xline_specific_mask) override
	{
		return new Shun(set_time, duration, source, reason, xline_specific_mask);
	}

	bool AutoApplyToUserList(XLine* x) override
	{
		return false;
	}
};

class CommandShun final
	: public Command
{
public:
	CommandShun(Module* Creator)
		: Command(Creator, "SHUN", 1, 3)
	{
		access_needed = CmdAccess::OPERATOR;
		syntax = { "<nick!user@host>[,<nick!user@host>]+ [<duration> :<reason>]" };
	}

	CmdResult Handle(User* user, const Params& parameters) override
	{
		/* syntax: SHUN nick!user@host time :reason goes here */
		/* 'time' is a human-readable timestring, like 2d3h2s. */
		if (CommandParser::LoopCall(user, this, parameters, 0))
			return CmdResult::SUCCESS;

		std::string target = parameters[0];

		auto* find = ServerInstance->Users.Find(target, true);
		if (find)
			target = "*!" + find->GetBanUser(true) + "@" + find->GetAddress();

		if (parameters.size() == 1)
		{
			std::string reason;

			if (ServerInstance->XLines->DelLine(parameters[0], "SHUN", reason, user))
			{
				ServerInstance->SNO.WriteToSnoMask('x', "{} removed SHUN on {}: {}", user->nick, parameters[0], reason);
			}
			else if (ServerInstance->XLines->DelLine(target, "SHUN", reason, user))
			{
				ServerInstance->SNO.WriteToSnoMask('x', "{} removed SHUN on {}: {}", user->nick, target, reason);
			}
			else
			{
				user->WriteNotice("*** Shun " + parameters[0] + " not found on the list.");
				return CmdResult::FAILURE;
			}
		}
		else
		{
			// Adding - XXX todo make this respect <insane> tag perhaps..
			unsigned long duration;
			std::string expr;
			if (parameters.size() > 2)
			{
				if (!Duration::TryFrom(parameters[1], duration))
				{
					user->WriteNotice("*** Invalid duration for SHUN.");
					return CmdResult::FAILURE;
				}
				expr = parameters[2];
			}
			else
			{
				duration = 0;
				expr = parameters[1];
			}

			auto* r = new Shun(ServerInstance->Time(), duration, user->nick, expr, target);
			if (ServerInstance->XLines->AddLine(r, user))
			{
				if (!duration)
				{
					ServerInstance->SNO.WriteToSnoMask('x', "{} added permanent SHUN for {}: {}",
						user->nick, target, expr);
				}
				else
				{
					ServerInstance->SNO.WriteToSnoMask('x', "{} added a timed SHUN on {}, expires in {} (on {}): {}",
						user->nick, target, Duration::ToLongString(duration),
						Time::FromNow(duration), expr);
				}
			}
			else
			{
				delete r;
				user->WriteNotice("*** Shun for " + target + " already exists.");
				return CmdResult::FAILURE;
			}
		}
		return CmdResult::SUCCESS;
	}
};

class ModuleShun final
	: public Module
	, public Stats::EventListener
{
private:
	CommandShun cmd;
	ShunFactory shun;
	bool allowconnect;
	bool allowtags;
	TokenList cleanedcommands;
	TokenList enabledcommands;
	bool notifyuser;

	bool IsShunned(LocalUser* user) const
	{
		// Exempt the user if they are not fully connected and allowconnect is enabled.
		if (allowconnect && !user->IsFullyConnected())
			return false;

		// Exempt the user from shuns if they are an oper with the servers/ignore-shun privilege.
		if (user->HasPrivPermission("servers/ignore-shun"))
			return false;

		// Check whether the user is actually shunned.
		return ServerInstance->XLines->MatchesLine("SHUN", user);
	}

public:
	ModuleShun()
		: Module(VF_VENDOR | VF_COMMON, "Adds the /SHUN command which allows server operators to prevent users from executing commands.")
		, Stats::EventListener(this)
		, cmd(this)
	{
	}

	void init() override
	{
		ServerInstance->XLines->RegisterFactory(&shun);
	}

	~ModuleShun() override
	{
		ServerInstance->XLines->DelAll("SHUN");
		ServerInstance->XLines->UnregisterFactory(&shun);
	}

	void Prioritize() override
	{
		Module* alias = ServerInstance->Modules.Find("alias");
		ServerInstance->Modules.SetPriority(this, I_OnPreCommand, PRIORITY_BEFORE, alias);
	}

	ModResult OnStats(Stats::Context& stats) override
	{
		if (stats.GetSymbol() != 'H')
			return MOD_RES_PASSTHRU;

		ServerInstance->XLines->InvokeStats("SHUN", stats);
		return MOD_RES_DENY;
	}

	void ReadConfig(ConfigStatus& status) override
	{
		cleanedcommands.Clear();
		enabledcommands.Clear();

		const auto& tag = ServerInstance->Config->ConfValue("shun");
		allowconnect = tag->getBool("allowconnect");
		allowtags = tag->getBool("allowtags");
		cleanedcommands.AddList(tag->getString("cleanedcommands", "AWAY PART QUIT"));
		enabledcommands.AddList(tag->getString("enabledcommands", "ADMIN OPER PING PONG QUIT"));
		notifyuser = tag->getBool("notifyuser", true);
	}

	ModResult OnPreCommand(std::string& command, CommandBase::Params& parameters, LocalUser* user, bool validated) override
	{
		if (validated || !IsShunned(user))
			return MOD_RES_PASSTHRU;

		if (!enabledcommands.Contains(command))
		{
			if (notifyuser)
				user->WriteNotice("*** " + command + " command not processed as you have been blocked from issuing commands.");
			return MOD_RES_DENY;
		}

		if (!allowtags)
		{
			// Remove all client tags.
			ClientProtocol::TagMap& tags = parameters.GetTags();
			for (ClientProtocol::TagMap::iterator tag = tags.begin(); tag != tags.end(); )
			{
				if (tag->first[0] == '+')
					tag = tags.erase(tag);
				else
					tag++;
			}
		}

		if (!cleanedcommands.Contains(command))
			return MOD_RES_PASSTHRU;

		switch (parameters.size())
		{
			case 0:
			{
				if (command == "AWAY" || command == "QUIT")
					parameters.clear();
				break;
			}
			case 1:
			{
				if (command == "CYCLE" || command == "KNOCK" || command == "PART")
					parameters.resize(1);
				break;
			}
		}

		return MOD_RES_PASSTHRU;
	}
};

MODULE_INIT(ModuleShun)
