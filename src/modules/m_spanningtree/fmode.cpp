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
#include "commands/cmd_whois.h"
#include "commands/cmd_stats.h"
#include "socket.h"
#include "wildcard.h"
#include "xline.h"
#include "transport.h"
#include "m_hash.h"
#include "socketengine.h"

#include "m_spanningtree/main.h"
#include "m_spanningtree/utils.h"
#include "m_spanningtree/treeserver.h"
#include "m_spanningtree/link.h"
#include "m_spanningtree/treesocket.h"
#include "m_spanningtree/resolvers.h"
#include "m_spanningtree/handshaketimer.h"

/* $ModDep: m_spanningtree/timesynctimer.h m_spanningtree/resolvers.h m_spanningtree/main.h m_spanningtree/utils.h m_spanningtree/treeserver.h m_spanningtree/link.h m_spanningtree/treesocket.h m_hash.h */

/** FMODE command - server mode with timestamp checks */
bool TreeSocket::ForceMode(const std::string &source, std::deque<std::string> &params)
{
	/* Chances are this is a 1.0 FMODE without TS */
	if (params.size() < 3)
	{
		/* No modes were in the command, probably a channel with no modes set on it */
		return true;
	}

	bool smode = false;
	std::string sourceserv;
	/* Are we dealing with an FMODE from a user, or from a server? */
	User* who = this->Instance->FindNick(source);
	if (who)
	{
		/* FMODE from a user, set sourceserv to the users server name */
		sourceserv = who->server;
	}
	else
	{
		/* FMODE from a server, use a fake user to receive mode feedback */
		who = this->Instance->FakeClient;
		smode = true;	   /* Setting this flag tells us we should free the User later */
		sourceserv = source;    /* Set sourceserv to the actual source string */
	}
	const char* modelist[64];
	time_t TS = 0;
	int n = 0;
	memset(&modelist,0,sizeof(modelist));
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
			modelist[n++] = params[q].c_str();
		}

	}
	/* Extract the TS value of the object, either User or Channel */
	User* dst = this->Instance->FindNick(params[0]);
	Channel* chan = NULL;
	time_t ourTS = 0;
	if (dst)
	{
		ourTS = dst->age;
	}
	else
	{
		chan = this->Instance->FindChan(params[0]);
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
		Instance->Log(DEFAULT,"*** BUG? *** TS of 0 sent to FMODE. Are some services authors smoking craq, or is it 1970 again?. Dropped.");
		Instance->SNO->WriteToSnoMask('d', "WARNING: The server %s is sending FMODE with a TS of zero. Total craq. Mode was dropped.", sourceserv.c_str());
		return true;
	}

	/* TS is equal or less: Merge the mode changes into ours and pass on.
	 */
	if (TS <= ourTS)
	{
		if ((TS < ourTS) && (!dst))
			Instance->Log(DEFAULT,"*** BUG *** Channel TS sent in FMODE to %s is %lu which is not equal to %lu!", params[0].c_str(), TS, ourTS);

		if (smode)
		{
			this->Instance->SendMode(modelist, n, who);
		}
		else
		{
			this->Instance->CallCommandHandler("MODE", modelist, n, who);
		}
		/* HOT POTATO! PASS IT ON! */
		Utils->DoOneToAllButSender(source,"FMODE",params,sourceserv);
	}
	/* If the TS is greater than ours, we drop the mode and dont pass it anywhere.
	 */
	return true;
}


