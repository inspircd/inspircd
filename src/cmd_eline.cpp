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
#include "commands/cmd_eline.h"

extern "C" DllExport command_t* init_command(InspIRCd* Instance)
{
	return new cmd_eline(Instance);
}

/** Handle /ELINE
 */
CmdResult cmd_eline::Handle (const char** parameters, int pcnt, userrec *user)
{
	std::string target = parameters[0];

	if (pcnt >= 3)
	{
		IdentHostPair ih;
		userrec* find = ServerInstance->FindNick(target.c_str());
		if (find)
		{
			ih.first = "*";
			ih.second = find->GetIPString();
			target = std::string("*@") + find->GetIPString();
		}
		else
			ih = ServerInstance->XLines->IdentSplit(target.c_str());
			
		if (ih.first.empty())
		{
			user->WriteServ("NOTICE %s :*** Target not found", user->nick);
			return CMD_FAILURE;
		}

		if (ServerInstance->HostMatchesEveryone(ih.first+"@"+ih.second,user))
			return CMD_FAILURE;

		long duration = ServerInstance->Duration(parameters[1]);
		if (ServerInstance->XLines->add_eline(duration,user->nick,parameters[2],target.c_str()))
		{
			FOREACH_MOD(I_OnAddELine,OnAddELine(duration, user, parameters[2], target.c_str()));

			if (!duration)
			{
				ServerInstance->SNO->WriteToSnoMask('x',"%s added permanent E-line for %s.",user->nick,target.c_str());
			}
			else
			{
				time_t c_requires_crap = duration + ServerInstance->Time();
				ServerInstance->SNO->WriteToSnoMask('x',"%s added timed E-line for %s, expires on %s",user->nick,target.c_str(),
						ServerInstance->TimeString(c_requires_crap).c_str());
			}
		}
		else
		{
			user->WriteServ("NOTICE %s :*** E-Line for %s already exists",user->nick,target.c_str());
		}
	}
	else
	{
		if (ServerInstance->XLines->del_eline(target.c_str()))
		{
			FOREACH_MOD(I_OnDelELine,OnDelELine(user, target.c_str()));
			ServerInstance->SNO->WriteToSnoMask('x',"%s Removed E-line on %s.",user->nick,target.c_str());
		}
		else
		{
			user->WriteServ("NOTICE %s :*** E-Line %s not found in list, try /stats e.",user->nick,target.c_str());
		}
	}

	return CMD_SUCCESS;
}
