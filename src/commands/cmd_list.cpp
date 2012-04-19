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
#include "commands/cmd_list.h"

/** Handle /LIST
 */
extern "C" DllExport Command* init_command(InspIRCd* Instance)
{
	return new CommandList(Instance);
}

CmdResult CommandList::Handle (const std::vector<std::string>& parameters, User *user)
{
	int minusers = 0, maxusers = 0;

	user->WriteNumeric(321, "%s Channel :Users Name",user->nick.c_str());

	/* Work around mIRC suckyness. YOU SUCK, KHALED! */
	if (parameters.size() == 1)
	{
		if (parameters[0][0] == '<')
		{
			maxusers = atoi((parameters[0].c_str())+1);
		}
		else if (parameters[0][0] == '>')
		{
			minusers = atoi((parameters[0].c_str())+1);
		}
	}

	for (chan_hash::const_iterator i = ServerInstance->chanlist->begin(); i != ServerInstance->chanlist->end(); i++)
	{
		// attempt to match a glob pattern
		long users = i->second->GetUserCounter();

		bool too_few = (minusers && (users <= minusers));
		bool too_many = (maxusers && (users >= maxusers));

		if (too_many || too_few)
			continue;

		if (parameters.size() && (parameters[0][0] != '<' && parameters[0][0] != '>'))
		{
			if (!InspIRCd::Match(i->second->name, parameters[0]) && !InspIRCd::Match(i->second->topic, parameters[0]))
				continue;
		}

		// if the channel is not private/secret, OR the user is on the channel anyway
		bool n = (i->second->HasUser(user) || user->HasPrivPermission("channels/auspex"));

		if (!n && i->second->IsModeSet('p'))
		{
			/* Channel is +p and user is outside/not privileged */
			user->WriteNumeric(322, "%s * %ld :",user->nick.c_str(), users);
		}
		else
		{
			if (n || !i->second->IsModeSet('s'))
			{
				/* User is in the channel/privileged, channel is not +s */
				user->WriteNumeric(322, "%s %s %ld :[+%s] %s",user->nick.c_str(),i->second->name.c_str(),users,i->second->ChanModes(n),i->second->topic.c_str());
			}
		}
	}
	user->WriteNumeric(323, "%s :End of channel list.",user->nick.c_str());

	return CMD_SUCCESS;
}

