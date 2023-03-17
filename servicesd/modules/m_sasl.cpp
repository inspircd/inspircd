/*
 *
 * (C) 2014-2023 Anope Team
 * Contact us at team@anope.org
 *
 * Please read COPYING and README for further details.
 */

#include "module.h"
#include "modules/sasl.h"
#include "modules/ns_cert.h"

using namespace SASL;

class Plain : public Mechanism {
  public:
    Plain(Module *o) : Mechanism(o, "PLAIN") { }

    void ProcessMessage(Session *sess, const SASL::Message &m) anope_override {
        if (m.type == "S") {
            sasl->SendMessage(sess, "C", "+");
        } else if (m.type == "C") {
            Anope::string decoded;
            Anope::B64Decode(m.data, decoded);

            size_t p = decoded.find('\0');
            if (p == Anope::string::npos) {
                sasl->Fail(sess);
                delete sess;
                return;
            }
            decoded = decoded.substr(p + 1);

            p = decoded.find('\0');
            if (p == Anope::string::npos) {
                sasl->Fail(sess);
                delete sess;
                return;
            }

            Anope::string acc = decoded.substr(0, p),
                          pass = decoded.substr(p + 1);

            if (acc.empty() || pass.empty() || !IRCD->IsNickValid(acc)
                    || pass.find_first_of("\r\n") != Anope::string::npos) {
                sasl->Fail(sess);
                delete sess;
                return;
            }

            SASL::IdentifyRequest *req = new SASL::IdentifyRequest(this->owner, m.source,
                    acc, pass, sess->hostname, sess->ip);
            FOREACH_MOD(OnCheckAuthentication, (NULL, req));
            req->Dispatch();
        }
    }
};

class External : public Mechanism {
    ServiceReference<CertService> certs;

    struct Session : SASL::Session {
        Anope::string cert;

        Session(Mechanism *m, const Anope::string &u) : SASL::Session(m, u) { }
    };

  public:
    External(Module *o) : Mechanism(o, "EXTERNAL"), certs("CertService", "certs") {
        if (!IRCD || !IRCD->CanCertFP) {
            throw ModuleException("No CertFP");
        }
    }

    Session* CreateSession(const Anope::string &uid) anope_override {
        return new Session(this, uid);
    }

    void ProcessMessage(SASL::Session *sess,
                        const SASL::Message &m) anope_override {
        Session *mysess = anope_dynamic_static_cast<Session *>(sess);

        if (m.type == "S") {
            mysess->cert = m.ext;

            sasl->SendMessage(sess, "C", "+");
        } else if (m.type == "C") {
            if (!certs || mysess->cert.empty()) {
                sasl->Fail(sess);
                delete sess;
                return;
            }

            Anope::string user = "A user";
            if (!mysess->hostname.empty() && !mysess->ip.empty()) {
                user = mysess->hostname + " (" + mysess->ip + ")";
            }

            NickCore *nc = certs->FindAccountFromCert(mysess->cert);
            if (!nc || nc->HasExt("NS_SUSPENDED") || nc->HasExt("UNCONFIRMED")) {
                Log(this->owner, "sasl",
                    Config->GetClient("NickServ")) << user <<
                                                   " failed to identify using certificate " << mysess->cert <<
                                                   " using SASL EXTERNAL";
                sasl->Fail(sess);
                delete sess;
                return;
            }

            Log(this->owner, "sasl",
                Config->GetClient("NickServ")) << user << " identified to account " <<
                                               nc->display << " using SASL EXTERNAL";
            sasl->Succeed(sess, nc);
            delete sess;
        }
    }
};

class SASLService : public SASL::Service, public Timer {
    std::map<Anope::string, SASL::Session *> sessions;

  public:
    SASLService(Module *o) : SASL::Service(o), Timer(o, 60, Anope::CurTime, true) { }

    ~SASLService() {
        for (std::map<Anope::string, Session *>::iterator it = sessions.begin();
                it != sessions.end(); it++) {
            delete it->second;
        }
    }

    void ProcessMessage(const SASL::Message &m) anope_override {
        if (m.target != "*") {
            Server *s = Server::Find(m.target);
            if (s != Me) {
                User *u = User::Find(m.target);
                if (!u || u->server != Me) {
                    return;
                }
            }
        }

        Session* session = GetSession(m.source);

        if (m.type == "S") {
            ServiceReference<Mechanism> mech("SASL::Mechanism", m.data);
            if (!mech) {
                Session tmp(NULL, m.source);

                sasl->SendMechs(&tmp);
                sasl->Fail(&tmp);
                return;
            }

            Anope::string hostname, ip;
            if (session) {
                // Copy over host/ip to mech-specific session
                hostname = session->hostname;
                ip = session->ip;
                delete session;
            }

            session = mech->CreateSession(m.source);
            if (session) {
                session->hostname = hostname;
                session->ip = ip;

                sessions[m.source] = session;
            }
        } else if (m.type == "D") {
            delete session;
            return;
        } else if (m.type == "H") {
            if (!session) {
                session = new Session(NULL, m.source);
                sessions[m.source] = session;
            }
            session->hostname = m.data;
            session->ip = m.ext;
        }

        if (session && session->mech) {
            session->mech->ProcessMessage(session, m);
        }
    }

    Anope::string GetAgent() anope_override {
        Anope::string agent = Config->GetModule(Service::owner)->Get<Anope::string>("agent", "NickServ");
        BotInfo *bi = Config->GetClient(agent);
        if (bi) {
            agent = bi->GetUID();
        }
        return agent;
    }

    Session* GetSession(const Anope::string &uid) anope_override {
        std::map<Anope::string, Session *>::iterator it = sessions.find(uid);
        if (it != sessions.end()) {
            return it->second;
        }
        return NULL;
    }

    void RemoveSession(Session *sess) anope_override {
        sessions.erase(sess->uid);
    }

    void DeleteSessions(Mechanism *mech, bool da) anope_override {
        for (std::map<Anope::string, Session *>::iterator it = sessions.begin(); it != sessions.end();) {
            std::map<Anope::string, Session *>::iterator del = it++;
            if (*del->second->mech == mech) {
                if (da) {
                    this->SendMessage(del->second, "D", "A");
                }
                delete del->second;
            }
        }
    }

    void SendMessage(Session *session, const Anope::string &mtype,
                     const Anope::string &data) anope_override {
        SASL::Message msg;
        msg.source = this->GetAgent();
        msg.target = session->uid;
        msg.type = mtype;
        msg.data = data;

        IRCD->SendSASLMessage(msg);
    }

    void Succeed(Session *session, NickCore *nc) anope_override {
        // If the user is already introduced then we log them in now.
        // Otherwise, we send an SVSLOGIN to log them in later.
        User *user = User::Find(session->uid);
        NickAlias *na = NickAlias::Find(nc->display);
        if (user) {
            user->Identify(na);
        } else {
            IRCD->SendSVSLogin(session->uid, nc->display, na->GetVhostIdent(),
                               na->GetVhostHost());
        }
        this->SendMessage(session, "D", "S");
    }

    void Fail(Session *session) anope_override {
        this->SendMessage(session, "D", "F");
    }

    void SendMechs(Session *session) anope_override {
        std::vector<Anope::string> mechs = Service::GetServiceKeys("SASL::Mechanism");
        Anope::string buf;
        for (unsigned j = 0; j < mechs.size(); ++j) {
            buf += "," + mechs[j];
        }

        this->SendMessage(session, "M", buf.empty() ? "" : buf.substr(1));
    }

    void Tick(time_t) anope_override {
        for (std::map<Anope::string, Session *>::iterator it = sessions.begin(); it != sessions.end();) {
            Anope::string key = it->first;
            Session *s = it->second;
            ++it;

            if (!s || s->created + 60 < Anope::CurTime) {
                delete s;
                sessions.erase(key);
            }
        }
    }
};

class ModuleSASL : public Module {
    SASLService sasl;

    Plain plain;
    External *external;

    std::vector<Anope::string> mechs;

    void CheckMechs() {
        std::vector<Anope::string> newmechs
            = ::Service::GetServiceKeys("SASL::Mechanism");
        if (newmechs == mechs) {
            return;
        }

        mechs = newmechs;

        // If we are connected to the network then broadcast the mechlist.
        if (Me && Me->IsSynced()) {
            IRCD->SendSASLMechanisms(mechs);
        }
    }

  public:
    ModuleSASL(const Anope::string &modname,
               const Anope::string &creator) : Module(modname, creator, VENDOR),
        sasl(this), plain(this), external(NULL) {
        try {
            external = new External(this);
            CheckMechs();
        } catch (ModuleException &) { }
    }

    ~ModuleSASL() {
        delete external;
    }

    void OnModuleLoad(User *, Module *) anope_override {
        CheckMechs();
    }

    void OnModuleUnload(User *, Module *) anope_override {
        CheckMechs();
    }

    void OnPreUplinkSync(Server *) anope_override {
        // We have not yet sent a mechanism list so always do it here.
        IRCD->SendSASLMechanisms(mechs);
    }
};

MODULE_INIT(ModuleSASL)
