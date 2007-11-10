/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2007 InspIRCd Development Team
 * See: http://www.inspircd.org/wiki/index.php/Credits
 *
 * This program is free but copyrighted software; see
 *	    the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"
#include "xline.h"
#include "commands/cmd_zline.h"



extern "C" DllExport Command* init_command(InspIRCd* Instance)
{
	return new CommandZline(Instance);
}

CmdResult CommandZline::Handle (const char** parameters, int pcnt, User *user)
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

		const char* ipaddr = parameters[0];
		User* find = ServerInstance->FindNick(parameters[0]);

		if (find)
		{
			ipaddr = find->GetIPString();
		}
		else
		{
			if (strchr(ipaddr,'@'))
			{
				while (*ipaddr != '@')
					ipaddr++;
				ipaddr++;
			}
		}
		ZLine* zl = new ZLine(ServerInstance, ServerInstance->Time(), duration, user->nick, parameters[2], ipaddr);
		if (ServerInstance->XLines->AddLine(zl,user))
		{
			if (!duration)
			{
				ServerInstance->SNO->WriteToSnoMask('x',"%s added permanent Z-line for %s.",user->nick,parameters[0]);
			}
			else
			{
				time_t c_requires_crap = duration + ServerInstance->Time();
				ServerInstance->SNO->WriteToSnoMask('x',"%s added timed Z-line for %s, expires on %s",user->nick,parameters[0],
						ServerInstance->TimeString(c_requires_crap).c_str());
			}
			ServerInstance->XLines->ApplyLines();
		}
		else
		{
			delete zl;
			user->WriteServ("NOTICE %s :*** Z-Line for %s already exists",user->nick,parameters[0]);
		}
	}
	else
	{
		if (ServerInstance->XLines->DelLine(parameters[0],"Z",user))
		{
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
