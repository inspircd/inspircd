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
#include "commands/cmd_quit.h"



extern "C" DllExport Command* init_command(InspIRCd* Instance)
{
	return new CommandQuit(Instance);
}

CmdResult CommandQuit::Handle (const char** parameters, int pcnt, User *user)
{

	std::string quitmsg;

	if (IS_LOCAL(user))
	{
		if (*ServerInstance->Config->FixedQuit)
			quitmsg = ServerInstance->Config->FixedQuit;
		else
			quitmsg = pcnt ?
				ServerInstance->Config->PrefixQuit + std::string(parameters[0]) + ServerInstance->Config->SuffixQuit
				: "Client exited";
	}
	else
		quitmsg = pcnt ? parameters[0] : "Client exited";

	User::QuitUser(ServerInstance, user, quitmsg);

	return CMD_SUCCESS;
}

