/* Logging routines.
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
#include "modules.h"
#include "commands.h"
#include "channels.h"
#include "users.h"
#include "logger.h"
#include "config.h"
#include "bots.h"
#include "servers.h"
#include "uplink.h"
#include "protocol.h"

#ifndef _WIN32
#include <sys/time.h>
#include <unistd.h>
#endif

static Anope::string GetTimeStamp() {
    char tbuf[256];
    time_t t;

    if (time(&t) < 0) {
        t = Anope::CurTime;
    }

    tm tm = *localtime(&t);
    if (Anope::Debug) {
        char *s;
        struct timeval tv;
        gettimeofday(&tv, NULL);
        strftime(tbuf, sizeof(tbuf) - 1, "[%b %d %H:%M:%S", &tm);
        s = tbuf + strlen(tbuf);
        s += snprintf(s, sizeof(tbuf) - (s - tbuf), ".%06d",
                      static_cast<int>(tv.tv_usec));
        strftime(s, sizeof(tbuf) - (s - tbuf) - 1, " %Y]", &tm);
    } else {
        strftime(tbuf, sizeof(tbuf) - 1, "[%b %d %H:%M:%S %Y]", &tm);
    }

    return tbuf;
}

static inline Anope::string CreateLogName(const Anope::string &file,
        time_t t = Anope::CurTime) {
    char timestamp[32];
    tm *tm = localtime(&t);
    strftime(timestamp, sizeof(timestamp), "%Y%m%d", tm);

    return Anope::LogDir + "/" + file + "." + timestamp;
}

LogFile::LogFile(const Anope::string &name) : filename(name),
    stream(name.c_str(), std::ios_base::out | std::ios_base::app) {
}

LogFile::~LogFile() {
    this->stream.close();
}

const Anope::string &LogFile::GetName() const {
    return this->filename;
}

Log::Log(LogType t, const Anope::string &cat, BotInfo *b) : bi(b), u(NULL),
    nc(NULL), c(NULL), source(NULL), chan(NULL), ci(NULL), s(NULL), m(NULL),
    type(t), category(cat) {
}

Log::Log(LogType t, CommandSource &src, Command *_c,
         ChannelInfo *_ci) : u(src.GetUser()), nc(src.nc), c(_c), source(&src),
    chan(NULL), ci(_ci), s(NULL), m(NULL), type(t) {
    if (!c) {
        throw CoreException("Invalid pointers passed to Log::Log");
    }

    if (type != LOG_COMMAND && type != LOG_OVERRIDE && type != LOG_ADMIN) {
        throw CoreException("This constructor does not support this log type");
    }

    size_t sl = c->name.find('/');
    this->bi = NULL;
    if (sl != Anope::string::npos) {
        this->bi = BotInfo::Find(c->name.substr(0, sl), true);
    }
    this->category = c->name;
}

Log::Log(User *_u, Channel *ch, const Anope::string &cat) : bi(NULL), u(_u),
    nc(NULL), c(NULL), source(NULL), chan(ch), ci(chan ? *chan->ci : NULL), s(NULL),
    m(NULL), type(LOG_CHANNEL), category(cat) {
    if (!chan) {
        throw CoreException("Invalid pointers passed to Log::Log");
    }
}

Log::Log(User *_u, const Anope::string &cat, BotInfo *_bi) : bi(_bi), u(_u),
    nc(NULL), c(NULL), source(NULL), chan(NULL), ci(NULL), s(NULL), m(NULL),
    type(LOG_USER), category(cat) {
    if (!u) {
        throw CoreException("Invalid pointers passed to Log::Log");
    }
}

Log::Log(Server *serv, const Anope::string &cat, BotInfo *_bi) : bi(_bi),
    u(NULL), nc(NULL), c(NULL), source(NULL), chan(NULL), ci(NULL), s(serv),
    m(NULL), type(LOG_SERVER), category(cat) {
    if (!s) {
        throw CoreException("Invalid pointer passed to Log::Log");
    }
}

Log::Log(BotInfo *b, const Anope::string &cat) : bi(b), u(NULL), nc(NULL),
    c(NULL), source(NULL), chan(NULL), ci(NULL), s(NULL), m(NULL), type(LOG_NORMAL),
    category(cat) {
}

Log::Log(Module *mod, const Anope::string &cat, BotInfo *_bi) : bi(_bi),
    u(NULL), nc(NULL), c(NULL), source(NULL), chan(NULL), ci(NULL), s(NULL), m(mod),
    type(LOG_MODULE), category(cat) {
}

Log::~Log() {
    if (Anope::NoFork && Anope::Debug && this->type >= LOG_NORMAL
            && this->type <= LOG_DEBUG + Anope::Debug - 1) {
        std::cout << GetTimeStamp() << " Debug: " << this->BuildPrefix() <<
                  this->buf.str() << std::endl;
    } else if (Anope::NoFork && this->type <= LOG_TERMINAL) {
        std::cout << GetTimeStamp() << " " << this->BuildPrefix() << this->buf.str() <<
                  std::endl;
    } else if (this->type == LOG_TERMINAL) {
        std::cout << this->BuildPrefix() << this->buf.str() << std::endl;
    }

    FOREACH_MOD(OnLog, (this));

    if (Config)
        for (unsigned i = 0; i < Config->LogInfos.size(); ++i)
            if (Config->LogInfos[i].HasType(this->type, this->category)) {
                Config->LogInfos[i].ProcessMessage(this);
            }
}

Anope::string Log::FormatSource() const {
    if (u)
        if (nc) {
            return this->u->GetMask() + " (" + this->nc->display + ")";
        } else {
            return this->u->GetMask();
        } else if (nc) {
        return nc->display;
    } else if (source) {
        Anope::string nickbuf = source->GetNick();
        if (!nickbuf.empty() && !source->ip.empty()) {
            nickbuf += " (" + source->ip + ")";
        }
        return nickbuf;
    }
    return "";
}

Anope::string Log::FormatCommand() const {
    Anope::string buffer = FormatSource() + " used " + (source != NULL
                           && !source->command.empty() ? source->command : this->c->name) + " ";
    if (this->ci) {
        buffer += "on " + this->ci->name + " ";
    }

    return buffer;
}

Anope::string Log::BuildPrefix() const {
    Anope::string buffer;

    switch (this->type) {
    case LOG_ADMIN: {
        if (!this->c) {
            break;
        }
        buffer += "ADMIN: " + FormatCommand();
        break;
    }
    case LOG_OVERRIDE: {
        if (!this->c) {
            break;
        }
        buffer += "OVERRIDE: " + FormatCommand();
        break;
    }
    case LOG_COMMAND: {
        if (!this->c) {
            break;
        }
        buffer += "COMMAND: " + FormatCommand();
        break;
    }
    case LOG_CHANNEL: {
        if (!this->chan) {
            break;
        }
        buffer += "CHANNEL: ";
        Anope::string src = FormatSource();
        if (!src.empty()) {
            buffer += src + " ";
        }
        buffer += this->category + " " + this->chan->name + " ";
        break;
    }
    case LOG_USER: {
        if (this->u) {
            buffer += "USERS: " + FormatSource() + " ";
        }
        break;
    }
    case LOG_SERVER: {
        if (this->s) {
            buffer += "SERVER: " + this->s->GetName() + " (" + this->s->GetDescription() +
                      ") ";
        }
        break;
    }
    case LOG_MODULE: {
        if (this->m) {
            buffer += this->m->name.upper() + ": ";
        }
        break;
    }
    default:
        break;
    }

    return buffer;
}

LogInfo::LogInfo(int la, bool rio, bool ldebug) : bot(NULL), last_day(0),
    log_age(la), raw_io(rio), debug(ldebug) {
}

LogInfo::~LogInfo() {
    for (unsigned i = 0; i < this->logfiles.size(); ++i) {
        delete this->logfiles[i];
    }
    this->logfiles.clear();
}

bool LogInfo::HasType(LogType ltype, const Anope::string &type) const {
    const std::vector<Anope::string> *list = NULL;
    switch (ltype) {
    case LOG_ADMIN:
        list = &this->admin;
        break;
    case LOG_OVERRIDE:
        list = &this->override;
        break;
    case LOG_COMMAND:
        list = &this->commands;
        break;
    case LOG_SERVER:
        list = &this->servers;
        break;
    case LOG_CHANNEL:
        list = &this->channels;
        break;
    case LOG_USER:
        list = &this->users;
        break;
    case LOG_TERMINAL:
        return true;
    case LOG_RAWIO:
        return (Anope::Debug || this->debug) ? true : this->raw_io;
    case LOG_DEBUG:
        return Anope::Debug ? true : this->debug;
    case LOG_DEBUG_2:
    case LOG_DEBUG_3:
    case LOG_DEBUG_4:
        break;
    case LOG_MODULE:
    case LOG_NORMAL:
    default:
        list = &this->normal;
        break;
    }

    if (list == NULL) {
        return false;
    }

    for (unsigned i = 0; i < list->size(); ++i) {
        Anope::string cat = list->at(i);
        bool inverse = false;
        if (cat[0] == '~') {
            cat.erase(cat.begin());
            inverse = true;
        }
        if (Anope::Match(type, cat)) {
            return !inverse;
        }
    }

    return false;
}

void LogInfo::OpenLogFiles() {
    for (unsigned i = 0; i < this->logfiles.size(); ++i) {
        delete this->logfiles[i];
    }
    this->logfiles.clear();

    for (unsigned i = 0; i < this->targets.size(); ++i) {
        const Anope::string &target = this->targets[i];

        if (target.empty() || target[0] == '#' || target == "globops"
                || target.find(":") != Anope::string::npos) {
            continue;
        }

        LogFile *lf = new LogFile(CreateLogName(target));
        if (!lf->stream.is_open()) {
            Log() << "Unable to open logfile " << lf->GetName();
            delete lf;
        } else {
            this->logfiles.push_back(lf);
        }
    }
}

void LogInfo::ProcessMessage(const Log *l) {
    if (!this->sources.empty()) {
        bool log = false;
        for (unsigned i = 0; i < this->sources.size() && !log; ++i) {
            const Anope::string &src = this->sources[i];

            if (l->bi && src == l->bi->nick) {
                log = true;
            } else if (l->u && src == l->u->nick) {
                log = true;
            } else if (l->nc && src == l->nc->display) {
                log = true;
            } else if (l->ci && src == l->ci->name) {
                log = true;
            } else if (l->m && src == l->m->name) {
                log = true;
            } else if (l->s && src == l->s->GetName()) {
                log = true;
            }
        }
        if (!log) {
            return;
        }
    }

    const Anope::string &buffer = l->BuildPrefix() + l->buf.str();

    FOREACH_MOD(OnLogMessage, (this, l, buffer));

    for (unsigned i = 0; i < this->targets.size(); ++i) {
        const Anope::string &target = this->targets[i];

        if (!target.empty() && target[0] == '#') {
            if (UplinkSock && l->type <= LOG_NORMAL && Me && Me->IsSynced()) {
                Channel *c = Channel::Find(target);
                if (!c) {
                    continue;
                }

                BotInfo *bi = l->bi;
                if (!bi) {
                    bi = this->bot;
                }
                if (!bi) {
                    bi = c->ci->WhoSends();
                }
                if (bi) {
                    IRCD->SendPrivmsg(bi, c->name, "%s", buffer.c_str());
                }
            }
        } else if (target == "globops") {
            if (UplinkSock && l->type <= LOG_NORMAL && Me && Me->IsSynced()) {
                BotInfo *bi = l->bi;
                if (!bi) {
                    bi = this->bot;
                }
                if (bi) {
                    IRCD->SendGlobops(bi, "%s", buffer.c_str());
                }
            }
        }
    }

    tm *tm = localtime(&Anope::CurTime);
    if (tm->tm_mday != this->last_day) {
        this->last_day = tm->tm_mday;
        this->OpenLogFiles();

        if (this->log_age)
            for (unsigned i = 0; i < this->targets.size(); ++i) {
                const Anope::string &target = this->targets[i];

                if (target.empty() || target[0] == '#' || target == "globops"
                        || target.find(":") != Anope::string::npos) {
                    continue;
                }

                Anope::string oldlog = CreateLogName(target,
                                                     Anope::CurTime - 86400 * this->log_age);
                if (IsFile(oldlog)) {
                    unlink(oldlog.c_str());
                    Log(LOG_DEBUG) << "Deleted old logfile " << oldlog;
                }
            }
    }

    for (unsigned i = 0; i < this->logfiles.size(); ++i) {
        LogFile *lf = this->logfiles[i];
        lf->stream << GetTimeStamp() << " " << buffer << std::endl;
    }
}
