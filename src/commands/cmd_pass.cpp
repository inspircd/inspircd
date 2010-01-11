/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2010 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"

/** Handle /PASS. These command handlers can be reloaded by the core,
 * and handle basic RFC1459 commands. Commands within modules work
 * the same way, however, they can be fully unloaded, where these
 * may not.
 */
class CommandPass : public SplitCommand
{
 public:
	/** Constructor for pass.
	 */
	CommandPass (Module* parent) : SplitCommand(parent,"PASS",1,1) { works_before_reg = true; Penalty = 0; syntax = "<password>"; }
	/** Handle command.
	 * @param parameters The parameters to the comamnd
	 * @param pcnt The number of parameters passed to teh command
	 * @param user The user issuing the command
	 * @return A value from CmdResult to indicate command success or failure.
	 */
	CmdResult HandleLocal(const std::vector<std::string>& parameters, LocalUser *user);
};


CmdResult CommandPass::HandleLocal(const std::vector<std::string>& parameters, LocalUser *user)
{
	// Check to make sure they haven't registered -- Fix by FCS
	if (user->registered == REG_ALL)
	{
		user->WriteNumeric(ERR_ALREADYREGISTERED, "%s :You may not reregister",user->nick.c_str());
		return CMD_FAILURE;
	}
	user->password = parameters[0];

	return CMD_SUCCESS;
}

COMMAND_INIT(CommandPass)
