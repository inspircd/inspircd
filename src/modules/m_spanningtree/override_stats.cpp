/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2007 Craig Edwards <craigedwards@brainbox.cc>
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


/* $ModDesc: Provides a spanning tree server link protocol */

#include "inspircd.h"
#include "socket.h"

#include "main.h"
#include "utils.h"
#include "treeserver.h"
#include "link.h"
#include "treesocket.h"

ModResult ModuleSpanningTree::OnStats(char statschar, User* user, string_list &results)
{
	if ((statschar == 'c') || (statschar == 'n'))
	{
		for (std::vector<reference<Link> >::iterator i = Utils->LinkBlocks.begin(); i != Utils->LinkBlocks.end(); ++i)
		{
			Link* L = *i;
			results.push_back(std::string(ServerInstance->Config->ServerName)+" 213 "+user->nick+" "+statschar+" *@"+(L->HiddenFromStats ? "<hidden>" : L->IPAddr)+" * "+(*i)->Name.c_str()+" "+ConvToStr(L->Port)+" "+(L->Hook.empty() ? "plaintext" : L->Hook));
			if (statschar == 'c')
				results.push_back(std::string(ServerInstance->Config->ServerName)+" 244 "+user->nick+" H * * "+L->Name.c_str());
		}
		return MOD_RES_DENY;
	}
	return MOD_RES_PASSTHRU;
}

