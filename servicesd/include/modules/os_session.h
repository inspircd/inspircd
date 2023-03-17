/*
 *
 * (C) 2011-2023 Anope Team
 * Contact us at team@anope.org
 *
 * Please read COPYING and README for further details.
 */

#ifndef OS_SESSION_H
#define OS_SESSION_H

struct Session {
    cidr addr;                      /* A cidr (sockaddrs + len) representing this session */
    unsigned count;                 /* Number of clients with this host */
    unsigned hits;                  /* Number of subsequent kills for a host */

    Session(const sockaddrs &ip, int len) : addr(ip, len), count(1), hits(0) { }
};

struct Exception : Serializable {
    Anope::string mask;     /* Hosts to which this exception applies */
    unsigned limit;         /* Session limit for exception */
    Anope::string who;      /* Nick of person who added the exception */
    Anope::string reason;       /* Reason for exception's addition */
    time_t time;            /* When this exception was added */
    time_t expires;         /* Time when it expires. 0 == no expiry */

    Exception() : Serializable("Exception") { }
    void Serialize(Serialize::Data &data) const anope_override;
    static Serializable* Unserialize(Serializable *obj, Serialize::Data &data);
};

class SessionService : public Service {
  public:
    typedef TR1NS::unordered_map<cidr, Session *, cidr::hash> SessionMap;
    typedef std::vector<Exception *> ExceptionVector;

    SessionService(Module *m) : Service(m, "SessionService", "session") { }

    virtual Exception *CreateException() = 0;

    virtual void AddException(Exception *e) = 0;

    virtual void DelException(Exception *e) = 0;

    virtual Exception *FindException(User *u) = 0;

    virtual Exception *FindException(const Anope::string &host) = 0;

    virtual ExceptionVector &GetExceptions() = 0;

    virtual Session *FindSession(const Anope::string &ip) = 0;

    virtual SessionMap &GetSessions() = 0;
};

static ServiceReference<SessionService> session_service("SessionService",
        "session");

void Exception::Serialize(Serialize::Data &data) const {
    data["mask"] << this->mask;
    data["limit"] << this->limit;
    data["who"] << this->who;
    data["reason"] << this->reason;
    data["time"] << this->time;
    data["expires"] << this->expires;
}

Serializable* Exception::Unserialize(Serializable *obj, Serialize::Data &data) {
    if (!session_service) {
        return NULL;
    }

    Exception *ex;
    if (obj) {
        ex = anope_dynamic_static_cast<Exception *>(obj);
    } else {
        ex = new Exception;
    }
    data["mask"] >> ex->mask;
    data["limit"] >> ex->limit;
    data["who"] >> ex->who;
    data["reason"] >> ex->reason;
    data["time"] >> ex->time;
    data["expires"] >> ex->expires;

    if (!obj) {
        session_service->AddException(ex);
    }
    return ex;
}

#endif
