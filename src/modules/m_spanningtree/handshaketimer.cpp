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
#include "commands/cmd_whois.h"
#include "commands/cmd_stats.h"
#include "socket.h"
#include "xline.h"
#include "../transport.h"

#include "main.h"
#include "utils.h"
#include "treeserver.h"
#include "link.h"
#include "treesocket.h"
#include "handshaketimer.h"

/* $ModDep: m_spanningtree/main.h m_spanningtree/utils.h m_spanningtree/treeserver.h m_spanningtree/link.h m_spanningtree/treesocket.h */

HandshakeTimer::HandshakeTimer(InspIRCd* Inst, TreeSocket* s, Link* l, SpanningTreeUtilities* u, int delay) : Timer(delay, Inst->Time(), true), Instance(Inst), sock(s), lnk(l), Utils(u)
{
	thefd = sock->GetFd();
}

HandshakeTimer::~HandshakeTimer()
{
	sock->hstimer = NULL;
}

void HandshakeTimer::Tick(time_t TIME)
{
	if (!sock->GetHook())
	{
		CancelRepeat();
		sock->SendCapabilities();
	}
	else if (BufferedSocketHSCompleteRequest(sock, (Module*)Utils->Creator, sock->GetHook()).Send())
	{
		CancelRepeat();
		BufferedSocketAttachCertRequest(sock, (Module*)Utils->Creator, sock->GetHook()).Send();
		sock->SendCapabilities();
	}
	// otherwise, try again later
}

