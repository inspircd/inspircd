/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2007-2008 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2005-2008 Robin Burchell <robin+git@viroteck.net>
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
#include "xline.h"
#include "modules/stats.h"

enum
{
	// InspIRCd-specific.
	ERR_BADCHANNEL = 926
};

/** Holds a CBAN item
 */
class CBan : public XLine
{
private:
	std::string matchtext;

public:
	CBan(time_t s_time, unsigned long d, const std::string& src, const std::string& re, const std::string& ch)
		: XLine(s_time, d, src, re, "CBAN")
		, matchtext(ch)
	{
	}

	// XXX I shouldn't have to define this
	bool Matches(User* u) override
	{
		return false;
	}

	bool Matches(const std::string& s) override
	{
		return irc::equals(matchtext, s);
	}

	const std::string& Displayable() override
	{
		return matchtext;
	}
};

/** An XLineFactory specialized to generate cban pointers
 */
class CBanFactory : public XLineFactory
{
 public:
	CBanFactory() : XLineFactory("CBAN") { }

	/** Generate a CBAN
 	*/
	XLine* Generate(time_t set_time, unsigned long duration, const std::string& source, const std::string& reason, const std::string& xline_specific_mask) override
	{
		return new CBan(set_time, duration, source, reason, xline_specific_mask);
	}

	bool AutoApplyToUserList(XLine* x) override
	{
		return false; // No, we apply to channels.
	}
};

/** Handle /CBAN
 */
class CommandCBan : public Command
{
 public:
	CommandCBan(Module* Creator) : Command(Creator, "CBAN", 1, 3)
	{
		flags_needed = 'o'; this->syntax = "<channel> [<duration> [:<reason>]]";
	}

	CmdResult Handle(User* user, const Params& parameters) override
	{
		/* syntax: CBAN #channel time :reason goes here */
		/* 'time' is a human-readable timestring, like 2d3h2s. */

		if (parameters.size() == 1)
		{
			std::string reason;

			if (ServerInstance->XLines->DelLine(parameters[0].c_str(), "CBAN", reason, user))
			{
				ServerInstance->SNO.WriteGlobalSno('x', "%s removed CBan on %s: %s", user->nick.c_str(), parameters[0].c_str(), reason.c_str());
			}
			else
			{
				user->WriteNotice("*** CBan " + parameters[0] + " not found in list, try /stats C.");
				return CMD_FAILURE;
			}
		}
		else
		{
			// Adding - XXX todo make this respect <insane> tag perhaps..
			unsigned long duration;
			if (!InspIRCd::Duration(parameters[1], duration))
			{
				user->WriteNotice("*** Invalid duration for CBan.");
				return CMD_FAILURE;
			}
			const char *reason = (parameters.size() > 2) ? parameters[2].c_str() : "No reason supplied";
			CBan* r = new CBan(ServerInstance->Time(), duration, user->nick.c_str(), reason, parameters[0].c_str());

			if (ServerInstance->XLines->AddLine(r, user))
			{
				if (!duration)
				{
					ServerInstance->SNO.WriteGlobalSno('x', "%s added permanent CBan for %s: %s", user->nick.c_str(), parameters[0].c_str(), reason);
				}
				else
				{
					ServerInstance->SNO.WriteGlobalSno('x', "%s added timed CBan for %s, expires in %s (on %s): %s",
						user->nick.c_str(), parameters[0].c_str(), InspIRCd::DurationString(duration).c_str(),
						InspIRCd::TimeString(ServerInstance->Time() + duration).c_str(), reason);
				}
			}
			else
			{
				delete r;
				user->WriteNotice("*** CBan for " + parameters[0] + " already exists");
				return CMD_FAILURE;
			}
		}
		return CMD_SUCCESS;
	}

	RouteDescriptor GetRouting(User* user, const Params& parameters) override
	{
		if (IS_LOCAL(user))
			return ROUTE_LOCALONLY; // spanningtree will send ADDLINE

		return ROUTE_BROADCAST;
	}
};

class ModuleCBan : public Module, public Stats::EventListener
{
	CommandCBan mycommand;
	CBanFactory f;

 public:
	ModuleCBan()
		: Stats::EventListener(this)
		, mycommand(this)
	{
	}

	void init() override
	{
		ServerInstance->XLines->RegisterFactory(&f);
	}

	~ModuleCBan()
	{
		ServerInstance->XLines->DelAll("CBAN");
		ServerInstance->XLines->UnregisterFactory(&f);
	}

	ModResult OnStats(Stats::Context& stats) override
	{
		if (stats.GetSymbol() != 'C')
			return MOD_RES_PASSTHRU;

		ServerInstance->XLines->InvokeStats("CBAN", 210, stats);
		return MOD_RES_DENY;
	}

	ModResult OnUserPreJoin(LocalUser* user, Channel* chan, const std::string& cname, std::string& privs, const std::string& keygiven) override
	{
		XLine *rl = ServerInstance->XLines->MatchesLine("CBAN", cname);

		if (rl)
		{
			// Channel is banned.
			user->WriteNumeric(ERR_BADCHANNEL, cname, InspIRCd::Format("Channel %s is CBANed: %s", cname.c_str(), rl->reason.c_str()));
			ServerInstance->SNO.WriteGlobalSno('a', "%s tried to join %s which is CBANed (%s)",
				 user->nick.c_str(), cname.c_str(), rl->reason.c_str());
			return MOD_RES_DENY;
		}

		return MOD_RES_PASSTHRU;
	}

	Version GetVersion() override
	{
		return Version("Gives /cban, aka C-lines. Think Q-lines, for channels.", VF_COMMON | VF_VENDOR);
	}
};

MODULE_INIT(ModuleCBan)
