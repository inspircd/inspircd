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
#include "socket.h"
#include "xline.h"
#include "../transport.h"
#include "socketengine.h"

#include "main.h"
#include "utils.h"
#include "treeserver.h"
#include "treesocket.h"

/* $ModDep: m_spanningtree/main.h m_spanningtree/utils.h m_spanningtree/treeserver.h m_spanningtree/treesocket.h */

bool TreeSocket::LocalPong(const std::string &prefix, parameterlist &params)
{
	if (params.size() < 1)
		return true;

	if (params.size() == 1)
	{
		TreeServer* ServerSource = Utils->FindServer(prefix);
		if (ServerSource)
		{
			ServerSource->SetPingFlag();
			timeval t;
			gettimeofday(&t, NULL);
			long ts = (t.tv_sec * 1000) + (t.tv_usec / 1000);
			ServerSource->rtt = ts - ServerSource->LastPingMsec;
		}
	}
	else
	{
		std::string forwardto = params[1];
		if (forwardto == ServerInstance->Config->GetSID() || forwardto == ServerInstance->Config->ServerName)
		{
			/*
			 * this is a PONG for us
			 * if the prefix is a user, check theyre local, and if they are,
			 * dump the PONG reply back to their fd. If its a server, do nowt.
			 * Services might want to send these s->s, but we dont need to yet.
			 */
			User* u = ServerInstance->FindNick(prefix);
			if (u)
			{
				u->WriteServ("PONG %s %s",params[0].c_str(),params[1].c_str());
			}

			TreeServer *ServerSource = Utils->FindServer(params[0]);

			if (ServerSource)
			{
				timeval t;
				gettimeofday(&t, NULL);
				long ts = (t.tv_sec * 1000) + (t.tv_usec / 1000);
				ServerSource->rtt = ts - ServerSource->LastPingMsec;
				ServerSource->SetPingFlag();
			}
		}
		else
		{
			// not for us, pass it on :)
			Utils->DoOneToOne(prefix,"PONG",params,forwardto);
		}
	}

	return true;
}

