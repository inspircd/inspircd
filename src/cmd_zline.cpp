/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2008 InspIRCd Development Team
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
	std::string target;

	if (pcnt >= 3)
	{
		if (strchr(target.c_str(),'@') || strchr(target.c_str(),'!'))
		{
			user->WriteServ("NOTICE %s :*** You cannot include a username or nickname in a zline, a zline must ban only an IP mask",user->nick);
			return CMD_FAILURE;
		}

		userrec *u = ServerInstance->FindNick(target.c_str());
		
		if (u)
		{
			target = u->GetIPString();
		}

		if (ServerInstance->IPMatchesEveryone(target.c_str(),user))
			return CMD_FAILURE;

		long duration = ServerInstance->Duration(parameters[1]);
		if (ServerInstance->XLines->add_zline(duration,user->nick,parameters[2],target.c_str()))
		{
			int to_apply = APPLY_ZLINES;

			FOREACH_MOD(I_OnAddZLine,OnAddZLine(duration, user, parameters[2], target.c_str()));
			if (!duration)
			{
				to_apply |= APPLY_PERM_ONLY;
				ServerInstance->SNO->WriteToSnoMask('x',"%s added permanent Z-line for %s.",user->nick,target.c_str());
			}
			else
			{
				time_t c_requires_crap = duration + ServerInstance->Time();
				ServerInstance->SNO->WriteToSnoMask('x',"%s added timed Z-line for %s, expires on %s",user->nick,target.c_str(),
						ServerInstance->TimeString(c_requires_crap).c_str());
			}
			ServerInstance->XLines->apply_lines(to_apply);
		}
		else
		{
			user->WriteServ("NOTICE %s :*** Z-Line for %s already exists",user->nick,target.c_str());
		}
	}
	else
	{
		if (ServerInstance->XLines->del_zline(target.c_str()))
		{
			FOREACH_MOD(I_OnDelZLine,OnDelZLine(user, target.c_str()));
			ServerInstance->SNO->WriteToSnoMask('x',"%s Removed Z-line on %s.",user->nick,target.c_str());
		}
		else
		{
			user->WriteServ("NOTICE %s :*** Z-Line %s not found in list, try /stats Z.",user->nick,target.c_str());
			return CMD_FAILURE;
		}
	}

	return CMD_SUCCESS;
}
