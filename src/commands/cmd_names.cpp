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

#ifndef CMD_NAMES_H
#define CMD_NAMES_H

// include the common header files

#include "users.h"
#include "channels.h"

/** Handle /NAMES. These command handlers can be reloaded by the core,
 * and handle basic RFC1459 commands. Commands within modules work
 * the same way, however, they can be fully unloaded, where these
 * may not.
 */
class CommandNames : public Command
{
 public:
	/** Constructor for names.
	 */
	CommandNames ( Module* parent) : Command(parent,"NAMES",0,0) { syntax = "{<channel>{,<channel>}}"; }
	/** Handle command.
	 * @param parameters The parameters to the comamnd
	 * @param pcnt The number of parameters passed to teh command
	 * @param user The user issuing the command
	 * @return A value from CmdResult to indicate command success or failure.
	 */
	CmdResult Handle(const std::vector<std::string>& parameters, User *user);
};

#endif


/** Handle /NAMES
 */
CmdResult CommandNames::Handle (const std::vector<std::string>& parameters, User *user)
{
	Channel* c;

	if (!parameters.size())
	{
		user->WriteNumeric(366, "%s * :End of /NAMES list.",user->nick.c_str());
		return CMD_SUCCESS;
	}

	if (ServerInstance->Parser->LoopCall(user, this, parameters, 0))
		return CMD_SUCCESS;

	c = ServerInstance->FindChan(parameters[0]);
	if (c)
	{
		if ((c->IsModeSet('s')) && (!c->HasUser(user)))
		{
		      user->WriteNumeric(401, "%s %s :No such nick/channel",user->nick.c_str(), c->name.c_str());
		      return CMD_FAILURE;
		}
		c->UserList(user);
	}
	else
	{
		user->WriteNumeric(401, "%s %s :No such nick/channel",user->nick.c_str(), parameters[0].c_str());
	}

	return CMD_SUCCESS;
}

COMMAND_INIT(CommandNames)
