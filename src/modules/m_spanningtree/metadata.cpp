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
#include "xline.h"

#include "m_spanningtree/treesocket.h"
#include "m_spanningtree/treeserver.h"
#include "m_spanningtree/utils.h"

/* $ModDep: m_spanningtree/utils.h m_spanningtree/treeserver.h m_spanningtree/treesocket.h */


bool TreeSocket::MetaData(const std::string &prefix, std::deque<std::string> &params)
{
	if (params.size() < 2)
		return true;
	else if (params.size() < 3)
		params.push_back("");
	TreeServer* ServerSource = Utils->FindServer(prefix);
	if (ServerSource)
	{
		if (params[0] == "*")
		{
			FOREACH_MOD_I(this->ServerInstance,I_OnDecodeMetaData,OnDecodeMetaData(TYPE_OTHER,NULL,params[1],params[2]));
		}
		else if (*(params[0].c_str()) == '#')
		{
			Channel* c = this->ServerInstance->FindChan(params[0]);
			if (c)
			{
				FOREACH_MOD_I(this->ServerInstance,I_OnDecodeMetaData,OnDecodeMetaData(TYPE_CHANNEL,c,params[1],params[2]));
			}
		}
		else if (*(params[0].c_str()) != '#')
		{
			User* u = this->ServerInstance->FindNick(params[0]);
			if (u)
			{
				FOREACH_MOD_I(this->ServerInstance,I_OnDecodeMetaData,OnDecodeMetaData(TYPE_USER,u,params[1],params[2]));
			}
		}
	}

	params[2] = ":" + params[2];
	Utils->DoOneToAllButSender(prefix,"METADATA",params,prefix);
	return true;
}

