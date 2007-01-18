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

HandshakeTimer::HandshakeTimer(InspIRCd* Inst, TreeSocket* s, Link* l, SpanningTreeUtilities* u) : InspTimer(1, time(NULL)), Instance(Inst), sock(s), lnk(l), Utils(u)
{
        thefd = sock->GetFd();
}

void HandshakeTimer::Tick(time_t TIME)
{
        if (Instance->SE->GetRef(thefd) == sock)
        {
                if (sock->GetHook() && InspSocketHSCompleteRequest(sock, (Module*)Utils->Creator, sock->GetHook()).Send())
                {
                        InspSocketAttachCertRequest(sock, (Module*)Utils->Creator, sock->GetHook()).Send();
                        sock->SendCapabilities();
                        if (sock->GetLinkState() == CONNECTING)
                        {
                                sock->WriteLine(std::string("SERVER ")+this->Instance->Config->ServerName+" "+lnk->SendPass+" 0 :"+this->Instance->Config->ServerDesc);
                        }
                }
                else
                {
                        Instance->Timers->AddTimer(new HandshakeTimer(Instance, sock, lnk, Utils));
                }
        }
}
