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

#include <string>
#include <vector>
#include "configreader.h"
#include "users.h"
#include "modules.h"
#include "commands.h"
#include "xline.h"
#include "helperfuncs.h"
#include "commands/cmd_eline.h"

extern int MODCOUNT;
extern ServerConfig* Config;
extern std::vector<Module*> modules;
extern std::vector<ircd_module*> factory;

void cmd_eline::Handle (const char** parameters, int pcnt, userrec *user)
{
	if (pcnt >= 3)
	{
		if (host_matches_everyone(parameters[0],user))
			return;

		add_eline(duration(parameters[1]),user->nick,parameters[2],parameters[0]);
		FOREACH_MOD(I_OnAddELine,OnAddELine(duration(parameters[1]), user, parameters[2], parameters[0]));

		if (!duration(parameters[1]))
		{
			WriteOpers("*** %s added permanent E-line for %s.",user->nick,parameters[0]);
		}
		else
		{
			WriteOpers("*** %s added timed E-line for %s, expires in %d seconds.",user->nick,parameters[0],duration(parameters[1]));
		}
	}
	else
	{
		if (del_eline(parameters[0]))
		{
			FOREACH_MOD(I_OnDelELine,OnDelELine(user, parameters[0]));
			WriteOpers("*** %s Removed E-line on %s.",user->nick,parameters[0]);
		}
		else
		{
			user->WriteServ("NOTICE %s :*** E-Line %s not found in list, try /stats e.",user->nick,parameters[0]);
		}
	}

	// no need to apply the lines for an eline
}
