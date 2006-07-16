/*   +------------------------------------+
 *   | Inspire Internet Relay Chat Daemon |
 *   +------------------------------------+
 *
 *  InspIRCd is copyright (C) 2002-2006 ChatSpike-Dev.
 *   E-mail:
 *<brain@chatspike.net>
 *<Craig@chatspike.net>
 *
 * Written by Craig Edwards, Craig McLure, and others.
 * This program is free but copyrighted software; see
 *the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "configreader.h"
#include "users.h"
#include "message.h"
#include "modules.h"
#include "commands.h"
#include "helperfuncs.h"
#include "commands/cmd_who.h"

extern ServerConfig* Config;
extern user_hash clientlist;
extern chan_hash chanlist;
extern std::vector<userrec*> all_opers;

/* get the last 'visible' chan of a user */
static char *getlastchanname(userrec *u)
{
	for (std::vector<ucrec*>::const_iterator v = u->chans.begin(); v != u->chans.end(); v++)
	{
		ucrec* temp = (ucrec*)*v;

		if (temp->channel)
		{
			if (!temp->channel->IsModeSet('s'))
				return temp->channel->name;
		}
	}

	return "*";
}

void cmd_who::Handle (const char** parameters, int pcnt, userrec *user)
{
	/*
	 * XXX - RFC says:
	 *   The <name> passed to WHO is matched against users' host, server, real
	 *   name and nickname
	 * Currently, we support WHO #chan, WHO nick, WHO 0, WHO *, and the addition of a 'o' flag, as per RFC.
	 */

	bool opt_viewopersonly = false;
	chanrec *ch = NULL;
	std::vector<std::string> whoresults;
	std::string initial = "352 " + std::string(user->nick) + " ";

	if (pcnt == 2)
	{
		/* parse flags */
		const char *iter = parameters[1];

		while (*iter)
		{
			switch (*iter)
			{
				case 'o':
					opt_viewopersonly = true;
					break;
			}

			*iter++;
		}
	}


	/* who on a channel? */
	ch = FindChan(parameters[0]);

	if (ch)
	{
		/* who on a channel. */
		CUList *cu = ch->GetUsers();

		for (CUList::iterator i = cu->begin(); i != cu->end(); i++)
		{
			/* opers only, please */
			if (opt_viewopersonly && !*(i->second)->oper)
				continue;


			/* XXX - code duplication; this could be more efficient -- w00t */
			std::string wholine = initial;

			wholine = wholine + getlastchanname(i->second) + " " + i->second->ident + " " + i->second->dhost + " " + 
					i->second->server + " " + i->second->nick + " ";

			/* away? */
			if (*(i->second)->awaymsg)
			{
				wholine.append("G");
			}
			else
			{
				wholine.append("H");
			}

			/* oper? */
			if (*(i->second)->oper)
			{
				wholine.append("*");
			}

			wholine = wholine + cmode(i->second, ch) + " :0 " + i->second->fullname;
			whoresults.push_back(wholine);
		}
	}
	else
	{
		/* uhggle. who on .. something else. */
		userrec *u = Find(parameters[0]);

		if (u)
		{
			/* who on a single user */
			std::string wholine = initial;

			wholine = wholine + getlastchanname(u) + " " + u->ident + " " + u->dhost + " " + 
					u->server + " " + u->nick + " ";

			/* away? */
			if (*u->awaymsg)
			{
				wholine.append("G");
			}
			else
			{
				wholine.append("H");
			}

			/* oper? */
			if (*u->oper)
			{
				wholine.append("*");
			}

			wholine = wholine + cmode(u, ch) + " :0 " + u->fullname;
			whoresults.push_back(wholine);
		}

		if (*parameters[0] == '*' || *parameters[0] == '0')
		{
			if (!opt_viewopersonly && !*user->oper)
				return; /* No way, jose */

			if (opt_viewopersonly)
			{
				for (std::vector<userrec*>::iterator i = all_opers.begin(); i != all_opers.end(); i++)
				{
					userrec* oper = (userrec*)*i;

					std::string wholine = initial;

					wholine = wholine + getlastchanname(oper) + " " + oper->ident + " " + oper->dhost + " " + 
							oper->server + " " + oper->nick + " ";

					/* away? */
					if (*oper->awaymsg)
					{
						wholine.append("G");
					}
					else
					{
						wholine.append("H");
					}

					/* oper? */
					if (*oper->oper)
					{
						wholine.append("*");
					}

					wholine = wholine + cmode(oper, ch) + " :0 " + oper->fullname;
					whoresults.push_back(wholine);
				}
			}
			else
			{
				for (user_hash::iterator i = clientlist.begin(); i != clientlist.end(); i++)
				{
					std::string wholine = initial;

					wholine = wholine + getlastchanname(i->second) + " " + i->second->ident + " " + i->second->dhost + " " + 
							i->second->server + " " + i->second->nick + " ";

					/* away? */
					if (*(i->second)->awaymsg)
					{
						wholine.append("G");
					}
					else
					{
						wholine.append("H");
					}

					/* oper? */
					if (*(i->second)->oper)
					{
						wholine.append("*");
					}

					wholine = wholine + cmode(i->second, ch) + " :0 " + i->second->fullname;
					whoresults.push_back(wholine);
				}
			}
		}
	}
}
