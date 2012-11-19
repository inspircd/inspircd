/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2010 Daniel De Graaf <danieldg@inspircd.org>
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
#include "commands.h"

#include "treesocket.h"
#include "treeserver.h"
#include "utils.h"

CmdResult CommandMetadata::Handle(const std::vector<std::string>& params, User *srcuser)
{
	std::string value = params.size() < 3 ? "" : params[2];
	ExtensionItem* item = ServerInstance->Extensions.GetItem(params[1]);
	if (params[0] == "*")
	{
		FOREACH_MOD(I_OnDecodeMetaData,OnDecodeMetaData(NULL,params[1],value));
	}
	else if (*(params[0].c_str()) == '#')
	{
		Channel* c = ServerInstance->FindChan(params[0]);
		if (c)
		{
			if (item)
				item->unserialize(FORMAT_NETWORK, c, value);
			FOREACH_MOD(I_OnDecodeMetaData,OnDecodeMetaData(c,params[1],value));
		}
	}
	else if (*(params[0].c_str()) != '#')
	{
		User* u = ServerInstance->FindUUID(params[0]);
		if ((u) && (!IS_SERVER(u)))
		{
			if (item)
				item->unserialize(FORMAT_NETWORK, u, value);
			FOREACH_MOD(I_OnDecodeMetaData,OnDecodeMetaData(u,params[1],value));
		}
	}

	return CMD_SUCCESS;
}

