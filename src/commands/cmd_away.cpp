/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2009 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"

#ifndef __CMD_AWAY_H__
#define __CMD_AWAY_H__

// include the common header files

#include "users.h"
#include "channels.h"

/** Handle /AWAY. These command handlers can be reloaded by the core,
 * and handle basic RFC1459 commands. Commands within modules work
 * the same way, however, they can be fully unloaded, where these
 * may not.
 */
class CommandAway : public Command
{
 public:
	/** Constructor for away.
	 */
	CommandAway (InspIRCd* Instance, Module* parent) : Command(Instance,parent,"AWAY",0,0) { syntax = "[<message>]"; }
	/** Handle command.
	 * @param parameters The parameters to the comamnd
	 * @param pcnt The number of parameters passed to teh command
	 * @param user The user issuing the command
	 * @return A value from CmdResult to indicate command success or failure.
	 */
	CmdResult Handle(const std::vector<std::string>& parameters, User *user);
};

#endif


/** Handle /AWAY
 */
CmdResult CommandAway::Handle (const std::vector<std::string>& parameters, User *user)
{
	ModResult MOD_RESULT;

	if ((parameters.size()) && (!parameters[0].empty()))
	{
		FIRST_MOD_RESULT(ServerInstance, OnSetAway, MOD_RESULT, (user, parameters[0]));

		if (MOD_RESULT == MOD_RES_DENY && IS_LOCAL(user))
			return CMD_FAILURE;

		user->awaytime = ServerInstance->Time();
		user->awaymsg.assign(parameters[0], 0, ServerInstance->Config->Limits.MaxAway);

		user->WriteNumeric(RPL_NOWAWAY, "%s :You have been marked as being away",user->nick.c_str());
	}
	else
	{
		FIRST_MOD_RESULT(ServerInstance, OnSetAway, MOD_RESULT, (user, ""));

		if (MOD_RESULT == MOD_RES_DENY && IS_LOCAL(user))
			return CMD_FAILURE;

		user->awaymsg.clear();
		user->WriteNumeric(RPL_UNAWAY, "%s :You are no longer marked as being away",user->nick.c_str());
	}

	return CMD_SUCCESS;
}

COMMAND_INIT(CommandAway)
