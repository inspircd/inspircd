/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2012 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"

/** Handle /WALLOPS. These command handlers can be reloaded by the core,
 * and handle basic RFC1459 commands. Commands within modules work
 * the same way, however, they can be fully unloaded, where these
 * may not.
 */
class CommandWallops : public Command
{
 public:
	/** Constructor for wallops.
	 */
	CommandWallops ( Module* parent) : Command(parent,"WALLOPS",1,1) { flags_needed = 'o'; syntax = "<any-text>"; }
	/** Handle command.
	 * @param parameters The parameters to the comamnd
	 * @param pcnt The number of parameters passed to teh command
	 * @param user The user issuing the command
	 * @return A value from CmdResult to indicate command success or failure.
	 */
	CmdResult Handle(const std::vector<std::string>& parameters, User *user);
};

CmdResult CommandWallops::Handle (const std::vector<std::string>& parameters, User *user)
{
	std::string wallop("WALLOPS :");
	wallop.append(parameters[0]);

	for (std::vector<LocalUser*>::const_iterator i = ServerInstance->Users->local_users.begin(); i != ServerInstance->Users->local_users.end(); i++)
	{
		User* t = *i;
		if (t->IsModeSet('w'))
			user->WriteTo(t,wallop);
	}

	FOREACH_MOD(I_OnWallops,OnWallops(user,parameters[0]));
	return CMD_SUCCESS;
}

COMMAND_INIT(CommandWallops)
