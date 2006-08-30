/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd is copyright (C) 2002-2006 ChatSpike-Dev.
 *		       E-mail:
 *		<brain@chatspike.net>
 *		<Craig@chatspike.net>
 *
 * Written by Craig Edwards, Craig McLure, and others.
 * This program is free but copyrighted software; see
 *	    the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "configreader.h"
#include "users.h"
#include "modules.h"
#include "xline.h"
#include "commands/cmd_gline.h"

void cmd_gline::Handle (const char** parameters, int pcnt, userrec *user)
{
	if (pcnt >= 3)
	{
		if (ServerInstance->HostMatchesEveryone(parameters[0],user))
			return;

		if (!strchr(parameters[0],'@'))
		{       
			user->WriteServ("NOTICE %s :*** G-Line must contain a username, e.g. *@%s",user->nick,parameters[0]);
			return;
		}

		ServerInstance->XLines->add_gline(ServerInstance->Duration(parameters[1]),user->nick,parameters[2],parameters[0]);
		FOREACH_MOD(I_OnAddGLine,OnAddGLine(ServerInstance->Duration(parameters[1]), user, parameters[2], parameters[0]));

		if (!ServerInstance->Duration(parameters[1]))
		{
			ServerInstance->SNO->WriteToSnoMask('x',"%s added permanent G-line for %s.",user->nick,parameters[0]);
		}
		else
		{
			ServerInstance->SNO->WriteToSnoMask('x',"%s added timed G-line for %s, expires in %d seconds.",user->nick,parameters[0],ServerInstance->Duration(parameters[1]));
		}

		ServerInstance->XLines->apply_lines(APPLY_GLINES);
	}
	else
	{
		if (ServerInstance->XLines->del_gline(parameters[0]))
		{
			FOREACH_MOD(I_OnDelGLine,OnDelGLine(user, parameters[0]));
			ServerInstance->SNO->WriteToSnoMask('x',"%s Removed G-line on %s.",user->nick,parameters[0]);
		}
		else
		{
			user->WriteServ("NOTICE %s :*** G-Line %s not found in list, try /stats g.",user->nick,parameters[0]);
		}
	}
}
