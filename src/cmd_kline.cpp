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

#include <time.h>
#include <string>
#include <map>
#include <sstream>
#include <vector>
#include <deque>

#include "inspircd_config.h"
#include "configreader.h"
#include "hash_map.h"
#include "users.h"
#include "modules.h"
#include "commands.h"
#include "xline.h"

#include "commands/cmd_kline.h"

void cmd_kline::Handle (const char** parameters, int pcnt, userrec *user)
{
	if (pcnt >= 3)
	{
		if (ServerInstance->HostMatchesEveryone(parameters[0],user))
			return;

		if (!strchr(parameters[0],'@'))
		{       
			user->WriteServ("NOTICE %s :*** K-Line must contain a username, e.g. *@%s",user->nick,parameters[0]);
			return;
		}

		ServerInstance->XLines->add_kline(ServerInstance->Duration(parameters[1]),user->nick,parameters[2],parameters[0]);
		FOREACH_MOD(I_OnAddKLine,OnAddKLine(ServerInstance->Duration(parameters[1]), user, parameters[2], parameters[0]));

		if (!ServerInstance->Duration(parameters[1]))
		{
			ServerInstance->WriteOpers("*** %s added permanent K-line for %s.",user->nick,parameters[0]);
		}
		else
		{
			ServerInstance->WriteOpers("*** %s added timed K-line for %s, expires in %d seconds.",user->nick,parameters[0],ServerInstance->Duration(parameters[1]));
		}

		ServerInstance->XLines->apply_lines(APPLY_KLINES);
	}
	else
	{
		if (ServerInstance->XLines->del_kline(parameters[0]))
		{
			FOREACH_MOD(I_OnDelKLine,OnDelKLine(user, parameters[0]));
			ServerInstance->WriteOpers("*** %s Removed K-line on %s.",user->nick,parameters[0]);
		}
		else
		{
			user->WriteServ("NOTICE %s :*** K-Line %s not found in list, try /stats k.",user->nick,parameters[0]);
		}
	}
}
