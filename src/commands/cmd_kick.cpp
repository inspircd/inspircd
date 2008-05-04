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
#include "commands/cmd_kick.h"

extern "C" DllExport Command* init_command(InspIRCd* Instance)
{
	return new CommandKick(Instance);
}

/** Handle /KICK
 */
CmdResult CommandKick::Handle (const std::vector<std::string>& parameters, User *user)
{
	char reason[MAXKICK];
	Channel* c = ServerInstance->FindChan(parameters[0]);
	User* u = ServerInstance->FindNick(parameters[1]);

	if (ServerInstance->Parser->LoopCall(user, this, parameters, parameters.size(), 1))
		return CMD_SUCCESS;

	if (!u || !c)
	{
		user->WriteServ( "401 %s %s :No such nick/channel", user->nick, u ? parameters[0].c_str() : parameters[1].c_str());
		return CMD_FAILURE;
	}

	if ((IS_LOCAL(user)) && (!c->HasUser(user)) && (!ServerInstance->ULine(user->server)))
	{
		user->WriteServ( "442 %s %s :You're not on that channel!", user->nick, parameters[0].c_str());
		return CMD_FAILURE;
	}

	if (parameters.size() > 2)
	{
		strlcpy(reason, parameters[2].c_str(), MAXKICK - 1);
	}
	else
	{
		strlcpy(reason, user->nick, MAXKICK - 1);
	}

	if (!c->KickUser(user, u, reason))
		/* Nobody left here, delete the Channel */
		delete c;

	return CMD_SUCCESS;
}
