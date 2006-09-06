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

#include "configreader.h"
#include "users.h"
#include "modules.h"
#include "commands/cmd_rehash.h"



extern "C" command_t* init_command(InspIRCd* Instance)
{
	return new cmd_rehash(Instance);
}

CmdResult cmd_rehash::Handle (const char** parameters, int pcnt, userrec *user)
{
	user->WriteServ("382 %s %s :Rehashing",user->nick,ServerConfig::CleanFilename(CONFIG_FILE));
	std::string parameter = "";
	if (pcnt)
	{
		parameter = parameters[0];
	}
	else
	{
		ServerInstance->WriteOpers("%s is rehashing config file %s",user->nick,ServerConfig::CleanFilename(CONFIG_FILE));
		ServerInstance->Config->Read(false,user);
	}
	FOREACH_MOD(I_OnRehash,OnRehash(parameter));

	return CMD_SUCCESS;
}

