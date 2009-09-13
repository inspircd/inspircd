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
#include "xline.h"

#include "treesocket.h"
#include "treeserver.h"
#include "utils.h"

/* $ModDep: m_spanningtree/utils.h m_spanningtree/treeserver.h m_spanningtree/treesocket.h */


bool TreeSocket::MetaData(const std::string &prefix, parameterlist &params)
{
	if (params.size() < 2)
		return true;
	else if (params.size() < 3)
		params.push_back("");
	TreeServer* ServerSource = Utils->FindServer(prefix);
	ExtensionItem* item = Extensible::GetItem(params[1]);
	if (ServerSource)
	{
		if (params[0] == "*")
		{
			FOREACH_MOD_I(this->ServerInstance,I_OnDecodeMetaData,OnDecodeMetaData(NULL,params[1],params[2]));
		}
		else if (*(params[0].c_str()) == '#')
		{
			Channel* c = this->ServerInstance->FindChan(params[0]);
			if (c)
			{
				if (item)
					item->unserialize(FORMAT_NETWORK, c, params[2]);
				FOREACH_MOD_I(this->ServerInstance,I_OnDecodeMetaData,OnDecodeMetaData(c,params[1],params[2]));
			}
		}
		else if (*(params[0].c_str()) != '#')
		{
			User* u = this->ServerInstance->FindNick(params[0]);
			if (u)
			{
				if (item)
					item->unserialize(FORMAT_NETWORK, u, params[2]);
				FOREACH_MOD_I(this->ServerInstance,I_OnDecodeMetaData,OnDecodeMetaData(u,params[1],params[2]));
			}
		}
	}

	params[2] = ":" + params[2];
	Utils->DoOneToAllButSender(prefix,"METADATA",params,prefix);
	return true;
}

