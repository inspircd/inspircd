/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2010 InspIRCd Development Team
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


/** Because the core won't let users or even SERVERS set +o,
 * we use the OPERTYPE command to do this.
 */
bool TreeSocket::OperType(const std::string &prefix, parameterlist &params)
{
	if (params.size() != 1)
		return true;
	std::string opertype = params[0];
	User* u = ServerInstance->FindNick(prefix);
	if (u)
	{
		if (!IS_OPER(u))
			ServerInstance->Users->all_opers.push_back(u);
		u->modes[UM_OPERATOR] = 1;
		OperIndex::iterator iter = ServerInstance->Config->oper_blocks.find(" " + opertype);
		if (iter != ServerInstance->Config->oper_blocks.end())
			u->oper = iter->second;
		else
		{
			u->oper = new OperInfo;
			u->oper->name = opertype;
		}
		Utils->DoOneToAllButSender(u->uuid, "OPERTYPE", params, u->server);

		TreeServer* remoteserver = Utils->FindServer(u->server);
		bool dosend = true;

		if (this->Utils->quiet_bursts)
		{
			/*
			 * If quiet bursts are enabled, and server is bursting or silent uline (i.e. services),
			 * then do nothing. -- w00t
			 */
			if (remoteserver->bursting || ServerInstance->SilentULine(u->server))
			{
				dosend = false;
			}
		}

		if (dosend)
			ServerInstance->SNO->WriteToSnoMask('O',"From %s: User %s (%s@%s) is now an IRC operator of type %s",u->server.c_str(), u->nick.c_str(),u->ident.c_str(), u->host.c_str(), irc::Spacify(opertype.c_str()));
	}
	return true;
}

