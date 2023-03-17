/*
 *
 * (C) 2003-2023 Anope Team
 * Contact us at team@anope.org
 *
 * Please read COPYING and README for further details.
 *
 * Based on the original code of Epona by Lara.
 * Based on the original code of Services by Andy Church.
 */

#include "services.h"
#include "anope.h"
#include "sockets.h"
#include "socketengine.h"
#include "config.h"

#include <sys/epoll.h>
#include <ulimit.h>
#include <errno.h>

static int EngineHandle;
static std::vector<epoll_event> events;

void SocketEngine::Init() {
    EngineHandle = epoll_create(4);

    if (EngineHandle == -1) {
        throw SocketException("Could not initialize epoll socket engine: " +
                              Anope::LastError());
    }

    events.resize(DefaultSize);
}

void SocketEngine::Shutdown() {
    while (!Sockets.empty()) {
        delete Sockets.begin()->second;
    }
}

void SocketEngine::Change(Socket *s, bool set, SocketFlag flag) {
    if (set == s->flags[flag]) {
        return;
    }

    bool before_registered = s->flags[SF_READABLE] || s->flags[SF_WRITABLE];

    s->flags[flag] = set;

    bool now_registered = s->flags[SF_READABLE] || s->flags[SF_WRITABLE];

    epoll_event ev;

    memset(&ev, 0, sizeof(ev));

    ev.events = (s->flags[SF_READABLE] ? EPOLLIN : 0) | (s->flags[SF_WRITABLE] ?
                EPOLLOUT : 0);
    ev.data.fd = s->GetFD();

    int mod;
    if (!before_registered && now_registered) {
        mod = EPOLL_CTL_ADD;
    } else if (before_registered && !now_registered) {
        mod = EPOLL_CTL_DEL;
    } else if (before_registered && now_registered) {
        mod = EPOLL_CTL_MOD;
    } else {
        return;
    }

    if (epoll_ctl(EngineHandle, mod, ev.data.fd, &ev) == -1) {
        throw SocketException("Unable to epoll_ctl() fd " + stringify(
                                  ev.data.fd) + " to epoll: " + Anope::LastError());
    }
}

void SocketEngine::Process() {
    if (Sockets.size() > events.size()) {
        events.resize(events.size() * 2);
    }

    int total = epoll_wait(EngineHandle, &events.front(), events.size(),
                           Config->ReadTimeout * 1000);
    Anope::CurTime = time(NULL);

    /* EINTR can be given if the read timeout expires */
    if (total == -1) {
        if (errno != EINTR) {
            Log() << "SockEngine::Process(): error: " << Anope::LastError();
        }
        return;
    }

    for (int i = 0; i < total; ++i) {
        epoll_event &ev = events[i];

        std::map<int, Socket *>::iterator it = Sockets.find(ev.data.fd);
        if (it == Sockets.end()) {
            continue;
        }
        Socket *s = it->second;

        if (ev.events & (EPOLLHUP | EPOLLERR)) {
            s->ProcessError();
            delete s;
            continue;
        }

        if (!s->Process()) {
            if (s->flags[SF_DEAD]) {
                delete s;
            }
            continue;
        }

        if ((ev.events & EPOLLIN) && !s->ProcessRead()) {
            s->flags[SF_DEAD] = true;
        }

        if ((ev.events & EPOLLOUT) && !s->ProcessWrite()) {
            s->flags[SF_DEAD] = true;
        }

        if (s->flags[SF_DEAD]) {
            delete s;
        }
    }
}
