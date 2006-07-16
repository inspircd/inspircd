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
#include "inspircd.h"
#include "configreader.h"
#include "users.h"
#include "modules.h"
#include "commands.h"
#include "xline.h"
#include "helperfuncs.h"
#include "commands/cmd_zline.h"

extern ServerConfig* Config;
extern int MODCOUNT;
extern ModuleList modules;
extern FactoryList factory;

void cmd_zline::Handle (const char** parameters, int pcnt, userrec *user)
{
	if (pcnt >= 3)
	{
		if (strchr(parameters[0],'@'))
		{
			WriteServ(user->fd,"NOTICE %s :*** You cannot include a username in a zline, a zline must ban only an IP mask",user->nick);
			return;
		}
		if (ip_matches_everyone(parameters[0],user))
			return;
		add_zline(duration(parameters[1]),user->nick,parameters[2],parameters[0]);
		FOREACH_MOD(I_OnAddZLine,OnAddZLine(duration(parameters[1]), user, parameters[2], parameters[0]));
		if (!duration(parameters[1]))
		{
			WriteOpers("*** %s added permanent Z-line for %s.",user->nick,parameters[0]);
		}
		else
		{
			WriteOpers("*** %s added timed Z-line for %s, expires in %d seconds.",user->nick,parameters[0],duration(parameters[1]));
		}
		apply_lines(APPLY_ZLINES);
	}
	else
	{
		if (del_zline(parameters[0]))
		{
			FOREACH_MOD(I_OnDelZLine,OnDelZLine(user, parameters[0]));
			WriteOpers("*** %s Removed Z-line on %s.",user->nick,parameters[0]);
		}
		else
		{
			WriteServ(user->fd,"NOTICE %s :*** Z-Line %s not found in list, try /stats Z.",user->nick,parameters[0]);
		}
	}
}
