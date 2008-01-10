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
#include "commands/cmd_part.h"

extern "C" DllExport Command* init_command(InspIRCd* Instance)
{
	return new CommandPart(Instance);
}

CmdResult CommandPart::Handle (const char** parameters, int pcnt, User *user)
{
	std::string reason;

	if (IS_LOCAL(user))
	{
		if (*ServerInstance->Config->FixedPart)
			reason = ServerInstance->Config->FixedPart;
		else
		{
			if (pcnt > 1)
				reason = ServerInstance->Config->PrefixPart + std::string(parameters[1]) + ServerInstance->Config->SuffixPart;
			else
				reason = "";
		}
	}
	else
	{
		reason = pcnt ? parameters[1] : "";
	}

	if (ServerInstance->Parser->LoopCall(user, this, parameters, pcnt, 0))
		return CMD_SUCCESS;

	Channel* c = ServerInstance->FindChan(parameters[0]);
	
	if (c)
	{
		const char *rreason = reason.empty() ? NULL : reason.c_str();
		if (!c->PartUser(user, rreason))
			/* Arse, who stole our channel! :/ */
			delete c;
	}
	else
	{
		user->WriteServ( "401 %s %s :No such channel", user->nick, parameters[0]);
		return CMD_FAILURE;
	}

	return CMD_SUCCESS;
}
