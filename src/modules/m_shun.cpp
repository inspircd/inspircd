/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2008 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2008 Craig Edwards <craigedwards@brainbox.cc>
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

class Shun : public XLine
{
public:
	std::string matchtext;

	Shun(time_t s_time, long d, const std::string& src, const std::string& re, const std::string& shunmask)
		: XLine(s_time, d, src, re, "SHUN")
		, matchtext(shunmask)
	{
	}

	bool Matches(User *u)
	{
		// E: overrides shun
		LocalUser* lu = IS_LOCAL(u);
		if (lu && lu->exempt)
			return false;

		if (InspIRCd::Match(u->GetFullHost(), matchtext) || InspIRCd::Match(u->GetFullRealHost(), matchtext) || InspIRCd::Match(u->nick+"!"+u->ident+"@"+u->GetIPString(), matchtext))
			return true;

		if (InspIRCd::MatchCIDR(u->GetIPString(), matchtext, ascii_case_insensitive_map))
			return true;

		return false;
	}

	bool Matches(const std::string &s)
	{
		if (matchtext == s)
			return true;
		return false;
	}

	const std::string& Displayable()
	{
		return matchtext;
	}
};

/** An XLineFactory specialized to generate shun pointers
 */
class ShunFactory : public XLineFactory
{
 public:
	ShunFactory() : XLineFactory("SHUN") { }

	/** Generate a shun
 	*/
	XLine* Generate(time_t set_time, long duration, std::string source, std::string reason, std::string xline_specific_mask)
	{
		return new Shun(set_time, duration, source, reason, xline_specific_mask);
	}

	bool AutoApplyToUserList(XLine *x)
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
		flags_needed = 'o'; this->syntax = "<nick!user@hostmask> [<shun-duration>] :<reason>";
	}

	CmdResult Handle(const std::vector<std::string>& parameters, User *user)
	{
		/* syntax: SHUN nick!user@host time :reason goes here */
		/* 'time' is a human-readable timestring, like 2d3h2s. */

		std::string target = parameters[0];

		User *find = ServerInstance->FindNick(target);
		if ((find) && (find->registered == REG_ALL))
			target = std::string("*!*@") + find->GetIPString();

		if (parameters.size() == 1)
		{
			if (ServerInstance->XLines->DelLine(target.c_str(), "SHUN", user))
			{
				ServerInstance->SNO->WriteToSnoMask('x',"%s removed SHUN on %s",user->nick.c_str(),target.c_str());
			}
			else
			{
				user->WriteNotice("*** Shun " + target + " not found in list, try /stats H.");
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
				duration = InspIRCd::Duration(parameters[1]);
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
					ServerInstance->SNO->WriteToSnoMask('x',"%s added permanent SHUN for %s: %s",
						user->nick.c_str(), target.c_str(), expr.c_str());
				}
				else
				{
					time_t c_requires_crap = duration + ServerInstance->Time();
					std::string timestr = InspIRCd::TimeString(c_requires_crap);
					ServerInstance->SNO->WriteToSnoMask('x', "%s added timed SHUN for %s to expire on %s: %s",
						user->nick.c_str(), target.c_str(), timestr.c_str(), expr.c_str());
				}
			}
			else
			{
				delete r;
				user->WriteNotice("*** Shun for " + target + " already exists");
				return CMD_FAILURE;
			}
		}
		return CMD_SUCCESS;
	}

	RouteDescriptor GetRouting(User* user, const std::vector<std::string>& parameters)
	{
		if (IS_LOCAL(user))
			return ROUTE_LOCALONLY; // spanningtree will send ADDLINE

		return ROUTE_BROADCAST;
	}
};

class ModuleShun : public Module
{
	CommandShun cmd;
	ShunFactory f;
	insp::flat_set<std::string> ShunEnabledCommands;
	bool NotifyOfShun;
	bool affectopers;

 public:
	ModuleShun() : cmd(this)
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

		std::stringstream dcmds(cmds);
		std::string thiscmd;

		while (dcmds >> thiscmd)
		{
			ShunEnabledCommands.insert(thiscmd);
		}

		NotifyOfShun = tag->getBool("notifyuser", true);
		affectopers = tag->getBool("affectopers", false);
	}

	ModResult OnPreCommand(std::string &command, std::vector<std::string>& parameters, LocalUser* user, bool validated, const std::string &original_line) CXX11_OVERRIDE
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
			parameters[1].clear();
		}

		/* if we're here, allow the command. */
		return MOD_RES_PASSTHRU;
	}

	Version GetVersion() CXX11_OVERRIDE
	{
		return Version("Provides the /SHUN command, which stops a user from executing all except configured commands.",VF_VENDOR|VF_COMMON);
	}
};

MODULE_INIT(ModuleShun)
