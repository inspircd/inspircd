/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2009 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"
#include "commands/cmd_connect.h"

/*
 * This is handled by the server linking module, if necessary. Do not remove this stub.
 */

extern "C" DllExport Command* init_command(InspIRCd* Instance)
{
	return new CommandConnect(Instance);
}

/** Handle /CONNECT
 */
CmdResult CommandConnect::Handle (const std::vector<std::string>&, User *user)
{
	user->WriteServ( "NOTICE %s :Look into loading a linking module (like m_spanningtree) if you want this to do anything useful.", user->nick.c_str());
	return CMD_SUCCESS;
}
