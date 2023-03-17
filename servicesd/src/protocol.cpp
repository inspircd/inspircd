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
#include "modules.h"
#include "protocol.h"
#include "users.h"
#include "servers.h"
#include "config.h"
#include "uplink.h"
#include "bots.h"
#include "channels.h"

IRCDProto *IRCD = NULL;

IRCDProto::IRCDProto(Module *creator, const Anope::string &p) : Service(creator,
            "IRCDProto", creator->name), proto_name(p) {
    DefaultPseudoclientModes = "+io";
    CanSVSNick = CanSVSJoin = CanSetVHost = CanSetVIdent = CanSNLine = CanSQLine =
            CanSQLineChannel
            = CanSZLine = CanSVSHold = CanSVSO = CanCertFP = RequiresID = AmbiguousID =
                                           false;
    MaxModes = 3;
    MaxLine = 512;

    if (IRCD == NULL) {
        IRCD = this;
    }
}

IRCDProto::~IRCDProto() {
    if (IRCD == this) {
        IRCD = NULL;
    }
}

const Anope::string &IRCDProto::GetProtocolName() {
    return this->proto_name;
}

static inline char nextID(int pos, Anope::string &buf) {
    char &c = buf[pos];
    if (c == 'Z') {
        c = '0';
    } else if (c != '9') {
        ++c;
    } else if (pos) {
        c = 'A';
    } else {
        c = '0';
    }
    return c;
}

Anope::string IRCDProto::UID_Retrieve() {
    if (!IRCD || !IRCD->RequiresID) {
        return "";
    }

    static Anope::string current_uid = "AAAAAA";

    do {
        int current_len = current_uid.length() - 1;
        while (current_len >= 0 && nextID(current_len--, current_uid) == 'A');
    } while (User::Find(Me->GetSID() + current_uid) != NULL);

    return Me->GetSID() + current_uid;
}

Anope::string IRCDProto::SID_Retrieve() {
    if (!IRCD || !IRCD->RequiresID) {
        return "";
    }

    static Anope::string current_sid =
        Config->GetBlock("serverinfo")->Get<const Anope::string>("id");
    if (current_sid.empty()) {
        current_sid = "00A";
    }

    do {
        int current_len = current_sid.length() - 1;
        while (current_len >= 0 && nextID(current_len--, current_sid) == 'A');
    } while (Server::Find(current_sid) != NULL);

    return current_sid;
}

void IRCDProto::SendKill(const MessageSource &source,
                         const Anope::string &target, const Anope::string &reason) {
    UplinkSocket::Message(source) << "KILL " << target << " :" << reason;
}

void IRCDProto::SendSVSKillInternal(const MessageSource &source, User *user,
                                    const Anope::string &buf) {
    UplinkSocket::Message(source) << "KILL " << user->GetUID() << " :" << buf;
}

void IRCDProto::SendModeInternal(const MessageSource &source,
                                 const Channel *dest, const Anope::string &buf) {
    UplinkSocket::Message(source) << "MODE " << dest->name << " " << buf;
}

void IRCDProto::SendModeInternal(const MessageSource &source, User *dest,
                                 const Anope::string &buf) {
    UplinkSocket::Message(source) << "MODE " << dest->GetUID() << " " << buf;
}

void IRCDProto::SendKickInternal(const MessageSource &source, const Channel *c,
                                 User *u, const Anope::string &r) {
    if (!r.empty()) {
        UplinkSocket::Message(source) << "KICK " << c->name << " " << u->GetUID() <<
                                      " :" << r;
    } else {
        UplinkSocket::Message(source) << "KICK " << c->name << " " << u->GetUID();
    }
}

void IRCDProto::SendNoticeInternal(const MessageSource &source,
                                   const Anope::string &dest, const Anope::string &msg) {
    UplinkSocket::Message(source) << "NOTICE " << dest << " :" << msg;
}

void IRCDProto::SendPrivmsgInternal(const MessageSource &source,
                                    const Anope::string &dest, const Anope::string &buf) {
    UplinkSocket::Message(source) << "PRIVMSG " << dest << " :" << buf;
}

void IRCDProto::SendQuitInternal(User *u, const Anope::string &buf) {
    if (!buf.empty()) {
        UplinkSocket::Message(u) << "QUIT :" << buf;
    } else {
        UplinkSocket::Message(u) << "QUIT";
    }
}

void IRCDProto::SendPartInternal(User *u, const Channel *chan,
                                 const Anope::string &buf) {
    if (!buf.empty()) {
        UplinkSocket::Message(u) << "PART " << chan->name << " :" << buf;
    } else {
        UplinkSocket::Message(u) << "PART " << chan->name;
    }
}

void IRCDProto::SendGlobopsInternal(const MessageSource &source,
                                    const Anope::string &buf) {
    UplinkSocket::Message(source) << "GLOBOPS :" << buf;
}

void IRCDProto::SendCTCPInternal(const MessageSource &source,
                                 const Anope::string &dest, const Anope::string &buf) {
    Anope::string s = Anope::NormalizeBuffer(buf);
    this->SendNoticeInternal(source, dest, "\1" + s + "\1");
}

void IRCDProto::SendNumericInternal(int numeric, const Anope::string &dest,
                                    const Anope::string &buf) {
    Anope::string n = stringify(numeric);
    if (numeric < 10) {
        n = "0" + n;
    }
    if (numeric < 100) {
        n = "0" + n;
    }
    UplinkSocket::Message(Me) << n << " " << dest << " " << buf;
}

void IRCDProto::SendTopic(const MessageSource &source, Channel *c) {
    UplinkSocket::Message(source) << "TOPIC " << c->name << " :" << c->topic;
}

void IRCDProto::SendSVSKill(const MessageSource &source, User *user,
                            const char *fmt, ...) {
    if (!user || !fmt) {
        return;
    }

    va_list args;
    char buf[BUFSIZE] = "";
    va_start(args, fmt);
    vsnprintf(buf, BUFSIZE - 1, fmt, args);
    va_end(args);
    SendSVSKillInternal(source, user, buf);
}

void IRCDProto::SendMode(const MessageSource &source, const Channel *dest,
                         const char *fmt, ...) {
    va_list args;
    char buf[BUFSIZE] = "";
    va_start(args, fmt);
    vsnprintf(buf, BUFSIZE - 1, fmt, args);
    va_end(args);
    SendModeInternal(source, dest, buf);
}

void IRCDProto::SendMode(const MessageSource &source, User *u, const char *fmt,
                         ...) {
    va_list args;
    char buf[BUFSIZE] = "";
    va_start(args, fmt);
    vsnprintf(buf, BUFSIZE - 1, fmt, args);
    va_end(args);
    SendModeInternal(source, u, buf);
}

void IRCDProto::SendKick(const MessageSource &source, const Channel *chan,
                         User *user, const char *fmt, ...) {
    if (!chan || !user) {
        return;
    }

    va_list args;
    char buf[BUFSIZE] = "";
    va_start(args, fmt);
    vsnprintf(buf, BUFSIZE - 1, fmt, args);
    va_end(args);
    SendKickInternal(source, chan, user, buf);
}

void IRCDProto::SendNotice(const MessageSource &source,
                           const Anope::string &dest, const char *fmt, ...) {
    va_list args;
    char buf[BUFSIZE] = "";
    va_start(args, fmt);
    vsnprintf(buf, BUFSIZE - 1, fmt, args);
    va_end(args);
    SendNoticeInternal(source, dest, buf);
}

void IRCDProto::SendAction(const MessageSource &source,
                           const Anope::string &dest, const char *fmt, ...) {
    va_list args;
    char buf[BUFSIZE] = "";
    va_start(args, fmt);
    vsnprintf(buf, BUFSIZE - 1, fmt, args);
    va_end(args);
    Anope::string actionbuf = Anope::string("\1ACTION ") + buf + '\1';
    SendPrivmsgInternal(source, dest, actionbuf);
}

void IRCDProto::SendPrivmsg(const MessageSource &source,
                            const Anope::string &dest, const char *fmt, ...) {
    va_list args;
    char buf[BUFSIZE] = "";
    va_start(args, fmt);
    vsnprintf(buf, BUFSIZE - 1, fmt, args);
    va_end(args);
    SendPrivmsgInternal(source, dest, buf);
}

void IRCDProto::SendQuit(User *u, const char *fmt, ...) {
    va_list args;
    char buf[BUFSIZE] = "";
    va_start(args, fmt);
    vsnprintf(buf, BUFSIZE - 1, fmt, args);
    va_end(args);
    SendQuitInternal(u, buf);
}

void IRCDProto::SendPing(const Anope::string &servname,
                         const Anope::string &who) {
    if (servname.empty()) {
        UplinkSocket::Message(Me) << "PING " << who;
    } else {
        UplinkSocket::Message(Me) << "PING " << servname << " " << who;
    }
}

/**
 * Send a PONG reply to a received PING.
 * servname should be left NULL to send a one param reply.
 * @param servname Daemon or client that is responding to the PING.
 * @param who Origin of the PING and destination of the PONG message.
 **/
void IRCDProto::SendPong(const Anope::string &servname,
                         const Anope::string &who) {
    if (servname.empty()) {
        UplinkSocket::Message(Me) << "PONG " << who;
    } else {
        UplinkSocket::Message(Me) << "PONG " << servname << " " << who;
    }
}

void IRCDProto::SendInvite(const MessageSource &source, const Channel *c,
                           User *u) {
    UplinkSocket::Message(source) << "INVITE " << u->GetUID() << " " << c->name;
}

void IRCDProto::SendPart(User *user, const Channel *chan, const char *fmt,
                         ...) {
    if (fmt) {
        va_list args;
        char buf[BUFSIZE] = "";
        va_start(args, fmt);
        vsnprintf(buf, BUFSIZE - 1, fmt, args);
        va_end(args);
        SendPartInternal(user, chan, buf);
    } else {
        SendPartInternal(user, chan, "");
    }
}

void IRCDProto::SendGlobops(const MessageSource &source, const char *fmt, ...) {
    va_list args;
    char buf[BUFSIZE] = "";
    va_start(args, fmt);
    vsnprintf(buf, BUFSIZE - 1, fmt, args);
    va_end(args);
    SendGlobopsInternal(source, buf);
}

void IRCDProto::SendSquit(Server *s, const Anope::string &message) {
    UplinkSocket::Message() << "SQUIT " << s->GetSID() << " :" << message;
}

void IRCDProto::SendNickChange(User *u, const Anope::string &newnick) {
    UplinkSocket::Message(u) << "NICK " << newnick << " " << Anope::CurTime;
}

void IRCDProto::SendForceNickChange(User *u, const Anope::string &newnick,
                                    time_t when) {
    UplinkSocket::Message() << "SVSNICK " << u->GetUID() << " " << newnick << " " <<
                            when;
}

void IRCDProto::SendCTCP(const MessageSource &source, const Anope::string &dest,
                         const char *fmt, ...) {
    va_list args;
    char buf[BUFSIZE] = "";
    va_start(args, fmt);
    vsnprintf(buf, BUFSIZE - 1, fmt, args);
    va_end(args);
    SendCTCPInternal(source, dest, buf);
}

void IRCDProto::SendNumeric(int numeric, const Anope::string &dest,
                            const char *fmt, ...) {
    va_list args;
    char buf[BUFSIZE] = "";
    va_start(args, fmt);
    vsnprintf(buf, BUFSIZE - 1, fmt, args);
    va_end(args);
    SendNumericInternal(numeric, dest, buf);
}

bool IRCDProto::IsNickValid(const Anope::string &nick) {
    /**
     * RFC: definition of a valid nick
     * nickname =  ( letter / special ) ( letter / digit / special / "-" )
     * letter   =  A-Z / a-z
     * digit    =  0-9
     * special  =  [, ], \, `, _, ^, {, |, }
     **/

    if (nick.empty()) {
        return false;
    }

    Anope::string special = "[]\\`_^{|}";

    for (unsigned i = 0; i < nick.length(); ++i)
        if (!(nick[i] >= 'A' && nick[i] <= 'Z') && !(nick[i] >= 'a' && nick[i] <= 'z')
                && special.find(nick[i]) == Anope::string::npos
                && (Config && Config->NickChars.find(nick[i]) == Anope::string::npos)
                && (!i || (!(nick[i] >= '0' && nick[i] <= '9') && nick[i] != '-'))) {
            return false;
        }

    return true;
}

bool IRCDProto::IsChannelValid(const Anope::string &chan) {
    if (chan.empty() || chan[0] != '#'
            || chan.length() > Config->GetBlock("networkinfo")->Get<unsigned>("chanlen")) {
        return false;
    }

    if (chan.find_first_of(" ,") != Anope::string::npos) {
        return false;
    }

    return true;
}

bool IRCDProto::IsIdentValid(const Anope::string &ident) {
    if (ident.empty()
            || ident.length() > Config->GetBlock("networkinfo")->Get<unsigned>("userlen")) {
        return false;
    }

    for (unsigned i = 0; i < ident.length(); ++i) {
        const char &c = ident[i];

        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')
                || c == '.' || c == '-') {
            continue;
        }

        return false;
    }

    return true;
}

bool IRCDProto::IsHostValid(const Anope::string &host) {
    if (host.empty()
            || host.length() > Config->GetBlock("networkinfo")->Get<unsigned>("hostlen")) {
        return false;
    }

    const Anope::string &vhostdisablebe =
        Config->GetBlock("networkinfo")->Get<const Anope::string>("disallow_start_or_end"),
        vhostchars =
            Config->GetBlock("networkinfo")->Get<const Anope::string>("vhost_chars");

    if (vhostdisablebe.find_first_of(host[0]) != Anope::string::npos) {
        return false;
    } else if (vhostdisablebe.find_first_of(host[host.length() - 1]) !=
               Anope::string::npos) {
        return false;
    }

    int dots = 0;
    for (unsigned i = 0; i < host.length(); ++i) {
        if (host[i] == '.') {
            ++dots;
        }
        if (vhostchars.find_first_of(host[i]) == Anope::string::npos) {
            return false;
        }
    }

    return dots > 0
           || Config->GetBlock("networkinfo")->Get<bool>("allow_undotted_vhosts");
}

void IRCDProto::SendOper(User *u) {
    SendNumericInternal(381, u->GetUID(),
                        ":You are now an IRC operator (set by services)");
    u->SetMode(NULL, "OPER");
}

unsigned IRCDProto::GetMaxListFor(Channel *c) {
    return c->HasMode("LBAN") ? 0 :
           Config->GetBlock("networkinfo")->Get<int>("modelistsize");
}

unsigned IRCDProto::GetMaxListFor(Channel *c, ChannelMode *cm) {
    return GetMaxListFor(c);
}

Anope::string IRCDProto::NormalizeMask(const Anope::string &mask) {
    if (IsExtbanValid(mask)) {
        return mask;
    }
    return Entry("", mask).GetNUHMask();
}

MessageSource::MessageSource(const Anope::string &src) : source(src), u(NULL),
    s(NULL) {
    /* no source for incoming message is our uplink */
    if (src.empty()) {
        this->s = Servers::GetUplink();
    } else if (IRCD->RequiresID || src.find('.') != Anope::string::npos) {
        this->s = Server::Find(src);
    }
    if (this->s == NULL) {
        this->u = User::Find(src);
    }
}

MessageSource::MessageSource(User *_u) : source(_u ? _u->nick : ""), u(_u),
    s(NULL) {
}

MessageSource::MessageSource(Server *_s) : source(_s ? _s->GetName() : ""),
    u(NULL), s(_s) {
}

const Anope::string &MessageSource::GetName() const {
    if (this->s) {
        return this->s->GetName();
    } else if (this->u) {
        return this->u->nick;
    } else {
        return this->source;
    }
}

const Anope::string &MessageSource::GetSource() const {
    return this->source;
}

User *MessageSource::GetUser() const {
    return this->u;
}

BotInfo *MessageSource::GetBot() const {
    return BotInfo::Find(this->GetName(), true);
}

Server *MessageSource::GetServer() const {
    return this->s;
}

IRCDMessage::IRCDMessage(Module *o, const Anope::string &n,
                         unsigned p) : Service(o, "IRCDMessage", o->name + "/" + n.lower()), name(n),
    param_count(p) {
}

unsigned IRCDMessage::GetParamCount() const {
    return this->param_count;
}

void IRCDMessage::Run(MessageSource &source,
                      const std::vector<Anope::string> &params,
                      const Anope::map<Anope::string> &tags) {
    // Most IRCds don't support message tags yet so use the tagless variant.
    Run(source, params);
}

