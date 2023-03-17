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

#ifdef _AIX
# undef FD_ZERO
# define FD_ZERO(p) memset((p), 0, sizeof(*(p)))
#endif /* _AIX */

static int MaxFD;
static unsigned FDCount;
static fd_set ReadFDs;
static fd_set WriteFDs;

void SocketEngine::Init() {
    FD_ZERO(&ReadFDs);
    FD_ZERO(&WriteFDs);
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
        if (s->GetFD() > MaxFD) {
            MaxFD = s->GetFD();
        }
        if (s->flags[SF_READABLE]) {
            FD_SET(s->GetFD(), &ReadFDs);
        }
        if (s->flags[SF_WRITABLE]) {
            FD_SET(s->GetFD(), &WriteFDs);
        }
        ++FDCount;
    } else if (before_registered && !now_registered) {
        if (s->GetFD() == MaxFD) {
            --MaxFD;
        }
        FD_CLR(s->GetFD(), &ReadFDs);
        FD_CLR(s->GetFD(), &WriteFDs);
        --FDCount;
    } else if (before_registered && now_registered) {
        if (s->flags[SF_READABLE]) {
            FD_SET(s->GetFD(), &ReadFDs);
        } else {
            FD_CLR(s->GetFD(), &ReadFDs);
        }

        if (s->flags[SF_WRITABLE]) {
            FD_SET(s->GetFD(), &WriteFDs);
        } else {
            FD_CLR(s->GetFD(), &WriteFDs);
        }
    }
}

void SocketEngine::Process() {
    fd_set rfdset = ReadFDs, wfdset = WriteFDs, efdset = ReadFDs;
    timeval tval;
    tval.tv_sec = Config->ReadTimeout;
    tval.tv_usec = 0;

#ifdef _WIN32
    /* We can use the socket engine to "sleep" services for a period of
     * time between connections to the uplink, which allows modules,
     * timers, etc to function properly. Windows, being as useful as it is,
     * does not allow to select() on 0 sockets and will immediately return error.
     * Thus:
     */
    if (FDCount == 0) {
        sleep(tval.tv_sec);
        return;
    }
#endif

    int sresult = select(MaxFD + 1, &rfdset, &wfdset, &efdset, &tval);
    Anope::CurTime = time(NULL);

    if (sresult == -1) {
        Log() << "SockEngine::Process(): error: " << Anope::LastError();
    } else if (sresult) {
        int processed = 0;
        for (std::map<int, Socket *>::const_iterator it = Sockets.begin(),
                it_end = Sockets.end(); it != it_end && processed != sresult;) {
            Socket *s = it->second;
            ++it;

            bool has_read = FD_ISSET(s->GetFD(), &rfdset), has_write = FD_ISSET(s->GetFD(),
                            &wfdset), has_error = FD_ISSET(s->GetFD(), &efdset);
            if (has_read || has_write || has_error) {
                ++processed;
            }

            if (has_error) {
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

            if (has_read && !s->ProcessRead()) {
                s->flags[SF_DEAD] = true;
            }

            if (has_write && !s->ProcessWrite()) {
                s->flags[SF_DEAD] = true;
            }

            if (s->flags[SF_DEAD]) {
                delete s;
            }
        }
    }
}
