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
#include "xline.h"
#include "commands/cmd_qline.h"

void cmd_qline::Handle (const char** parameters, int pcnt, userrec *user)
{
	if (pcnt >= 3)
	{
		if (ServerInstance->NickMatchesEveryone(parameters[0],user))
			return;
		ServerInstance->XLines->add_qline(ServerInstance->Duration(parameters[1]),user->nick,parameters[2],parameters[0]);
		FOREACH_MOD(I_OnAddQLine,OnAddQLine(ServerInstance->Duration(parameters[1]), user, parameters[2], parameters[0]));
		if (!ServerInstance->Duration(parameters[1]))
		{
			ServerInstance->SNO->WriteToSnoMask('x',"%s added permanent Q-line for %s.",user->nick,parameters[0]);
		}
		else
		{
			ServerInstance->SNO->WriteToSnoMask('x',"%s added timed Q-line for %s, expires in %d seconds.",user->nick,parameters[0],ServerInstance->Duration(parameters[1]));
		}
		ServerInstance->XLines->apply_lines(APPLY_QLINES);
	}
	else
	{
		if (ServerInstance->XLines->del_qline(parameters[0]))
		{
			FOREACH_MOD(I_OnDelQLine,OnDelQLine(user, parameters[0]));
			ServerInstance->SNO->WriteToSnoMask('x',"%s Removed Q-line on %s.",user->nick,parameters[0]);
		}
		else
		{
			user->WriteServ("NOTICE %s :*** Q-Line %s not found in list, try /stats q.",user->nick,parameters[0]);
		}
	}
}
