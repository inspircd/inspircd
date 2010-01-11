/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2010 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"

#include "main.h"
#include "utils.h"
#include "treeserver.h"
#include "treesocket.h"

bool TreeSocket::Away(const std::string &prefix, parameterlist &params)
{
	User* u = ServerInstance->FindNick(prefix);
	if (!u)
		return true;
	if (params.size())
	{
		FOREACH_MOD(I_OnSetAway, OnSetAway(u, params[params.size() - 1]));

		if (params.size() > 1)
			u->awaytime = atoi(params[0].c_str());
		else
			u->awaytime = ServerInstance->Time();

		u->awaymsg = params[params.size() - 1];

		params[params.size() - 1] = ":" + params[params.size() - 1];
	}
	else
	{
		FOREACH_MOD(I_OnSetAway, OnSetAway(u, ""));
		u->awaymsg.clear();
	}
	Utils->DoOneToAllButSender(prefix,"AWAY",params,u->server);
	return true;
}
