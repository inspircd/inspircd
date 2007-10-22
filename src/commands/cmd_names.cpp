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
#include "commands/cmd_names.h"

extern "C" DllExport Command* init_command(InspIRCd* Instance)
{
	return new CommandNames(Instance);
}

/** Handle /NAMES
 */
CmdResult CommandNames::Handle (const char** parameters, int pcnt, User *user)
{
	Channel* c;

	if (!pcnt)
	{
		user->WriteServ("366 %s * :End of /NAMES list.",user->nick);
		return CMD_SUCCESS;
	}

	if (ServerInstance->Parser->LoopCall(user, this, parameters, pcnt, 0))
		return CMD_SUCCESS;

	c = ServerInstance->FindChan(parameters[0]);
	if (c)
	{
		if ((c->IsModeSet('s')) && (!c->HasUser(user)))
		{
		      user->WriteServ("401 %s %s :No such nick/channel",user->nick, c->name);
		      return CMD_FAILURE;
		}
		c->UserList(user);
	}
	else
	{
		user->WriteServ("401 %s %s :No such nick/channel",user->nick, parameters[0]);
	}

	return CMD_SUCCESS;
}
