/* OperServ core functions
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

class SGLineManager : public XLineManager {
  public:
    SGLineManager(Module *creator) : XLineManager(creator, "xlinemanager/sgline",
                'G') { }

    void OnMatch(User *u, XLine *x) anope_override {
        this->Send(u, x);
    }

    void OnExpire(const XLine *x) anope_override {
        Log(Config->GetClient("OperServ"), "expire/akill") << "AKILL on \002" << x->mask << "\002 has expired";
    }

    void Send(User *u, XLine *x) anope_override {
        IRCD->SendAkill(u, x);
    }

    void SendDel(XLine *x) anope_override {
        IRCD->SendAkillDel(x);
    }

    bool Check(User *u, const XLine *x) anope_override {
        if (x->regex) {
            Anope::string uh = u->GetIdent() + "@" + u->host,
            nuhr = u->nick + "!" + uh + "#" + u->realname;
            return x->regex->Matches(uh) || x->regex->Matches(nuhr);
        }

        if (!x->GetNick().empty() && !Anope::Match(u->nick, x->GetNick())) {
            return false;
        }

        if (!x->GetUser().empty() && !Anope::Match(u->GetIdent(), x->GetUser())) {
            return false;
        }

        if (!x->GetReal().empty() && !Anope::Match(u->realname, x->GetReal())) {
            return false;
        }

        if (x->c && x->c->match(u->ip)) {
            return true;
        }

        if (x->GetHost().empty() || Anope::Match(u->host, x->GetHost()) || Anope::Match(u->ip.addr(), x->GetHost())) {
            return true;
        }

        return false;
    }
};

class SQLineManager : public XLineManager {
    ServiceReference<NickServService> nickserv;

  public:
    SQLineManager(Module *creator) : XLineManager(creator, "xlinemanager/sqline",
                'Q'), nickserv("NickServService", "NickServ") { }

    void OnMatch(User *u, XLine *x) anope_override {
        this->Send(u, x);
    }

    void OnExpire(const XLine *x) anope_override {
        Log(Config->GetClient("OperServ"), "expire/sqline") << "SQLINE on \002" << x->mask << "\002 has expired";
    }

    void Send(User *u, XLine *x) anope_override {
        if (!IRCD->CanSQLine) {
            if (!u)
                ;
            else if (nickserv) {
                nickserv->Collide(u, NULL);
            } else {
                u->Kill(Config->GetClient("OperServ"), "Q-Lined: " + x->reason);
            }
        } else if (x->IsRegex()) {
            if (u) {
                u->Kill(Config->GetClient("OperServ"), "Q-Lined: " + x->reason);
            }
        } else if (x->mask[0] != '#' || IRCD->CanSQLineChannel) {
            IRCD->SendSQLine(u, x);
            /* If it is an oper, assume they're walking it, otherwise kill for good measure */
            if (u && !u->HasMode("OPER")) {
                u->Kill(Config->GetClient("OperServ"), "Q-Lined: " + x->reason);
            }
        }
    }

    void SendDel(XLine *x) anope_override {
        if (!IRCD->CanSQLine || x->IsRegex())
            ;
        else if (x->mask[0] != '#' || IRCD->CanSQLineChannel) {
            IRCD->SendSQLineDel(x);
        }
    }

    bool Check(User *u, const XLine *x) anope_override {
        if (x->regex) {
            return x->regex->Matches(u->nick);
        }
        return Anope::Match(u->nick, x->mask);
    }

    XLine *CheckChannel(Channel *c) {
        for (std::vector<XLine *>::const_iterator it = this->GetList().begin(),
                it_end = this->GetList().end(); it != it_end; ++it) {
            XLine *x = *it;

            if (x->regex) {
                if (x->regex->Matches(c->name)) {
                    return x;
                }
            } else {
                if (x->mask.empty() || x->mask[0] != '#') {
                    continue;
                }

                if (Anope::Match(c->name, x->mask, false, true)) {
                    return x;
                }
            }
        }
        return NULL;
    }
};

class SNLineManager : public XLineManager {
  public:
    SNLineManager(Module *creator) : XLineManager(creator, "xlinemanager/snline",
                'N') { }

    void OnMatch(User *u, XLine *x) anope_override {
        this->Send(u, x);
    }

    void OnExpire(const XLine *x) anope_override {
        Log(Config->GetClient("OperServ"), "expire/snline") << "SNLINE on \002" << x->mask << "\002 has expired";
    }

    void Send(User *u, XLine *x) anope_override {
        if (IRCD->CanSNLine && !x->IsRegex()) {
            IRCD->SendSGLine(u, x);
        }

        if (u) {
            u->Kill(Config->GetClient("OperServ"), "SNLined: " + x->reason);
        }
    }

    void SendDel(XLine *x) anope_override {
        if (IRCD->CanSNLine && !x->IsRegex()) {
            IRCD->SendSGLineDel(x);
        }
    }

    bool Check(User *u, const XLine *x) anope_override {
        if (x->regex) {
            return x->regex->Matches(u->realname);
        }
        return Anope::Match(u->realname, x->mask, false, true);
    }
};

class OperServCore : public Module {
    Reference<BotInfo> OperServ;
    SGLineManager sglines;
    SQLineManager sqlines;
    SNLineManager snlines;

  public:
    OperServCore(const Anope::string &modname,
                 const Anope::string &creator) : Module(modname, creator, PSEUDOCLIENT | VENDOR),
        sglines(this), sqlines(this), snlines(this) {

        /* Yes, these are in this order for a reason. Most violent->least violent. */
        XLineManager::RegisterXLineManager(&sglines);
        XLineManager::RegisterXLineManager(&sqlines);
        XLineManager::RegisterXLineManager(&snlines);
    }

    ~OperServCore() {
        this->sglines.Clear();
        this->sqlines.Clear();
        this->snlines.Clear();

        XLineManager::UnregisterXLineManager(&sglines);
        XLineManager::UnregisterXLineManager(&sqlines);
        XLineManager::UnregisterXLineManager(&snlines);
    }

    void OnReload(Configuration::Conf *conf) anope_override {
        const Anope::string &osnick = conf->GetModule(this)->Get<const Anope::string>("client");

        if (osnick.empty()) {
            throw ConfigException(this->name + ": <client> must be defined");
        }

        BotInfo *bi = BotInfo::Find(osnick, true);
        if (!bi) {
            throw ConfigException(this->name + ": no bot named " + osnick);
        }

        OperServ = bi;
    }

    EventReturn OnBotPrivmsg(User *u, BotInfo *bi,
                             Anope::string &message) anope_override {
        if (bi == OperServ && !u->HasMode("OPER") && Config->GetModule(this)->Get<bool>("opersonly")) {
            u->SendMessage(bi, ACCESS_DENIED);
            Log(bi, "bados") << "Denied access to " << bi->nick << " from " << u->GetMask()
                             << " (non-oper)";
            return EVENT_STOP;
        }

        return EVENT_CONTINUE;
    }

    void OnServerQuit(Server *server) anope_override {
        if (server->IsJuped()) {
            Log(server, "squit", OperServ) << "Received SQUIT for juped server " <<
                                           server->GetName();
        }
    }

    void OnUserModeSet(const MessageSource &setter, User *u,
                       const Anope::string &mname) anope_override {
        if (mname == "OPER") {
            Log(u, "oper", OperServ) << "is now an IRC operator.";
        }
    }

    void OnUserModeUnset(const MessageSource &setter, User *u,
                         const Anope::string &mname) anope_override {
        if (mname == "OPER") {
            Log(u, "oper", OperServ) << "is no longer an IRC operator";
        }
    }

    void OnUserConnect(User *u, bool &exempt) anope_override {
        if (!u->Quitting() && !exempt) {
            XLineManager::CheckAll(u);
        }
    }

    void OnUserNickChange(User *u, const Anope::string &oldnick) anope_override {
        if (!u->HasMode("OPER")) {
            this->sqlines.CheckAllXLines(u);
        }
    }

    EventReturn OnCheckKick(User *u, Channel *c, Anope::string &mask,
                            Anope::string &reason) anope_override {
        XLine *x = this->sqlines.CheckChannel(c);
        if (x) {
            this->sqlines.OnMatch(u, x);
            reason = x->reason;
            return EVENT_STOP;
        }

        return EVENT_CONTINUE;
    }

    EventReturn OnPreHelp(CommandSource &source,
                          const std::vector<Anope::string> &params) anope_override {
        if (!params.empty() || source.c || source.service != *OperServ) {
            return EVENT_CONTINUE;
        }
        source.Reply(_("%s commands:"), OperServ->nick.c_str());
        return EVENT_CONTINUE;
    }

    void OnLog(Log *l) anope_override {
        if (l->type == LOG_SERVER) {
            l->bi = OperServ;
        }
    }
};

MODULE_INIT(OperServCore)
