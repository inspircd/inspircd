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

#ifndef __CMD_KICK_H__
#define __CMD_KICK_H__

// include the common header files

#include "users.h"
#include "channels.h"

/** Handle /KICK. These command handlers can be reloaded by the core,
 * and handle basic RFC1459 commands. Commands within modules work
 * the same way, however, they can be fully unloaded, where these
 * may not.
 */
class CommandKick : public Command
{
 public:
	/** Constructor for kick.
	 */
	CommandKick ( Module* parent) : Command(parent,"KICK",0,2) { syntax = "<channel> <nick>{,<nick>} [<reason>]"; }
	/** Handle command.
	 * @param parameters The parameters to the comamnd
	 * @param pcnt The number of parameters passed to teh command
	 * @param user The user issuing the command
	 * @return A value from CmdResult to indicate command success or failure.
	 */
	CmdResult Handle(const std::vector<std::string>& parameters, User *user);
};

#endif


/** Handle /KICK
 */
CmdResult CommandKick::Handle (const std::vector<std::string>& parameters, User *user)
{
	std::string reason;
	Channel* c = ServerInstance->FindChan(parameters[0]);
	User* u = ServerInstance->FindNick(parameters[1]);

	if (ServerInstance->Parser->LoopCall(user, this, parameters, 1))
		return CMD_SUCCESS;

	if (!u || !c)
	{
		user->WriteServ( "401 %s %s :No such nick/channel", user->nick.c_str(), u ? parameters[0].c_str() : parameters[1].c_str());
		return CMD_FAILURE;
	}

	if ((IS_LOCAL(user)) && (!c->HasUser(user)) && (!ServerInstance->ULine(user->server)))
	{
		user->WriteServ( "442 %s %s :You're not on that channel!", user->nick.c_str(), parameters[0].c_str());
		return CMD_FAILURE;
	}

	if (parameters.size() > 2)
	{
		reason.assign(parameters[2], 0, ServerInstance->Config->Limits.MaxKick);
	}
	else
	{
		reason.assign(user->nick, 0, ServerInstance->Config->Limits.MaxKick);
	}

	if (!c->KickUser(user, u, reason.c_str()))
		/* Nobody left here, delete the Channel */
		delete c;

	return CMD_SUCCESS;
}

COMMAND_INIT(CommandKick)
