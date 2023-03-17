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

#include "module.h"
#include "modules/dns.h"

using namespace DNS;

namespace {
Anope::string admin, nameservers;
int refresh;
time_t timeout;
}

/** A full packet sent or received to/from the nameserver
 */
class Packet : public Query {
    static bool IsValidName(const Anope::string &name) {
        return name.find_first_not_of("0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ.-")
               == Anope::string::npos;
    }

    void PackName(unsigned char *output, unsigned short output_size,
                  unsigned short &pos, const Anope::string &name) {
        if (pos + name.length() + 2 > output_size) {
            throw SocketException("Unable to pack name");
        }

        Log(LOG_DEBUG_2) << "Resolver: PackName packing " << name;

        sepstream sep(name, '.');
        Anope::string token;

        while (sep.GetToken(token)) {
            output[pos++] = token.length();
            memcpy(&output[pos], token.c_str(), token.length());
            pos += token.length();
        }

        output[pos++] = 0;
    }

    Anope::string UnpackName(const unsigned char *input, unsigned short input_size,
                             unsigned short &pos) {
        Anope::string name;
        unsigned short pos_ptr = pos, lowest_ptr = input_size;
        bool compressed = false;

        if (pos_ptr >= input_size) {
            throw SocketException("Unable to unpack name - no input");
        }

        while (input[pos_ptr] > 0) {
            unsigned short offset = input[pos_ptr];

            if (offset & POINTER) {
                if ((offset & POINTER) != POINTER) {
                    throw SocketException("Unable to unpack name - bogus compression header");
                }
                if (pos_ptr + 1 >= input_size) {
                    throw SocketException("Unable to unpack name - bogus compression header");
                }

                /* Place pos at the second byte of the first (farthest) compression pointer */
                if (compressed == false) {
                    ++pos;
                    compressed = true;
                }

                pos_ptr = (offset & LABEL) << 8 | input[pos_ptr + 1];

                /* Pointers can only go back */
                if (pos_ptr >= lowest_ptr) {
                    throw SocketException("Unable to unpack name - bogus compression pointer");
                }
                lowest_ptr = pos_ptr;
            } else {
                if (pos_ptr + offset + 1 >= input_size) {
                    throw SocketException("Unable to unpack name - offset too large");
                }
                if (!name.empty()) {
                    name += ".";
                }
                for (unsigned i = 1; i <= offset; ++i) {
                    name += input[pos_ptr + i];
                }

                pos_ptr += offset + 1;
                if (compressed == false)
                    /* Move up pos */
                {
                    pos = pos_ptr;
                }
            }
        }

        /* +1 pos either to one byte after the compression pointer or one byte after the ending \0 */
        ++pos;

        /* Empty names are valid (root domain) */

        Log(LOG_DEBUG_2) << "Resolver: UnpackName successfully unpacked " << name;

        return name;
    }

    Question UnpackQuestion(const unsigned char *input, unsigned short input_size,
                            unsigned short &pos) {
        Question question;

        question.name = this->UnpackName(input, input_size, pos);

        if (pos + 4 > input_size) {
            throw SocketException("Unable to unpack question");
        }

        if (!IsValidName(question.name)) {
            throw SocketException("Invalid question name");
        }

        question.type = static_cast<QueryType>(input[pos] << 8 | input[pos + 1]);
        pos += 2;

        question.qclass = input[pos] << 8 | input[pos + 1];
        pos += 2;

        return question;
    }

    ResourceRecord UnpackResourceRecord(const unsigned char *input,
                                        unsigned short input_size, unsigned short &pos) {
        ResourceRecord record = static_cast<ResourceRecord>(this->UnpackQuestion(input,
                                input_size, pos));

        if (pos + 6 > input_size) {
            throw SocketException("Unable to unpack resource record");
        }

        record.ttl = (input[pos] << 24) | (input[pos + 1] << 16) |
                     (input[pos + 2] << 8) | input[pos + 3];
        pos += 4;

        //record.rdlength = input[pos] << 8 | input[pos + 1];
        pos += 2;

        switch (record.type) {
        case QUERY_A: {
            if (pos + 4 > input_size) {
                throw SocketException("Unable to unpack resource record");
            }

            in_addr a;
            a.s_addr = input[pos] | (input[pos + 1] << 8) | (input[pos + 2] << 16)  |
                       (input[pos + 3] << 24);
            pos += 4;

            sockaddrs addrs;
            addrs.ntop(AF_INET, &a);
            if (!addrs.valid()) {
                throw SocketException("Invalid IP");
            }

            record.rdata = addrs.addr();
            break;
        }
        case QUERY_AAAA: {
            if (pos + 16 > input_size) {
                throw SocketException("Unable to unpack resource record");
            }

            in6_addr a;
            for (int j = 0; j < 16; ++j) {
                a.s6_addr[j] = input[pos + j];
            }
            pos += 16;

            sockaddrs addrs;
            addrs.ntop(AF_INET6, &a);
            if (!addrs.valid()) {
                throw SocketException("Invalid IP");
            }

            record.rdata = addrs.addr();
            break;
        }
        case QUERY_CNAME:
        case QUERY_PTR: {
            record.rdata = this->UnpackName(input, input_size, pos);

            if (!IsValidName(record.rdata)) {
                throw SocketException("Invalid cname/ptr record data");
            }

            break;
        }
        default:
            break;
        }

        Log(LOG_DEBUG_2) << "Resolver: " << record.name << " -> " << record.rdata;

        return record;
    }

  public:
    static const int POINTER = 0xC0;
    static const int LABEL = 0x3F;
    static const int HEADER_LENGTH = 12;

    Manager *manager;
    /* Source or destination of the packet */
    sockaddrs addr;
    /* ID for this packet */
    unsigned short id;
    /* Flags on the packet */
    unsigned short flags;

    Packet(Manager *m, sockaddrs *a) : manager(m), id(0), flags(0) {
        if (a) {
            addr = *a;
        }
    }

    void Fill(const unsigned char *input, const unsigned short len) {
        if (len < HEADER_LENGTH) {
            throw SocketException("Unable to fill packet");
        }

        unsigned short packet_pos = 0;

        this->id = (input[packet_pos] << 8) | input[packet_pos + 1];
        packet_pos += 2;

        this->flags = (input[packet_pos] << 8) | input[packet_pos + 1];
        packet_pos += 2;

        unsigned short qdcount = (input[packet_pos] << 8) | input[packet_pos + 1];
        packet_pos += 2;

        unsigned short ancount = (input[packet_pos] << 8) | input[packet_pos + 1];
        packet_pos += 2;

        unsigned short nscount = (input[packet_pos] << 8) | input[packet_pos + 1];
        packet_pos += 2;

        unsigned short arcount = (input[packet_pos] << 8) | input[packet_pos + 1];
        packet_pos += 2;

        Log(LOG_DEBUG_2) << "Resolver: qdcount: " << qdcount << " ancount: " << ancount
                         << " nscount: " << nscount << " arcount: " << arcount;

        for (unsigned i = 0; i < qdcount; ++i) {
            this->questions.push_back(this->UnpackQuestion(input, len, packet_pos));
        }

        for (unsigned i = 0; i < ancount; ++i) {
            this->answers.push_back(this->UnpackResourceRecord(input, len, packet_pos));
        }

        try {
            for (unsigned i = 0; i < nscount; ++i) {
                this->authorities.push_back(this->UnpackResourceRecord(input, len,
                                            packet_pos));
            }

            for (unsigned i = 0; i < arcount; ++i) {
                this->additional.push_back(this->UnpackResourceRecord(input, len, packet_pos));
            }
        } catch (const SocketException &ex) {
            Log(LOG_DEBUG_2) << "Unable to parse ns/ar records: " << ex.GetReason();
        }
    }

    unsigned short Pack(unsigned char *output, unsigned short output_size) {
        if (output_size < HEADER_LENGTH) {
            throw SocketException("Unable to pack packet");
        }

        unsigned short pos = 0;

        output[pos++] = this->id >> 8;
        output[pos++] = this->id & 0xFF;
        output[pos++] = this->flags >> 8;
        output[pos++] = this->flags & 0xFF;
        output[pos++] = this->questions.size() >> 8;
        output[pos++] = this->questions.size() & 0xFF;
        output[pos++] = this->answers.size() >> 8;
        output[pos++] = this->answers.size() & 0xFF;
        output[pos++] = this->authorities.size() >> 8;
        output[pos++] = this->authorities.size() & 0xFF;
        output[pos++] = this->additional.size() >> 8;
        output[pos++] = this->additional.size() & 0xFF;

        for (unsigned i = 0; i < this->questions.size(); ++i) {
            Question &q = this->questions[i];

            if (q.type == QUERY_PTR) {
                sockaddrs ip(q.name);
                if (!ip.valid()) {
                    throw SocketException("Invalid IP");
                }

                switch (ip.family()) {
                case AF_INET6:
                    q.name = ip.reverse() + ".ip6.arpa";
                    break;
                case AF_INET:
                    q.name = ip.reverse() + ".in-addr.arpa";
                    break;
                default:
                    throw SocketException("Unsupported IP Family");
                }
            }

            this->PackName(output, output_size, pos, q.name);

            if (pos + 4 >= output_size) {
                throw SocketException("Unable to pack packet");
            }

            short s = htons(q.type);
            memcpy(&output[pos], &s, 2);
            pos += 2;

            s = htons(q.qclass);
            memcpy(&output[pos], &s, 2);
            pos += 2;
        }

        std::vector<ResourceRecord> types[] = { this->answers, this->authorities, this->additional };
        for (int i = 0; i < 3; ++i)
            for (unsigned j = 0; j < types[i].size(); ++j) {
                ResourceRecord &rr = types[i][j];

                this->PackName(output, output_size, pos, rr.name);

                if (pos + 8 >= output_size) {
                    throw SocketException("Unable to pack packet");
                }

                short s = htons(rr.type);
                memcpy(&output[pos], &s, 2);
                pos += 2;

                s = htons(rr.qclass);
                memcpy(&output[pos], &s, 2);
                pos += 2;

                long l = htonl(rr.ttl);
                memcpy(&output[pos], &l, 4);
                pos += 4;

                switch (rr.type) {
                case QUERY_A: {
                    if (pos + 6 > output_size) {
                        throw SocketException("Unable to pack packet");
                    }

                    sockaddrs a(rr.rdata);
                    if (!a.valid()) {
                        throw SocketException("Invalid IP");
                    }

                    s = htons(4);
                    memcpy(&output[pos], &s, 2);
                    pos += 2;

                    memcpy(&output[pos], &a.sa4.sin_addr, 4);
                    pos += 4;
                    break;
                }
                case QUERY_AAAA: {
                    if (pos + 18 > output_size) {
                        throw SocketException("Unable to pack packet");
                    }

                    sockaddrs a(rr.rdata);
                    if (!a.valid()) {
                        throw SocketException("Invalid IP");
                    }

                    s = htons(16);
                    memcpy(&output[pos], &s, 2);
                    pos += 2;

                    memcpy(&output[pos], &a.sa6.sin6_addr, 16);
                    pos += 16;
                    break;
                }
                case QUERY_NS:
                case QUERY_CNAME:
                case QUERY_PTR: {
                    if (pos + 2 >= output_size) {
                        throw SocketException("Unable to pack packet");
                    }

                    unsigned short packet_pos_save = pos;
                    pos += 2;

                    this->PackName(output, output_size, pos, rr.rdata);

                    s = htons(pos - packet_pos_save - 2);
                    memcpy(&output[packet_pos_save], &s, 2);
                    break;
                }
                case QUERY_SOA: {
                    if (pos + 2 >= output_size) {
                        throw SocketException("Unable to pack packet");
                    }

                    unsigned short packet_pos_save = pos;
                    pos += 2;

                    std::vector<Anope::string> ns;
                    spacesepstream(nameservers).GetTokens(ns);
                    this->PackName(output, output_size, pos, !ns.empty() ? ns[0] : "");
                    this->PackName(output, output_size, pos, admin.replace_all_cs('@', '.'));

                    if (pos + 20 >= output_size) {
                        throw SocketException("Unable to pack SOA");
                    }

                    l = htonl(manager->GetSerial());
                    memcpy(&output[pos], &l, 4);
                    pos += 4;

                    l = htonl(refresh); // Refresh
                    memcpy(&output[pos], &l, 4);
                    pos += 4;

                    l = htonl(refresh); // Retry
                    memcpy(&output[pos], &l, 4);
                    pos += 4;

                    l = htonl(604800); // Expire
                    memcpy(&output[pos], &l, 4);
                    pos += 4;

                    l = htonl(0); // Minimum
                    memcpy(&output[pos], &l, 4);
                    pos += 4;

                    s = htons(pos - packet_pos_save - 2);
                    memcpy(&output[packet_pos_save], &s, 2);

                    break;
                }
                default:
                    break;
                }
            }

        return pos;
    }
};

namespace DNS {
class ReplySocket : public virtual Socket {
  public:
    virtual ~ReplySocket() { }
    virtual void Reply(Packet *p) = 0;
};
}

/* Listens for TCP requests */
class TCPSocket : public ListenSocket {
    Manager *manager;

  public:
    /* A TCP client */
    class Client : public ClientSocket, public Timer, public ReplySocket {
        Manager *manager;
        Packet *packet;
        unsigned char packet_buffer[524];
        int length;

      public:
        Client(Manager *m, TCPSocket *l, int fd, const sockaddrs &addr) : Socket(fd,
                    l->IsIPv6()), ClientSocket(l, addr), Timer(5),
            manager(m), packet(NULL), length(0) {
            Log(LOG_DEBUG_2) << "Resolver: New client from " << addr.addr();
        }

        ~Client() {
            Log(LOG_DEBUG_2) << "Resolver: Exiting client from " << clientaddr.addr();
            delete packet;
        }

        /* Times out after a few seconds */
        void Tick(time_t) anope_override { }

        void Reply(Packet *p) anope_override {
            delete packet;
            packet = p;
            SocketEngine::Change(this, true, SF_WRITABLE);
        }

        bool ProcessRead() anope_override {
            Log(LOG_DEBUG_2) << "Resolver: Reading from DNS TCP socket";

            int i = recv(this->GetFD(), reinterpret_cast<char *>(packet_buffer) + length, sizeof(packet_buffer) - length, 0);
            if (i <= 0) {
                return false;
            }

            length += i;

            unsigned short want_len = packet_buffer[0] << 8 | packet_buffer[1];
            if (length >= want_len + 2) {
                int len = length - 2;
                length -= want_len + 2;
                return this->manager->HandlePacket(this, packet_buffer + 2, len, NULL);
            }
            return true;
        }

        bool ProcessWrite() anope_override {
            Log(LOG_DEBUG_2) << "Resolver: Writing to DNS TCP socket";

            if (packet != NULL) {
                try {
                    unsigned char buffer[65535];
                    unsigned short len = packet->Pack(buffer + 2, sizeof(buffer) - 2);

                    short s = htons(len);
                    memcpy(buffer, &s, 2);
                    len += 2;

                    send(this->GetFD(), reinterpret_cast<char *>(buffer), len, 0);
                } catch (const SocketException &) { }

                delete packet;
                packet = NULL;
            }

            SocketEngine::Change(this, false, SF_WRITABLE);
            return true; /* Do not return false here, bind is unhappy we close the connection so soon after sending */
        }
    };

    TCPSocket(Manager *m, const Anope::string &ip, int port) : Socket(-1,
                ip.find(':') != Anope::string::npos), ListenSocket(ip, port,
                        ip.find(':') != Anope::string::npos), manager(m) { }

    ClientSocket *OnAccept(int fd, const sockaddrs &addr) anope_override {
        return new Client(this->manager, this, fd, addr);
    }
};

/* Listens for UDP requests */
class UDPSocket : public ReplySocket {
    Manager *manager;
    std::deque<Packet *> packets;

  public:
    UDPSocket(Manager *m, const Anope::string &ip, int port) : Socket(-1,
                ip.find(':') != Anope::string::npos, SOCK_DGRAM), manager(m) { }

    ~UDPSocket() {
        for (unsigned i = 0; i < packets.size(); ++i) {
            delete packets[i];
        }
    }

    void Reply(Packet *p) anope_override {
        packets.push_back(p);
        SocketEngine::Change(this, true, SF_WRITABLE);
    }

    std::deque<Packet *>& GetPackets() {
        return packets;
    }

    bool ProcessRead() anope_override {
        Log(LOG_DEBUG_2) << "Resolver: Reading from DNS UDP socket";

        unsigned char packet_buffer[524];
        sockaddrs from_server;
        socklen_t x = sizeof(from_server);
        int length = recvfrom(this->GetFD(), reinterpret_cast<char *>(&packet_buffer), sizeof(packet_buffer), 0, &from_server.sa, &x);
        return this->manager->HandlePacket(this, packet_buffer, length, &from_server);
    }

    bool ProcessWrite() anope_override {
        Log(LOG_DEBUG_2) << "Resolver: Writing to DNS UDP socket";

        Packet *r = !packets.empty() ? packets.front() : NULL;
        if (r != NULL) {
            try {
                unsigned char buffer[524];
                unsigned short len = r->Pack(buffer, sizeof(buffer));

                sendto(this->GetFD(), reinterpret_cast<char *>(buffer), len, 0, &r->addr.sa,
                       r->addr.size());
            } catch (const SocketException &) { }

            delete r;
            packets.pop_front();
        }

        if (packets.empty()) {
            SocketEngine::Change(this, false, SF_WRITABLE);
        }

        return true;
    }
};

class NotifySocket : public Socket {
    Packet *packet;
  public:
    NotifySocket(bool v6, Packet *p) : Socket(-1, v6, SOCK_DGRAM), packet(p) {
        SocketEngine::Change(this, false, SF_READABLE);
        SocketEngine::Change(this, true, SF_WRITABLE);
    }

    bool ProcessWrite() anope_override {
        if (!packet) {
            return false;
        }

        Log(LOG_DEBUG_2) << "Resolver: Notifying slave " << packet->addr.addr();

        try {
            unsigned char buffer[524];
            unsigned short len = packet->Pack(buffer, sizeof(buffer));

            sendto(this->GetFD(), reinterpret_cast<char *>(buffer), len, 0,
                   &packet->addr.sa, packet->addr.size());
        } catch (const SocketException &) { }

        delete packet;
        packet = NULL;

        return false;
    }
};

class MyManager : public Manager, public Timer {
    uint32_t serial;

    typedef TR1NS::unordered_map<Question, Query, Question::hash> cache_map;
    cache_map cache;

    TCPSocket *tcpsock;
    UDPSocket *udpsock;

    bool listen;
    sockaddrs addrs;

    std::vector<std::pair<Anope::string, short> > notify;
  public:
    std::map<unsigned short, Request *> requests;

    MyManager(Module *creator) : Manager(creator), Timer(300, Anope::CurTime, true),
        serial(Anope::CurTime), tcpsock(NULL), udpsock(NULL),
        listen(false), cur_id(rand()) {
    }

    ~MyManager() {
        delete udpsock;
        delete tcpsock;

        for (std::map<unsigned short, Request *>::iterator it = this->requests.begin(),
                it_end = this->requests.end(); it != it_end;) {
            Request *request = it->second;
            ++it;

            Query rr(*request);
            rr.error = ERROR_UNKNOWN;
            request->OnError(&rr);

            delete request;
        }
        this->requests.clear();

        this->cache.clear();
    }

    void SetIPPort(const Anope::string &nameserver, const Anope::string &ip,
                   unsigned short port, std::vector<std::pair<Anope::string, short> > n) {
        delete udpsock;
        delete tcpsock;

        udpsock = NULL;
        tcpsock = NULL;

        try {
            this->addrs.pton(nameserver.find(':') != Anope::string::npos ? AF_INET6 :
                             AF_INET, nameserver, 53);

            udpsock = new UDPSocket(this, ip, port);

            if (!ip.empty()) {
                udpsock->Bind(ip, port);
                tcpsock = new TCPSocket(this, ip, port);
                listen = true;
            }
        } catch (const SocketException &ex) {
            Log() << "Unable to bind dns to " << ip << ":" << port << ": " <<
                  ex.GetReason();
        }

        notify = n;
    }

  private:
    unsigned short cur_id;

    unsigned short GetID() {
        if (this->udpsock->GetPackets().size() == 65535) {
            throw SocketException("DNS queue full");
        }

        do {
            cur_id = (cur_id + 1) & 0xFFFF;
        } while (!cur_id || this->requests.count(cur_id));

        return cur_id;
    }

  public:
    void Process(Request *req) anope_override {
        Log(LOG_DEBUG_2) << "Resolver: Processing request to lookup " << req->name << ", of type " << req->type;

        if (req->use_cache && this->CheckCache(req)) {
            Log(LOG_DEBUG_2) << "Resolver: Using cached result";
            delete req;
            return;
        }

        if (!this->udpsock) {
            throw SocketException("No dns socket");
        }

        req->id = GetID();
        this->requests[req->id] = req;

        req->SetSecs(timeout);

        Packet *p = new Packet(this, &this->addrs);
        p->flags = QUERYFLAGS_RD;
        p->id = req->id;
        p->questions.push_back(*req);

        this->udpsock->Reply(p);
    }

    void RemoveRequest(Request *req) anope_override {
        this->requests.erase(req->id);
    }

    bool HandlePacket(ReplySocket *s, const unsigned char *const packet_buffer,
                      int length, sockaddrs *from) anope_override {
        if (length < Packet::HEADER_LENGTH) {
            return true;
        }

        Packet recv_packet(this, from);

        try {
            recv_packet.Fill(packet_buffer, length);
        } catch (const SocketException &ex) {
            Log(LOG_DEBUG_2) << ex.GetReason();
            return true;
        }

        if (!(recv_packet.flags & QUERYFLAGS_QR)) {
            if (!listen) {
                return true;
            } else if (recv_packet.questions.empty()) {
                Log(LOG_DEBUG_2) << "Resolver: Received a question with no questions?";
                return true;
            }

            Packet *packet = new Packet(recv_packet);
            packet->flags |= QUERYFLAGS_QR; /* This is a response */
            packet->flags |= QUERYFLAGS_AA; /* And we are authoritative */

            packet->answers.clear();
            packet->authorities.clear();
            packet->additional.clear();

            for (unsigned i = 0; i < recv_packet.questions.size(); ++i) {
                const Question& q = recv_packet.questions[i];

                if (q.type == QUERY_AXFR || q.type == QUERY_SOA) {
                    ResourceRecord rr(q.name, QUERY_SOA);
                    packet->answers.push_back(rr);

                    if (q.type == QUERY_AXFR) {
                        Anope::string token;
                        spacesepstream sep(nameservers);
                        while (sep.GetToken(token)) {
                            ResourceRecord rr2(q.name, QUERY_NS);
                            rr2.rdata = token;
                            packet->answers.push_back(rr2);
                        }
                    }
                    break;
                }
            }

            FOREACH_MOD(OnDnsRequest, (recv_packet, packet));

            for (unsigned i = 0; i < recv_packet.questions.size(); ++i) {
                const Question& q = recv_packet.questions[i];

                if (q.type == QUERY_AXFR) {
                    ResourceRecord rr(q.name, QUERY_SOA);
                    packet->answers.push_back(rr);
                    break;
                }
            }

            if (packet->answers.empty() && packet->authorities.empty()
                    && packet->additional.empty() && packet->error == ERROR_NONE) {
                packet->error =
                    ERROR_REFUSED;    // usually safe, won't cause an NXDOMAIN to get cached
            }

            s->Reply(packet);
            return true;
        }

        if (from == NULL) {
            Log(LOG_DEBUG_2) <<
                             "Resolver: Received an answer over TCP. This is not supported.";
            return true;
        } else if (this->addrs != *from) {
            Log(LOG_DEBUG_2) <<
                             "Resolver: Received an answer from the wrong nameserver, Bad NAT or DNS forging attempt? '"
                             << this->addrs.addr() << "' != '" << from->addr() << "'";
            return true;
        }

        std::map<unsigned short, Request *>::iterator it = this->requests.find(recv_packet.id);
        if (it == this->requests.end()) {
            Log(LOG_DEBUG_2) <<
                             "Resolver: Received an answer for something we didn't request";
            return true;
        }
        Request *request = it->second;

        if (recv_packet.flags & QUERYFLAGS_OPCODE) {
            Log(LOG_DEBUG_2) << "Resolver: Received a nonstandard query";
            recv_packet.error = ERROR_NONSTANDARD_QUERY;
            request->OnError(&recv_packet);
        } else if (recv_packet.flags & QUERYFLAGS_RCODE) {
            Error error = ERROR_UNKNOWN;

            switch (recv_packet.flags & QUERYFLAGS_RCODE) {
            case 1:
                Log(LOG_DEBUG_2) << "Resolver: format error";
                error = ERROR_FORMAT_ERROR;
                break;
            case 2:
                Log(LOG_DEBUG_2) << "Resolver: server error";
                error = ERROR_SERVER_FAILURE;
                break;
            case 3:
                Log(LOG_DEBUG_2) << "Resolver: domain not found";
                error = ERROR_DOMAIN_NOT_FOUND;
                break;
            case 4:
                Log(LOG_DEBUG_2) << "Resolver: not implemented";
                error = ERROR_NOT_IMPLEMENTED;
                break;
            case 5:
                Log(LOG_DEBUG_2) << "Resolver: refused";
                error = ERROR_REFUSED;
                break;
            default:
                break;
            }

            recv_packet.error = error;
            request->OnError(&recv_packet);
        } else if (recv_packet.questions.empty() || recv_packet.answers.empty()) {
            Log(LOG_DEBUG_2) << "Resolver: No resource records returned";
            recv_packet.error = ERROR_NO_RECORDS;
            request->OnError(&recv_packet);
        } else {
            Log(LOG_DEBUG_2) << "Resolver: Lookup complete for " << request->name;
            request->OnLookupComplete(&recv_packet);
            this->AddCache(recv_packet);
        }

        delete request;
        return true;
    }

    void UpdateSerial() anope_override {
        serial = Anope::CurTime;
    }

    void Notify(const Anope::string &zone) anope_override {
        /* notify slaves of the update */
        for (unsigned i = 0; i < notify.size(); ++i) {
            const Anope::string &ip = notify[i].first;
            short port = notify[i].second;

            sockaddrs addr;
            addr.pton(ip.find(':') != Anope::string::npos ? AF_INET6 : AF_INET, ip, port);
            if (!addr.valid()) {
                return;
            }

            Packet *packet = new Packet(this, &addr);
            packet->flags = QUERYFLAGS_AA | QUERYFLAGS_OPCODE_NOTIFY;
            try {
                packet->id = GetID();
            } catch (const SocketException &) {
                delete packet;
                continue;
            }

            packet->questions.push_back(Question(zone, QUERY_SOA));

            new NotifySocket(ip.find(':') != Anope::string::npos, packet);
        }
    }

    uint32_t GetSerial() const anope_override {
        return serial;
    }

    void Tick(time_t now) anope_override {
        Log(LOG_DEBUG_2) << "Resolver: Purging DNS cache";

        for (cache_map::iterator it = this->cache.begin(), it_next; it != this->cache.end(); it = it_next) {
            const Query &q = it->second;
            const ResourceRecord &req = q.answers[0];
            it_next = it;
            ++it_next;

            if (req.created + static_cast<time_t>(req.ttl) < now) {
                this->cache.erase(it);
            }
        }
    }

  private:
    /** Add a record to the dns cache
     * @param r The record
     */
    void AddCache(Query &r) {
        const ResourceRecord &rr = r.answers[0];
        Log(LOG_DEBUG_3) << "Resolver cache: added cache for " << rr.name << " -> " <<
                         rr.rdata << ", ttl: " << rr.ttl;
        this->cache[r.questions[0]] = r;
    }

    /** Check the DNS cache to see if request can be handled by a cached result
     * @return true if a cached result was found.
     */
    bool CheckCache(Request *request) {
        cache_map::iterator it = this->cache.find(*request);
        if (it != this->cache.end()) {
            Query &record = it->second;
            Log(LOG_DEBUG_3) << "Resolver: Using cached result for " << request->name;
            request->OnLookupComplete(&record);
            return true;
        }

        return false;
    }

};

class ModuleDNS : public Module {
    MyManager manager;

    Anope::string nameserver;
    Anope::string ip;
    int port;

    std::vector<std::pair<Anope::string, short> > notify;

  public:
    ModuleDNS(const Anope::string &modname,
              const Anope::string &creator) : Module(modname, creator, EXTRA | VENDOR),
        manager(this) {

    }

    ~ModuleDNS() {
        for (std::map<int, Socket *>::const_iterator it = SocketEngine::Sockets.begin(),
                it_end = SocketEngine::Sockets.end(); it != it_end;) {
            Socket *s = it->second;
            ++it;

            if (dynamic_cast<NotifySocket *>(s) || dynamic_cast<TCPSocket::Client *>(s)) {
                delete s;
            }
        }
    }

    void OnReload(Configuration::Conf *conf) anope_override {
        Configuration::Block *block = conf->GetModule(this);

        nameserver = block->Get<const Anope::string>("nameserver", "127.0.0.1");
        timeout =  block->Get<time_t>("timeout", "5");
        ip = block->Get<const Anope::string>("ip", "0.0.0.0");
        port = block->Get<int>("port", "53");
        admin = block->Get<const Anope::string>("admin", "admin@example.com");
        nameservers = block->Get<const Anope::string>("nameservers", "ns1.example.com");
        refresh = block->Get<int>("refresh", "3600");

        for (int i = 0; i < block->CountBlock("notify"); ++i) {
            Configuration::Block *n = block->GetBlock("notify", i);
            Anope::string nip = n->Get<Anope::string>("ip");
            short nport = n->Get<short>("port");

            notify.push_back(std::make_pair(nip, nport));
        }

        if (Anope::IsFile(nameserver)) {
            std::ifstream f(nameserver.c_str());
            bool success = false;

            if (f.is_open()) {
                for (Anope::string server; std::getline(f, server.str());) {
                    if (server.find("nameserver") == 0) {
                        size_t i = server.find_first_of("123456789");
                        if (i != Anope::string::npos) {
                            if (server.substr(i).is_pos_number_only()) {
                                nameserver = server.substr(i);
                                Log(LOG_DEBUG) << "Nameserver set to " << nameserver;
                                success = true;
                                break;
                            }
                        }
                    }
                }

                f.close();
            }

            if (!success) {
                Log() << "Unable to find nameserver, defaulting to 127.0.0.1";
                nameserver = "127.0.0.1";
            }
        }

        try {
            this->manager.SetIPPort(nameserver, ip, port, notify);
        } catch (const SocketException &ex) {
            throw ModuleException(ex.GetReason());
        }
    }

    void OnModuleUnload(User *u, Module *m) anope_override {
        for (std::map<unsigned short, Request *>::iterator it = this->manager.requests.begin(), it_end = this->manager.requests.end(); it != it_end;) {
            unsigned short id = it->first;
            Request *req = it->second;
            ++it;

            if (req->creator == m) {
                Query rr(*req);
                rr.error = ERROR_UNLOADED;
                req->OnError(&rr);

                delete req;
                this->manager.requests.erase(id);
            }
        }
    }
};

MODULE_INIT(ModuleDNS)
