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

#include "users.h"
#include "commands/cmd_squit.h"

/*
 * This is handled by the server linking module, if necessary. Do not remove this stub.
 */


extern "C" command_t* init_command(InspIRCd* Instance)
{
	return new cmd_squit(Instance);
}

CmdResult cmd_squit::Handle (const char** parameters, int pcnt, userrec *user)
{
	user->WriteServ( "NOTICE %s :You are a nub. Load a linking module.", user->nick);
	return CMD_FAILURE;
}
