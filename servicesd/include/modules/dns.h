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

#ifndef DNS_H
#define DNS_H

namespace DNS {
/** Valid query types
 */
enum QueryType {
    /* Nothing */
    QUERY_NONE,
    /* A simple A lookup */
    QUERY_A = 1,
    /* An authoritative name server */
    QUERY_NS = 2,
    /* A CNAME lookup */
    QUERY_CNAME = 5,
    /* Start of a zone of authority */
    QUERY_SOA = 6,
    /* Reverse DNS lookup */
    QUERY_PTR = 12,
    /* IPv6 AAAA lookup */
    QUERY_AAAA = 28,
    /* Zone transfer */
    QUERY_AXFR = 252,
    /* A lookup for any record */
    QUERY_ANY = 255
};

/** Flags that can be AND'd into DNSPacket::flags to receive certain values
 */
enum {
    QUERYFLAGS_QR = 0x8000,
    QUERYFLAGS_OPCODE = 0x7800,
    QUERYFLAGS_OPCODE_NOTIFY = 0x2000,
    QUERYFLAGS_AA = 0x400,
    QUERYFLAGS_TC = 0x200,
    QUERYFLAGS_RD = 0x100,
    QUERYFLAGS_RA = 0x80,
    QUERYFLAGS_Z = 0x70,
    QUERYFLAGS_RCODE = 0xF
};

enum Error {
    ERROR_NONE,
    ERROR_UNKNOWN,
    ERROR_UNLOADED,
    ERROR_TIMEDOUT,
    ERROR_NOT_AN_ANSWER,
    ERROR_NONSTANDARD_QUERY,
    ERROR_FORMAT_ERROR,
    ERROR_SERVER_FAILURE,
    ERROR_DOMAIN_NOT_FOUND,
    ERROR_NOT_IMPLEMENTED,
    ERROR_REFUSED,
    ERROR_NO_RECORDS,
    ERROR_INVALIDTYPE
};

struct Question {
    Anope::string name;
    QueryType type;
    unsigned short qclass;

    Question() : type(QUERY_NONE), qclass(0) { }
    Question(const Anope::string &n, QueryType t, unsigned short c = 1) : name(n),
        type(t), qclass(c) { }
    inline bool operator==(const Question & other) const {
        return name == other.name && type == other.type && qclass == other.qclass;
    }

    struct hash {
        size_t operator()(const Question &q) const {
            return Anope::hash_ci()(q.name);
        }
    };
};

struct ResourceRecord : Question {
    unsigned int ttl;
    Anope::string rdata;
    time_t created;

    ResourceRecord(const Anope::string &n, QueryType t,
                   unsigned short c = 1) : Question(n, t, c), ttl(0), created(Anope::CurTime) { }
    ResourceRecord(const Question &q) : Question(q), ttl(0),
        created(Anope::CurTime) { }
};

struct Query {
    std::vector<Question> questions;
    std::vector<ResourceRecord> answers, authorities, additional;
    Error error;

    Query() : error(ERROR_NONE) { }
    Query(const Question &q) : error(ERROR_NONE) {
        questions.push_back(q);
    }
};

class ReplySocket;
class Request;

/** DNS manager
 */
class Manager : public Service {
  public:
    Manager(Module *creator) : Service(creator, "DNS::Manager", "dns/manager") { }
    virtual ~Manager() { }

    virtual void Process(Request *req) = 0;
    virtual void RemoveRequest(Request *req) = 0;

    virtual bool HandlePacket(ReplySocket *s, const unsigned char *const data,
                              int len, sockaddrs *from) = 0;

    virtual void UpdateSerial() = 0;
    virtual void Notify(const Anope::string &zone) = 0;
    virtual uint32_t GetSerial() const = 0;
};

/** A DNS query.
 */
class Request : public Timer, public Question {
    Manager *manager;
  public:
    /* Use result cache if available */
    bool use_cache;
    /* Request id */
    unsigned short id;
    /* Creator of this request */
    Module *creator;

    Request(Manager *mgr, Module *c, const Anope::string &addr, QueryType qt,
            bool cache = false) : Timer(0), Question(addr, qt), manager(mgr),
        use_cache(cache), id(0), creator(c) { }

    virtual ~Request() {
        manager->RemoveRequest(this);
    }

    /** Called when this request succeeds
     * @param r The query sent back from the nameserver
     */
    virtual void OnLookupComplete(const Query *r) = 0;

    /** Called when this request fails or times out.
     * @param r The query sent back from the nameserver, check the error code.
     */
    virtual void OnError(const Query *r) { }

    /** Used to time out the query, xalls OnError and lets the TimerManager
     * delete this request.
     */
    void Tick(time_t) anope_override {
        Log(LOG_DEBUG_2) << "Resolver: timeout for query " << this->name;
        Query rr(*this);
        rr.error = ERROR_TIMEDOUT;
        this->OnError(&rr);
    }
};

} // namespace DNS

#endif // DNS_H
