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
#include "commands/cmd_gline.h"

extern "C" DllExport command_t* init_command(InspIRCd* Instance)
{
	return new cmd_gline(Instance);
}

/** Handle /GLINE
 */
CmdResult cmd_gline::Handle (const char** parameters, int pcnt, userrec *user)
{
	std::string target = parameters[0];
	
	if (pcnt >= 3)
	{
		IdentHostPair ih;
		userrec* find = ServerInstance->FindNick(target.c_str());
		if (find)
		{
			std::string c = std::string("*@") + find->GetIPString();
			ih.first = "*";
			ih.second = find->GetIPString();
			target = c.c_str();
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

		else if (strchr(target.c_str(),'!'))
		{
			user->WriteServ("NOTICE %s :*** G-Line cannot operate on nick!user@host masks",user->nick);
			return CMD_FAILURE;
		}

		long duration = ServerInstance->Duration(parameters[1]);
		if (ServerInstance->XLines->add_gline(duration,user->nick,parameters[2],target.c_str()))
		{
			int to_apply = APPLY_GLINES;

			FOREACH_MOD(I_OnAddGLine,OnAddGLine(duration, user, parameters[2], target.c_str()));

			if (!duration)
			{
				ServerInstance->SNO->WriteToSnoMask('x',"%s added permanent G-line for %s.",user->nick,target.c_str());
				to_apply |= APPLY_PERM_ONLY;
			}
			else
			{
				time_t c_requires_crap = duration + ServerInstance->Time();
				ServerInstance->SNO->WriteToSnoMask('x',"%s added timed G-line for %s, expires on %s",user->nick,target.c_str(),
						ServerInstance->TimeString(c_requires_crap).c_str());
			}

			ServerInstance->XLines->apply_lines(to_apply);
		}
		else
		{
			user->WriteServ("NOTICE %s :*** G-Line for %s already exists",user->nick,target.c_str());
		}

	}
	else
	{
		if (ServerInstance->XLines->del_gline(target.c_str()))
		{
			FOREACH_MOD(I_OnDelGLine,OnDelGLine(user, target.c_str()));
			ServerInstance->SNO->WriteToSnoMask('x',"%s Removed G-line on %s.",user->nick,target.c_str());
		}
		else
		{
			user->WriteServ("NOTICE %s :*** G-line %s not found in list, try /stats g.",user->nick,target.c_str());
		}
	}

	return CMD_SUCCESS;
}

