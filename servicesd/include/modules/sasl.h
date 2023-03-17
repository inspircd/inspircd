/*
 *
 * (C) 2014-2023 Anope Team
 * Contact us at team@anope.org
 *
 * Please read COPYING and README for further details.
 */

namespace SASL {
struct Message {
    Anope::string source;
    Anope::string target;
    Anope::string type;
    Anope::string data;
    Anope::string ext;
};

class Mechanism;
struct Session;

class Service : public ::Service {
  public:
    Service(Module *o) : ::Service(o, "SASL::Service", "sasl") { }

    virtual void ProcessMessage(const Message &) = 0;

    virtual Anope::string GetAgent() = 0;

    virtual Session* GetSession(const Anope::string &uid) = 0;

    virtual void SendMessage(SASL::Session *session, const Anope::string &type,
                             const Anope::string &data) = 0;

    virtual void Succeed(Session *, NickCore *) = 0;
    virtual void Fail(Session *) = 0;
    virtual void SendMechs(Session *) = 0;
    virtual void DeleteSessions(Mechanism *, bool = false) = 0;
    virtual void RemoveSession(Session *) = 0;
};

static ServiceReference<SASL::Service> sasl("SASL::Service", "sasl");

struct Session {
    time_t created;
    Anope::string uid;
    Anope::string hostname, ip;
    Reference<Mechanism> mech;

    Session(Mechanism *m, const Anope::string &u) : created(Anope::CurTime), uid(u),
        mech(m) { }
    virtual ~Session() {
        if (sasl) {
            sasl->RemoveSession(this);
        }
    }
};

/* PLAIN, EXTERNAL, etc */
class Mechanism : public ::Service {
  public:
    Mechanism(Module *o, const Anope::string &sname) : Service(o, "SASL::Mechanism",
                sname) { }

    virtual Session* CreateSession(const Anope::string &uid) {
        return new Session(this, uid);
    }

    virtual void ProcessMessage(Session *session, const Message &) = 0;

    virtual ~Mechanism() {
        if (sasl) {
            sasl->DeleteSessions(this, true);
        }
    }
};

class IdentifyRequest : public ::IdentifyRequest {
    Anope::string uid;
    Anope::string hostname, ip;

  public:
    IdentifyRequest(Module *m, const Anope::string &id, const Anope::string &acc,
                    const Anope::string &pass, const Anope::string &h,
                    const Anope::string &i) : ::IdentifyRequest(m, acc, pass), uid(id), hostname(h),
        ip(i) { }

    void OnSuccess() anope_override {
        if (!sasl) {
            return;
        }

        NickAlias *na = NickAlias::Find(GetAccount());
        if (!na || na->nc->HasExt("NS_SUSPENDED") || na->nc->HasExt("UNCONFIRMED")) {
            return OnFail();
        }

        unsigned int maxlogins = Config->GetModule("ns_identify")->Get<unsigned int>("maxlogins");
        if (maxlogins && na->nc->users.size() >= maxlogins) {
            return OnFail();
        }

        Session *s = sasl->GetSession(uid);
        if (s) {
            Anope::string user = "A user";
            if (!hostname.empty() && !ip.empty()) {
                user = hostname + " (" + ip + ")";
            }

            Log(this->GetOwner(), "sasl",
                Config->GetClient("NickServ")) << user << " identified to account " <<
                                               this->GetAccount() << " using SASL";
            sasl->Succeed(s, na->nc);
            delete s;
        }
    }

    void OnFail() anope_override {
        if (!sasl) {
            return;
        }

        Session *s = sasl->GetSession(uid);
        if (s) {
            sasl->Fail(s);
            delete s;
        }

        Anope::string accountstatus;
        NickAlias *na = NickAlias::Find(GetAccount());
        if (!na) {
            accountstatus = "nonexistent ";
        } else if (na->nc->HasExt("NS_SUSPENDED")) {
            accountstatus = "suspended ";
        } else if (na->nc->HasExt("UNCONFIRMED")) {
            accountstatus = "unconfirmed ";
        }

        Anope::string user = "A user";
        if (!hostname.empty() && !ip.empty()) {
            user = hostname + " (" + ip + ")";
        }

        Log(this->GetOwner(), "sasl", Config->GetClient("NickServ")) << user << " failed to identify for " << accountstatus << "account " << this->GetAccount() << " using SASL";
    }
};
}
