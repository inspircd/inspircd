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
#include "commands/cmd_part.h"

extern "C" DllExport Command* init_command(InspIRCd* Instance)
{
	return new CommandPart(Instance);
}

CmdResult CommandPart::Handle (const std::vector<std::string>& parameters, User *user)
{
	std::string reason;

	if (IS_LOCAL(user))
	{
		if (*ServerInstance->Config->FixedPart)
			reason = ServerInstance->Config->FixedPart;
		else
		{
			if (parameters.size() > 1)
				reason = ServerInstance->Config->PrefixPart + std::string(parameters[1]) + ServerInstance->Config->SuffixPart;
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
		if (!c->PartUser(user, reason))
			/* Arse, who stole our channel! :/ */
			delete c;
	}
	else
	{
		user->WriteServ( "401 %s %s :No such channel", user->nick.c_str(), parameters[0].c_str());
		return CMD_FAILURE;
	}

	return CMD_SUCCESS;
}
