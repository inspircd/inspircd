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
#include "socketengine.h"

#include "m_spanningtree/main.h"
#include "m_spanningtree/utils.h"
#include "m_spanningtree/treeserver.h"
#include "m_spanningtree/link.h"
#include "m_spanningtree/treesocket.h"
#include "m_spanningtree/resolvers.h"
#include "m_spanningtree/handshaketimer.h"

/* $ModDep: m_spanningtree/timesynctimer.h m_spanningtree/resolvers.h m_spanningtree/main.h m_spanningtree/utils.h m_spanningtree/treeserver.h m_spanningtree/link.h m_spanningtree/treesocket.h */

bool TreeSocket::AddLine(const std::string &prefix, std::deque<std::string> &params)
{
	if (params.size() < 6)
	{
		this->Instance->SNO->WriteToSnoMask('x',"%s sent me a malformed ADDLINE of type %s.",prefix.c_str(),params[0].c_str());
		return true;
	}

	XLineFactory* xlf = Instance->XLines->GetFactory(params[0]);

	if (!xlf)
	{
		this->Instance->SNO->WriteToSnoMask('x',"%s sent me an unknown ADDLINE type (%s).",prefix.c_str(),params[0].c_str());
		return true;
	}

	XLine* xl = xlf->Generate(Instance->Time(), atoi(params[4].c_str()), params[2].c_str(), params[5].c_str(), params[1].c_str());
	xl->SetCreateTime(atoi(params[3].c_str()));
	if (Instance->XLines->AddLine(xl,NULL))
	{
		if (xl->duration)
		{
			this->Instance->SNO->WriteToSnoMask('x',"%s added %s%s on %s to expire on %s (%s).",prefix.c_str(),params[0].c_str(),params[0].length() == 1 ? "LINE" : "",
					params[1].c_str(),Instance->TimeString(xl->expiry).c_str(),params[5].c_str());
		}
		else
		{
			this->Instance->SNO->WriteToSnoMask('x',"%s added permanent %s%s on %s (%s).",prefix.c_str(),params[0].c_str(),params[0].length() == 1 ? "LINE" : "",
					params[1].c_str(),params[5].c_str());
		}
		params[5] = ":" + params[5];

		User* u = Instance->FindNick(prefix);
		Utils->DoOneToAllButSender(prefix, "ADDLINE", params, u ? u->server : prefix);
		TreeServer *remoteserver = Utils->FindServer(u ? u->server : prefix);
		
		if (!remoteserver->bursting)
		{
			Instance->XLines->ApplyLines();
		}
	}
	else
		delete xl;

	return true;
}

