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

#include "inspircd_config.h"
#include "configreader.h"
#include "users.h"
#include "modules.h"
#include "commands.h"
#include "helperfuncs.h"
#include "cmd_rehash.h"

extern ServerConfig* Config;
extern int MODCOUNT;
extern ModuleList modules;
extern FactoryList factory;

void cmd_rehash::Handle (char **parameters, int pcnt, userrec *user)
{
	WriteServ(user->fd,"382 %s %s :Rehashing",user->nick,CleanFilename(CONFIG_FILE));
	std::string parameter = "";
	if (pcnt)
	{
		parameter = parameters[0];
	}
	else
	{
		WriteOpers("%s is rehashing config file %s",user->nick,CleanFilename(CONFIG_FILE));
		Config->Read(false,user);
	}
	FOREACH_MOD(I_OnRehash,OnRehash(parameter));
}
