/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd is copyright (C) 2002-2006 ChatSpike-Dev.
 *                       E-mail:
 *                <brain@chatspike.net>
 *                <Craig@chatspike.net>
 *
 * Written by Craig Edwards, Craig McLure, and others.
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "configreader.h"
#include "users.h"
#include "commands/cmd_user.h"



extern "C" command_t* init_command(InspIRCd* Instance)
{
	return new cmd_user(Instance);
}

CmdResult cmd_user::Handle (const char** parameters, int pcnt, userrec *user)
{
	if (user->registered < REG_NICKUSER)
	{
		if (!ServerInstance->IsIdent(parameters[0])) {
			// This kinda Sucks, According to the RFC thou, its either this,
			// or "You have already registered" :p -- Craig
			user->WriteServ("461 %s USER :Not enough parameters",user->nick);
			return CMD_FAILURE;
		}
		else {
			/* We're not checking ident, but I'm not sure I like the idea of '~' prefixing.. */
			/* XXX - The ident field is IDENTMAX+2 in size to account for +1 for the optional
			 * ~ character, and +1 for null termination, therefore we can safely use up to
			 * IDENTMAX here.
			 */
			strlcpy(user->ident, parameters[0], IDENTMAX);
			strlcpy(user->fullname,parameters[3],MAXGECOS);
			user->registered = (user->registered | REG_USER);
		}
	}
	else
	{
		user->WriteServ("462 %s :You may not reregister",user->nick);
		return CMD_FAILURE;
	}
	/* parameters 2 and 3 are local and remote hosts, ignored when sent by client connection */
	if (user->registered == REG_NICKUSER)
	{
		/* user is registered now, bit 0 = USER command, bit 1 = sent a NICK command */
		FOREACH_MOD(I_OnUserRegister,OnUserRegister(user));
	}

	return CMD_SUCCESS;
}
