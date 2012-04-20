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
	CommandClose (InspIRCd* Instance) : Command(Instance,"CLOSE", "o", 0)
	{
		this->source = "m_close.so";
	}

	CmdResult Handle (const std::vector<std::string> &parameters, User *user)
	{
		std::map<std::string,int> closed;

		for (std::vector<User*>::iterator u = ServerInstance->Users->local_users.begin(); u != ServerInstance->Users->local_users.end(); u++)
		{
			if ((*u)->registered != REG_ALL)
			{
				ServerInstance->Users->QuitUser(*u, "Closing all unknown connections per request");
				std::string key = ConvToStr((*u)->GetIPString())+"."+ConvToStr((*u)->GetPort());
				closed[key]++;
			}
		}

		int total = 0;
		for (std::map<std::string,int>::iterator ci = closed.begin(); ci != closed.end(); ci++)
		{
			user->WriteServ("NOTICE %s :*** Closed %d unknown connection%s from [%s]",user->nick.c_str(),(*ci).second,((*ci).second>1)?"s":"",(*ci).first.c_str());
			total += (*ci).second;
		}
		if (total)
			user->WriteServ("NOTICE %s :*** %i unknown connection%s closed",user->nick.c_str(),total,(total>1)?"s":"");
		else
			user->WriteServ("NOTICE %s :*** No unknown connections found",user->nick.c_str());

		return CMD_LOCALONLY;
	}
};

class ModuleClose : public Module
{
	CommandClose* newcommand;
 public:
	ModuleClose(InspIRCd* Me)
		: Module(Me)
	{
		// Create a new command
		newcommand = new CommandClose(ServerInstance);
		ServerInstance->AddCommand(newcommand);

	}

	virtual ~ModuleClose()
	{
	}

	virtual Version GetVersion()
	{
		return Version("$Id$", VF_VENDOR, API_VERSION);
	}
};

MODULE_INIT(ModuleClose)
