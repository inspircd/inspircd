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

#include "inspircd_config.h"
#include "configreader.h"
#include "users.h"
#include "message.h"
#include "commands.h"
#include "helperfuncs.h"
#include "commands/cmd_user.h"

extern ServerConfig* Config;
extern int MODCOUNT;
extern ModuleList modules;
extern FactoryList factory;

void cmd_user::Handle (const char** parameters, int pcnt, userrec *user)
{
	if (user->registered < REG_NICKUSER)
	{
		if (!isident(parameters[0])) {
			// This kinda Sucks, According to the RFC thou, its either this,
			// or "You have already registered" :p -- Craig
			WriteServ(user->fd,"461 %s USER :Not enough parameters",user->nick);
		}
		else {
			/* We're not checking ident, but I'm not sure I like the idea of '~' prefixing.. */
			/* XXX - Should this IDENTMAX + 1 be IDENTMAX - 1? Ok, users.h has it defined as
			 * char ident[IDENTMAX+2]; - WTF?
			 */
			strlcpy(user->ident, parameters[0], IDENTMAX);
			strlcpy(user->fullname,parameters[3],MAXGECOS);
			user->registered = (user->registered | REG_USER);
		}
	}
	else
	{
		WriteServ(user->fd,"462 %s :You may not reregister",user->nick);
		return;
	}
	/* parameters 2 and 3 are local and remote hosts, ignored when sent by client connection */
	if (user->registered == REG_NICKUSER)
	{
		/* user is registered now, bit 0 = USER command, bit 1 = sent a NICK command */
		FOREACH_MOD(I_OnUserRegister,OnUserRegister(user));
		//ConnectUser(user,NULL);
	}
}
