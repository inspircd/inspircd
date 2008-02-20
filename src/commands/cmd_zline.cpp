/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2008 InspIRCd Development Team
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

CmdResult CommandZline::Handle (const char* const* parameters, int pcnt, User *user)
{
	std::string target = parameters[0];

	if (pcnt >= 3)
	{
		if (strchr(target.c_str(),'@') || strchr(target.c_str(),'!'))
		{
			user->WriteServ("NOTICE %s :*** You cannot include a username or nickname in a zline, a zline must ban only an IP mask",user->nick);
			return CMD_FAILURE;
		}

		User *u = ServerInstance->FindNick(target.c_str());
		
		if (u)
		{
			target = u->GetIPString();
		}

		if (ServerInstance->IPMatchesEveryone(target.c_str(),user))
			return CMD_FAILURE;

		long duration = ServerInstance->Duration(parameters[1]);

		const char* ipaddr = target.c_str();
		User* find = ServerInstance->FindNick(target.c_str());

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
				ServerInstance->SNO->WriteToSnoMask('x',"%s added permanent Z-line for %s.",user->nick,target.c_str());
			}
			else
			{
				time_t c_requires_crap = duration + ServerInstance->Time();
				ServerInstance->SNO->WriteToSnoMask('x',"%s added timed Z-line for %s, expires on %s",user->nick,target.c_str(),
						ServerInstance->TimeString(c_requires_crap).c_str());
			}
			ServerInstance->XLines->ApplyLines();
		}
		else
		{
			delete zl;
			user->WriteServ("NOTICE %s :*** Z-Line for %s already exists",user->nick,target.c_str());
		}
	}
	else
	{
		if (ServerInstance->XLines->DelLine(target.c_str(),"Z",user))
		{
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
