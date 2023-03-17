/*
 *
 * (C) 2003-2023 Anope Team
 * Contact us at team@anope.org
 *
 * Please read COPYING and README for further details.
 */

namespace Redis {
struct Reply {
    enum Type {
        NOT_PARSED,
        NOT_OK,
        OK,
        INT,
        BULK,
        MULTI_BULK
    }
    type;

    Reply() {
        Clear();
    }
    ~Reply() {
        Clear();
    }

    void Clear() {
        type = NOT_PARSED;
        i = 0;
        bulk.clear();
        multi_bulk_size = 0;
        for (unsigned j = 0; j < multi_bulk.size(); ++j) {
            delete multi_bulk[j];
        }
        multi_bulk.clear();
    }

    int64_t i;
    Anope::string bulk;
    int multi_bulk_size;
    std::deque<Reply *> multi_bulk;
};

class Interface {
  public:
    Module *owner;

    Interface(Module *m) : owner(m) { }
    virtual ~Interface() { }

    virtual void OnResult(const Reply &r) = 0;
    virtual void OnError(const Anope::string &error) {
        Log(owner) << error;
    }
};

class Provider : public Service {
  public:
    Provider(Module *c, const Anope::string &n) : Service(c, "Redis::Provider", n) { }

    virtual bool IsSocketDead() = 0;

    virtual void SendCommand(Interface *i,
                             const std::vector<Anope::string> &cmds) = 0;
    virtual void SendCommand(Interface *i, const Anope::string &str) = 0;

    virtual bool BlockAndProcess() = 0;

    virtual void Subscribe(Interface *i, const Anope::string &pattern) = 0;
    virtual void Unsubscribe(const Anope::string &pattern) = 0;

    virtual void StartTransaction() = 0;
    virtual void CommitTransaction() = 0;
};
}
