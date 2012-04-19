/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2012 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"
#include "commands/cmd_trace.h"

extern "C" DllExport Command* init_command(InspIRCd* Instance)
{
	return new CommandTrace(Instance);
}

/** XXX: This is crap. someone fix this when you have time, to be more useful.
 */
CmdResult CommandTrace::Handle (const std::vector<std::string>&, User *user)
{
	/*for (user_hash::iterator i = ServerInstance->clientlist->begin(); i != ServerInstance->clientlist->end(); i++)
	{
		if (i->second->registered == REG_ALL)
		{
			if (IS_OPER(i->second))
			{
				user->WriteNumeric(205, "%s :Oper 0 %s",user->nick,i->second->nick);
			}
			else
			{
				user->WriteNumeric(204, "%s :User 0 %s",user->nick,i->second->nick);
			}
		}
		else
		{
			user->WriteNumeric(203, "%s :???? 0 [%s]",user->nick,i->second->host);
		}
	}*/
	return CMD_SUCCESS;
}
