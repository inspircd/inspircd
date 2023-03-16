/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 * Copyright (C) 2014 WindowsUser <jasper@jasperswebsite.com>
 * Based off the core xline methods and partially the services account module.
 *
 * This file is part of InspIRCd. InspIRCd is free software: you can
 * redistribute it and/or modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation, version 2.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

/// $ModAuthor: WindowsUser
/// $ModDesc: Gives /ALINE and /GALINE, short for auth-lines. Users affected by these will have to use SASL to connect, while any users already connected but not identified to services will be disconnected in a similar manner to G-lines.
/// $ModDepends: core 3

#include "inspircd.h"
#include "xline.h"
#include "modules/account.h"
#include "modules/stats.h"


static bool isLoggedIn(User* user)
{
	const AccountExtItem* accountext = GetAccountExtItem();
	return (accountext && accountext->get(user));
}

class GALine : public XLine
{
 protected:
	/** Ident mask (ident part only)
	*/
	std::string identmask;
	/** Host mask (host part only)
	*/
	std::string hostmask;

	std::string matchtext;

 public:
	GALine(time_t s_time, long d, const std::string& src, const std::string& re, const std::string& ident, const std::string& host, std::string othertext = "GA")
		: XLine(s_time, d, src, re, othertext), identmask(ident), hostmask(host)
	{
		matchtext = identmask;
		matchtext.append("@").append(this->hostmask);
	}

	void Apply(User* u) CXX11_OVERRIDE
	{
		if (!isLoggedIn(u))
		{
			u->WriteNotice("*** NOTICE -- You need to identify via SASL to use this server (your host is " + type + "-lined).");
			ServerInstance->Users->QuitUser(u, type + "-lined: "+this->reason);
		}
	}

	void DisplayExpiry() CXX11_OVERRIDE
	{
		ServerInstance->SNO->WriteToSnoMask('x', "Removing expired %s-line %s@%s (set by %s %s ago): %s",
			type.c_str(), identmask.c_str(), hostmask.c_str(), source.c_str(), InspIRCd::DurationString(ServerInstance->Time() - this->set_time).c_str(), reason.c_str());
	}

	bool Matches(User* u) CXX11_OVERRIDE
	{
		LocalUser* lu = IS_LOCAL(u);
		if (lu && lu->exempt)
			return false;

		if (InspIRCd::Match(u->ident, this->identmask, ascii_case_insensitive_map))
		{
			if (InspIRCd::MatchCIDR(u->GetRealHost(), this->hostmask, ascii_case_insensitive_map) || InspIRCd::MatchCIDR(u->GetIPString(), this->hostmask, ascii_case_insensitive_map))
			{
				return true;
			}
		}

		return false;
	}

	bool Matches(const std::string& s) CXX11_OVERRIDE
	{
		return (matchtext == s);
	}

	const std::string& Displayable() CXX11_OVERRIDE
	{
		return matchtext;
	}
};

class ALine : public GALine
{
 public:
	ALine(time_t s_time, long d, const std::string& src, const std::string& re, const std::string& ident, const std::string& host)
		: GALine(s_time, d, src, re, ident, host, "A") {}

	bool IsBurstable() CXX11_OVERRIDE
	{
		return false;
	}
};

class ALineFactory : public XLineFactory
{
 public:
	ALineFactory() : XLineFactory("A") { }

	/** Generate an ALine
	 */
	ALine* Generate(time_t set_time, unsigned long duration, const std::string& source, const std::string& reason, const std::string& xline_specific_mask) CXX11_OVERRIDE
	{
		IdentHostPair ih = ServerInstance->XLines->IdentSplit(xline_specific_mask);
		return new ALine(set_time, duration, source, reason, ih.first, ih.second);
	}
};

class GALineFactory : public XLineFactory
{
 public:
	GALineFactory() : XLineFactory("GA") { }

	/** Generate a GALine
	*/
	GALine* Generate(time_t set_time, unsigned long duration, const std::string& source, const std::string& reason, const std::string& xline_specific_mask) CXX11_OVERRIDE
	{
		IdentHostPair ih = ServerInstance->XLines->IdentSplit(xline_specific_mask);
		return new GALine(set_time, duration, source, reason, ih.first, ih.second);
	}
};

class CommandGALine: public Command
{
 protected:
	std::string linename;
	char statschar;

 public:
	CommandGALine(Module* c, const std::string& linetype = "GA", char stats = 'A')
		: Command(c, linetype+"LINE", 1, 3)
	{
		flags_needed = 'o';
		this->syntax = "<user@host> [<duration> :<reason>]";
		this->linename = linetype;
		statschar = stats;
	}

	CmdResult Handle(User* user, const Params& parameters) CXX11_OVERRIDE
	{
		std::string target = parameters[0];
		if (parameters.size() >= 3)
		{
			IdentHostPair ih;
			User* find = ServerInstance->FindNick(target);
			if ((find) && (find->registered == REG_ALL))
			{
				ih.first = "*";
				ih.second = find->GetIPString();
				target = std::string("*@") + find->GetIPString();
			}
			else
				ih = ServerInstance->XLines->IdentSplit(target);

			if (ih.first.empty())
			{
				user->WriteNotice("*** Target not found.");
				return CMD_FAILURE;
			}

			else if (target.find('!') != std::string::npos)
			{
				user->WriteNotice(linename + "-line cannot operate on nick!user@host masks.");
				return CMD_FAILURE;
			}

			XLineFactory* xlf = ServerInstance->XLines->GetFactory(linename);
			if (!xlf)
				return CMD_FAILURE;

			unsigned long duration;
			if (!InspIRCd::Duration(parameters[1], duration))
			{
				user->WriteNotice("*** Invalid duration for " + linename + "-line.");
				return CMD_FAILURE;
			}
			XLine* al = xlf->Generate(ServerInstance->Time(), duration, user->nick, parameters[2], target);
			if (ServerInstance->XLines->AddLine(al, user))
			{
				if (!duration)
				{
					ServerInstance->SNO->WriteToSnoMask('x', "%s added permanent %s-line for %s: %s", user->nick.c_str(), linename.c_str(), target.c_str(), parameters[2].c_str());
				}
				else
				{
					ServerInstance->SNO->WriteToSnoMask('x', "%s added timed %s-line for %s, expires in %s (on %s): %s",
						user->nick.c_str(), linename.c_str(), target.c_str(), InspIRCd::DurationString(duration).c_str(),
						InspIRCd::TimeString(ServerInstance->Time() + duration).c_str(), parameters[2].c_str());
				}
				ServerInstance->XLines->ApplyLines();
			}
			else
			{
				delete al;
				user->WriteNotice("*** " + linename + "-line for " + target + " already exists.");
			}
		}
		else
		{
			std::string reason;
			if (ServerInstance->XLines->DelLine(target.c_str(), linename, reason, user))
			{
				ServerInstance->SNO->WriteToSnoMask('x', "%s removed %s-line on %s: %s",
					user->nick.c_str(), linename.c_str(), target.c_str(), reason.c_str());
			}
			else
			{
				user->WriteNotice("*** " + linename + "-line " + target + " not found in list, try /stats " + ConvToStr(statschar) + ".");
			}
		}

		return CMD_SUCCESS;
	}

};

class CommandALine: public CommandGALine
{
 public:
	CommandALine(Module* c) : CommandGALine(c, "A", 'a') {}
};

class ModuleRequireAuth : public Module, public Stats::EventListener
{
	CommandALine cmd1;
	CommandGALine cmd2;
	ALineFactory fact1;
	GALineFactory fact2;

 public:
	ModuleRequireAuth()
		: Stats::EventListener(this)
		, cmd1(this)
		, cmd2(this)
	{
	}

	void init() CXX11_OVERRIDE
	{
		ServerInstance->XLines->RegisterFactory(&fact1);
		ServerInstance->XLines->RegisterFactory(&fact2);
	}

	ModResult OnStats(Stats::Context& stats) CXX11_OVERRIDE
	{
		/*stats A does global lines, stats a local lines.*/
		if (stats.GetSymbol() == 'A')
		{
#if defined INSPIRCD_VERSION_BEFORE && INSPIRCD_VERSION_BEFORE(3, 6)
			ServerInstance->XLines->InvokeStats("GA", 210, stats);
#else
			ServerInstance->XLines->InvokeStats("GA", stats);
#endif
			return MOD_RES_DENY;
		}
		else if (stats.GetSymbol() == 'a')
		{
#if defined INSPIRCD_VERSION_BEFORE && INSPIRCD_VERSION_BEFORE(3, 6)
			ServerInstance->XLines->InvokeStats("A", 210, stats);
#else
			ServerInstance->XLines->InvokeStats("A", stats);
#endif
			return MOD_RES_DENY;
		}
		return MOD_RES_PASSTHRU;
	}

	~ModuleRequireAuth()
	{
		ServerInstance->XLines->DelAll("A");
		ServerInstance->XLines->DelAll("GA");
		ServerInstance->XLines->UnregisterFactory(&fact1);
		ServerInstance->XLines->UnregisterFactory(&fact2);
	}

	Version GetVersion() CXX11_OVERRIDE
	{
		return Version("Gives /ALINE and /GALINE, short for auth-lines. Users affected by these will have to use SASL to connect, while any users already connected but not identified to services will be disconnected in a similar manner to G-lines.", VF_COMMON);
	}

	ModResult OnCheckReady(LocalUser* user) CXX11_OVERRIDE
	{
		/*I'm afraid that using the normal xline methods would then result in this line being checked at the wrong time.*/
		if (!isLoggedIn(user))
		{
			XLine* locallines = ServerInstance->XLines->MatchesLine("A", user);
			XLine* globallines = ServerInstance->XLines->MatchesLine("GA", user);
			if (locallines)
			{
				user->WriteNotice("*** NOTICE -- You need to identify via SASL to use this server (your host is A-lined).");
				ServerInstance->Users->QuitUser(user, "A-lined: "+locallines->reason);
				return MOD_RES_DENY;
			}
			else if (globallines)
			{
				user->WriteNotice("*** NOTICE -- You need to identify via SASL to use this server (your host is GA-lined).");
				ServerInstance->Users->QuitUser(user, "GA-lined: "+globallines->reason);
				return MOD_RES_DENY;
			}
		}
		return MOD_RES_PASSTHRU;
	}
};

MODULE_INIT(ModuleRequireAuth)
