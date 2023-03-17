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
#include "logger.h"
#include "config.h"

#include <sys/types.h>
#include <sys/event.h>
#include <sys/time.h>
#include <errno.h>

static int kq_fd;
static std::vector<struct kevent> change_events, event_events;
static unsigned change_count;

static inline struct kevent *GetChangeEvent() {
    if (change_count == change_events.size()) {
        change_events.resize(change_count * 2);
    }

    return &change_events[change_count++];
}

void SocketEngine::Init() {
    kq_fd = kqueue();

    if (kq_fd < 0) {
        throw SocketException("Unable to create kqueue engine: " + Anope::LastError());
    }

    change_events.resize(DefaultSize);
    event_events.resize(DefaultSize);
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

    s->flags[flag] = set;

    int mod;
    if (flag == SF_READABLE) {
        mod = EVFILT_READ;
    } else if (flag == SF_WRITABLE) {
        mod = EVFILT_WRITE;
    } else {
        return;
    }

    struct kevent *event = GetChangeEvent();
    EV_SET(event, s->GetFD(), mod, set ? EV_ADD : EV_DELETE, 0, 0, NULL);
}

void SocketEngine::Process() {
    if (Sockets.size() > event_events.size()) {
        event_events.resize(event_events.size() * 2);
    }

    static timespec kq_timespec = { Config->ReadTimeout, 0 };
    int total = kevent(kq_fd, &change_events.front(), change_count,
                       &event_events.front(), event_events.size(), &kq_timespec);
    change_count = 0;
    Anope::CurTime = time(NULL);

    /* EINTR can be given if the read timeout expires */
    if (total == -1) {
        if (errno != EINTR) {
            Log() << "SockEngine::Process(): error: " << Anope::LastError();
        }
        return;
    }

    for (int i = 0; i < total; ++i) {
        struct kevent &event = event_events[i];
        if (event.flags & EV_ERROR) {
            continue;
        }

        std::map<int, Socket *>::iterator it = Sockets.find(event.ident);
        if (it == Sockets.end()) {
            continue;
        }
        Socket *s = it->second;

        if (event.flags & EV_EOF) {
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

        if (event.filter == EVFILT_READ && !s->ProcessRead()) {
            s->flags[SF_DEAD] = true;
        } else if (event.filter == EVFILT_WRITE && !s->ProcessWrite()) {
            s->flags[SF_DEAD] = true;
        }

        if (s->flags[SF_DEAD]) {
            delete s;
        }
    }
}
