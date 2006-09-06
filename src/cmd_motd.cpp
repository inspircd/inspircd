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
#include "commands/cmd_motd.h"



extern "C" command_t* init_command(InspIRCd* Instance)
{
	return new cmd_motd(Instance);
}

CmdResult cmd_motd::Handle (const char** parameters, int pcnt, userrec *user)
{
	user->ShowMOTD();
	return CMD_SUCCESS;
}
