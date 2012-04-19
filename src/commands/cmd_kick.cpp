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
#include "commands/cmd_kick.h"

extern "C" DllExport Command* init_command(InspIRCd* Instance)
{
	return new CommandKick(Instance);
}

/** Handle /KICK
 */
CmdResult CommandKick::Handle (const std::vector<std::string>& parameters, User *user)
{
	std::string reason;
	Channel* c = ServerInstance->FindChan(parameters[0]);
	User* u = ServerInstance->FindNick(parameters[1]);

	if (ServerInstance->Parser->LoopCall(user, this, parameters, 1))
		return CMD_SUCCESS;

	if (!u || !c)
	{
		user->WriteServ( "401 %s %s :No such nick/channel", user->nick.c_str(), u ? parameters[0].c_str() : parameters[1].c_str());
		return CMD_FAILURE;
	}

	if ((IS_LOCAL(user)) && (!c->HasUser(user)) && (!ServerInstance->ULine(user->server)))
	{
		user->WriteServ( "442 %s %s :You're not on that channel!", user->nick.c_str(), parameters[0].c_str());
		return CMD_FAILURE;
	}

	if (parameters.size() > 2)
	{
		reason.assign(parameters[2], 0, ServerInstance->Config->Limits.MaxKick);
	}
	else
	{
		reason.assign(user->nick, 0, ServerInstance->Config->Limits.MaxKick);
	}

	if (!c->KickUser(user, u, reason.c_str()))
		/* Nobody left here, delete the Channel */
		delete c;

	return CMD_SUCCESS;
}
