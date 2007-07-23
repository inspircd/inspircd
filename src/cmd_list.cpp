/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2007 InspIRCd Development Team
 * See: http://www.inspircd.org/wiki/index.php/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"
#include "users.h"
#include "inspircd.h"
#include "commands/cmd_list.h"
#include "wildcard.h"

/** Handle /LIST
 */
extern "C" DllExport command_t* init_command(InspIRCd* Instance)
{
	return new cmd_list(Instance);
}

CmdResult cmd_list::Handle (const char** parameters, int pcnt, userrec *user)
{
	int minusers = 0, maxusers = 0;

	user->WriteServ("321 %s Channel :Users Name",user->nick);

	/* Work around mIRC suckyness. YOU SUCK, KHALED! */
	if (pcnt == 1)
	{
		if (*parameters[0] == '<')
		{
			maxusers = atoi(parameters[0]+1);
			pcnt = 0;
		}
		else if (*parameters[0] == '>')
		{
			minusers = atoi(parameters[0]+1);
			pcnt = 0;
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

		if (pcnt)
		{
			if (!match(i->second->name, parameters[0]) && !match(i->second->topic, parameters[0]))
				continue;
		}

		// if the channel is not private/secret, OR the user is on the channel anyway
		bool n = i->second->HasUser(user);
		if ((i->second->IsModeSet('p')) && (!n))
		{
			if (users)
				user->WriteServ("322 %s *",user->nick,i->second->name);
		}
		else
		{
			if (((!(i->second->IsModeSet('p'))) && (!(i->second->IsModeSet('p')))) || (n))
			{
				long users = i->second->GetUserCounter();
				if (users)
					user->WriteServ("322 %s %s %d :[+%s] %s",user->nick,i->second->name,users,i->second->ChanModes(n),i->second->topic);
			}
		}
	}
	user->WriteServ("323 %s :End of channel list.",user->nick);

	return CMD_SUCCESS;
}

