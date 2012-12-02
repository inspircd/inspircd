/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2007 Carsten Valdemar Munk <carsten.munk+inspircd@gmail.com>
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

/* $ModDesc: Provides /CLOSE functionality */

/** Handle /CLOSE
 */
class CommandClose : public Command
{
 public:
	/* Command 'close', needs operator */
	CommandClose(Module* Creator) : Command(Creator,"CLOSE", 0)
	{
	flags_needed = 'o'; }

	CmdResult Handle (const std::vector<std::string> &parameters, User *src)
	{
		std::map<std::string,int> closed;

		for (LocalUserList::const_iterator u = ServerInstance->Users->local_users.begin(); u != ServerInstance->Users->local_users.end(); ++u)
		{
			LocalUser* user = *u;
			if (user->registered != REG_ALL)
			{
				ServerInstance->Users->QuitUser(user, "Closing all unknown connections per request");
				std::string key = ConvToStr(user->GetIPString())+"."+ConvToStr(user->GetServerPort());
				closed[key]++;
			}
		}

		int total = 0;
		for (std::map<std::string,int>::iterator ci = closed.begin(); ci != closed.end(); ci++)
		{
			src->WriteServ("NOTICE %s :*** Closed %d unknown connection%s from [%s]",src->nick.c_str(),(*ci).second,((*ci).second>1)?"s":"",(*ci).first.c_str());
			total += (*ci).second;
		}
		if (total)
			src->WriteServ("NOTICE %s :*** %i unknown connection%s closed",src->nick.c_str(),total,(total>1)?"s":"");
		else
			src->WriteServ("NOTICE %s :*** No unknown connections found",src->nick.c_str());

		return CMD_SUCCESS;
	}
};

class ModuleClose : public Module
{
	CommandClose cmd;
 public:
	ModuleClose()
		: cmd(this)
	{
	}

	void init()
	{
		ServerInstance->Modules->AddService(cmd);
	}

	virtual ~ModuleClose()
	{
	}

	virtual Version GetVersion()
	{
		return Version("Provides /CLOSE functionality", VF_VENDOR);
	}
};

MODULE_INIT(ModuleClose)
