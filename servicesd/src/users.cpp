/* Routines to maintain a list of online users.
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
#include "users.h"
#include "account.h"
#include "protocol.h"
#include "servers.h"
#include "channels.h"
#include "bots.h"
#include "config.h"
#include "opertype.h"
#include "language.h"
#include "sockets.h"
#include "uplink.h"

user_map UserListByNick, UserListByUID;

int OperCount = 0;
unsigned MaxUserCount = 0;
time_t MaxUserTime = 0;

std::list<User *> User::quitting_users;

User::User(const Anope::string &snick, const Anope::string &sident,
           const Anope::string &shost, const Anope::string &svhost,
           const Anope::string &uip, Server *sserver, const Anope::string &srealname,
           time_t ts, const Anope::string &smodes, const Anope::string &suid,
           NickCore *account) : ip(uip) {
    if (snick.empty() || sident.empty() || shost.empty()) {
        throw CoreException("Bad args passed to User::User");
    }

    /* we used to do this by calloc, no more. */
    quit = false;
    server = NULL;
    invalid_pw_count = invalid_pw_time = lastmemosend = lastnickreg = lastmail = 0;
    on_access = false;

    this->nick = snick;
    this->ident = sident;
    this->host = shost;
    this->vhost = svhost;
    this->chost = svhost;
    this->server = sserver;
    this->realname = srealname;
    this->timestamp = this->signon = ts;
    this->SetModesInternal(sserver, "%s", smodes.c_str());
    this->uid = suid;
    this->super_admin = false;
    this->nc = NULL;

    size_t old = UserListByNick.size();
    UserListByNick[snick] = this;
    if (!suid.empty()) {
        UserListByUID[suid] = this;
    }
    if (old == UserListByNick.size()) {
        Log(LOG_DEBUG) << "Duplicate user " << snick << " in user table?";
    }

    this->Login(account);
    this->UpdateHost();

    if (sserver) { // Our bots are introduced on startup with no server
        ++sserver->users;
        if (server->IsSynced()) {
            Log(this, "connect") << (!vhost.empty()
                                     && vhost != host ? "(" + vhost + ") " : "") << "(" << srealname << ") " <<
                                 (!uip.empty()
                                  && uip != host ? "[" + uip + "] " : "") << "connected to the network (" <<
                                 sserver->GetName() << ")";
        }
    }

    if (UserListByNick.size() > MaxUserCount) {
        MaxUserCount = UserListByNick.size();
        MaxUserTime = Anope::CurTime;
        if (sserver && sserver->IsSynced()) {
            Log(this, "maxusers") << "connected - new maximum user count: " <<
                                  UserListByNick.size();
        }
    }

    bool exempt = false;
    if (server && server->IsULined()) {
        exempt = true;
    }
    FOREACH_MOD(OnUserConnect, (this, exempt));
}

static void CollideKill(User *target, const Anope::string &reason) {
    if (target->server != Me) {
        target->Kill(Me, reason);
    } else {
        // Be sure my user is really dead
        IRCD->SendQuit(target, "%s", reason.c_str());

        // Reintroduce my client
        if (BotInfo *bi = dynamic_cast<BotInfo *>(target)) {
            bi->OnKill();
        } else {
            target->Quit(reason);
        }
    }
}

static void Collide(User *u, const Anope::string &id,
                    const Anope::string &type) {
    // Kill incoming user
    IRCD->SendKill(Me, id, type);
    // Quit colliding user
    CollideKill(u, type);
}

User* User::OnIntroduce(const Anope::string &snick, const Anope::string &sident,
                        const Anope::string &shost, const Anope::string &svhost,
                        const Anope::string &sip, Server *sserver, const Anope::string &srealname,
                        time_t ts, const Anope::string &smodes, const Anope::string &suid,
                        NickCore *nc) {
    // How IRCds handle collisions varies a lot, for safety well just always kill both sides
    // With properly set qlines, this can almost never happen anyway

    User *u = User::Find(snick, true);
    if (u) {
        Collide(u, !suid.empty() ? suid : snick, "Nick collision");
        return NULL;
    }

    if (!suid.empty()) {
        u = User::Find(suid);
        if (u) {
            Collide(u, suid, "ID collision");
            return NULL;
        }
    }

    return new User(snick, sident, shost, svhost, sip, sserver, srealname, ts,
                    smodes, suid, nc);
}

void User::ChangeNick(const Anope::string &newnick, time_t ts) {
    /* Sanity check to make sure we don't segfault */
    if (newnick.empty()) {
        throw CoreException("User::ChangeNick() got a bad argument");
    }

    this->super_admin = false;
    Log(this, "nick") << "(" << this->realname << ") changed nick to " << newnick;

    Anope::string old = this->nick;
    this->timestamp = ts;

    if (this->nick.equals_ci(newnick)) {
        this->nick = newnick;
    } else {
        NickAlias *old_na = NickAlias::Find(this->nick);
        if (old_na && (this->IsIdentified(true) || this->IsRecognized())) {
            old_na->last_seen = Anope::CurTime;
        }

        UserListByNick.erase(this->nick);

        this->nick = newnick;

        User* &other = UserListByNick[this->nick];
        if (other) {
            CollideKill(this, "Nick collision");
            CollideKill(other, "Nick collision");
            return;
        }
        other = this;

        on_access = false;
        NickAlias *na = NickAlias::Find(this->nick);
        if (na) {
            on_access = na->nc->IsOnAccess(this);
        }

        if (na && na->nc == this->Account()) {
            na->last_seen = Anope::CurTime;
            this->UpdateHost();
        }
    }

    FOREACH_MOD(OnUserNickChange, (this, old));
}

void User::SetDisplayedHost(const Anope::string &shost) {
    if (shost.empty()) {
        throw CoreException("empty host? in MY services? it seems it's more likely than I thought.");
    }

    this->vhost = shost;

    Log(this, "host") << "changed vhost to " << shost;

    FOREACH_MOD(OnSetDisplayedHost, (this));

    this->UpdateHost();
}

const Anope::string &User::GetDisplayedHost() const {
    if (!this->vhost.empty()) {
        return this->vhost;
    } else if (this->HasMode("CLOAK") && !this->GetCloakedHost().empty()) {
        return this->GetCloakedHost();
    } else {
        return this->host;
    }
}

void User::SetCloakedHost(const Anope::string &newhost) {
    if (newhost.empty()) {
        throw CoreException("empty host in User::SetCloakedHost");
    }

    chost = newhost;

    Log(this, "host") << "changed cloaked host to " << newhost;

    this->UpdateHost();
}

const Anope::string &User::GetCloakedHost() const {
    return chost;
}

const Anope::string &User::GetUID() const {
    if (!this->uid.empty() && IRCD->RequiresID) {
        return this->uid;
    } else {
        return this->nick;
    }
}

void User::SetVIdent(const Anope::string &sident) {
    this->vident = sident;

    Log(this, "ident") << "changed vident to " << sident;

    this->UpdateHost();
}

const Anope::string &User::GetVIdent() const {
    if (!this->vident.empty()) {
        return this->vident;
    } else {
        return this->ident;
    }
}

void User::SetIdent(const Anope::string &sident) {
    this->ident = sident;

    Log(this, "ident") << "changed real ident to " << sident;

    this->UpdateHost();
}

const Anope::string &User::GetIdent() const {
    return this->ident;
}

Anope::string User::GetMask() const {
    return this->nick + "!" + this->ident + "@" + this->host;
}

Anope::string User::GetDisplayedMask() const {
    return this->nick + "!" + this->GetVIdent() + "@" + this->GetDisplayedHost();
}

void User::SetRealname(const Anope::string &srealname) {
    if (srealname.empty()) {
        throw CoreException("realname empty in SetRealname");
    }

    this->realname = srealname;
    NickAlias *na = NickAlias::Find(this->nick);

    if (na && (this->IsIdentified(true) || this->IsRecognized())) {
        na->last_realname = srealname;
    }

    Log(this, "realname") << "changed realname to " << srealname;
}

User::~User() {
    UnsetExtensibles();

    if (this->server != NULL) {
        if (this->server->IsSynced()) {
            Log(this, "disconnect") << "(" << this->realname <<
                                    ") disconnected from the network (" << this->server->GetName() << ")";
        }
        --this->server->users;
    }

    FOREACH_MOD(OnPreUserLogoff, (this));

    ModeManager::StackerDel(this);
    this->Logout();

    if (this->HasMode("OPER")) {
        --OperCount;
    }

    while (!this->chans.empty()) {
        this->chans.begin()->second->chan->DeleteUser(this);
    }

    UserListByNick.erase(this->nick);
    if (!this->uid.empty()) {
        UserListByUID.erase(this->uid);
    }

    FOREACH_MOD(OnPostUserLogoff, (this));
}

void User::SendMessage(BotInfo *source, const char *fmt, ...) {
    va_list args;
    char buf[BUFSIZE] = "";

    const char *translated_message = Language::Translate(this, fmt);

    va_start(args, fmt);
    vsnprintf(buf, BUFSIZE - 1, translated_message, args);

    this->SendMessage(source, Anope::string(buf));

    va_end(args);
}

void User::SendMessage(BotInfo *source, const Anope::string &msg) {
    const char *translated_message = Language::Translate(this, msg.c_str());

    /* Send privmsg instead of notice if:
    * - UsePrivmsg is enabled
    * - The user is not registered and NSDefMsg is enabled
    * - The user is registered and has set /ns set msg on
    */
    bool send_privmsg = Config->UsePrivmsg && ((!this->nc && Config->DefPrivmsg)
                        || (this->nc && this->nc->HasExt("MSG")));
    sepstream sep(translated_message, '\n', true);
    for (Anope::string tok; sep.GetToken(tok);) {
        if (send_privmsg) {
            IRCD->SendPrivmsg(source, this->GetUID(), "%s", tok.c_str());
        } else {
            IRCD->SendNotice(source, this->GetUID(), "%s", tok.c_str());
        }
    }
}

void User::Identify(NickAlias *na) {
    if (this->nick.equals_ci(na->nick)) {
        na->last_usermask = this->GetIdent() + "@" + this->GetDisplayedHost();
        na->last_realhost = this->GetIdent() + "@" + this->host;
        na->last_realname = this->realname;
        na->last_seen = Anope::CurTime;
    }

    IRCD->SendLogin(this, na);

    this->Login(na->nc);

    FOREACH_MOD(OnNickIdentify, (this));

    if (this->IsServicesOper()) {
        if (!this->nc->o->ot->modes.empty()) {
            this->SetModes(NULL, "%s", this->nc->o->ot->modes.c_str());
            this->SendMessage(NULL, "Changing your usermodes to \002%s\002",
                              this->nc->o->ot->modes.c_str());
            UserMode *um = ModeManager::FindUserModeByName("OPER");
            if (um && !this->HasMode("OPER")
                    && this->nc->o->ot->modes.find(um->mchar) != Anope::string::npos) {
                IRCD->SendOper(this);
            }
        }
        if (IRCD->CanSetVHost && !this->nc->o->vhost.empty()) {
            this->SendMessage(NULL, "Changing your vhost to \002%s\002",
                              this->nc->o->vhost.c_str());
            this->SetDisplayedHost(this->nc->o->vhost);
            IRCD->SendVhost(this, "", this->nc->o->vhost);
        }
    }
}


void User::Login(NickCore *core) {
    if (!core || core == this->nc) {
        return;
    }

    this->Logout();
    this->nc = core;
    core->users.push_back(this);

    this->UpdateHost();

    if (this->server->IsSynced()) {
        Log(this, "account") << "is now identified as " << this->nc->display;
    }

    FOREACH_MOD(OnUserLogin, (this));
}

void User::Logout() {
    if (!this->nc) {
        return;
    }

    Log(this, "account") << "is no longer identified as " << this->nc->display;

    std::list<User *>::iterator it = std::find(this->nc->users.begin(),
                                     this->nc->users.end(), this);
    if (it != this->nc->users.end()) {
        this->nc->users.erase(it);
    }

    this->nc = NULL;
}

NickCore *User::Account() const {
    return this->nc;
}

bool User::IsIdentified(bool check_nick) const {
    if (check_nick && this->nc) {
        NickAlias *na = NickAlias::Find(this->nick);
        return na && *na->nc == *this->nc;
    }

    return this->nc ? true : false;
}

bool User::IsRecognized(bool check_secure) const {
    if (check_secure && on_access) {
        const NickAlias *na = NickAlias::Find(this->nick);

        if (!na || na->nc->HasExt("NS_SECURE")) {
            return false;
        }
    }

    return on_access;
}

bool User::IsServicesOper() {
    if (!this->nc || !this->nc->IsServicesOper())
        // No opertype.
    {
        return false;
    } else if (this->nc->o->require_oper && !this->HasMode("OPER")) {
        return false;
    } else if (!this->nc->o->certfp.empty()
               && this->fingerprint != this->nc->o->certfp)
        // Certfp mismatch
    {
        return false;
    } else if (!this->nc->o->hosts.empty()) {
        bool match = false;
        Anope::string match_host = this->GetIdent() + "@" + this->host;
        for (unsigned i = 0; i < this->nc->o->hosts.size(); ++i)
            if (Anope::Match(match_host, this->nc->o->hosts[i])) {
                match = true;
            }
        if (match == false) {
            return false;
        }
    }

    EventReturn MOD_RESULT;
    FOREACH_RESULT(IsServicesOper, MOD_RESULT, (this));
    if (MOD_RESULT == EVENT_STOP) {
        return false;
    }

    return true;
}

bool User::HasCommand(const Anope::string &command) {
    if (this->IsServicesOper()) {
        return this->nc->o->ot->HasCommand(command);
    }
    return false;
}

bool User::HasPriv(const Anope::string &priv) {
    if (this->IsServicesOper()) {
        return this->nc->o->ot->HasPriv(priv);
    }
    return false;
}

void User::UpdateHost() {
    if (this->host.empty()) {
        return;
    }

    NickAlias *na = NickAlias::Find(this->nick);
    on_access = false;
    if (na) {
        on_access = na->nc->IsOnAccess(this);
    }

    if (na && (this->IsIdentified(true) || this->IsRecognized())) {
        Anope::string last_usermask = this->GetIdent() + "@" + this->GetDisplayedHost();
        Anope::string last_realhost = this->GetIdent() + "@" + this->host;
        na->last_usermask = last_usermask;
        na->last_realhost = last_realhost;
        // This is called on signon, and if users are introduced with an account it won't update
        na->last_realname = this->realname;
    }
}

bool User::HasMode(const Anope::string &mname) const {
    return this->modes.count(mname);
}

void User::SetModeInternal(const MessageSource &source, UserMode *um,
                           const Anope::string &param) {
    if (!um) {
        return;
    }

    this->modes[um->name] = param;

    if (um->name == "OPER") {
        ++OperCount;

        if (this->IsServicesOper()) {
            if (!this->nc->o->ot->modes.empty()) {
                this->SetModes(NULL, "%s", this->nc->o->ot->modes.c_str());
                this->SendMessage(NULL, "Changing your usermodes to \002%s\002",
                                  this->nc->o->ot->modes.c_str());
                UserMode *oper = ModeManager::FindUserModeByName("OPER");
                if (oper && !this->HasMode("OPER")
                        && this->nc->o->ot->modes.find(oper->mchar) != Anope::string::npos) {
                    IRCD->SendOper(this);
                }
            }
            if (IRCD->CanSetVHost && !this->nc->o->vhost.empty()) {
                this->SendMessage(NULL, "Changing your vhost to \002%s\002",
                                  this->nc->o->vhost.c_str());
                this->SetDisplayedHost(this->nc->o->vhost);
                IRCD->SendVhost(this, "", this->nc->o->vhost);
            }
        }
    }

    if (um->name == "CLOAK" || um->name == "VHOST") {
        this->UpdateHost();
    }

    FOREACH_MOD(OnUserModeSet, (source, this, um->name));
}

void User::RemoveModeInternal(const MessageSource &source, UserMode *um) {
    if (!um) {
        return;
    }

    this->modes.erase(um->name);

    if (um->name == "OPER") {
        --OperCount;

        // Don't let people de-oper and remain a SuperAdmin
        this->super_admin = false;
    }

    if (um->name == "CLOAK" || um->name == "VHOST") {
        this->vhost.clear();
        this->UpdateHost();
    }

    FOREACH_MOD(OnUserModeUnset, (source, this, um->name));
}

void User::SetMode(BotInfo *bi, UserMode *um, const Anope::string &param) {
    if (!um || HasMode(um->name)) {
        return;
    }

    ModeManager::StackerAdd(bi, this, um, true, param);
    SetModeInternal(bi, um, param);
}

void User::SetMode(BotInfo *bi, const Anope::string &uname,
                   const Anope::string &param) {
    SetMode(bi, ModeManager::FindUserModeByName(uname), param);
}

void User::RemoveMode(BotInfo *bi, UserMode *um, const Anope::string &param) {
    if (!um || !HasMode(um->name)) {
        return;
    }

    ModeManager::StackerAdd(bi, this, um, false, param);
    RemoveModeInternal(bi, um);
}

void User::RemoveMode(BotInfo *bi, const Anope::string &name,
                      const Anope::string &param) {
    RemoveMode(bi, ModeManager::FindUserModeByName(name), param);
}

void User::SetModes(BotInfo *bi, const char *umodes, ...) {
    char buf[BUFSIZE] = "";
    va_list args;
    Anope::string modebuf, sbuf;
    int add = -1;
    va_start(args, umodes);
    vsnprintf(buf, BUFSIZE - 1, umodes, args);
    va_end(args);

    spacesepstream sep(buf);
    sep.GetToken(modebuf);
    for (unsigned i = 0, end = modebuf.length(); i < end; ++i) {
        UserMode *um;

        switch (modebuf[i]) {
        case '+':
            add = 1;
            continue;
        case '-':
            add = 0;
            continue;
        default:
            if (add == -1) {
                continue;
            }
            um = ModeManager::FindUserModeByChar(modebuf[i]);
            if (!um) {
                continue;
            }
        }

        if (add) {
            if (um->type == MODE_PARAM && sep.GetToken(sbuf)) {
                this->SetMode(bi, um, sbuf);
            } else {
                this->SetMode(bi, um);
            }
        } else {
            this->RemoveMode(bi, um);
        }
    }
}

void User::SetModesInternal(const MessageSource &source, const char *umodes,
                            ...) {
    char buf[BUFSIZE] = "";
    va_list args;
    Anope::string modebuf, sbuf;
    int add = -1;
    va_start(args, umodes);
    vsnprintf(buf, BUFSIZE - 1, umodes, args);
    va_end(args);

    if (this->server && this->server->IsSynced() && Anope::string(buf) != "+") {
        Log(this, "mode") << "changes modes to " << buf;
    }

    spacesepstream sep(buf);
    sep.GetToken(modebuf);
    for (unsigned i = 0, end = modebuf.length(); i < end; ++i) {
        UserMode *um;

        switch (modebuf[i]) {
        case '+':
            add = 1;
            continue;
        case '-':
            add = 0;
            continue;
        default:
            if (add == -1) {
                continue;
            }
            um = ModeManager::FindUserModeByChar(modebuf[i]);
            if (!um) {
                continue;
            }
        }

        if (add) {
            if (um->type == MODE_PARAM && sep.GetToken(sbuf)) {
                this->SetModeInternal(source, um, sbuf);
            } else {
                this->SetModeInternal(source, um);
            }
        } else {
            this->RemoveModeInternal(source, um);
        }
    }
}

Anope::string User::GetModes() const {
    Anope::string m, params;

    for (ModeList::const_iterator it = this->modes.begin(),
            it_end = this->modes.end(); it != it_end; ++it) {
        UserMode *um = ModeManager::FindUserModeByName(it->first);
        if (um == NULL) {
            continue;
        }

        m += um->mchar;

        if (!it->second.empty()) {
            params += " " + it->second;
        }
    }

    return m + params;
}

const User::ModeList &User::GetModeList() const {
    return modes;
}

ChanUserContainer *User::FindChannel(Channel *c) const {
    User::ChanUserList::const_iterator it = this->chans.find(c);
    if (it != this->chans.end()) {
        return it->second;
    }
    return NULL;
}

bool User::IsProtected() {
    return this->HasMode("PROTECTED") || this->HasMode("GOD")
           || this->HasPriv("protected") || (this->server && this->server->IsULined());
}

void User::Kill(const MessageSource &source, const Anope::string &reason) {
    Anope::string real_reason = source.GetName() + " (" + reason + ")";

    IRCD->SendSVSKill(source, this, "%s", real_reason.c_str());
}

void User::KillInternal(const MessageSource &source,
                        const Anope::string &reason) {
    if (this->quit) {
        Log(LOG_DEBUG) << "Duplicate quit for " << this->nick;
        return;
    }

    Log(this, "killed") << "was killed by " << source.GetName() << " (Reason: " <<
                        reason << ")";

    this->Quit(reason);
}

void User::Quit(const Anope::string &reason) {
    if (this->quit) {
        Log(LOG_DEBUG) << "Duplicate quit for " << this->nick;
        return;
    }

    FOREACH_MOD(OnUserQuit, (this, reason));

    this->quit = true;
    quitting_users.push_back(this);
}

bool User::Quitting() const {
    return this->quit;
}

Anope::string User::Mask() const {
    Anope::string mask;
    const Anope::string &mident = this->GetIdent();
    const Anope::string &mhost = this->GetDisplayedHost();

    if (mident[0] == '~') {
        mask = "*" + mident + "@";
    } else {
        mask = mident + "@";
    }

    sockaddrs addr(mhost);
    if (addr.valid() && addr.sa.sa_family == AF_INET) {
        size_t dot = mhost.rfind('.');
        mask += mhost.substr(0, dot) + (dot == Anope::string::npos ? "" : ".*");
    } else {
        size_t dot = mhost.find('.');
        if (dot != Anope::string::npos
                && mhost.find('.', dot + 1) != Anope::string::npos) {
            mask += "*" + mhost.substr(dot);
        } else {
            mask += mhost;
        }
    }

    return mask;
}

bool User::BadPassword() {
    if (!Config->GetBlock("options")->Get<int>("badpasslimit")) {
        return false;
    }

    if (Config->GetBlock("options")->Get<time_t>("badpasstimeout") > 0
            && this->invalid_pw_time > 0
            && this->invalid_pw_time < Anope::CurTime -
            Config->GetBlock("options")->Get<time_t>("badpasstimeout")) {
        this->invalid_pw_count = 0;
    }
    ++this->invalid_pw_count;
    this->invalid_pw_time = Anope::CurTime;
    if (this->invalid_pw_count >=
            Config->GetBlock("options")->Get<int>("badpasslimit")) {
        this->Kill(Me, "Too many invalid passwords");
        return true;
    }

    return false;
}

User* User::Find(const Anope::string &name, bool nick_only) {
    if (!nick_only && IRCD && IRCD->RequiresID) {
        user_map::iterator it = UserListByUID.find(name);
        if (it != UserListByUID.end()) {
            return it->second;
        }

        if (IRCD->AmbiguousID) {
            return NULL;
        }
    }

    user_map::iterator it = UserListByNick.find(name);
    if (it != UserListByNick.end()) {
        return it->second;
    }

    return NULL;
}

void User::QuitUsers() {
    for (std::list<User *>::iterator it = quitting_users.begin(),
            it_end = quitting_users.end(); it != it_end; ++it) {
        delete *it;
    }
    quitting_users.clear();
}
