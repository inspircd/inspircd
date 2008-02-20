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
#include "commands/cmd_kline.h"

extern "C" DllExport Command* init_command(InspIRCd* Instance)
{
	return new CommandKline(Instance);
}

/** Handle /KLINE
 */
CmdResult CommandKline::Handle (const char* const* parameters, int pcnt, User *user)
{
    std::string target = parameters[0];

	if (pcnt >= 3)
	{
		IdentHostPair ih;
		User* find = ServerInstance->FindNick(target.c_str());
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

		if (strchr(target.c_str(),'!'))
		{
			user->WriteServ("NOTICE %s :*** K-Line cannot operate on nick!user@host masks",user->nick);
			return CMD_FAILURE;
		}

		long duration = ServerInstance->Duration(parameters[1]);
		KLine* kl = new KLine(ServerInstance, ServerInstance->Time(), duration, user->nick, parameters[2], ih.first.c_str(), ih.second.c_str());
		if (ServerInstance->XLines->AddLine(kl,user))
		{
			if (!duration)
			{
				ServerInstance->SNO->WriteToSnoMask('x',"%s added permanent K-line for %s.",user->nick,target.c_str());
			}
			else
			{
				time_t c_requires_crap = duration + ServerInstance->Time();
				ServerInstance->SNO->WriteToSnoMask('x',"%s added timed K-line for %s, expires on %s",user->nick,target.c_str(),
						ServerInstance->TimeString(c_requires_crap).c_str());
			}

			ServerInstance->XLines->ApplyLines();
		}
		else
		{
			delete kl;
			user->WriteServ("NOTICE %s :*** K-Line for %s already exists",user->nick,target.c_str());
		}
	}
	else
	{
		if (ServerInstance->XLines->DelLine(target.c_str(),"K",user))
		{
			ServerInstance->SNO->WriteToSnoMask('x',"%s Removed K-line on %s.",user->nick,target.c_str());
		}
		else
		{
			user->WriteServ("NOTICE %s :*** K-Line %s not found in list, try /stats k.",user->nick,target.c_str());
		}
	}

	return CMD_SUCCESS;
}

