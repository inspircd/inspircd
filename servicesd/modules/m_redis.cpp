/*
 *
 * (C) 2003-2023 Anope Team
 * Contact us at team@anope.org
 *
 * Please read COPYING and README for further details.
 */

#include "module.h"
#include "modules/redis.h"

using namespace Redis;

class MyRedisService;

class RedisSocket : public BinarySocket, public ConnectionSocket {
    size_t ParseReply(Reply &r, const char *buf, size_t l);
  public:
    MyRedisService *provider;
    std::deque<Interface *> interfaces;
    std::map<Anope::string, Interface *> subinterfaces;

    RedisSocket(MyRedisService *pro, bool v6) : Socket(-1, v6), provider(pro) { }

    ~RedisSocket();

    void OnConnect() anope_override;
    void OnError(const Anope::string &error) anope_override;

    bool Read(const char *buffer, size_t l) anope_override;
};

class Transaction : public Interface {
  public:
    std::deque<Interface *> interfaces;

    Transaction(Module *creator) : Interface(creator) { }

    ~Transaction() {
        for (unsigned i = 0; i < interfaces.size(); ++i) {
            Interface *inter = interfaces[i];

            if (!inter) {
                continue;
            }

            inter->OnError("Interface going away");
        }
    }

    void OnResult(const Reply &r) anope_override {
        /* This is a multi bulk reply of the results of the queued commands
         * in this transaction
         */

        Log(LOG_DEBUG_2) << "redis: transaction complete with " << r.multi_bulk.size() << " results";

        for (unsigned i = 0; i < r.multi_bulk.size(); ++i) {
            const Reply *reply = r.multi_bulk[i];

            if (interfaces.empty()) {
                break;
            }

            Interface *inter = interfaces.front();
            interfaces.pop_front();

            if (inter) {
                inter->OnResult(*reply);
            }
        }
    }
};

class MyRedisService : public Provider {
  public:
    Anope::string host;
    int port;
    unsigned db;

    RedisSocket *sock, *sub;

    Transaction ti;
    bool in_transaction;

    MyRedisService(Module *c, const Anope::string &n, const Anope::string &h, int p,
                   unsigned d) : Provider(c, n), host(h), port(p), db(d), sock(NULL), sub(NULL),
        ti(c), in_transaction(false) {
        sock = new RedisSocket(this, host.find(':') != Anope::string::npos);
        sock->Connect(host, port);

        sub = new RedisSocket(this, host.find(':') != Anope::string::npos);
        sub->Connect(host, port);
    }

    ~MyRedisService() {
        if (sock) {
            sock->flags[SF_DEAD] = true;
            sock->provider = NULL;
        }

        if (sub) {
            sub->flags[SF_DEAD] = true;
            sub->provider = NULL;
        }
    }

  private:
    inline void Pack(std::vector<char> &buffer, const char *buf, size_t sz = 0) {
        if (!sz) {
            sz = strlen(buf);
        }

        size_t old_size = buffer.size();
        buffer.resize(old_size + sz);
        std::copy(buf, buf + sz, buffer.begin() + old_size);
    }

    void Send(RedisSocket *s, Interface *i,
              const std::vector<std::pair<const char *, size_t> > &args) {
        std::vector<char> buffer;

        Pack(buffer, "*");
        Pack(buffer, stringify(args.size()).c_str());
        Pack(buffer, "\r\n");

        for (unsigned j = 0; j < args.size(); ++j) {
            const std::pair<const char *, size_t> &pair = args[j];

            Pack(buffer, "$");
            Pack(buffer, stringify(pair.second).c_str());
            Pack(buffer, "\r\n");

            Pack(buffer, pair.first, pair.second);
            Pack(buffer, "\r\n");
        }

        if (buffer.empty()) {
            return;
        }

        s->Write(&buffer[0], buffer.size());
        if (in_transaction) {
            ti.interfaces.push_back(i);
            s->interfaces.push_back(NULL); // For the +Queued response
        } else {
            s->interfaces.push_back(i);
        }
    }

  public:
    bool IsSocketDead() anope_override {
        return this->sock && this->sock->flags[SF_DEAD];
    }

    void SendCommand(RedisSocket *s, Interface *i,
                     const std::vector<Anope::string> &cmds) {
        std::vector<std::pair<const char *, size_t> > args;
        for (unsigned j = 0; j < cmds.size(); ++j) {
            args.push_back(std::make_pair(cmds[j].c_str(), cmds[j].length()));
        }
        this->Send(s, i, args);
    }

    void SendCommand(RedisSocket *s, Interface *i, const Anope::string &str) {
        std::vector<Anope::string> args;
        spacesepstream(str).GetTokens(args);
        this->SendCommand(s, i, args);
    }

    void Send(Interface *i, const std::vector<std::pair<const char *, size_t> >
              &args) {
        if (!sock) {
            sock = new RedisSocket(this, host.find(':') != Anope::string::npos);
            sock->Connect(host, port);
        }

        this->Send(sock, i, args);
    }

    void SendCommand(Interface *i,
                     const std::vector<Anope::string> &cmds) anope_override {
        std::vector<std::pair<const char *, size_t> > args;
        for (unsigned j = 0; j < cmds.size(); ++j) {
            args.push_back(std::make_pair(cmds[j].c_str(), cmds[j].length()));
        }
        this->Send(i, args);
    }

    void SendCommand(Interface *i, const Anope::string &str) anope_override {
        std::vector<Anope::string> args;
        spacesepstream(str).GetTokens(args);
        this->SendCommand(i, args);
    }

  public:
    bool BlockAndProcess() anope_override {
        if (!this->sock->ProcessWrite()) {
            this->sock->flags[SF_DEAD] = true;
        }
        this->sock->SetBlocking(true);
        if (!this->sock->ProcessRead()) {
            this->sock->flags[SF_DEAD] = true;
        }
        this->sock->SetBlocking(false);
        return !this->sock->interfaces.empty();
    }

    void Subscribe(Interface *i, const Anope::string &pattern) anope_override {
        if (sub == NULL) {
            sub = new RedisSocket(this, host.find(':') != Anope::string::npos);
            sub->Connect(host, port);
        }

        std::vector<Anope::string> args;
        args.push_back("PSUBSCRIBE");
        args.push_back(pattern);
        this->SendCommand(sub, NULL, args);

        sub->subinterfaces[pattern] = i;
    }

    void Unsubscribe(const Anope::string &pattern) anope_override {
        if (sub) {
            sub->subinterfaces.erase(pattern);
        }
    }

    void StartTransaction() anope_override {
        if (in_transaction) {
            throw CoreException();
        }

        this->SendCommand(NULL, "MULTI");
        in_transaction = true;
    }

    void CommitTransaction() anope_override {
        /* The result of the transaction comes back to the reply of EXEC as a multi bulk.
         * The reply to the individual commands that make up the transaction when executed
         * is a simple +QUEUED
         */
        in_transaction = false;
        this->SendCommand(&this->ti, "EXEC");
    }
};

RedisSocket::~RedisSocket() {
    if (provider) {
        if (provider->sock == this) {
            provider->sock = NULL;
        } else if (provider->sub == this) {
            provider->sub = NULL;
        }
    }

    for (unsigned i = 0; i < interfaces.size(); ++i) {
        Interface *inter = interfaces[i];

        if (!inter) {
            continue;
        }

        inter->OnError("Interface going away");
    }
}

void RedisSocket::OnConnect() {
    Log() << "redis: Successfully connected to " << provider->name <<
          (this == this->provider->sub ? " (sub)" : "");

    this->provider->SendCommand(NULL, "CLIENT SETNAME Anope");
    this->provider->SendCommand(NULL, "SELECT " + stringify(provider->db));

    if (this != this->provider->sub) {
        this->provider->SendCommand(this, NULL, "CONFIG SET notify-keyspace-events KA");
    }
}

void RedisSocket::OnError(const Anope::string &error) {
    Log() << "redis: Error on " << provider->name << (this == this->provider->sub ?
            " (sub)" : "") << ": " << error;
}

size_t RedisSocket::ParseReply(Reply &r, const char *buffer, size_t l) {
    size_t used = 0;

    if (!l) {
        return used;
    }

    if (r.type == Reply::MULTI_BULK) {
        goto multi_bulk_cont;
    }

    switch (*buffer) {
    case '+': {
        Anope::string reason(buffer, 1, l - 1);
        size_t nl = reason.find("\r\n");
        Log(LOG_DEBUG_2) << "redis: status ok: " << reason.substr(0, nl);
        if (nl != Anope::string::npos) {
            r.type = Reply::OK;
            used = 1 + nl + 2;
        }
        break;
    }
    case '-': {
        Anope::string reason(buffer, 1, l - 1);
        size_t nl = reason.find("\r\n");
        Log(LOG_DEBUG) << "redis: status error: " << reason.substr(0, nl);
        if (nl != Anope::string::npos) {
            r.type = Reply::NOT_OK;
            used = 1 + nl + 2;
        }
        break;
    }
    case ':': {
        Anope::string ibuf(buffer, 1, l - 1);
        size_t nl = ibuf.find("\r\n");
        if (nl != Anope::string::npos) {
            try {
                r.i = convertTo<int64_t>(ibuf.substr(0, nl));
            } catch (const ConvertException &) { }

            r.type = Reply::INT;
            used = 1 + nl + 2;
        }
        break;
    }
    case '$': {
        Anope::string reply(buffer + 1, l - 1);
        /* This assumes one bulk can always fit in our recv buffer */
        size_t nl = reply.find("\r\n");
        if (nl != Anope::string::npos) {
            int len;
            try {
                len = convertTo<int>(reply.substr(0, nl));
                if (len >= 0) {
                    if (1 + nl + 2 + len + 2 <= l) {
                        used = 1 + nl + 2 + len + 2;
                        r.bulk = reply.substr(nl + 2, len);
                        r.type = Reply::BULK;
                    }
                } else {
                    used = 1 + nl + 2 + 2;
                    r.type = Reply::BULK;
                }
            } catch (const ConvertException &) { }
        }
        break;
    }
multi_bulk_cont:
    case '*': {
        if (r.type != Reply::MULTI_BULK) {
            Anope::string reply(buffer + 1, l - 1);
            size_t nl = reply.find("\r\n");
            if (nl != Anope::string::npos) {
                r.type = Reply::MULTI_BULK;
                try {
                    r.multi_bulk_size = convertTo<int>(reply.substr(0, nl));
                } catch (const ConvertException &) { }

                used = 1 + nl + 2;
            } else {
                break;
            }
        } else if (r.multi_bulk_size >= 0
                   && r.multi_bulk.size() == static_cast<unsigned>(r.multi_bulk_size)) {
            /* This multi bulk is already complete, so check the sub bulks */
            for (unsigned i = 0; i < r.multi_bulk.size(); ++i)
                if (r.multi_bulk[i]->type == Reply::MULTI_BULK) {
                    ParseReply(*r.multi_bulk[i], buffer + used, l - used);
                }
            break;
        }

        for (int i = r.multi_bulk.size(); i < r.multi_bulk_size; ++i) {
            Reply *reply = new Reply();
            size_t u = ParseReply(*reply, buffer + used, l - used);
            if (!u) {
                Log(LOG_DEBUG) << "redis: ran out of data to parse";
                delete reply;
                break;
            }
            r.multi_bulk.push_back(reply);
            used += u;
        }
        break;
    }
    default:
        Log(LOG_DEBUG) << "redis: unknown reply " << *buffer;
    }

    return used;
}

bool RedisSocket::Read(const char *buffer, size_t l) {
    static std::vector<char> save;
    std::vector<char> copy;

    if (!save.empty()) {
        std::copy(buffer, buffer + l, std::back_inserter(save));

        copy = save;

        buffer = &copy[0];
        l = copy.size();
    }

    while (l) {
        static Reply r;

        size_t used = this->ParseReply(r, buffer, l);
        if (!used) {
            Log(LOG_DEBUG) << "redis: used == 0 ?";
            r.Clear();
            break;
        } else if (used > l) {
            Log(LOG_DEBUG) << "redis: used > l ?";
            r.Clear();
            break;
        }

        /* Full result is not here yet */
        if (r.type == Reply::MULTI_BULK
                && static_cast<unsigned>(r.multi_bulk_size) != r.multi_bulk.size()) {
            buffer += used;
            l -= used;
            break;
        }

        if (this == provider->sub) {
            if (r.multi_bulk.size() == 4) {
                /* pmessage
                 * pattern subscribed to
                 * __keyevent@0__:set
                 * key
                 */
                std::map<Anope::string, Interface *>::iterator it = this->subinterfaces.find(
                            r.multi_bulk[1]->bulk);
                if (it != this->subinterfaces.end()) {
                    it->second->OnResult(r);
                }
            }
        } else {
            if (this->interfaces.empty()) {
                Log(LOG_DEBUG) << "redis: no interfaces?";
            } else {
                Interface *i = this->interfaces.front();
                this->interfaces.pop_front();

                if (i) {
                    if (r.type != Reply::NOT_OK) {
                        i->OnResult(r);
                    } else {
                        i->OnError(r.bulk);
                    }
                }
            }
        }

        buffer += used;
        l -= used;

        r.Clear();
    }

    if (l) {
        save.resize(l);
        std::copy(buffer, buffer + l, save.begin());
    } else {
        std::vector<char>().swap(save);
    }

    return true;
}


class ModuleRedis : public Module {
    std::map<Anope::string, MyRedisService *> services;

  public:
    ModuleRedis(const Anope::string &modname,
                const Anope::string &creator) : Module(modname, creator, EXTRA | VENDOR) {
    }

    ~ModuleRedis() {
        for (std::map<Anope::string, MyRedisService *>::iterator it = services.begin();
                it != services.end(); ++it) {
            MyRedisService *p = it->second;

            delete p->sock;
            p->sock = NULL;
            delete p->sub;
            p->sub = NULL;

            delete p;
        }
    }

    void OnReload(Configuration::Conf *conf) anope_override {
        Configuration::Block *block = conf->GetModule(this);
        std::vector<Anope::string> new_services;

        for (int i = 0; i < block->CountBlock("redis"); ++i) {
            Configuration::Block *redis = block->GetBlock("redis", i);

            const Anope::string &n = redis->Get<const Anope::string>("name"),
            &ip = redis->Get<const Anope::string>("ip");
            int port = redis->Get<int>("port");
            unsigned db = redis->Get<unsigned>("db");

            delete services[n];
            services[n] = new MyRedisService(this, n, ip, port, db);
            new_services.push_back(n);
        }

        for (std::map<Anope::string, MyRedisService *>::iterator it = services.begin(); it != services.end();) {
            Provider *p = it->second;
            ++it;

            if (std::find(new_services.begin(), new_services.end(),
                          p->name) == new_services.end()) {
                delete it->second;
            }
        }
    }

    void OnModuleUnload(User *, Module *m) anope_override {
        for (std::map<Anope::string, MyRedisService *>::iterator it = services.begin(); it != services.end(); ++it) {
            MyRedisService *p = it->second;

            if (p->sock)
                for (unsigned i = p->sock->interfaces.size(); i > 0; --i) {
                    Interface *inter = p->sock->interfaces[i - 1];

                    if (inter && inter->owner == m) {
                        inter->OnError(m->name + " being unloaded");
                        p->sock->interfaces.erase(p->sock->interfaces.begin() + i - 1);
                    }
                }

            if (p->sub)
                for (unsigned i = p->sub->interfaces.size(); i > 0; --i) {
                    Interface *inter = p->sub->interfaces[i - 1];

                    if (inter && inter->owner == m) {
                        inter->OnError(m->name + " being unloaded");
                        p->sub->interfaces.erase(p->sub->interfaces.begin() + i - 1);
                    }
                }

            for (unsigned i = p->ti.interfaces.size(); i > 0; --i) {
                Interface *inter = p->ti.interfaces[i - 1];

                if (inter && inter->owner == m) {
                    inter->OnError(m->name + " being unloaded");
                    p->ti.interfaces.erase(p->ti.interfaces.begin() + i - 1);
                }
            }
        }
    }
};

MODULE_INIT(ModuleRedis)
