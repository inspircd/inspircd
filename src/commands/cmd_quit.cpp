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

#ifndef __CMD_QUIT_H__
#define __CMD_QUIT_H__

// include the common header files

#include "users.h"
#include "channels.h"

/** Handle /QUIT. These command handlers can be reloaded by the core,
 * and handle basic RFC1459 commands. Commands within modules work
 * the same way, however, they can be fully unloaded, where these
 * may not.
 */
class CommandQuit : public Command
{
 public:
	/** Constructor for quit.
	 */
	CommandQuit (InspIRCd* Instance, Module* parent) : Command(Instance,parent,"QUIT",0,0,true) { syntax = "[<message>]"; }
	/** Handle command.
	 * @param parameters The parameters to the comamnd
	 * @param pcnt The number of parameters passed to teh command
	 * @param user The user issuing the command
	 * @return A value from CmdResult to indicate command success or failure.
	 */
	CmdResult Handle(const std::vector<std::string>& parameters, User *user);
};

#endif




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

	std::string* operquit;
	if (user->GetExt("operquit", operquit))
	{
		ServerInstance->Users->QuitUser(user, quitmsg, operquit->c_str());
		delete operquit;
	}
	else
	{
		ServerInstance->Users->QuitUser(user, quitmsg);
	}

	return CMD_SUCCESS;
}


COMMAND_INIT(CommandQuit)
