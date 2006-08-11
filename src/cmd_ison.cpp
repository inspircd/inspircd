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
#include "ctables.h"
#include "users.h"
#include "commands.h"
#include "helperfuncs.h"
#include "inspircd.h"
#include "commands/cmd_ison.h"



void cmd_ison::Handle (const char** parameters, int pcnt, userrec *user)
{
	char retbuf[MAXBUF];
	userrec *u;

	snprintf(retbuf, MAXBUF, "303 %s :", user->nick);

	for (int i = 0; i < pcnt; i++)
	{
		u = ServerInstance->FindNick(parameters[i]);

		if (u)
		{
			strlcat(retbuf, u->nick, MAXBUF);
			charlcat(retbuf, ' ', MAXBUF);
		}
	}

	user->WriteServ( retbuf);
}
