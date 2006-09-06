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
#include "commands/cmd_users.h"



extern "C" command_t* init_command(InspIRCd* Instance)
{
	return new cmd_users(Instance);
}

CmdResult cmd_users::Handle (const char** parameters, int pcnt, userrec *user)
{
	user->WriteServ("445 %s :USERS has been disabled (depreciated command)",user->nick);
	return CMD_FAILURE;
}
