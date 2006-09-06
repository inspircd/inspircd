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
#include "commands/cmd_mode.h"

extern "C" command_t* init_command(InspIRCd* Instance)
{
	return new cmd_mode(Instance);
}

CmdResult cmd_mode::Handle (const char** parameters, int pcnt, userrec *user)
{
	ServerInstance->Modes->Process(parameters, pcnt, user, false);
	return CMD_SUCCESS;
}

