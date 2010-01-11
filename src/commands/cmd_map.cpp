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

class CommandMap : public Command
{
 public:
	/** Constructor for map.
	 */
	CommandMap ( Module* parent) : Command(parent,"MAP",0,0) { Penalty=2; }
	/** Handle command.
	 * @param parameters The parameters to the comamnd
	 * @param pcnt The number of parameters passed to teh command
	 * @param user The user issuing the command
	 * @return A value from CmdResult to indicate command success or failure.
	 */
	CmdResult Handle(const std::vector<std::string>& parameters, User *user);
};

/** Handle /MAP
 */
CmdResult CommandMap::Handle (const std::vector<std::string>&, User *user)
{
	// as with /LUSERS this does nothing without a linking
	// module to override its behaviour and display something
	// better.

	if (IS_OPER(user))
	{
		user->WriteNumeric(006, "%s :%s [%s]", user->nick.c_str(), ServerInstance->Config->ServerName.c_str(), ServerInstance->Config->GetSID().c_str());
		user->WriteNumeric(007, "%s :End of /MAP", user->nick.c_str());
		return CMD_SUCCESS;
	}
	user->WriteNumeric(006, "%s :%s",user->nick.c_str(),ServerInstance->Config->ServerName.c_str());
	user->WriteNumeric(007, "%s :End of /MAP",user->nick.c_str());

	return CMD_SUCCESS;
}

COMMAND_INIT(CommandMap)
