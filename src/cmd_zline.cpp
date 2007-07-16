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
#include "commands/cmd_zline.h"



extern "C" DllExport command_t* init_command(InspIRCd* Instance)
{
	return new cmd_zline(Instance);
}

CmdResult cmd_zline::Handle (const char** parameters, int pcnt, userrec *user)
{
	if (pcnt >= 3)
	{
		if (strchr(parameters[0],'@') || strchr(parameters[0],'!'))
		{
			user->WriteServ("NOTICE %s :*** You cannot include a username or nickname in a zline, a zline must ban only an IP mask",user->nick);
			return CMD_FAILURE;
		}

		if (ServerInstance->IPMatchesEveryone(parameters[0],user))
			return CMD_FAILURE;

		long duration = ServerInstance->Duration(parameters[1]);
		if (ServerInstance->XLines->add_zline(duration,user->nick,parameters[2],parameters[0]))
		{
			int to_apply = APPLY_ZLINES;

			FOREACH_MOD(I_OnAddZLine,OnAddZLine(duration, user, parameters[2], parameters[0]));
			if (!duration)
			{
				to_apply |= APPLY_PERM_ONLY;
				ServerInstance->SNO->WriteToSnoMask('x',"%s added permanent Z-line for %s.",user->nick,parameters[0]);
			}
			else
			{
				time_t c_requires_crap = duration + ServerInstance->Time();
				ServerInstance->SNO->WriteToSnoMask('x',"%s added timed Z-line for %s, expires on %s",user->nick,parameters[0],
						ServerInstance->TimeString(c_requires_crap).c_str());
			}
			ServerInstance->XLines->apply_lines(to_apply);
		}
		else
		{
			user->WriteServ("NOTICE %s :*** Z-Line for %s already exists",user->nick,parameters[0]);
		}
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
			return CMD_FAILURE;
		}
	}

	return CMD_SUCCESS;
}
