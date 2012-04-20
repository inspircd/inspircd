/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2008 Robin Burchell <robin+git@viroteck.net>
 *
 * This file is part of InspIRCd.  InspIRCd is free software: you can
 * redistribute it and/or modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation, version 2.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */


#include "inspircd.h"
#include "commands/cmd_whois.h"
#include "commands/cmd_stats.h"
#include "socket.h"
#include "xline.h"
#include "transport.h"
#include "socketengine.h"

#include "m_spanningtree/main.h"
#include "m_spanningtree/utils.h"
#include "m_spanningtree/treeserver.h"
#include "m_spanningtree/treesocket.h"

/* $ModDep: m_spanningtree/main.h m_spanningtree/utils.h m_spanningtree/treeserver.h m_spanningtree/treesocket.h */

bool TreeSocket::Time(const std::string &prefix, std::deque<std::string> &params)
{
	// :source.server TIME remote.server sendernick
	// :remote.server TIME source.server sendernick TS
	if (params.size() == 2)
	{
		// someone querying our time?
		if (this->ServerInstance->Config->ServerName == params[0] || this->ServerInstance->Config->GetSID() == params[0])
		{
			User* u = this->ServerInstance->FindNick(params[1]);
			if (u)
			{
				params.push_back(ConvToStr(ServerInstance->Time()));
				params[0] = prefix;
				Utils->DoOneToOne(this->ServerInstance->Config->GetSID(),"TIME",params,params[0]);
			}
		}
		else
		{
			// not us, pass it on
			User* u = this->ServerInstance->FindNick(params[1]);
			if (u)
				Utils->DoOneToOne(prefix,"TIME",params,params[0]);
		}
	}
	else if (params.size() == 3)
	{
		// a response to a previous TIME
		User* u = this->ServerInstance->FindNick(params[1]);
		if ((u) && (IS_LOCAL(u)))
		{
			std::string sourceserv = Utils->FindServer(prefix)->GetName();
			time_t rawtime = atol(params[2].c_str());
			struct tm * timeinfo;
			timeinfo = localtime(&rawtime);
			char tms[26];
			snprintf(tms,26,"%s",asctime(timeinfo));
			tms[24] = 0;
			u->WriteNumeric(RPL_TIME, "%s %s :%s",u->nick.c_str(),sourceserv.c_str(),tms);
		}
		else
		{
			if (u)
				Utils->DoOneToOne(prefix,"TIME",params,u->server);
		}
	}
	return true;
}

