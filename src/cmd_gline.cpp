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

extern InspIRCd* ServerInstance;
extern int MODCOUNT;
extern std::vector<Module*> modules;
extern std::vector<ircd_module*> factory;

void cmd_gline::Handle (const char** parameters, int pcnt, userrec *user)
{
	if (pcnt >= 3)
	{
		if (host_matches_everyone(parameters[0],user))
			return;

		add_gline(duration(parameters[1]),user->nick,parameters[2],parameters[0]);
		FOREACH_MOD(I_OnAddGLine,OnAddGLine(duration(parameters[1]), user, parameters[2], parameters[0]));

		if (!duration(parameters[1]))
		{
			WriteOpers("*** %s added permanent G-line for %s.",user->nick,parameters[0]);
		}
		else
		{
			WriteOpers("*** %s added timed G-line for %s, expires in %d seconds.",user->nick,parameters[0],duration(parameters[1]));
		}

		apply_lines(APPLY_GLINES);
	}
	else
	{
		if (del_gline(parameters[0]))
		{
			FOREACH_MOD(I_OnDelGLine,OnDelGLine(user, parameters[0]));
			WriteOpers("*** %s Removed G-line on %s.",user->nick,parameters[0]);
		}
		else
		{
			user->WriteServ("NOTICE %s :*** G-Line %s not found in list, try /stats g.",user->nick,parameters[0]);
		}
	}
}
