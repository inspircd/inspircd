/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2006-2007 Craig Edwards <craigedwards@brainbox.cc>
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

/* $ModDesc: Provides /tline command used to test who a mask matches */

/** Handle /TLINE
 */
class CommandTline : public Command
{
 public:
	CommandTline(Module* Creator) : Command(Creator,"TLINE", 1)
	{
		flags_needed = 'o'; this->syntax = "<mask>";
	}

	CmdResult Handle (const std::vector<std::string> &parameters, User *user)
	{
		float n_counted = 0;
		float n_matched = 0;
		float n_match_host = 0;
		float n_match_ip = 0;

		for (user_hash::const_iterator u = ServerInstance->Users->clientlist->begin(); u != ServerInstance->Users->clientlist->end(); u++)
		{
			n_counted++;
			if (InspIRCd::Match(u->second->GetFullRealHost(),parameters[0]))
			{
				n_matched++;
				n_match_host++;
			}
			else
			{
				std::string host = u->second->ident + "@" + u->second->GetIPString();
				if (InspIRCd::MatchCIDR(host, parameters[0]))
				{
					n_matched++;
					n_match_ip++;
				}
			}
		}
		if (n_matched)
			user->WriteServ( "NOTICE %s :*** TLINE: Counted %0.0f user(s). Matched '%s' against %0.0f user(s) (%0.2f%% of the userbase). %0.0f by hostname and %0.0f by IP address.",user->nick.c_str(), n_counted, parameters[0].c_str(), n_matched, (n_matched/n_counted)*100, n_match_host, n_match_ip);
		else
			user->WriteServ( "NOTICE %s :*** TLINE: Counted %0.0f user(s). Matched '%s' against no user(s).", user->nick.c_str(), n_counted, parameters[0].c_str());

		return CMD_SUCCESS;
	}
};

class ModuleTLine : public Module
{
	CommandTline cmd;
 public:
	ModuleTLine()
		: cmd(this)
	{
	}

	void init()
	{
		ServerInstance->Modules->AddService(cmd);
	}

	virtual ~ModuleTLine()
	{
	}

	virtual Version GetVersion()
	{
		return Version("Provides /tline command used to test who a mask matches", VF_VENDOR);
	}
};

MODULE_INIT(ModuleTLine)

