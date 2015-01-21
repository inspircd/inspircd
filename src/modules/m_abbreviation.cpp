/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2008 Craig Edwards <craigedwards@brainbox.cc>
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

/* $ModDesc: Provides the ability to abbreviate commands a-la BBC BASIC keywords. */

class ModuleAbbreviation : public Module
{
 public:
	void init()
	{
		ServerInstance->Modules->Attach(I_OnPreCommand, this);
	}

	void Prioritize()
	{
		ServerInstance->Modules->SetPriority(this, I_OnPreCommand, PRIORITY_FIRST);
	}

	virtual Version GetVersion()
	{
		return Version("Provides the ability to abbreviate commands a-la BBC BASIC keywords.",VF_VENDOR);
	}

	virtual ModResult OnPreCommand(std::string &command, std::vector<std::string> &parameters, LocalUser *user, bool validated, const std::string &original_line)
	{
		/* Command is already validated, has a length of 0, or last character is not a . */
		if (validated || command.empty() || *command.rbegin() != '.')
			return MOD_RES_PASSTHRU;

		/* Whack the . off the end */
		command.erase(command.end() - 1);

		/* Look for any command that starts with the same characters, if it does, replace the command string with it */
		size_t clen = command.length();
		std::string foundcommand, matchlist;
		bool foundmatch = false;
		for (Commandtable::iterator n = ServerInstance->Parser->cmdlist.begin(); n != ServerInstance->Parser->cmdlist.end(); ++n)
		{
			if (n->first.length() < clen)
				continue;

			if (command == n->first.substr(0, clen))
			{
				if (matchlist.length() > 450)
				{
					user->WriteNumeric(420, "%s :Ambiguous abbreviation and too many possible matches.", user->nick.c_str());
					return MOD_RES_DENY;
				}

				if (!foundmatch)
				{
					/* Found the command */
					foundcommand = n->first;
					foundmatch = true;
				}
				else
					matchlist.append(" ").append(n->first);
			}
		}

		/* Ambiguous command, list the matches */
		if (!matchlist.empty())
		{
			user->WriteNumeric(420, "%s :Ambiguous abbreviation, possible matches: %s%s", user->nick.c_str(), foundcommand.c_str(), matchlist.c_str());
			return MOD_RES_DENY;
		}

		if (foundcommand.empty())
		{
			/* No match, we have to put the . back again so that the invalid command numeric looks correct. */
			command += '.';
		}
		else
		{
			command = foundcommand;
		}

		return MOD_RES_PASSTHRU;
	}
};

MODULE_INIT(ModuleAbbreviation)
