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
	if (params[0] == "*")
	{
		std::string value = params.size() < 3 ? "" : params[2];
		FOREACH_MOD(I_OnDecodeMetaData,OnDecodeMetaData(NULL,params[1],value));
	}
	else if (*(params[0].c_str()) == '#')
	{
		Channel* c = ServerInstance->FindChan(params[0]);
		if (c)
		{
			/*
			 * Since protocol version 1204 we have the channel TS in METADATA concerning channels and
			 * we only accept them if the timestamps match. Previous versions don't send a timestamp
			 * so we need to fall back to the old behaviour and accept what they sent.
			 *
			 * Pre-1204:       :sid METADATA #chan metaname :value
			 * 1204 and later: :sid METADATA #chan chants metaname :value
			 *
			 */

			// Determine the protocol version of the sender
			SpanningTreeUtilities* Utils = ((ModuleSpanningTree*) (Module*) creator)->Utils;
			TreeServer* srcserver = Utils->FindServer(srcuser->server);
			if (!srcserver)
			{
				// Illegal prefix in METADATA
				return CMD_FAILURE;
			}

			bool has_ts = (srcserver->Socket->proto_version >= 1204);

			// If we got a channel TS compare it with ours. If it's different, drop the command.
			// Also drop the command if we are using 1204, but there aren't enough parameters.
			if ((has_ts) && ((ConvToInt(params[1]) != c->age) || params.size() < 3))
				return CMD_FAILURE;

			// Now do things as usual but if required, apply an offset to the index when accessing params
			unsigned int indexoffset = has_ts ? 1 : 0;

			std::string value = params.size() < (3+indexoffset) ? "" : params[2+indexoffset];
			ExtensionItem* item = ServerInstance->Extensions.GetItem(params[1+indexoffset]);
			if (item)
				item->unserialize(FORMAT_NETWORK, c, value);
			FOREACH_MOD(I_OnDecodeMetaData,OnDecodeMetaData(c,params[1+indexoffset],value));

			// Finally,	if they haven't sent us a channel TS add ours before passing the command on
			if (!has_ts)
			{
				parameterlist& p = const_cast<parameterlist&>(params);
				p.insert(p.begin()+1, ConvToStr(c->age));
			}
		}
	}
	else if (*(params[0].c_str()) != '#')
	{
		User* u = ServerInstance->FindNick(params[0]);
		if (u)
		{
			std::string value = params.size() < 3 ? "" : params[2];
			ExtensionItem* item = ServerInstance->Extensions.GetItem(params[1]);
			if (item)
				item->unserialize(FORMAT_NETWORK, u, value);
			FOREACH_MOD(I_OnDecodeMetaData,OnDecodeMetaData(u,params[1],value));
		}
	}

	return CMD_SUCCESS;
}

