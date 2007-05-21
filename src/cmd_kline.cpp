/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2007 InspIRCd Development Team
 * See: http://www.inspircd.org/wiki/index.php/Credits
 *
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
#include "commands/cmd_kline.h"

extern "C" DllExport command_t* init_command(InspIRCd* Instance)
{
	return new cmd_kline(Instance);
}

/** Handle /KLINE
 */
CmdResult cmd_kline::Handle (const char** parameters, int pcnt, userrec *user)
{
	if (pcnt >= 3)
	{
		if (ServerInstance->HostMatchesEveryone(parameters[0],user))
			return CMD_FAILURE;

		if (!strchr(parameters[0],'@'))
		{       
			user->WriteServ("NOTICE %s :*** K-Line must contain a username, e.g. *@%s",user->nick,parameters[0]);
			return CMD_FAILURE;
		}
		else if (strchr(parameters[0],'!'))
		{
			user->WriteServ("NOTICE %s :*** K-Line cannot contain a nickname!",user->nick);
			return CMD_FAILURE;
		}

		if (ServerInstance->XLines->add_kline(ServerInstance->Duration(parameters[1]),user->nick,parameters[2],parameters[0]))
		{
			int to_apply = APPLY_KLINES;

			FOREACH_MOD(I_OnAddKLine,OnAddKLine(ServerInstance->Duration(parameters[1]), user, parameters[2], parameters[0]));
	
			if (!ServerInstance->Duration(parameters[1]))
			{
				ServerInstance->SNO->WriteToSnoMask('x',"%s added permanent K-line for %s.",user->nick,parameters[0]);
				to_apply |= APPLY_PERM_ONLY;
			}
			else
			{
				time_t c_requires_crap = ServerInstance->Duration(parameters[1]) + ServerInstance->Time();
				ServerInstance->SNO->WriteToSnoMask('x',"%s added timed K-line for %s, expires on %s",user->nick,parameters[0],
						ServerInstance->TimeString(c_requires_crap).c_str());
			}

			ServerInstance->XLines->apply_lines(to_apply);
		}
	}
	else
	{
		if (ServerInstance->XLines->del_kline(parameters[0]))
		{
			FOREACH_MOD(I_OnDelKLine,OnDelKLine(user, parameters[0]));
			ServerInstance->SNO->WriteToSnoMask('x',"%s Removed K-line on %s.",user->nick,parameters[0]);
		}
		else
		{
			user->WriteServ("NOTICE %s :*** K-Line %s not found in list, try /stats k.",user->nick,parameters[0]);
		}
	}

	return CMD_SUCCESS;
}

