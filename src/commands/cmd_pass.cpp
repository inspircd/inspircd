/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2008 InspIRCd Development Team
 * See: http://www.inspircd.org/wiki/index.php/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"
#include "commands/cmd_pass.h"

extern "C" DllExport Command* init_command(InspIRCd* Instance)
{
	return new CommandPass(Instance);
}

CmdResult CommandPass::Handle (const std::vector<std::string>& parameters, User *user)
{
	// Check to make sure they havnt registered -- Fix by FCS
	if (user->registered == REG_ALL)
	{
		user->WriteNumeric(462, "%s :You may not reregister",user->nick);
		return CMD_FAILURE;
	}
	ConnectClass* a = user->GetClass();
	if (!a)
		return CMD_FAILURE;

	strlcpy(user->password, parameters[0].c_str(), 63);
	if (!ServerInstance->PassCompare(user, a->GetPass().c_str(), parameters[0].c_str(), a->GetHash().c_str()))
		user->haspassed = true;

	return CMD_SUCCESS;
}
