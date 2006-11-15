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
#include "modules.h"
#include "wildcard.h"
#include "commands/cmd_who.h"

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

bool whomatch(userrec* user, const char* matchtext, bool opt_realname, bool opt_showrealhost, bool opt_mode)
{
	bool realhost = false;
	bool realname = false;
	bool positive = true;

	if (user->registered != REG_ALL)
		return false;

	if (opt_mode)
	{
		for (const char* n = matchtext; *n; n++)
		{
			if (*n == '+')
			{
				positive = true;
				continue;
			}
			else if (*n == '-')
			{
				positive = false;
				continue;
			}
			if (user->IsModeSet(*n) != positive)
				return false;
		}
		return true;
	}

	if (opt_realname)
		realname = match(user->fullname, matchtext);

	if (opt_showrealhost)
		realhost = match(user->host, matchtext);

	return ((realname) || (realhost) || (match(user->dhost, matchtext)) || (match(user->nick, matchtext)) || (match(user->server, matchtext)));
}



extern "C" command_t* init_command(InspIRCd* Instance)
{
	return new cmd_who(Instance);
}

CmdResult cmd_who::Handle (const char** parameters, int pcnt, userrec *user)
{
	/*
	 * XXX - RFC says:
	 *   The <name> passed to WHO is matched against users' host, server, real
	 *   name and nickname
	 * Currently, we support WHO #chan, WHO nick, WHO 0, WHO *, and the addition of a 'o' flag, as per RFC.
	 */

	/* WHO options */
	bool opt_viewopersonly = false;
	bool opt_showrealhost = false;
	bool opt_unlimit = false;
	bool opt_realname = false;
	bool opt_mode = false;

	chanrec *ch = NULL;
	std::vector<std::string> whoresults;
	std::string initial = "352 " + std::string(user->nick) + " ";

	const char* matchtext = NULL;

	/* Change '0' into '*' so the wildcard matcher can grok it */
	matchtext = parameters[0];
	if (!strcmp(matchtext,"0"))
		matchtext = "*";

	if (pcnt > 1)
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
				case 'h':
					if (*user->oper)
						opt_showrealhost = true;
				break;
				case 'u':
					if (*user->oper)
						opt_unlimit = true;
				break;
				case 'r':
					opt_realname = true;
				break;
				case 'm':
					opt_mode = true;
				break;
			}

			*iter++;
		}
	}


	/* who on a channel? */
	ch = ServerInstance->FindChan(matchtext);

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

			wholine = wholine + ch->name + " " + i->second->ident + " " + (opt_showrealhost ? i->second->host : i->second->dhost) + " " + 
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

			wholine = wholine + ch->GetPrefixChar(i->second) + " :0 " + i->second->fullname;
			whoresults.push_back(wholine);
		}
	}
	else
	{
		/* Match against wildcard of nick, server or host */

		if (opt_viewopersonly)
		{
			/* Showing only opers */
			for (std::vector<userrec*>::iterator i = ServerInstance->all_opers.begin(); i != ServerInstance->all_opers.end(); i++)
			{
				userrec* oper = *i;

				if (whomatch(oper, matchtext, opt_realname, opt_showrealhost, opt_mode))
				{
					std::string wholine = initial;
	
					wholine = wholine + getlastchanname(oper) + " " + oper->ident + " " + (opt_showrealhost ? oper->host : oper->dhost) + " " + 
							oper->server + " " + oper->nick + " ";

					ch = ServerInstance->FindChan(getlastchanname(oper));

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
	
					wholine = wholine + (ch ? ch->GetPrefixChar(oper) : "") + " :0 " + oper->fullname;
					whoresults.push_back(wholine);
				}
			}
		}
		else
		{
			for (user_hash::iterator i = ServerInstance->clientlist.begin(); i != ServerInstance->clientlist.end(); i++)
			{
				if (whomatch(i->second, matchtext, opt_realname, opt_showrealhost, opt_mode))
				{
					std::string wholine = initial;
	
					wholine = wholine + getlastchanname(i->second) + " " + i->second->ident + " " + (opt_showrealhost ? i->second->host : i->second->dhost) + " " + 
						i->second->server + " " + i->second->nick + " ";
	
					ch = ServerInstance->FindChan(getlastchanname(i->second));

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

					wholine = wholine + (ch ? ch->GetPrefixChar(i->second) : "") + " :0 " + i->second->fullname;
					whoresults.push_back(wholine);
				}
			}
		}
	}
	/* Send the results out */
	if ((whoresults.size() < (size_t)ServerInstance->Config->MaxWhoResults) && (!opt_unlimit))
	{
		for (std::vector<std::string>::const_iterator n = whoresults.begin(); n != whoresults.end(); n++)
			user->WriteServ(*n);
		user->WriteServ("315 %s %s :End of /WHO list.",user->nick, *parameters[0] ? parameters[0] : "*");
		return CMD_SUCCESS;
	}
	else
	{
		/* BZZT! Too many results. */
		user->WriteServ("315 %s %s :Too many results",user->nick, parameters[0]);
		return CMD_FAILURE;
	}
}
