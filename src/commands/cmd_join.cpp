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
#include "commands/cmd_join.h"

extern "C" DllExport Command* init_command(InspIRCd* Instance)
{
	return new CommandJoin(Instance);
}

/** Handle /JOIN
 */
CmdResult CommandJoin::Handle (const std::vector<std::string>& parameters, User *user)
{
	if (parameters.size() > 1)
	{
		if (ServerInstance->Parser->LoopCall(user, this, parameters, 0, 1))
			return CMD_SUCCESS;

		if (ServerInstance->IsChannel(parameters[0].c_str()))
		{
			Channel::JoinUser(ServerInstance, user, parameters[0].c_str(), false, parameters[1].c_str(), false);
			return CMD_SUCCESS;
		}
	}
	else
	{
		if (ServerInstance->Parser->LoopCall(user, this, parameters, 0))
			return CMD_SUCCESS;

		if (ServerInstance->IsChannel(parameters[0].c_str()))
		{
			Channel::JoinUser(ServerInstance, user, parameters[0].c_str(), false, "", false);
			return CMD_SUCCESS;
		}
	}

	user->WriteNumeric(403, "%s %s :Invalid channel name",user->nick.c_str(), parameters[0].c_str());
	return CMD_FAILURE;
}
