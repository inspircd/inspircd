/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2017 Sadie Powell <sadie@witchery.services>
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

/// $ModAuthor: Sadie Powell
/// $ModAuthorMail: sadie@witchery.services
/// $ModDepends: core 3
/// $ModDesc: Allows users to be managed using services-assigned teams.


#include "inspircd.h"
#include "modules/whois.h"

enum
{
	// InspIRCd specific.
	RPL_WHOISTEAMS = 695
};

// Represents a list of teams that a user is a member of.
typedef insp::flat_set<std::string, irc::insensitive_swo> TeamList;

class TeamExt : public SimpleExtItem<TeamList>
{
 public:
	TeamExt(Module* Creator)
		: SimpleExtItem<TeamList>("teams", ExtensionItem::EXT_USER, Creator)
	{
	}

	std::string ToNetwork(const Extensible* container, void* item) const CXX11_OVERRIDE
	{
		TeamList* teamlist = static_cast<TeamList*>(item);
		return teamlist ? stdalgo::string::join(*teamlist) : "";
	}

	void FromNetwork(Extensible* container, const std::string& value) CXX11_OVERRIDE
	{
		// Create a new team list from the input.
		TeamList* newteamlist = new TeamList();
		irc::spacesepstream teamstream(value);
		for (std::string teamname; teamstream.GetToken(teamname); )
			newteamlist->insert(teamname);

		if (newteamlist->empty())
		{
			// If the new team list is empty then delete both the new and old team lists.
			unset(container);
			delete newteamlist;
		}
		else
		{
			// Otherwise install the new team list.
			set(container, newteamlist);
		}
	}
};

class ModuleTeams
	: public Module
	, public Whois::EventListener
{
 private:
	bool active;
	TeamExt ext;
	std::string teamchar;

	size_t ExecuteCommand(LocalUser* source, const char* cmd, CommandBase::Params& parameters,
		const std::string& team, size_t nickindex)
	{
		size_t targets = 0;
		std::string command(cmd);
		const user_hash& users = ServerInstance->Users->GetUsers();
		for (user_hash::const_iterator iter = users.begin(); iter != users.end(); ++iter)
		{
			User* user = iter->second;
			if (user->registered != REG_ALL)
				continue;
	
			TeamList* teams = ext.get(user);
			if (!teams || teams->count(team))
				continue;

			parameters[nickindex] = user->nick;
			ModResult modres;
			FIRST_MOD_RESULT(OnPreCommand, modres, (command, parameters, source, true));
			if (modres != MOD_RES_DENY)
			{
				ServerInstance->Parser.CallHandler(command, parameters, source);
				targets++;
			}
		}
		return targets;
	}

	bool IsTeam(const std::string& param, std::string& team)
	{
		if (param.length() <= teamchar.length() || param.compare(0, teamchar.length(), teamchar) != 0)
			return false;

		team.assign(param, teamchar.length() - 1, std::string::npos);
		return true;
	}

	ModResult HandleInvite(LocalUser* source, CommandBase::Params& parameters)
	{
		// Check we have enough parameters and a valid team.
		std::string team;
		if (parameters.size() < 2 || !IsTeam(parameters[0], team))
			return MOD_RES_PASSTHRU;

		active = true;
		size_t penalty = ExecuteCommand(source, "INVITE", parameters, team, 0);
		source->CommandFloodPenalty += std::min(penalty, 5UL);
		active = false;
		return MOD_RES_DENY;
	}

 public:
	ModuleTeams()
		: Whois::EventListener(this)
		, active(false)
		, ext(this)
	{
	}

	void ReadConfig(ConfigStatus& status) CXX11_OVERRIDE
	{
		ConfigTag* tag = ServerInstance->Config->ConfValue("teams");
		teamchar = tag->getString("prefix", "^", 1);
	}

	void On005Numeric(std::map<std::string, std::string>& tokens) CXX11_OVERRIDE
	{
		tokens["TEAMCHAR"] = teamchar;
	}

	ModResult OnCheckBan(User* user, Channel* channel, const std::string& mask) CXX11_OVERRIDE
	{
		if (mask.length() <= 2 || mask[0] != 't' || mask[1] != ':')
			return MOD_RES_PASSTHRU;

		TeamList* teams = ext.get(user);
		if (!teams)
			return MOD_RES_PASSTHRU;

		const std::string submask = mask.substr(2);
		for (TeamList::const_iterator iter = teams->begin(); iter != teams->end(); ++iter)
		{
			if (InspIRCd::Match(*iter, submask))
				return MOD_RES_DENY;
		}

		return MOD_RES_PASSTHRU;
	}

	ModResult OnPreCommand(std::string& command, CommandBase::Params& parameters, LocalUser* user, bool validated) CXX11_OVERRIDE
	{
		if (user->registered != REG_ALL || !validated || active)
			return MOD_RES_PASSTHRU;

		if (command == "INVITE")
			return HandleInvite(user, parameters);

		return MOD_RES_PASSTHRU;
	}

	void OnWhois(Whois::Context& whois) CXX11_OVERRIDE
	{
		TeamList* teams = ext.get(whois.GetTarget());
		if (teams)
		{
			const std::string teamstr = stdalgo::string::join(*teams);
			whois.SendLine(RPL_WHOISTEAMS, teamstr, "is a member of these teams");
		}
	}

	Version GetVersion() CXX11_OVERRIDE
	{
		return Version("Allows users to be managed using services-assigned teams", VF_OPTCOMMON);
	}
};

MODULE_INIT(ModuleTeams)
