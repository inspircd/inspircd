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
#include "configreader.h"
#include "users.h"
#include "channels.h"
#include "modules.h"
#include "commands/cmd_whois.h"
#include "commands/cmd_stats.h"
#include "socket.h"
#include "wildcard.h"
#include "xline.h"
#include "transport.h"

#include "m_spanningtree/main.h"
#include "m_spanningtree/utils.h"
#include "m_spanningtree/treeserver.h"
#include "m_spanningtree/link.h"
#include "m_spanningtree/treesocket.h"
#include "m_spanningtree/handshaketimer.h"

/* $ModDep: m_spanningtree/main.h m_spanningtree/utils.h m_spanningtree/treeserver.h m_spanningtree/link.h m_spanningtree/treesocket.h */

HandshakeTimer::HandshakeTimer(InspIRCd* Inst, TreeSocket* s, Link* l, SpanningTreeUtilities* u, int delay) : InspTimer(delay, time(NULL)), Instance(Inst), sock(s), lnk(l), Utils(u)
{
	thefd = sock->GetFd();
}

void HandshakeTimer::Tick(time_t TIME)
{
	if (Instance->SE->GetRef(thefd) == sock)
	{
		if (!sock->GetHook())
		{
			sock->SendCapabilities();
		}
		else
		{
			if (sock->GetHook() && InspSocketHSCompleteRequest(sock, (Module*)Utils->Creator, sock->GetHook()).Send())
			{
				InspSocketAttachCertRequest(sock, (Module*)Utils->Creator, sock->GetHook()).Send();
				sock->SendCapabilities();
			}
			else
			{
				Instance->Timers->AddTimer(new HandshakeTimer(Instance, sock, lnk, Utils, 1));
			}
		}
	}
}

