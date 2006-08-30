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
#include "configreader.h"
#include "users.h"
#include "modules.h"
#include "xline.h"
#include "commands/cmd_zline.h"

void cmd_zline::Handle (const char** parameters, int pcnt, userrec *user)
{
	if (pcnt >= 3)
	{
		if (strchr(parameters[0],'@'))
		{
			user->WriteServ("NOTICE %s :*** You cannot include a username in a zline, a zline must ban only an IP mask",user->nick);
			return;
		}
		if (ServerInstance->IPMatchesEveryone(parameters[0],user))
			return;
		ServerInstance->XLines->add_zline(ServerInstance->Duration(parameters[1]),user->nick,parameters[2],parameters[0]);
		FOREACH_MOD(I_OnAddZLine,OnAddZLine(ServerInstance->Duration(parameters[1]), user, parameters[2], parameters[0]));
		if (!ServerInstance->Duration(parameters[1]))
		{
			ServerInstance->SNO->WriteToSnoMask('x',"%s added permanent Z-line for %s.",user->nick,parameters[0]);
		}
		else
		{
			ServerInstance->SNO->WriteToSnoMask('x',"%s added timed Z-line for %s, expires in %d seconds.",user->nick,parameters[0],ServerInstance->Duration(parameters[1]));
		}
		ServerInstance->XLines->apply_lines(APPLY_ZLINES);
	}
	else
	{
		if (ServerInstance->XLines->del_zline(parameters[0]))
		{
			FOREACH_MOD(I_OnDelZLine,OnDelZLine(user, parameters[0]));
			ServerInstance->SNO->WriteToSnoMask('x',"%s Removed Z-line on %s.",user->nick,parameters[0]);
		}
		else
		{
			user->WriteServ("NOTICE %s :*** Z-Line %s not found in list, try /stats Z.",user->nick,parameters[0]);
		}
	}
}
