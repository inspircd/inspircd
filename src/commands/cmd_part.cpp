/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2007-2008 Robin Burchell <robin+git@viroteck.net>
 *
 * This file is part of InspIRCd.  InspIRCd is free software: you can
 * redistribute it and/or modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation, version 2.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
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
