/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2019 Matt Schatz <genius3000@g3k.solutions>
 *   Copyright (C) 2018 linuxdaemon <linuxdaemon.irc@gmail.com>
 *   Copyright (C) 2017-2018 B00mX0r <b00mx0r@aureus.pw>
 *   Copyright (C) 2013, 2017-2018 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012-2016 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012, 2018-2019 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2012 Jens Voss <DukePyrolator@anope.org>
 *   Copyright (C) 2009 Uli Schlachter <psychon@inspircd.org>
 *   Copyright (C) 2009 Matt Smith <dz@inspircd.org>
 *   Copyright (C) 2009 John Brooks <special@inspircd.org>
 *   Copyright (C) 2009 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2008-2010 Craig Edwards <brain@inspircd.org>
 *   Copyright (C) 2008-2009 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2008 Thomas Stagner <aquanight@inspircd.org>
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
#include "xline.h"
#include "modules/shun.h"
#include "modules/stats.h"


/** An XLineFactory specialized to generate shun pointers
 */
class ShunFactory : public XLineFactory
{
 public:
	ShunFactory() : XLineFactory("SHUN") { }

	/** Generate a shun
 	*/
	XLine* Generate(time_t set_time, unsigned long duration, const std::string& source, const std::string& reason, const std::string& xline_specific_mask) CXX11_OVERRIDE
	{
		return new Shun(set_time, duration, source, reason, xline_specific_mask);
	}

	bool AutoApplyToUserList(XLine* x) CXX11_OVERRIDE
	{
		return false;
	}
};

//typedef std::vector<Shun> shunlist;

class CommandShun : public Command
{
 public:
	CommandShun(Module* Creator) : Command(Creator, "SHUN", 1, 3)
	{
		flags_needed = 'o'; this->syntax = "<nick!user@host> [<duration> :<reason>]";
	}

	CmdResult Handle(User* user, const Params& parameters) CXX11_OVERRIDE
	{
		/* syntax: SHUN nick!user@host time :reason goes here */
		/* 'time' is a human-readable timestring, like 2d3h2s. */

		std::string target = parameters[0];

		User *find = ServerInstance->FindNick(target);
		if ((find) && (find->registered == REG_ALL))
			target = std::string("*!*@") + find->GetIPString();

		if (parameters.size() == 1)
		{
			std::string reason;

			if (ServerInstance->XLines->DelLine(parameters[0].c_str(), "SHUN", reason, user))
			{
				ServerInstance->SNO->WriteToSnoMask('x', "%s removed SHUN on %s: %s", user->nick.c_str(), parameters[0].c_str(), reason.c_str());
			}
			else if (ServerInstance->XLines->DelLine(target.c_str(), "SHUN", reason, user))
			{
				ServerInstance->SNO->WriteToSnoMask('x', "%s removed SHUN on %s: %s", user->nick.c_str(), target.c_str(), reason.c_str());
			}
			else
			{
				user->WriteNotice("*** Shun " + parameters[0] + " not found on the list.");
				return CMD_FAILURE;
			}
		}
		else
		{
			// Adding - XXX todo make this respect <insane> tag perhaps..
			unsigned long duration;
			std::string expr;
			if (parameters.size() > 2)
			{
				if (!InspIRCd::Duration(parameters[1], duration))
				{
					user->WriteNotice("*** Invalid duration for SHUN.");
					return CMD_FAILURE;
				}
				expr = parameters[2];
			}
			else
			{
				duration = 0;
				expr = parameters[1];
			}

			Shun* r = new Shun(ServerInstance->Time(), duration, user->nick.c_str(), expr.c_str(), target.c_str());
			if (ServerInstance->XLines->AddLine(r, user))
			{
				if (!duration)
				{
					ServerInstance->SNO->WriteToSnoMask('x', "%s added permanent SHUN for %s: %s",
						user->nick.c_str(), target.c_str(), expr.c_str());
				}
				else
				{
					ServerInstance->SNO->WriteToSnoMask('x', "%s added timed SHUN for %s, expires in %s (on %s): %s",
						user->nick.c_str(), target.c_str(), InspIRCd::DurationString(duration).c_str(),
						InspIRCd::TimeString(ServerInstance->Time() + duration).c_str(), expr.c_str());
				}
			}
			else
			{
				delete r;
				user->WriteNotice("*** Shun for " + target + " already exists.");
				return CMD_FAILURE;
			}
		}
		return CMD_SUCCESS;
	}

	RouteDescriptor GetRouting(User* user, const Params& parameters) CXX11_OVERRIDE
	{
		if (IS_LOCAL(user))
			return ROUTE_LOCALONLY; // spanningtree will send ADDLINE

		return ROUTE_BROADCAST;
	}
};

class ModuleShun : public Module, public Stats::EventListener
{
	CommandShun cmd;
	ShunFactory f;
	insp::flat_set<std::string> ShunEnabledCommands;
	bool NotifyOfShun;
	bool affectopers;

 public:
	ModuleShun()
		: Stats::EventListener(this)
		, cmd(this)
	{
	}

	void init() CXX11_OVERRIDE
	{
		ServerInstance->XLines->RegisterFactory(&f);
	}

	~ModuleShun()
	{
		ServerInstance->XLines->DelAll("SHUN");
		ServerInstance->XLines->UnregisterFactory(&f);
	}

	void Prioritize() CXX11_OVERRIDE
	{
		Module* alias = ServerInstance->Modules->Find("m_alias.so");
		ServerInstance->Modules->SetPriority(this, I_OnPreCommand, PRIORITY_BEFORE, alias);
	}

	ModResult OnStats(Stats::Context& stats) CXX11_OVERRIDE
	{
		if (stats.GetSymbol() != 'H')
			return MOD_RES_PASSTHRU;

		ServerInstance->XLines->InvokeStats("SHUN", 223, stats);
		return MOD_RES_DENY;
	}

	void ReadConfig(ConfigStatus& status) CXX11_OVERRIDE
	{
		ConfigTag* tag = ServerInstance->Config->ConfValue("shun");
		std::string cmds = tag->getString("enabledcommands");
		std::transform(cmds.begin(), cmds.end(), cmds.begin(), ::toupper);

		if (cmds.empty())
			cmds = "PING PONG QUIT";

		ShunEnabledCommands.clear();

		irc::spacesepstream dcmds(cmds);
		std::string thiscmd;

		while (dcmds.GetToken(thiscmd))
		{
			ShunEnabledCommands.insert(thiscmd);
		}

		NotifyOfShun = tag->getBool("notifyuser", true);
		affectopers = tag->getBool("affectopers", false);
	}

	ModResult OnPreCommand(std::string& command, CommandBase::Params& parameters, LocalUser* user, bool validated) CXX11_OVERRIDE
	{
		if (validated)
			return MOD_RES_PASSTHRU;

		if (!ServerInstance->XLines->MatchesLine("SHUN", user))
		{
			/* Not shunned, don't touch. */
			return MOD_RES_PASSTHRU;
		}

		if (!affectopers && user->IsOper())
		{
			/* Don't do anything if the user is an operator and affectopers isn't set */
			return MOD_RES_PASSTHRU;
		}

		if (!ShunEnabledCommands.count(command))
		{
			if (NotifyOfShun)
				user->WriteNotice("*** Command " + command + " not processed, as you have been blocked from issuing commands (SHUN)");
			return MOD_RES_DENY;
		}

		if (command == "QUIT")
		{
			/* Allow QUIT but dont show any quit message */
			parameters.clear();
		}
		else if ((command == "PART") && (parameters.size() > 1))
		{
			/* same for PART */
			parameters.pop_back();
		}

		/* if we're here, allow the command. */
		return MOD_RES_PASSTHRU;
	}

	Version GetVersion() CXX11_OVERRIDE
	{
		return Version("Provides the SHUN command, which stops a user from executing all except configured commands", VF_VENDOR|VF_COMMON);
	}
};

MODULE_INIT(ModuleShun)
