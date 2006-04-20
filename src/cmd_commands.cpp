/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd is copyright (C) 2002-2006 ChatSpike-Dev.
 *                       E-mail:
 *                <brain@chatspike.net>
 *                <Craig@chatspike.net>
 *
 * Written by Craig Edwards, Craig McLure, and others.
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"
#include "users.h"
#include "commands.h"
#include "helperfuncs.h"
#include "commands/cmd_commands.h"

extern InspIRCd* ServerInstance;

void cmd_commands::Handle (char **parameters, int pcnt, userrec *user)
{
	for (nspace::hash_map<std::string,command_t*>::iterator i = ServerInstance->Parser->cmdlist.begin(); i != ServerInstance->Parser->cmdlist.end(); i++)
	{
		WriteServ(user->fd,"902 %s :%s %s %d",user->nick,i->second->command.c_str(),i->second->source.c_str(),i->second->min_params);
	}
	WriteServ(user->fd,"903 %s :End of COMMANDS list",user->nick);
}
