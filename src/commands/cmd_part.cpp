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

/** Handle /PART. These command handlers can be reloaded by the core,
 * and handle basic RFC1459 commands. Commands within modules work
 * the same way, however, they can be fully unloaded, where these
 * may not.
 */
class CommandPart : public Command
{
 public:
	/** Constructor for part.
	 */
	CommandPart (Module* parent) : Command(parent,"PART", 1, 2) { Penalty = 5; syntax = "<channel>{,<channel>} [<reason>]"; }
	/** Handle command.
	 * @param parameters The parameters to the comamnd
	 * @param pcnt The number of parameters passed to teh command
	 * @param user The user issuing the command
	 * @return A value from CmdResult to indicate command success or failure.
	 */
	CmdResult Handle(const std::vector<std::string>& parameters, User *user);
};

CmdResult CommandPart::Handle (const std::vector<std::string>& parameters, User *user)
{
	std::string reason;

	if (IS_LOCAL(user))
	{
		if (!ServerInstance->Config->FixedPart.empty())
			reason = ServerInstance->Config->FixedPart;
		else
		{
			if (parameters.size() > 1)
				reason = ServerInstance->Config->PrefixPart + parameters[1] + ServerInstance->Config->SuffixPart;
			else
				reason = "";
		}
	}
	else
	{
		reason = parameters.size() > 1 ? parameters[1] : "";
	}

	if (ServerInstance->Parser->LoopCall(user, this, parameters, 0))
		return CMD_SUCCESS;

	Channel* c = ServerInstance->FindChan(parameters[0]);

	if (c)
	{
		c->PartUser(user, reason);
	}
	else
	{
		user->WriteServ( "401 %s %s :No such channel", user->nick.c_str(), parameters[0].c_str());
		return CMD_FAILURE;
	}

	return CMD_SUCCESS;
}

COMMAND_INIT(CommandPart)
