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
#include "commands/cmd_kline.h"

extern "C" DllExport Command* init_command(InspIRCd* Instance)
{
	return new CommandKline(Instance);
}

/** Handle /KLINE
 */
CmdResult CommandKline::Handle (const char** parameters, int pcnt, User *user)
{
	if (pcnt >= 3)
	{
		IdentHostPair ih;
		User* find = ServerInstance->FindNick(parameters[0]);
		if (find)
		{
			ih.first = "*";
			ih.second = find->GetIPString();
		}
		else
			ih = ServerInstance->XLines->IdentSplit(parameters[0]);

		if (ServerInstance->HostMatchesEveryone(ih.first+"@"+ih.second,user))
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

		long duration = ServerInstance->Duration(parameters[1]);
		KLine* kl = new KLine(ServerInstance, ServerInstance->Time(), duration, user->nick, parameters[2], ih.first.c_str(), ih.second.c_str());
		if (ServerInstance->XLines->AddLine(kl,user))
		{
			if (!duration)
			{
				ServerInstance->SNO->WriteToSnoMask('x',"%s added permanent K-line for %s.",user->nick,parameters[0]);
			}
			else
			{
				time_t c_requires_crap = duration + ServerInstance->Time();
				ServerInstance->SNO->WriteToSnoMask('x',"%s added timed K-line for %s, expires on %s",user->nick,parameters[0],
						ServerInstance->TimeString(c_requires_crap).c_str());
			}

			ServerInstance->XLines->ApplyLines();
		}
		else
		{
			delete kl;
			user->WriteServ("NOTICE %s :*** K-Line for %s already exists",user->nick,parameters[0]);
		}
	}
	else
	{
		if (ServerInstance->XLines->DelLine(parameters[0],"K",user))
		{
			ServerInstance->SNO->WriteToSnoMask('x',"%s Removed K-line on %s.",user->nick,parameters[0]);
		}
		else
		{
			user->WriteServ("NOTICE %s :*** K-Line %s not found in list, try /stats k.",user->nick,parameters[0]);
		}
	}

	return CMD_SUCCESS;
}

