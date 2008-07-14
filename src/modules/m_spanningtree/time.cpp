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

bool TreeSocket::Time(const std::string &prefix, std::deque<std::string> &params)
{
	// :source.server TIME remote.server sendernick
	// :remote.server TIME source.server sendernick TS
	if (params.size() == 2)
	{
		// someone querying our time?
		if (this->Instance->Config->ServerName == params[0] || this->Instance->Config->GetSID() == params[0])
		{
			User* u = this->Instance->FindNick(params[1]);
			if (u)
			{
				params.push_back(ConvToStr(Instance->Time()));
				params[0] = prefix;
				Utils->DoOneToOne(this->Instance->Config->GetSID(),"TIME",params,params[0]);
			}
		}
		else
		{
			// not us, pass it on
			User* u = this->Instance->FindNick(params[1]);
			if (u)
				Utils->DoOneToOne(prefix,"TIME",params,params[0]);
		}
	}
	else if (params.size() == 3)
	{
		// a response to a previous TIME
		User* u = this->Instance->FindNick(params[1]);
		if ((u) && (IS_LOCAL(u)))
		{
			time_t rawtime = atol(params[2].c_str());
			struct tm * timeinfo;
			timeinfo = localtime(&rawtime);
			char tms[26];
			snprintf(tms,26,"%s",asctime(timeinfo));
			tms[24] = 0;
			u->WriteNumeric(RPL_TIME, "%s %s :%s",u->nick.c_str(),prefix.c_str(),tms);
		}
		else
		{
			if (u)
				Utils->DoOneToOne(prefix,"TIME",params,u->server);
		}
	}
	return true;
}

