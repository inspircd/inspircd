/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2010 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"

#ifndef __CMD_JOIN_H__
#define __CMD_JOIN_H__

// include the common header files

#include "users.h"
#include "channels.h"

/** Handle /JOIN. These command handlers can be reloaded by the core,
 * and handle basic RFC1459 commands. Commands within modules work
 * the same way, however, they can be fully unloaded, where these
 * may not.
 */
class CommandJoin : public Command
{
 public:
	/** Constructor for join.
	 */
	CommandJoin ( Module* parent) : Command(parent,"JOIN", 1, 2) { syntax = "<channel>{,<channel>} {<key>{,<key>}}"; Penalty = 2; }
	/** Handle command.
	 * @param parameters The parameters to the comamnd
	 * @param pcnt The number of parameters passed to teh command
	 * @param user The user issuing the command
	 * @return A value from CmdResult to indicate command success or failure.
	 */
	CmdResult Handle(const std::vector<std::string>& parameters, User *user);
};

#endif


/** Handle /JOIN
 */
CmdResult CommandJoin::Handle (const std::vector<std::string>& parameters, User *user)
{
	if (parameters.size() > 1)
	{
		if (ServerInstance->Parser->LoopCall(user, this, parameters, 0, 1, false))
			return CMD_SUCCESS;

		if (ServerInstance->IsChannel(parameters[0].c_str(), ServerInstance->Config->Limits.ChanMax))
		{
			Channel::JoinUser(user, parameters[0].c_str(), false, parameters[1].c_str(), false);
			return CMD_SUCCESS;
		}
	}
	else
	{
		if (ServerInstance->Parser->LoopCall(user, this, parameters, 0, -1, false))
			return CMD_SUCCESS;

		if (ServerInstance->IsChannel(parameters[0].c_str(), ServerInstance->Config->Limits.ChanMax))
		{
			Channel::JoinUser(user, parameters[0].c_str(), false, "", false);
			return CMD_SUCCESS;
		}
	}

	user->WriteNumeric(ERR_NOSUCHCHANNEL, "%s %s :Invalid channel name",user->nick.c_str(), parameters[0].c_str());
	return CMD_FAILURE;
}

COMMAND_INIT(CommandJoin)
