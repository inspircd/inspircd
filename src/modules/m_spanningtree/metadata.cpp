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

