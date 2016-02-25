/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2006-2008 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2008 Pippijn van Steenhoven <pip88nl@gmail.com>
 *   Copyright (C) 2006-2008 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2006 Craig Edwards <craigedwards@brainbox.cc>
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

namespace
{
	bool silent;
}

/** Holds a SVSHold item
 */
class SVSHold : public XLine
{
public:
	std::string nickname;

	SVSHold(time_t s_time, long d, const std::string& src, const std::string& re, const std::string& nick)
		: XLine(s_time, d, src, re, "SVSHOLD")
	{
		this->nickname = nick;
	}

	bool Matches(User *u)
	{
		if (u->nick == nickname)
			return true;
		return false;
	}

	bool Matches(const std::string &s)
	{
		return InspIRCd::Match(s, nickname);
	}

	void DisplayExpiry()
	{
		if (!silent)
		{
			ServerInstance->SNO->WriteToSnoMask('x', "Removing expired SVSHOLD %s (set by %s %ld seconds ago)",
				nickname.c_str(), source.c_str(), (long)(ServerInstance->Time() - set_time));
		}
	}

	const std::string& Displayable()
	{
		return nickname;
	}
};

/** An XLineFactory specialized to generate SVSHOLD pointers
 */
class SVSHoldFactory : public XLineFactory
{
 public:
	SVSHoldFactory() : XLineFactory("SVSHOLD") { }

	/** Generate a shun
 	*/
	XLine* Generate(time_t set_time, long duration, std::string source, std::string reason, std::string xline_specific_mask)
	{
		return new SVSHold(set_time, duration, source, reason, xline_specific_mask);
	}

	bool AutoApplyToUserList(XLine *x)
	{
		return false;
	}
};

/** Handle /SVSHold
 */
class CommandSvshold : public Command
{
 public:
	CommandSvshold(Module* Creator) : Command(Creator, "SVSHOLD", 1)
	{
		flags_needed = 'o'; this->syntax = "<nickname> [<duration> :<reason>]";
	}

	CmdResult Handle(const std::vector<std::string> &parameters, User *user)
	{
		/* syntax: svshold nickname time :reason goes here */
		/* 'time' is a human-readable timestring, like 2d3h2s. */

		if (!user->server->IsULine())
		{
			/* don't allow SVSHOLD from non-ulined clients */
			return CMD_FAILURE;
		}

		if (parameters.size() == 1)
		{
			if (ServerInstance->XLines->DelLine(parameters[0].c_str(), "SVSHOLD", user))
			{
				if (!silent)
					ServerInstance->SNO->WriteToSnoMask('x',"%s removed SVSHOLD on %s",user->nick.c_str(),parameters[0].c_str());
			}
			else
			{
				user->WriteNotice("*** SVSHOLD " + parameters[0] + " not found in list, try /stats S.");
			}
		}
		else
		{
			if (parameters.size() < 3)
				return CMD_FAILURE;

			unsigned long duration = InspIRCd::Duration(parameters[1]);
			SVSHold* r = new SVSHold(ServerInstance->Time(), duration, user->nick.c_str(), parameters[2].c_str(), parameters[0].c_str());

			if (ServerInstance->XLines->AddLine(r, user))
			{
				if (silent)
					return CMD_SUCCESS;

				if (!duration)
				{
					ServerInstance->SNO->WriteGlobalSno('x', "%s added permanent SVSHOLD for %s: %s", user->nick.c_str(), parameters[0].c_str(), parameters[2].c_str());
				}
				else
				{
					time_t c_requires_crap = duration + ServerInstance->Time();
					std::string timestr = InspIRCd::TimeString(c_requires_crap);
					ServerInstance->SNO->WriteGlobalSno('x', "%s added timed SVSHOLD for %s, expires on %s: %s", user->nick.c_str(), parameters[0].c_str(), timestr.c_str(), parameters[2].c_str());
				}
			}
			else
			{
				delete r;
				return CMD_FAILURE;
			}
		}

		return CMD_SUCCESS;
	}

	RouteDescriptor GetRouting(User* user, const std::vector<std::string>& parameters)
	{
		return ROUTE_BROADCAST;
	}
};

class ModuleSVSHold : public Module
{
	CommandSvshold cmd;
	SVSHoldFactory s;


 public:
	ModuleSVSHold() : cmd(this)
	{
	}

	void init() CXX11_OVERRIDE
	{
		ServerInstance->XLines->RegisterFactory(&s);
	}

	void ReadConfig(ConfigStatus& status) CXX11_OVERRIDE
	{
		ConfigTag* tag = ServerInstance->Config->ConfValue("svshold");
		silent = tag->getBool("silent", true);
	}

	ModResult OnStats(Stats::Context& stats) CXX11_OVERRIDE
	{
		if (stats.GetSymbol() != 'S')
			return MOD_RES_PASSTHRU;

		ServerInstance->XLines->InvokeStats("SVSHOLD", 210, stats);
		return MOD_RES_DENY;
	}

	ModResult OnUserPreNick(LocalUser* user, const std::string& newnick) CXX11_OVERRIDE
	{
		XLine *rl = ServerInstance->XLines->MatchesLine("SVSHOLD", newnick);

		if (rl)
		{
			user->WriteNumeric(ERR_ERRONEUSNICKNAME, newnick, InspIRCd::Format("Services reserved nickname: %s", rl->reason.c_str()));
			return MOD_RES_DENY;
		}

		return MOD_RES_PASSTHRU;
	}

	~ModuleSVSHold()
	{
		ServerInstance->XLines->DelAll("SVSHOLD");
		ServerInstance->XLines->UnregisterFactory(&s);
	}

	Version GetVersion() CXX11_OVERRIDE
	{
		return Version("Implements SVSHOLD. Like Q:Lines, but can only be added/removed by Services.", VF_COMMON | VF_VENDOR);
	}
};

MODULE_INIT(ModuleSVSHold)
