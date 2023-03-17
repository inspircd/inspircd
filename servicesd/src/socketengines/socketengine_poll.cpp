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

#include <errno.h>

#ifndef _WIN32
# include <sys/poll.h>
# include <poll.h>
# include <sys/types.h>
# include <sys/time.h>
# include <sys/resource.h>
# ifndef POLLRDHUP
#  define POLLRDHUP 0
# endif
#else
# define poll WSAPoll
# define POLLRDHUP POLLHUP
#endif

static std::vector<pollfd> events;
static std::map<int, unsigned> socket_positions;

void SocketEngine::Init() {
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

    if (!before_registered && now_registered) {
        pollfd ev;
        memset(&ev, 0, sizeof(ev));

        ev.fd = s->GetFD();
        ev.events = (s->flags[SF_READABLE] ? POLLIN : 0) | (s->flags[SF_WRITABLE] ?
                    POLLOUT : 0);

        socket_positions[ev.fd] = events.size();
        events.push_back(ev);
    } else if (before_registered && !now_registered) {
        std::map<int, unsigned>::iterator pos = socket_positions.find(s->GetFD());
        if (pos == socket_positions.end()) {
            throw SocketException("Unable to remove fd " + stringify(
                                      s->GetFD()) + " from poll, it does not exist?");
        }

        if (pos->second != events.size() - 1) {
            pollfd &ev = events[pos->second],
                    &last_ev = events[events.size() - 1];

            ev = last_ev;

            socket_positions[ev.fd] = pos->second;
        }

        socket_positions.erase(pos);
        events.pop_back();
    } else if (before_registered && now_registered) {
        std::map<int, unsigned>::iterator pos = socket_positions.find(s->GetFD());
        if (pos == socket_positions.end()) {
            throw SocketException("Unable to modify fd " + stringify(
                                      s->GetFD()) + " in poll, it does not exist?");
        }

        pollfd &ev = events[pos->second];
        ev.events = (s->flags[SF_READABLE] ? POLLIN : 0) | (s->flags[SF_WRITABLE] ?
                    POLLOUT : 0);
    }
}

void SocketEngine::Process() {
    int total = poll(&events.front(), events.size(), Config->ReadTimeout * 1000);
    Anope::CurTime = time(NULL);

    /* EINTR can be given if the read timeout expires */
    if (total < 0) {
        if (errno != EINTR) {
            Log() << "SockEngine::Process(): error: " << Anope::LastError();
        }
        return;
    }

    for (unsigned i = 0, processed = 0; i < events.size()
            && processed != static_cast<unsigned>(total); ++i) {
        pollfd *ev = &events[i];

        if (ev->revents != 0) {
            ++processed;
        }

        std::map<int, Socket *>::iterator it = Sockets.find(ev->fd);
        if (it == Sockets.end()) {
            continue;
        }
        Socket *s = it->second;

        if (ev->revents & (POLLERR | POLLRDHUP)) {
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

        if ((ev->revents & POLLIN) && !s->ProcessRead()) {
            s->flags[SF_DEAD] = true;
        }

        if ((ev->revents & POLLOUT) && !s->ProcessWrite()) {
            s->flags[SF_DEAD] = true;
        }

        if (s->flags[SF_DEAD]) {
            delete s;
        }
    }
}
