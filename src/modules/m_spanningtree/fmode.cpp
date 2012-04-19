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
#include "xline.h"

#include "m_spanningtree/treesocket.h"
#include "m_spanningtree/treeserver.h"
#include "m_spanningtree/utils.h"

/* $ModDep: m_spanningtree/utils.h m_spanningtree/treeserver.h m_spanningtree/treesocket.h */


/** FMODE command - server mode with timestamp checks */
bool TreeSocket::ForceMode(const std::string &source, std::deque<std::string> &params)
{
	/* Chances are this is a 1.0 FMODE without TS */
	if (params.size() < 3)
	{
		/* No modes were in the command, probably a channel with no modes set on it */
		return true;
	}

	std::string sourceserv;

	/* Are we dealing with an FMODE from a user, or from a server? */
	User* who = this->ServerInstance->FindNick(source);
	if (who)
	{
		/* FMODE from a user, set sourceserv to the users server name */
		sourceserv = who->server;
	}
	else
	{
		/* FMODE from a server, use a fake user to receive mode feedback */
		who = Utils->ServerUser;
		sourceserv = source;    /* Set sourceserv to the actual source string */
	}
	std::vector<std::string> modelist;
	time_t TS = 0;
	for (unsigned int q = 0; (q < params.size()) && (q < 64); q++)
	{
		if (q == 1)
		{
			/* The timestamp is in this position.
			 * We don't want to pass that up to the
			 * server->client protocol!
			 */
			TS = atoi(params[q].c_str());
		}
		else
		{
			/* Everything else is fine to append to the modelist */
			modelist.push_back(params[q]);
		}

	}
	/* Extract the TS value of the object, either User or Channel */
	User* dst = this->ServerInstance->FindNick(params[0]);
	Channel* chan = NULL;
	time_t ourTS = 0;

	if (dst)
	{
		ourTS = dst->age;
	}
	else
	{
		chan = this->ServerInstance->FindChan(params[0]);
		if (chan)
		{
			ourTS = chan->age;
		}
		else
			/* Oops, channel doesnt exist! */
			return true;
	}

	if (!TS)
	{
		ServerInstance->Logs->Log("m_spanningtree",DEFAULT,"*** BUG? *** TS of 0 sent to FMODE. Are some services authors smoking craq, or is it 1970 again?. Dropped.");
		ServerInstance->SNO->WriteToSnoMask('d', "WARNING: The server %s is sending FMODE with a TS of zero. Total craq. Mode was dropped.", sourceserv.c_str());
		return true;
	}

	/* TS is equal or less: Merge the mode changes into ours and pass on.
	 */
	if (TS <= ourTS)
	{
		ServerInstance->Modes->Process(modelist, who, (who == Utils->ServerUser));

		/* HOT POTATO! PASS IT ON! */
		Utils->DoOneToAllButSender(source,"FMODE",params,sourceserv);
	}
	/* If the TS is greater than ours, we drop the mode and dont pass it anywhere.
	 */
	return true;
}


