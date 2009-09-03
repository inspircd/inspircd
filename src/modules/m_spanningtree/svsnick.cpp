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
#include "socket.h"
#include "xline.h"
#include "../transport.h"
#include "socketengine.h"

#include "main.h"
#include "utils.h"
#include "treeserver.h"
#include "treesocket.h"

/* $ModDep: m_spanningtree/main.h m_spanningtree/utils.h m_spanningtree/treeserver.h m_spanningtree/treesocket.h */

/** Because Andy insists that services-compatible servers must
 * implement SVSNICK and SVSJOIN, that's exactly what we do :p
 */
bool TreeSocket::SVSNick(const std::string &prefix, parameterlist &params)
{
	if (params.size() < 3)
		return true;

	User* u = this->ServerInstance->FindNick(params[0]);

	if (u)
	{
		Utils->DoOneToAllButSender(prefix,"SVSNICK",params,prefix);

		if (IS_LOCAL(u))
		{
			parameterlist par;
			par.push_back(params[1]);

			if (!u->ForceNickChange(params[1].c_str()))
			{
				/* buh. UID them */
				if (!u->ForceNickChange(u->uuid.c_str()))
				{
					this->ServerInstance->Users->QuitUser(u, "Nickname collision");
					return true;
				}
			}

			u->age = atoi(params[2].c_str());
		}
	}

	return true;
}

