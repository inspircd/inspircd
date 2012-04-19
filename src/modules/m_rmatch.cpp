/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2010 Jackmcbarn <jackmcbarn@jackmcbarn.no-ip.org>
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
#include "m_regex.h"

/* $ModDesc: Provides /rmatch command used to view users a regular expression matches */

/** Handle /RMATCH
 */
class CommandRmatch : public Command
{
 public:
	dynamic_reference<RegexFactory> rxfactory;
	CommandRmatch(Module* Creator) : Command(Creator,"RMATCH", 1, 1), rxfactory("regex")
	{
		flags_needed = 'o'; this->syntax = "<regex>";
	}

	CmdResult Handle (const std::vector<std::string> &parameters, User *user)
	{
		unsigned long n_counted = 0;
		unsigned long n_matched = 0;
		Regex* regex = rxfactory->Create(parameters[0], REGEX_NONE);
		std::string compare;

		for (user_hash::const_iterator u = ServerInstance->Users->clientlist->begin(); u != ServerInstance->Users->clientlist->end(); u++)
		{
			++n_counted;
			compare = u->second->nick + "!" + u->second->ident + "@" + u->second->host + " " + u->second->fullname;
			if(regex->Matches(compare))
			{
				++n_matched;
				user->WriteServ("NOTICE %s :*** RMATCH: Matched user: %s", user->nick.c_str(), compare.c_str());
			}
		}
		delete regex;
		if (n_matched)
			user->WriteServ( "NOTICE %s :*** RMATCH: Counted %lu user(s). Matched /%s/ against %lu user(s) (%0.2f%% of the userbase).",user->nick.c_str(), n_counted, parameters[0].c_str(), n_matched, n_matched*100.0/n_counted);
		else
			user->WriteServ( "NOTICE %s :*** RMATCH: Counted %lu user(s). Matched /%s/ against no users.", user->nick.c_str(), n_counted, parameters[0].c_str());

		return CMD_SUCCESS;
	}
};

class ModuleRmatch : public Module
{
	CommandRmatch cmd;
 public:
	ModuleRmatch() : cmd(this) {}

	void init()
	{
		ServerInstance->AddCommand(&cmd);
	}

	void ReadConfig(ConfigReadStatus&)
	{
		std::string newrxengine = ServerInstance->Config->GetTag("rmatch")->getString("engine");

		if (newrxengine.empty())
			cmd.rxfactory.SetProvider("regex");
		else
			cmd.rxfactory.SetProvider("regex/" + newrxengine);
		if (!cmd.rxfactory)
			ServerInstance->SNO->WriteToSnoMask('a', "WARNING: Regex engine '%s' is not loaded - RMATCH command disabled until this is corrected.", newrxengine.c_str());
	}

	Version GetVersion()
	{
		return Version("Provides /rmatch command used to view users a regular expression matches", VF_VENDOR);
	}
};

MODULE_INIT(ModuleRmatch)

