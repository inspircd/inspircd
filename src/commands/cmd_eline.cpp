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
#include "commands/cmd_eline.h"

extern "C" DllExport Command* init_command(InspIRCd* Instance)
{
	return new CommandEline(Instance);
}

/** Handle /ELINE
 */
CmdResult CommandEline::Handle (const char** parameters, int pcnt, User *user)
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
			user->WriteServ("NOTICE %s :*** E-Line must contain a username, e.g. *@%s",user->nick,parameters[0]);
			return CMD_FAILURE;
		}

		long duration = ServerInstance->Duration(parameters[1]);

		ELine* el = new ELine(ServerInstance, ServerInstance->Time(), duration, user->nick, parameters[2], ih.first.c_str(), ih.second.c_str());
		if (ServerInstance->XLines->AddLine(el, user))
		{
			if (!duration)
			{
				ServerInstance->SNO->WriteToSnoMask('x',"%s added permanent E-line for %s.",user->nick,parameters[0]);
			}
			else
			{
				time_t c_requires_crap = duration + ServerInstance->Time();
				ServerInstance->SNO->WriteToSnoMask('x',"%s added timed E-line for %s, expires on %s",user->nick,parameters[0],
						ServerInstance->TimeString(c_requires_crap).c_str());
			}
		}
		else
		{
			delete el;
			user->WriteServ("NOTICE %s :*** E-Line for %s already exists",user->nick,parameters[0]);
		}
	}
	else
	{
		if (ServerInstance->XLines->DelLine(parameters[0], "E", user))
		{
			ServerInstance->SNO->WriteToSnoMask('x',"%s Removed E-line on %s.",user->nick,parameters[0]);
		}
		else
		{
			user->WriteServ("NOTICE %s :*** E-Line %s not found in list, try /stats e.",user->nick,parameters[0]);
		}
	}

	return CMD_SUCCESS;
}
