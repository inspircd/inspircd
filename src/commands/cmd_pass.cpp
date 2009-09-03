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

#ifndef __CMD_PASS_H__
#define __CMD_PASS_H__

// include the common header files

#include <string>
#include <vector>
#include "inspircd.h"
#include "users.h"
#include "channels.h"

/** Handle /PASS. These command handlers can be reloaded by the core,
 * and handle basic RFC1459 commands. Commands within modules work
 * the same way, however, they can be fully unloaded, where these
 * may not.
 */
class CommandPass : public Command
{
 public:
	/** Constructor for pass.
	 */
	CommandPass (InspIRCd* Instance, Module* parent) : Command(Instance,parent,"PASS",0,1,true,0) { syntax = "<password>"; }
	/** Handle command.
	 * @param parameters The parameters to the comamnd
	 * @param pcnt The number of parameters passed to teh command
	 * @param user The user issuing the command
	 * @return A value from CmdResult to indicate command success or failure.
	 */
	CmdResult Handle(const std::vector<std::string>& parameters, User *user);
};

#endif


CmdResult CommandPass::Handle (const std::vector<std::string>& parameters, User *user)
{
	// Check to make sure they havnt registered -- Fix by FCS
	if (user->registered == REG_ALL)
	{
		user->WriteNumeric(ERR_ALREADYREGISTERED, "%s :You may not reregister",user->nick.c_str());
		return CMD_FAILURE;
	}
	ConnectClass* a = user->GetClass();
	if (!a)
		return CMD_FAILURE;

	user->password.assign(parameters[0], 0, 63);
	if (!ServerInstance->PassCompare(user, a->pass.c_str(), parameters[0].c_str(), a->hash.c_str()))
		user->haspassed = true;

	return CMD_SUCCESS;
}

COMMAND_INIT(CommandPass)
