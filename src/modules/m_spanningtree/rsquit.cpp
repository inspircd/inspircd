/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2007 InspIRCd Development Team
 * See: http://www.inspircd.org/wiki/index.php/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"
#include "commands/cmd_whois.h"
#include "commands/cmd_stats.h"
#include "socket.h"
#include "wildcard.h"
#include "xline.h"
#include "transport.h"

#include "m_spanningtree/timesynctimer.h"
#include "m_spanningtree/resolvers.h"
#include "m_spanningtree/main.h"
#include "m_spanningtree/utils.h"
#include "m_spanningtree/treeserver.h"
#include "m_spanningtree/link.h"
#include "m_spanningtree/treesocket.h"
#include "m_spanningtree/rsquit.h"

/* $ModDep: m_spanningtree/timesynctimer.h m_spanningtree/resolvers.h m_spanningtree/main.h m_spanningtree/utils.h m_spanningtree/treeserver.h m_spanningtree/link.h m_spanningtree/treesocket.h m_spanningtree/rsquit.h */

cmd_rsquit::cmd_rsquit (InspIRCd* Instance, Module* Callback, SpanningTreeUtilities* Util) : Command(Instance, "RSQUIT", 'o', 1), Creator(Callback), Utils(Util)
{
	this->source = "m_spanningtree.so";
	syntax = "<remote-server-mask> [reason]";
}

CmdResult cmd_rsquit::Handle (const char** parameters, int pcnt, User *user)
{
	TreeServer *server_target; // one to squit
	TreeServer *server_linked; // one it's linked to

	server_target = Utils->FindServerMask(parameters[0]);
	if (!server_target)
	{
		user->WriteServ("NOTICE %s :*** RSQUIT: Server \002%s\002 isn't connected to the network!", user->nick, parameters[0]);
		return CMD_FAILURE;
	}

	server_linked = server_target->GetParent();
	user->WriteServ("NOTICE %s :*** RSQUIT: Sending instruction to squit server \002%s\002 to parent server \002%s\002.",user->nick, server_target->GetName().c_str(), server_linked->GetName().c_str());

	if (server_linked == Utils->TreeRoot)
	{
		// I have been asked to remove the server.
		if (server_target == Utils->TreeRoot)
		{
			NoticeUser(user, "*** RSQUIT: Foolish mortal, you cannot make a server SQUIT itself! ("+ConvToStr(parameters[0])+" matches local server name)");
			return CMD_FAILURE;
		}

		TreeSocket* sock = server_target->GetSocket();
		if (sock)
		{
			const char *reason = pcnt == 2 ? parameters[1] : "No reason";
			ServerInstance->SNO->WriteToSnoMask('l',"RSQUIT: Server \002%s\002 removed from network by %s (%s)", parameters[0], user->nick, reason);
			sock->Squit(server_target, std::string("Server quit by ") + user->GetFullRealHost() + " (" + reason + ")");
			ServerInstance->SE->DelFd(sock);
			sock->Close();
			return CMD_LOCALONLY;
		}
	}

	// Route the RSQUIT, no match yet.
	return CMD_SUCCESS;
}

void cmd_rsquit::NoticeUser(User* user, const std::string &msg)
{
	if (IS_LOCAL(user))
	{
		user->WriteServ("NOTICE %s :%s",user->nick,msg.c_str());
	}
	else
	{
		std::deque<std::string> params;
		params.push_back(user->nick);
		params.push_back("NOTICE "+ConvToStr(user->nick)+" :"+msg);
		Utils->DoOneToOne(ServerInstance->Config->GetSID(), "PUSH", params, user->server);
	}
}

