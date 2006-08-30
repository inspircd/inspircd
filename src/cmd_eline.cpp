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
#include "commands/cmd_eline.h"

void cmd_eline::Handle (const char** parameters, int pcnt, userrec *user)
{
	if (pcnt >= 3)
	{
		if (ServerInstance->HostMatchesEveryone(parameters[0],user))
			return;

		if (!strchr(parameters[0],'@'))
		{
			user->WriteServ("NOTICE %s :*** E-Line must contain a username, e.g. *@%s",user->nick,parameters[0]);
			return;
		}

		ServerInstance->XLines->add_eline(ServerInstance->Duration(parameters[1]),user->nick,parameters[2],parameters[0]);
		FOREACH_MOD(I_OnAddELine,OnAddELine(ServerInstance->Duration(parameters[1]), user, parameters[2], parameters[0]));

		if (!ServerInstance->Duration(parameters[1]))
		{
			ServerInstance->SNO->WriteToSnoMask('x',"%s added permanent E-line for %s.",user->nick,parameters[0]);
		}
		else
		{
			ServerInstance->SNO->WriteToSnoMask('x',"%s added timed E-line for %s, expires in %d seconds.",user->nick,parameters[0],ServerInstance->Duration(parameters[1]));
		}
	}
	else
	{
		if (ServerInstance->XLines->del_eline(parameters[0]))
		{
			FOREACH_MOD(I_OnDelELine,OnDelELine(user, parameters[0]));
			ServerInstance->SNO->WriteToSnoMask('x',"%s Removed E-line on %s.",user->nick,parameters[0]);
		}
		else
		{
			user->WriteServ("NOTICE %s :*** E-Line %s not found in list, try /stats e.",user->nick,parameters[0]);
		}
	}

	// no need to apply the lines for an eline
}
