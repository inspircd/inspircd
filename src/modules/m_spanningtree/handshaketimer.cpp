/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2007 Craig Edwards <craigedwards@brainbox.cc>
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
#include "commands/cmd_whois.h"
#include "commands/cmd_stats.h"
#include "socket.h"
#include "xline.h"
#include "transport.h"

#include "m_spanningtree/main.h"
#include "m_spanningtree/utils.h"
#include "m_spanningtree/treeserver.h"
#include "m_spanningtree/link.h"
#include "m_spanningtree/treesocket.h"
#include "m_spanningtree/handshaketimer.h"

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

