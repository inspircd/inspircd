/* NickServ core functions
 *
 * (C) 2003-2023 Anope Team
 * Contact us at team@anope.org
 *
 * Please read COPYING and README for further details.
 *
 * Based on the original code of Epona by Lara.
 * Based on the original code of Services by Andy Church.
 */

#include "module.h"

class NickServCollide;
static std::set<NickServCollide *> collides;

/** Timer for colliding nicks to force people off of nicknames
 */
class NickServCollide : public Timer {
    NickServService *service;
    Reference<User> u;
    time_t ts;
    Reference<NickAlias> na;

  public:
    NickServCollide(Module *me, NickServService *nss, User *user, NickAlias *nick,
                    time_t delay) : Timer(me, delay), service(nss), u(user), ts(user->timestamp),
        na(nick) {
        collides.insert(this);
    }

    ~NickServCollide() {
        collides.erase(this);
    }

    User *GetUser() {
        return u;
    }

    NickAlias *GetNick() {
        return na;
    }

    void Tick(time_t t) anope_override {
        if (!u || !na) {
            return;
        }
        /* If they identified or don't exist anymore, don't kill them. */
        if (u->Account() == na->nc || u->timestamp > ts) {
            return;
        }

        service->Collide(u, na);
    }
};

/** Timer for removing HELD status from nicks.
 */
class NickServHeld : public Timer {
    Reference<NickAlias> na;
    Anope::string nick;
  public:
    NickServHeld(Module *me, NickAlias *n, long l) : Timer(me, l), na(n),
        nick(na->nick) {
        n->Extend<bool>("HELD");
    }

    void Tick(time_t) {
        if (na) {
            na->Shrink<bool>("HELD");
        }
    }
};

class NickServRelease;
static Anope::map<NickServRelease *> NickServReleases;

/** Timer for releasing nicks to be available for use
 */
class NickServRelease : public User, public Timer {
    Anope::string nick;

  public:
    NickServRelease(Module *me, NickAlias *na, time_t delay) : User(na->nick,
                Config->GetModule("nickserv")->Get<const Anope::string>("enforceruser", "user"),
                Config->GetModule("nickserv")->Get<const Anope::string>("enforcerhost",
                        Me->GetName()), "", "", Me, "Services Enforcer", Anope::CurTime, "",
                IRCD->UID_Retrieve(), NULL), Timer(me, delay), nick(na->nick) {
        /* Erase the current release timer and use the new one */
        Anope::map<NickServRelease *>::iterator nit = NickServReleases.find(this->nick);
        if (nit != NickServReleases.end()) {
            IRCD->SendQuit(nit->second, "");
            delete nit->second;
        }

        NickServReleases.insert(std::make_pair(this->nick, this));

        IRCD->SendClientIntroduction(this);
    }

    ~NickServRelease() {
        IRCD->SendQuit(this, "");
        NickServReleases.erase(this->nick);
    }

    void Tick(time_t t) anope_override { }
};

class NickServCore : public Module, public NickServService {
    Reference<BotInfo> NickServ;
    std::vector<Anope::string> defaults;
    ExtensibleItem<bool> held, collided;

    void OnCancel(User *u, NickAlias *na) {
        if (collided.HasExt(na)) {
            collided.Unset(na);

            new NickServHeld(this, na,
                             Config->GetModule("nickserv")->Get<time_t>("releasetimeout", "1m"));

            if (IRCD->CanSVSHold) {
                IRCD->SendSVSHold(na->nick,
                                  Config->GetModule("nickserv")->Get<time_t>("releasetimeout", "1m"));
            } else {
                new NickServRelease(this, na,
                                    Config->GetModule("nickserv")->Get<time_t>("releasetimeout", "1m"));
            }
        }
    }

  public:
    NickServCore(const Anope::string &modname,
                 const Anope::string &creator) : Module(modname, creator, PSEUDOCLIENT | VENDOR),
        NickServService(this), held(this, "HELD"), collided(this, "COLLIDED") {
    }

    ~NickServCore() {
        OnShutdown();
    }

    void OnShutdown() anope_override {
        /* On shutdown, restart, or mod unload, remove all of our holds for nicks (svshold or qlines)
         * because some IRCds do not allow us to have these automatically expire
         */
        for (nickalias_map::const_iterator it = NickAliasList->begin(); it != NickAliasList->end(); ++it) {
            this->Release(it->second);
        }
    }

    void OnRestart() anope_override {
        OnShutdown();
    }

    void Validate(User *u) anope_override {
        NickAlias *na = NickAlias::Find(u->nick);
        if (!na) {
            return;
        }

        EventReturn MOD_RESULT;
        FOREACH_RESULT(OnNickValidate, MOD_RESULT, (u, na));
        if (MOD_RESULT == EVENT_STOP) {
            this->Collide(u, na);
            return;
        } else if (MOD_RESULT == EVENT_ALLOW) {
            return;
        }

        if (!na->nc->HasExt("NS_SECURE") && u->IsRecognized()) {
            na->last_seen = Anope::CurTime;
            na->last_usermask = u->GetIdent() + "@" + u->GetDisplayedHost();
            na->last_realname = u->realname;
            return;
        }

        if (Config->GetModule("nickserv")->Get<bool>("nonicknameownership")) {
            return;
        }

        bool on_access = u->IsRecognized(false);

        if (on_access || !na->nc->HasExt("KILL_IMMED")) {
            if (na->nc->HasExt("NS_SECURE")) {
                u->SendMessage(NickServ, NICK_IS_SECURE, Config->StrictPrivmsg.c_str(),
                               NickServ->nick.c_str());
            } else {
                u->SendMessage(NickServ, NICK_IS_REGISTERED, Config->StrictPrivmsg.c_str(),
                               NickServ->nick.c_str());
            }
        }
        if (na->nc->HasExt("KILLPROTECT") && !on_access) {
            if (na->nc->HasExt("KILL_IMMED")) {
                u->SendMessage(NickServ, FORCENICKCHANGE_NOW);
                this->Collide(u, na);
            } else if (na->nc->HasExt("KILL_QUICK")) {
                time_t killquick = Config->GetModule("nickserv")->Get<time_t>("killquick",
                                   "20s");
                u->SendMessage(NickServ,
                               _("If you do not change within %s, I will change your nick."),
                               Anope::Duration(killquick, u->Account()).c_str());
                new NickServCollide(this, this, u, na, killquick);
            } else {
                time_t kill = Config->GetModule("nickserv")->Get<time_t>("kill", "60s");
                u->SendMessage(NickServ,
                               _("If you do not change within %s, I will change your nick."),
                               Anope::Duration(kill, u->Account()).c_str());
                new NickServCollide(this, this, u, na, kill);
            }
        }

    }

    void OnUserLogin(User *u) anope_override {
        NickAlias *na = NickAlias::Find(u->nick);
        if (na && *na->nc == u->Account() && !Config->GetModule("nickserv")->Get<bool>("nonicknameownership") && !na->nc->HasExt("UNCONFIRMED")) {
            u->SetMode(NickServ, "REGISTERED");
        }

        const Anope::string &modesonid = Config->GetModule(this)->Get<Anope::string>("modesonid");
        if (!modesonid.empty()) {
            u->SetModes(NickServ, "%s", modesonid.c_str());
        }
    }

    void Collide(User *u, NickAlias *na) anope_override {
        if (na) {
            collided.Set(na);
        }

        if (IRCD->CanSVSNick) {
            unsigned nicklen = Config->GetBlock("networkinfo")->Get<unsigned>("nicklen");
            const Anope::string &guestprefix =
            Config->GetModule("nickserv")->Get<const Anope::string>("guestnickprefix",
                    "Guest");

            Anope::string guestnick;

            int i = 0;
            do {
                guestnick = guestprefix + stringify(static_cast<uint16_t>(rand()));
                if (guestnick.length() > nicklen) {
                    guestnick = guestnick.substr(0, nicklen);
                }
            } while (User::Find(guestnick) && i++ < 10);

            if (i == 11) {
                u->Kill(*NickServ, "Services nickname-enforcer kill");
            } else {
                u->SendMessage(*NickServ, _("Your nickname is now being changed to \002%s\002"),
                               guestnick.c_str());
                IRCD->SendForceNickChange(u, guestnick, Anope::CurTime);
            }
        } else {
            u->Kill(*NickServ, "Services nickname-enforcer kill");
        }
    }

    void Release(NickAlias *na) anope_override {
        if (held.HasExt(na)) {
            if (IRCD->CanSVSHold) {
                IRCD->SendSVSHoldDel(na->nick);
            } else {
                User *u = User::Find(na->nick);
                if (u && u->server == Me) {
                    u->Quit();
                }
            }

            held.Unset(na);
        }
        collided.Unset(na); /* clear pending collide */
    }

    void OnReload(Configuration::Conf *conf) anope_override {
        const Anope::string &nsnick = conf->GetModule(this)->Get<const Anope::string>("client");

        if (nsnick.empty()) {
            throw ConfigException(Module::name + ": <client> must be defined");
        }

        BotInfo *bi = BotInfo::Find(nsnick, true);
        if (!bi) {
            throw ConfigException(Module::name + ": no bot named " + nsnick);
        }

        NickServ = bi;

        spacesepstream(conf->GetModule(this)->Get<const Anope::string>("defaults", "ns_secure memo_signon memo_receive")).GetTokens(defaults);
        if (defaults.empty()) {
            defaults.push_back("NS_SECURE");
            defaults.push_back("MEMO_SIGNON");
            defaults.push_back("MEMO_RECEIVE");
        } else if (defaults[0].equals_ci("none")) {
            defaults.clear();
        }
    }

    void OnDelNick(NickAlias *na) anope_override {
        User *u = User::Find(na->nick);
        if (u && u->Account() == na->nc) {
            IRCD->SendLogout(u);
            u->RemoveMode(NickServ, "REGISTERED");
            u->Logout();
        }
    }

    void OnDelCore(NickCore *nc) anope_override {
        Log(NickServ, "nick") << "Deleting nickname group " << nc->display;

        /* Clean up this nick core from any users online */
        for (std::list<User *>::iterator it = nc->users.begin(); it != nc->users.end();) {
            User *user = *it++;
            IRCD->SendLogout(user);
            user->RemoveMode(NickServ, "REGISTERED");
            user->Logout();
            FOREACH_MOD(OnNickLogout, (user));
        }
        nc->users.clear();
    }

    void OnChangeCoreDisplay(NickCore *nc,
                             const Anope::string &newdisplay) anope_override {
        Log(LOG_NORMAL, "nick", NickServ) << "Changing " << nc->display << " nickname group display to " << newdisplay;
    }

    void OnNickIdentify(User *u) anope_override {
        Configuration::Block *block = Config->GetModule(this);

        if (block->Get<bool>("modeonid", "yes"))

            for (User::ChanUserList::iterator it = u->chans.begin(), it_end = u->chans.end(); it != it_end; ++it) {
                ChanUserContainer *cc = it->second;
                Channel *c = cc->chan;
                if (c) {
                    c->SetCorrectModes(u, true);
                }
            }

        const Anope::string &modesonid = block->Get<const Anope::string>("modesonid");
        if (!modesonid.empty()) {
            u->SetModes(NickServ, "%s", modesonid.c_str());
        }

        if (block->Get<bool>("forceemail", "yes") && u->Account()->email.empty()) {
            u->SendMessage(NickServ, _("You must now supply an e-mail for your nick.\n"
                                       "This e-mail will allow you to retrieve your password in\n"
                                       "case you forget it."));
            u->SendMessage(NickServ,
                           _("Type \002%s%s SET EMAIL \037e-mail\037\002 in order to set your e-mail.\n"
                             "Your privacy is respected; this e-mail won't be given to\n"
                             "any third-party person."), Config->StrictPrivmsg.c_str(),
                           NickServ->nick.c_str());
        }

        for (std::set<NickServCollide *>::iterator it = collides.begin(); it != collides.end(); ++it) {
            NickServCollide *c = *it;
            if (c->GetUser() == u && c->GetNick() && c->GetNick()->nc == u->Account()) {
                delete c;
                break;
            }
        }
    }

    void OnNickGroup(User *u, NickAlias *target) anope_override {
        if (!target->nc->HasExt("UNCONFIRMED")) {
            u->SetMode(NickServ, "REGISTERED");
        }
    }

    void OnNickUpdate(User *u) anope_override {
        for (User::ChanUserList::iterator it = u->chans.begin(), it_end = u->chans.end(); it != it_end; ++it) {
            ChanUserContainer *cc = it->second;
            Channel *c = cc->chan;
            if (c) {
                c->SetCorrectModes(u, true);
            }
        }
    }

    void OnUserConnect(User *u, bool &exempt) anope_override {
        if (u->Quitting() || !u->server->IsSynced() || u->server->IsULined()) {
            return;
        }

        const NickAlias *na = NickAlias::Find(u->nick);

        const Anope::string &unregistered_notice = Config->GetModule(this)->Get<const Anope::string>("unregistered_notice");
        if (!Config->GetModule("nickserv")->Get<bool>("nonicknameownership") && !unregistered_notice.empty() && !na && !u->Account()) {
            u->SendMessage(NickServ, unregistered_notice.replace_all_cs("%n", u->nick));
        } else if (na && !u->IsIdentified(true)) {
            this->Validate(u);
        }
    }

    void OnPostUserLogoff(User *u) anope_override {
        NickAlias *na = NickAlias::Find(u->nick);
        if (na) {
            OnCancel(u, na);
        }
    }

    void OnServerSync(Server *s) anope_override {
        for (user_map::const_iterator it = UserListByNick.begin(); it != UserListByNick.end(); ++it) {
            User *u = it->second;

            if (u->server == s) {
                if (u->HasMode("REGISTERED") && !u->IsIdentified(true)) {
                    u->RemoveMode(NickServ, "REGISTERED");
                }
                if (!u->IsIdentified()) {
                    this->Validate(u);
                }
            }
        }
    }

    void OnUserNickChange(User *u, const Anope::string &oldnick) anope_override {
        NickAlias *old_na = NickAlias::Find(oldnick), *na = NickAlias::Find(u->nick);
        /* If the new nick isn't registered or it's registered and not yours */
        if (!na || na->nc != u->Account()) {
            /* Remove +r, but keep an account associated with the user */
            u->RemoveMode(NickServ, "REGISTERED");

            this->Validate(u);
        } else {
            /* Reset +r and re-send account (even though it really should be set at this point) */
            IRCD->SendLogin(u, na);
            if (!Config->GetModule("nickserv")->Get<bool>("nonicknameownership")
                    && na->nc == u->Account() && !na->nc->HasExt("UNCONFIRMED")) {
                u->SetMode(NickServ, "REGISTERED");
            }
            Log(u, "", NickServ) << u->GetMask() << " automatically identified for group "
                                 << u->Account()->display;
        }

        if (!u->nick.equals_ci(oldnick) && old_na) {
            OnCancel(u, old_na);
        }
    }

    void OnUserModeSet(const MessageSource &setter, User *u,
                       const Anope::string &mname) anope_override {
        if (u->server->IsSynced() && mname == "REGISTERED" && !u->IsIdentified(true)) {
            u->RemoveMode(NickServ, mname);
        }
    }

    EventReturn OnPreHelp(CommandSource &source,
                          const std::vector<Anope::string> &params) anope_override {
        if (!params.empty() || source.c || source.service != *NickServ) {
            return EVENT_CONTINUE;
        }
        if (!Config->GetModule("nickserv")->Get<bool>("nonicknameownership"))
            source.Reply(_("\002%s\002 allows you to register a nickname and\n"
                           "prevent others from using it. The following\n"
                           "commands allow for registration and maintenance of\n"
                           "nicknames; to use them, type \002%s%s \037command\037\002.\n"
                           "For more information on a specific command, type\n"
                           "\002%s%s %s \037command\037\002.\n"), NickServ->nick.c_str(), Config->StrictPrivmsg.c_str(), NickServ->nick.c_str(), Config->StrictPrivmsg.c_str(), NickServ->nick.c_str(), source.command.c_str());
        else
            source.Reply(_("\002%s\002 allows you to register an account.\n"
                           "The following commands allow for registration and maintenance of\n"
                           "accounts; to use them, type \002%s%s \037command\037\002.\n"
                           "For more information on a specific command, type\n"
                           "\002%s%s %s \037command\037\002.\n"), NickServ->nick.c_str(), Config->StrictPrivmsg.c_str(), NickServ->nick.c_str(), Config->StrictPrivmsg.c_str(), NickServ->nick.c_str(), source.command.c_str());
        return EVENT_CONTINUE;
    }

    void OnPostHelp(CommandSource &source,
                    const std::vector<Anope::string> &params) anope_override {
        if (!params.empty() || source.c || source.service != *NickServ) {
            return;
        }
        if (source.IsServicesOper())
            source.Reply(_(" \n"
                           "Services Operators can also drop any nickname without needing\n"
                           "to identify for the nick, and may view the access list for\n"
                           "any nickname."));
        time_t nickserv_expire = Config->GetModule(this)->Get<time_t>("expire", "21d");
        if (nickserv_expire >= 86400)
            source.Reply(_(" \n"
                           "Accounts that are not used anymore are subject to\n"
                           "the automatic expiration, i.e. they will be deleted\n"
                           "after %d days if not used."), nickserv_expire / 86400);
    }

    void OnNickCoreCreate(NickCore *nc) anope_override {
        /* Set default flags */
        for (unsigned i = 0; i < defaults.size(); ++i) {
            nc->Extend<bool>(defaults[i].upper());
        }
    }

    void OnUserQuit(User *u, const Anope::string &msg) anope_override {
        if (u->server && !u->server->GetQuitReason().empty() && Config->GetModule(this)->Get<bool>("hidenetsplitquit")) {
            return;
        }

        /* Update last quit and last seen for the user */
        NickAlias *na = NickAlias::Find(u->nick);
        if (na && !na->nc->HasExt("NS_SUSPENDED") && (u->IsRecognized() || u->IsIdentified(true))) {
            na->last_seen = Anope::CurTime;
            na->last_quit = msg;
        }
    }

    void OnExpireTick() anope_override {
        if (Anope::NoExpire || Anope::ReadOnly) {
            return;
        }

        time_t nickserv_expire = Config->GetModule(this)->Get<time_t>("expire", "21d");

        for (nickalias_map::const_iterator it = NickAliasList->begin(), it_end = NickAliasList->end(); it != it_end; ) {
            NickAlias *na = it->second;
            ++it;

            User *u = User::Find(na->nick, true);
            if (u && (u->IsIdentified(true) || u->IsRecognized())) {
                na->last_seen = Anope::CurTime;
            }

            bool expire = false;

            if (nickserv_expire && Anope::CurTime - na->last_seen >= nickserv_expire) {
                expire = true;
            }

            FOREACH_MOD(OnPreNickExpire, (na, expire));

            if (expire) {
                Log(LOG_NORMAL, "nickserv/expire",
                    NickServ) << "Expiring nickname " << na->nick << " (group: " << na->nc->display
                              << ") (e-mail: " << (na->nc->email.empty() ? "none" : na->nc->email) << ")";
                FOREACH_MOD(OnNickExpire, (na));
                delete na;
            }
        }
    }

    void OnNickInfo(CommandSource &source, NickAlias *na, InfoFormatter &info,
                    bool show_hidden) anope_override {
        if (!na->nc->HasExt("UNCONFIRMED")) {
            time_t nickserv_expire = Config->GetModule(this)->Get<time_t>("expire", "21d");
            if (!na->HasExt("NS_NO_EXPIRE") && nickserv_expire && !Anope::NoExpire
                    && (source.HasPriv("nickserv/auspex") || na->last_seen != Anope::CurTime)) {
                info[_("Expires")] = Anope::strftime(na->last_seen + nickserv_expire,
                                                     source.GetAccount());
            }
        } else {
            time_t unconfirmed_expire =
                Config->GetModule("ns_register")->Get<time_t>("unconfirmedexpire", "1d");
            info[_("Expires")] = Anope::strftime(na->time_registered + unconfirmed_expire,
                                                 source.GetAccount());
        }
    }
};

MODULE_INIT(NickServCore)
