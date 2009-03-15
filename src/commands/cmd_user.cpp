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
#include "commands/cmd_user.h"

extern "C" DllExport Command* init_command(InspIRCd* Instance)
{
	return new CommandUser(Instance);
}

CmdResult CommandUser::Handle (const std::vector<std::string>& parameters, User *user)
{
	/* A user may only send the USER command once */
	if (!(user->registered & REG_USER))
	{
		if (!ServerInstance->IsIdent(parameters[0].c_str()))
		{
			/*
			 * RFC says we must use this numeric, so we do. Let's make it a little more nub friendly though. :)
			 *  -- Craig, and then w00t.
			 */
			user->WriteNumeric(461, "%s USER :Your username is not valid",user->nick.c_str());
			return CMD_FAILURE;
		}
		else
		{
			/*
			 * The ident field is IDENTMAX+2 in size to account for +1 for the optional
			 * ~ character, and +1 for null termination, therefore we can safely use up to
			 * IDENTMAX here.
			 */
			user->ident.assign(parameters[0], 0, ServerInstance->Config->Limits.IdentMax);
			user->fullname.assign(parameters[3].empty() ? std::string("No info") : parameters[3], 0, ServerInstance->Config->Limits.MaxGecos);
			user->registered = (user->registered | REG_USER);
		}
	}
	else
	{
		user->WriteNumeric(462, "%s :You may not reregister", user->nick.c_str());
		return CMD_FAILURE;
	}

	/* parameters 2 and 3 are local and remote hosts, and are ignored */
	if (user->registered == REG_NICKUSER)
	{
		int MOD_RESULT = 0;

		/* user is registered now, bit 0 = USER command, bit 1 = sent a NICK command */
		FOREACH_RESULT(I_OnUserRegister,OnUserRegister(user));
		if (MOD_RESULT > 0)
			return CMD_FAILURE;

	}

	return CMD_SUCCESS;
}
