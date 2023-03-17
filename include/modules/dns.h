/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2017, 2019, 2021 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2014-2015 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2013, 2015-2016 Adam <Adam@anope.org>
 *
 * This file is part of InspIRCd.  InspIRCd is free software: you can
 * redistribute it and/or modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation, version 2.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

namespace DNS {
/** Valid query types
 */
enum QueryType {
    /* Nothing */
    QUERY_NONE,
    /* A simple A lookup */
    QUERY_A = 1,
    /* A CNAME lookup */
    QUERY_CNAME = 5,
    /* Reverse DNS lookup */
    QUERY_PTR = 12,
    /* TXT */
    QUERY_TXT = 16,
    /* IPv6 AAAA lookup */
    QUERY_AAAA = 28
};

/** Flags that can be AND'd into DNSPacket::flags to receive certain values
 */
enum {
    QUERYFLAGS_QR = 0x8000,
    QUERYFLAGS_OPCODE = 0x7800,
    QUERYFLAGS_AA = 0x400,
    QUERYFLAGS_TC = 0x200,
    QUERYFLAGS_RD = 0x100,
    QUERYFLAGS_RA = 0x80,
    QUERYFLAGS_Z = 0x70,
    QUERYFLAGS_RCODE = 0xF
};

enum Error {
    ERROR_NONE,
    ERROR_DISABLED,
    ERROR_UNKNOWN,
    ERROR_UNLOADED,
    ERROR_TIMEDOUT,
    ERROR_MALFORMED,
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

typedef uint16_t RequestId;

const int PORT = 53;

class Exception : public ModuleException {
  public:
    Exception(const std::string& message) : ModuleException(message) { }
};

struct Question {
    std::string name;
    QueryType type;

    Question() : type(QUERY_NONE) { }
    Question(const std::string& n, QueryType t) : name(n), type(t) { }
    bool operator==(const Question& other) const {
        return ((name == other.name) && (type == other.type));
    }
    bool operator!=(const Question& other) const {
        return (!(*this == other));
    }

    struct hash {
        size_t operator()(const Question& question) const {
            return irc::insensitive()(question.name);
        }
    };
};

struct ResourceRecord : Question {
    unsigned int ttl;
    std::string rdata;
    time_t created;

    ResourceRecord(const std::string& n, QueryType t) : Question(n, t), ttl(0),
        created(ServerInstance->Time()) { }
    ResourceRecord(const Question& question) : Question(question), ttl(0),
        created(ServerInstance->Time()) { }
};

struct Query {
    Question question;
    std::vector<ResourceRecord> answers;
    Error error;
    bool cached;

    Query() : error(ERROR_NONE), cached(false) { }
    Query(const Question& q) : question(q), error(ERROR_NONE), cached(false) { }

    const ResourceRecord* FindAnswerOfType(QueryType qtype) const {
        for (std::vector<DNS::ResourceRecord>::const_iterator i = answers.begin();
                i != answers.end(); ++i) {
            const DNS::ResourceRecord& rr = *i;
            if (rr.type == qtype) {
                return &rr;
            }
        }

        return NULL;
    }
};

class ReplySocket;
class Request;

/** DNS manager
 */
class Manager : public DataProvider {
  public:
    Manager(Module* mod) : DataProvider(mod, "DNS") { }

    virtual void Process(Request* req) = 0;
    virtual void RemoveRequest(Request* req) = 0;
    virtual std::string GetErrorStr(Error) = 0;
    virtual std::string GetTypeStr(QueryType) = 0;
};

/** A DNS query.
 */
class Request : public Timer {
  protected:
    Manager* const manager;
  public:
    Question question;
    /* Use result cache if available */
    bool use_cache;
    /* Request id */
    RequestId id;
    /* Creator of this request */
    Module* const creator;

    Request(Manager* mgr, Module* mod, const std::string& addr, QueryType qt,
            bool usecache = true, unsigned int timeout = 0)
        : Timer(timeout ? timeout :
                ServerInstance->Config->ConfValue("dns")->getDuration("timeout", 5, 1))
        , manager(mgr)
        , question(addr, qt)
        , use_cache(usecache)
        , id(0)
        , creator(mod) {
    }

    virtual ~Request() {
        manager->RemoveRequest(this);
    }

    /** Called when this request succeeds
     * @param req The query sent back from the nameserver
     */
    virtual void OnLookupComplete(const Query* req) = 0;

    /** Called when this request fails or times out.
     * @param req The query sent back from the nameserver, check the error code.
     */
    virtual void OnError(const Query* req) { }

    /** Used to time out the query, calls OnError and asks the TimerManager
     * to delete this request
     */
    bool Tick(time_t now) CXX11_OVERRIDE {
        Query rr(this->question);
        rr.error = ERROR_TIMEDOUT;
        this->OnError(&rr);
        delete this;
        return false;
    }
};

} // namespace DNS
