/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2007 Robin Burchell <robin+git@viroteck.net>
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

/** Handle /COMMANDS. These command handlers can be reloaded by the core,
 * and handle basic RFC1459 commands. Commands within modules work
 * the same way, however, they can be fully unloaded, where these
 * may not.
 */
class CommandCommands : public Command
{
 public:
	/** Constructor for commands.
	 */
	CommandCommands(Module* parent) : Command(parent,"COMMANDS",0,0)
	{
		Penalty = 3;
	}

	/** Handle command.
	 * @param parameters The parameters to the comamnd
	 * @param pcnt The number of parameters passed to teh command
	 * @param user The user issuing the command
	 * @return A value from CmdResult to indicate command success or failure.
	 */
	CmdResult Handle(const std::vector<std::string>& parameters, User *user);
};

/** Handle /COMMANDS
 */
CmdResult CommandCommands::Handle (const std::vector<std::string>&, User *user)
{
	std::vector<std::string> list;
	list.reserve(ServerInstance->Parser->cmdlist.size());
	for (Commandtable::iterator i = ServerInstance->Parser->cmdlist.begin(); i != ServerInstance->Parser->cmdlist.end(); i++)
	{
		// Don't show S2S commands to users
		if (i->second->flags_needed == FLAG_SERVERONLY)
			continue;

		Module* src = i->second->creator;
		char buffer[MAXBUF];
		snprintf(buffer, MAXBUF, ":%s %03d %s :%s %s %d %d",
			ServerInstance->Config->ServerName.c_str(), RPL_COMMANDS, user->nick.c_str(),
			i->second->name.c_str(), src->ModuleSourceFile.c_str(),
			i->second->min_params, i->second->Penalty);
		list.push_back(buffer);
	}
	sort(list.begin(), list.end());
	for(unsigned int i=0; i < list.size(); i++)
		user->Write(list[i]);
	user->WriteNumeric(RPL_COMMANDSEND, "%s :End of COMMANDS list",user->nick.c_str());
	return CMD_SUCCESS;
}

COMMAND_INIT(CommandCommands)
