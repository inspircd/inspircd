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
#include "configreader.h"
#include "users.h"
#include "commands/cmd_user.h"

extern "C" DllExport command_t* init_command(InspIRCd* Instance)
{
	return new cmd_user(Instance);
}

CmdResult cmd_user::Handle (const char** parameters, int pcnt, userrec *user)
{
	/* A user may only send the USER command once */
	if (!(user->registered & REG_USER))
	{
		if (!ServerInstance->IsIdent(parameters[0]))
		{
			/*
			 * RFC says we must use this numeric, so we do. Let's make it a little more nub friendly though. :)
			 *  -- Craig, and then w00t.
			 */
			user->WriteServ("461 %s USER :Your username is not valid",user->nick);
			return CMD_FAILURE;
		}
		else
		{
			/*
			 * The ident field is IDENTMAX+2 in size to account for +1 for the optional
			 * ~ character, and +1 for null termination, therefore we can safely use up to
			 * IDENTMAX here.
			 */
			strlcpy(user->ident, parameters[0], IDENTMAX);
			strlcpy(user->fullname, *parameters[3] ? parameters[3] : "No info", MAXGECOS);
			user->registered = (user->registered | REG_USER);
		}
	}
	else
	{
		user->WriteServ("462 %s :You may not reregister",user->nick);
		return CMD_FAILURE;
	}

	/* parameters 2 and 3 are local and remote hosts, and are ignored */
	if (user->registered == REG_NICKUSER)
	{
		int MOD_RESULT = 0;
		/* user is registered now, bit 0 = USER command, bit 1 = sent a NICK command */
		if (ServerInstance->next_call > ServerInstance->Time() + ServerInstance->Config->dns_timeout)
			ServerInstance->next_call = ServerInstance->Time() + ServerInstance->Config->dns_timeout;
		FOREACH_RESULT(I_OnUserRegister,OnUserRegister(user));
		if (MOD_RESULT > 0)
			return CMD_FAILURE;

	}

	return CMD_SUCCESS;
}
