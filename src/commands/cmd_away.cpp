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
#include "commands/cmd_away.h"

extern "C" DllExport Command* init_command(InspIRCd* Instance)
{
	return new CommandAway(Instance);
}

/** Handle /AWAY
 */
CmdResult CommandAway::Handle (const std::vector<std::string>& parameters, User *user)
{
	int MOD_RESULT = 0;

	if ((parameters.size()) && (!parameters[0].empty()))
	{
		FOREACH_RESULT(I_OnSetAway, OnSetAway(user, parameters[0]));

		if (MOD_RESULT != 0 && IS_LOCAL(user))
			return CMD_FAILURE;

		user->awaytime = ServerInstance->Time();
		user->awaymsg.assign(parameters[0], 0, ServerInstance->Config->Limits.MaxAway);

		user->WriteNumeric(RPL_NOWAWAY, "%s :You have been marked as being away",user->nick.c_str());
	}
	else
	{
		FOREACH_RESULT(I_OnSetAway, OnSetAway(user, ""));

		if (MOD_RESULT != 0 && IS_LOCAL(user))
			return CMD_FAILURE;

		user->awaymsg.clear();
		user->WriteNumeric(RPL_UNAWAY, "%s :You are no longer marked as being away",user->nick.c_str());
	}

	return CMD_SUCCESS;
}
