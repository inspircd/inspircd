/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2010 Daniel De Graaf <danieldg@inspircd.org>
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

/* $ModDesc: Provides the /cline command, which allows a connect class to be assigned */

class CLine : public XLine
{
public:
	std::string matchtext;

	CLine(time_t s_time, long d, const std::string& src, const std::string& Reason, const std::string& mask)
		: XLine(s_time, d, src, Reason, "CLINE"), matchtext(mask)
	{
	}

	bool Matches(User *u)
	{
		if (InspIRCd::Match(u->GetFullHost(), matchtext) || InspIRCd::Match(u->GetFullRealHost(), matchtext) || InspIRCd::Match(u->nick+"!"+u->ident+"@"+u->GetIPString(), matchtext))
			return true;

		return false;
	}

	bool Matches(const std::string &s)
	{
		if (matchtext == s)
			return true;
		return false;
	}

	void Apply(User *u)
	{
	}

	void DisplayExpiry()
	{
		ServerInstance->SNO->WriteToSnoMask('x',"Removing expired C:line %s (set by %s %ld seconds ago)",
			this->matchtext.c_str(), this->source.c_str(), (long int)(ServerInstance->Time() - this->set_time));
	}

	const char* Displayable()
	{
		// Note: we can't add class here, Displayable is used in sync and so forth
		return matchtext.c_str();
	}
};

/** An XLineFactory specialized to generate c:line pointers
 */
class CLineFactory : public XLineFactory
{
 public:
	CLineFactory() : XLineFactory("CLINE") { }

	/** Generate a c:line
 	*/
	XLine* Generate(time_t set_time, long duration, std::string source, std::string reason, std::string xline_specific_mask)
	{
		return new CLine(set_time, duration, source, reason, xline_specific_mask);
	}
};

class CommandCLine : public Command
{
 public:
	CommandCLine(Module* Creator) : Command(Creator, "CLINE", 1, 3)
	{
		flags_needed = 'o'; this->syntax = "<nick!user@hostmask> [<duration>] classname :<reason>";
	}

	CmdResult Handle(const std::vector<std::string>& parameters, User *user)
	{
		/* syntax: CLINE nick!user@host time class :reason goes here */
		/* 'time' is a human-readable timestring, like 2d3h2s. */

		std::string target = parameters[0];
		
		User *find = ServerInstance->FindNick(target.c_str());
		if (find)
			target = std::string("*!*@") + find->GetIPString();

		if (parameters.size() == 1)
		{
			if (ServerInstance->XLines->DelLine(target.c_str(), "CLINE", user))
			{
				ServerInstance->SNO->WriteToSnoMask('x',"%s removed CLINE on %s",user->nick.c_str(),target.c_str());
				return CMD_SUCCESS;
			}
			else
			{
				user->WriteServ("NOTICE %s :*** CLine %s not found in list, try /stats C.",user->nick.c_str(),target.c_str());
				return CMD_FAILURE;
			}
		}
		else if (parameters.size() >= 2)
		{
			long duration = ServerInstance->Duration(parameters[1]);
			std::string expr;
			if (parameters.size() > 2 && (duration || parameters[1][0] == '0'))
			{
				// they specified a time
				expr = parameters[2];
			}
			else
			{
				// no time specification; the two parameters may need to be joined
				duration = 0;
				if (parameters.size() > 2)
					expr = parameters[1] + " " + parameters[2];
				else
					expr = parameters[1];
			}
			CLine *r = NULL;

			try
			{
				r = new CLine(ServerInstance->Time(), duration, user->nick, expr, target);
			}
			catch (...)
			{
				return CMD_FAILURE; // Do nothing. If we get here, the regex was fucked up, and they already got told it fucked up.
			}

			if (ServerInstance->XLines->AddLine(r, user))
			{
				if (!duration)
				{
					ServerInstance->SNO->WriteToSnoMask('x',"%s added permanent CLINE for %s: %s",
						user->nick.c_str(), target.c_str(), expr.c_str());
				}
				else
				{
					time_t c_requires_crap = duration + ServerInstance->Time();
					ServerInstance->SNO->WriteToSnoMask('x', "%s added timed CLINE for %s to expire on %s: %s",
						user->nick.c_str(), target.c_str(), ServerInstance->TimeString(c_requires_crap).c_str(), expr.c_str());
				}

				ServerInstance->XLines->ApplyLines();
			}
			else
			{
				delete r;
				user->WriteServ("NOTICE %s :*** CLine for %s already exists", user->nick.c_str(), target.c_str());
			}
		}

		return CMD_FAILURE;
	}

	RouteDescriptor GetRouting(User* user, const std::vector<std::string>& parameters)
	{
		return ROUTE_BROADCAST;
	}
};

class ModuleCLine : public Module
{
	CommandCLine cmd;
	CLineFactory f;

 public:
	ModuleCLine() : cmd(this) {}

	void init()
	{
		ServerInstance->XLines->RegisterFactory(&f);
		ServerInstance->AddCommand(&cmd);

		ServerInstance->Modules->Attach(I_OnStats, this);
		ServerInstance->Modules->Attach(I_OnCheckReady, this);
	}

	~ModuleCLine()
	{
		ServerInstance->XLines->DelAll("CLINE");
		ServerInstance->XLines->UnregisterFactory(&f);
	}

	ModResult OnStats(char symbol, User* user, string_list& out)
	{
		if (symbol != 'C')
			return MOD_RES_PASSTHRU;

		ServerInstance->XLines->InvokeStats("CLINE", 223, user, out);
		return MOD_RES_DENY;
	}

	ModResult OnCheckReady(LocalUser* user)
	{
		if (ServerInstance->ForcedClass.get(user))
			return MOD_RES_PASSTHRU;

		XLine* line = ServerInstance->XLines->MatchesLine("CLINE", user);
		if (!line)
			return MOD_RES_PASSTHRU;

		std::string::size_type cbrk = line->reason.find(' ');
		std::string cname = line->reason.substr(0, cbrk);

		ServerInstance->ForcedClass.set(user, cname);

		return MOD_RES_PASSTHRU;
	}

	Version GetVersion()
	{
		return Version("Provides the /cline command, which allows a connect class to be set on a user.",VF_VENDOR);
	}
};

MODULE_INIT(ModuleCLine)

