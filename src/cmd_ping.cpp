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
#include "commands/cmd_ping.h"



extern "C" command_t* init_command(InspIRCd* Instance)
{
	return new cmd_ping(Instance);
}

CmdResult cmd_ping::Handle (const char** parameters, int pcnt, userrec *user)
{
	user->WriteServ("PONG %s :%s",ServerInstance->Config->ServerName,parameters[0]);
	return CMD_SUCCESS;
}
