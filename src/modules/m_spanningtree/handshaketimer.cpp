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

#include "configreader.h"
#include "users.h"
#include "channels.h"
#include "modules.h"
#include "commands/cmd_whois.h"
#include "commands/cmd_stats.h"
#include "socket.h"
#include "inspircd.h"
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

HandshakeTimer::HandshakeTimer(InspIRCd* Inst, TreeSocket* s, Link* l, SpanningTreeUtilities* u) : InspTimer(1, time(NULL)), Instance(Inst), sock(s), lnk(l), Utils(u)
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
			if (sock->GetLinkState() == CONNECTING)
				sock->WriteLine(std::string("SERVER ")+this->Instance->Config->ServerName+" "+sock->MakePass(lnk->SendPass, sock->GetTheirChallenge())+" 0 :"+this->Instance->Config->ServerDesc);
		}
		else
		{
			if (sock->GetHook() && InspSocketHSCompleteRequest(sock, (Module*)Utils->Creator, sock->GetHook()).Send())
			{
				InspSocketAttachCertRequest(sock, (Module*)Utils->Creator, sock->GetHook()).Send();
				sock->SendCapabilities();
				if (sock->GetLinkState() == CONNECTING)
					sock->WriteLine(std::string("SERVER ")+this->Instance->Config->ServerName+" "+sock->MakePass(lnk->SendPass, sock->GetTheirChallenge())+" 0 :"+this->Instance->Config->ServerDesc);
			}
			else
			{
				Instance->Timers->AddTimer(new HandshakeTimer(Instance, sock, lnk, Utils));
			}
		}
	}
}

