/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2012 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
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
		User* u = ServerInstance->FindNick(params[0]);
		if (u)
		{
			if (item)
				item->unserialize(FORMAT_NETWORK, u, value);
			FOREACH_MOD(I_OnDecodeMetaData,OnDecodeMetaData(u,params[1],value));
		}
	}

	return CMD_SUCCESS;
}

