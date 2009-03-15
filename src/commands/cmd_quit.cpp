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
#include "commands/cmd_quit.h"



extern "C" DllExport Command* init_command(InspIRCd* Instance)
{
	return new CommandQuit(Instance);
}

CmdResult CommandQuit::Handle (const std::vector<std::string>& parameters, User *user)
{

	std::string quitmsg;

	if (IS_LOCAL(user))
	{
		if (*ServerInstance->Config->FixedQuit)
			quitmsg = ServerInstance->Config->FixedQuit;
		else
			quitmsg = parameters.size() ?
				ServerInstance->Config->PrefixQuit + std::string(parameters[0]) + ServerInstance->Config->SuffixQuit
				: "Client exited";
	}
	else
		quitmsg = parameters.size() ? parameters[0] : "Client exited";

	ServerInstance->Users->QuitUser(user, quitmsg);

	return CMD_SUCCESS;
}

