/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2009 InspIRCd Development Team
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

CmdResult CommandZline::Handle (const std::vector<std::string>& parameters, User *user)
{
	std::string target = parameters[0];

	if (parameters.size() >= 3)
	{
		if (target.find('!') != std::string::npos)
		{
			user->WriteServ("NOTICE %s :*** You cannot include a nickname in a zline, a zline must ban only an IP mask",user->nick.c_str());
			return CMD_FAILURE;
		}

		User *u = ServerInstance->FindNick(target.c_str());

		if (u)
		{
			target = u->GetIPString();
		}

		const char* ipaddr = target.c_str();

		if (strchr(ipaddr,'@'))
		{
			while (*ipaddr != '@')
				ipaddr++;
			ipaddr++;
		}

		if (ServerInstance->IPMatchesEveryone(ipaddr,user))
			return CMD_FAILURE;

		long duration = ServerInstance->Duration(parameters[1].c_str());

		ZLine* zl = new ZLine(ServerInstance, ServerInstance->Time(), duration, user->nick.c_str(), parameters[2].c_str(), ipaddr);
		if (ServerInstance->XLines->AddLine(zl,user))
		{
			if (!duration)
			{
				ServerInstance->SNO->WriteToSnoMask('x',"%s added permanent Z-line for %s: %s", user->nick.c_str(), target.c_str(), parameters[2].c_str());
			}
			else
			{
				time_t c_requires_crap = duration + ServerInstance->Time();
				ServerInstance->SNO->WriteToSnoMask('x',"%s added timed Z-line for %s, expires on %s: %s",user->nick.c_str(),target.c_str(),
						ServerInstance->TimeString(c_requires_crap).c_str(), parameters[2].c_str());
			}
			ServerInstance->XLines->ApplyLines();
		}
		else
		{
			delete zl;
			user->WriteServ("NOTICE %s :*** Z-Line for %s already exists",user->nick.c_str(),target.c_str());
		}
	}
	else
	{
		if (ServerInstance->XLines->DelLine(target.c_str(),"Z",user))
		{
			ServerInstance->SNO->WriteToSnoMask('x',"%s Removed Z-line on %s.",user->nick.c_str(),target.c_str());
		}
		else
		{
			user->WriteServ("NOTICE %s :*** Z-Line %s not found in list, try /stats Z.",user->nick.c_str(),target.c_str());
			return CMD_FAILURE;
		}
	}

	return CMD_SUCCESS;
}
