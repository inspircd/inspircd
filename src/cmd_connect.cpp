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
#include "users.h"
#include "commands/cmd_connect.h"

/*
 * This is handled by the server linking module, if necessary. Do not remove this stub.
 */

extern "C" DllExport command_t* init_command(InspIRCd* Instance)
{
	return new cmd_connect(Instance);
}

/** Handle /CONNECT
 */
CmdResult cmd_connect::Handle (const char** parameters, int pcnt, userrec *user)
{
	user->WriteServ( "NOTICE %s :Look into loading a linking module (like m_spanningtree) if you want this to do anything useful.", user->nick);
	return CMD_SUCCESS;
}
