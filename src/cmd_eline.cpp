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
#include "commands/cmd_eline.h"

extern "C" DllExport command_t* init_command(InspIRCd* Instance)
{
	return new cmd_eline(Instance);
}

/** Handle /ELINE
 */
CmdResult cmd_eline::Handle (const char** parameters, int pcnt, userrec *user)
{
	if (pcnt >= 3)
	{
		IdentHostPair ih = ServerInstance->XLines->IdentSplit(parameters[0]);
		if (ServerInstance->HostMatchesEveryone(ih.first+"@"+ih.second,user))
			return CMD_FAILURE;

		if (!strchr(parameters[0],'@'))
		{
			user->WriteServ("NOTICE %s :*** E-Line must contain a username, e.g. *@%s",user->nick,parameters[0]);
			return CMD_FAILURE;
		}

		long duration = ServerInstance->Duration(parameters[1]);
		if (ServerInstance->XLines->add_eline(duration,user->nick,parameters[2],parameters[0]))
		{
			FOREACH_MOD(I_OnAddELine,OnAddELine(duration, user, parameters[2], parameters[0]));

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
			user->WriteServ("NOTICE %s :*** E-Line for %s already exists",user->nick,parameters[0]);
		}
	}
	else
	{
		if (ServerInstance->XLines->del_eline(parameters[0]))
		{
			FOREACH_MOD(I_OnDelELine,OnDelELine(user, parameters[0]));
			ServerInstance->SNO->WriteToSnoMask('x',"%s Removed E-line on %s.",user->nick,parameters[0]);
		}
		else
		{
			user->WriteServ("NOTICE %s :*** E-Line %s not found in list, try /stats e.",user->nick,parameters[0]);
		}
	}

	return CMD_SUCCESS;
}
