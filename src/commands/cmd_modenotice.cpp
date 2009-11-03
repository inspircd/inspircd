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

class CommandModeNotice : public Command
{
 public:
	CommandModeNotice(Module* parent) : Command(parent,"MODENOTICE",2,2) { syntax = "<modes> <message>"; }

	CmdResult Handle(const std::vector<std::string>& parameters, User *src)
	{
		int mlen = parameters[0].length();
		for (std::vector<LocalUser*>::const_iterator i = ServerInstance->Users->local_users.begin(); i != ServerInstance->Users->local_users.end(); i++)
		{
			User* user = *i;
			for (int n = 0; n < mlen; n++)
			{
				if (!user->IsModeSet(parameters[0][n]))
					goto next_user;
			}
			user->Write(":%s NOTICE %s :*** From %s: %s", ServerInstance->Config->ServerName.c_str(),
				user->nick.c_str(), src->nick.c_str(), parameters[1].c_str());
next_user:	;
		}
		return CMD_SUCCESS;
	}

	RouteDescriptor GetRouting(User* user, const std::vector<std::string>& parameters)
	{
		return ROUTE_BROADCAST;
	}
};

COMMAND_INIT(CommandModeNotice)
