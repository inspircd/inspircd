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

#include "users.h"
#include "commands/cmd_squit.h"

/*
 * This is handled by the server linking module, if necessary. Do not remove this stub.
 */


extern "C" command_t* init_command(InspIRCd* Instance)
{
	return new cmd_squit(Instance);
}

void cmd_squit::Handle (const char** parameters, int pcnt, userrec *user)
{
	user->WriteServ( "NOTICE %s :You are a nub. Load a linking module.", user->nick);
}
