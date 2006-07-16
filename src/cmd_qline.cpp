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
#include "xline.h"
#include "helperfuncs.h"
#include "commands/cmd_qline.h"

extern ServerConfig* Config;
extern int MODCOUNT;
extern ModuleList modules;
extern FactoryList factory;

void cmd_qline::Handle (const char** parameters, int pcnt, userrec *user)
{
	if (pcnt >= 3)
	{
		if (nick_matches_everyone(parameters[0],user))
			return;
		add_qline(duration(parameters[1]),user->nick,parameters[2],parameters[0]);
		FOREACH_MOD(I_OnAddQLine,OnAddQLine(duration(parameters[1]), user, parameters[2], parameters[0]));
		if (!duration(parameters[1]))
		{
			WriteOpers("*** %s added permanent Q-line for %s.",user->nick,parameters[0]);
		}
		else
		{
			WriteOpers("*** %s added timed Q-line for %s, expires in %d seconds.",user->nick,parameters[0],duration(parameters[1]));
		}
		apply_lines(APPLY_QLINES);
	}
	else
	{
		if (del_qline(parameters[0]))
		{
			FOREACH_MOD(I_OnDelQLine,OnDelQLine(user, parameters[0]));
			WriteOpers("*** %s Removed Q-line on %s.",user->nick,parameters[0]);
		}
		else
		{
			WriteServ(user->fd,"NOTICE %s :*** Q-Line %s not found in list, try /stats q.",user->nick,parameters[0]);
		}
	}
}
