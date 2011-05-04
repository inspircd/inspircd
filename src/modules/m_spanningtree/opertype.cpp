/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2011 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"
#include "commands.h"
#include "treeserver.h"
#include "utils.h"

/** Because the core won't let users or even SERVERS set +o,
 * we use the OPERTYPE command to do this.
 */
CmdResult CommandOpertype::Handle(const std::vector<std::string>& params, User *u)
{
	SpanningTreeUtilities* Utils = ((ModuleSpanningTree*)(Module*)creator)->Utils;
	std::string opertype = params[0];
	if (!IS_OPER(u))
		ServerInstance->Users->all_opers.push_back(u);
	u->modes[UM_OPERATOR] = 1;
	OperIndex::iterator iter = ServerInstance->Config->oper_blocks.find(" " + opertype);
	if (iter != ServerInstance->Config->oper_blocks.end())
		u->oper = iter->second;
	else
		u->oper = new OperInfo(opertype);

	TreeServer* remoteserver = Utils->FindServer(u->server);
	bool dosend = true;

	if (Utils->quiet_bursts)
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
	return CMD_SUCCESS;
}

