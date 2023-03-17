/*
 *
 * (C) 2003-2023 Anope Team
 * Contact us at team@anope.org
 *
 * Please read COPYING and README for further details.
 */

#include "module.h"
#include "modules/dns.h"

using namespace DNS;

static ServiceReference<XLineManager> akills("XLineManager",
        "xlinemanager/sgline");
static ServiceReference<Manager> dnsmanager("DNS::Manager", "dns/manager");

struct Blacklist {
    struct Reply {
        int code;
        Anope::string reason;
        bool allow_account;

        Reply() : code(0), allow_account(false) { }
    };

    Anope::string name;
    time_t bantime;
    Anope::string reason;
    std::vector<Reply> replies;

    Blacklist() : bantime(0) { }

    Reply *Find(int code) {
        for (unsigned int i = 0; i < replies.size(); ++i)
            if (replies[i].code == code) {
                return &replies[i];
            }
        return NULL;
    }
};

class DNSBLResolver : public Request {
    Reference<User> user;
    Blacklist blacklist;
    bool add_to_akill;

  public:
    DNSBLResolver(Module *c, User *u, const Blacklist &b, const Anope::string &host,
                  bool add_akill) : Request(dnsmanager, c, host, QUERY_A, true), user(u),
        blacklist(b), add_to_akill(add_akill) { }

    void OnLookupComplete(const Query *record) anope_override {
        if (!user || user->Quitting()) {
            return;
        }

        const ResourceRecord &ans_record = record->answers[0];
        // Replies should be in 127.0.0.0/8
        if (ans_record.rdata.find("127.") != 0) {
            return;
        }

        sockaddrs sresult;
        sresult.pton(AF_INET, ans_record.rdata);
        int result = sresult.sa4.sin_addr.s_addr >> 24;

        Blacklist::Reply *reply = blacklist.Find(result);
        if (!blacklist.replies.empty() && !reply) {
            return;
        }

        if (reply && reply->allow_account && user->Account()) {
            return;
        }

        Anope::string reason = this->blacklist.reason, addr = user->ip.addr();
        reason = reason.replace_all_cs("%n", user->nick);
        reason = reason.replace_all_cs("%u", user->GetIdent());
        reason = reason.replace_all_cs("%g", user->realname);
        reason = reason.replace_all_cs("%h", user->host);
        reason = reason.replace_all_cs("%i", addr);
        reason = reason.replace_all_cs("%r", reply ? reply->reason : "");
        reason = reason.replace_all_cs("%N", Config->GetBlock("networkinfo")->Get<const Anope::string>("networkname"));

        BotInfo *OperServ = Config->GetClient("OperServ");
        Log(creator, "dnsbl", OperServ) << user->GetMask() << " (" << addr << ") appears in " << this->blacklist.name;
        XLine *x = new XLine("*@" + addr, OperServ ? OperServ->nick : "m_dnsbl", Anope::CurTime + this->blacklist.bantime, reason, XLineManager::GenerateUID());
        if (this->add_to_akill && akills) {
            akills->AddXLine(x);
            akills->Send(NULL, x);
        } else {
            IRCD->SendAkill(NULL, x);
            delete x;
        }
    }
};

class ModuleDNSBL : public Module {
    std::vector<Blacklist> blacklists;
    std::set<cidr> exempts;
    bool check_on_connect;
    bool check_on_netburst;
    bool add_to_akill;

  public:
    ModuleDNSBL(const Anope::string &modname,
                const Anope::string &creator) : Module(modname, creator, VENDOR | EXTRA) {

    }

    void OnReload(Configuration::Conf *conf) anope_override {
        Configuration::Block *block = conf->GetModule(this);
        this->check_on_connect = block->Get<bool>("check_on_connect");
        this->check_on_netburst = block->Get<bool>("check_on_netburst");
        this->add_to_akill = block->Get<bool>("add_to_akill", "yes");

        this->blacklists.clear();
        for (int i = 0; i < block->CountBlock("blacklist"); ++i) {
            Configuration::Block *bl = block->GetBlock("blacklist", i);
            Blacklist blacklist;

            blacklist.name = bl->Get<Anope::string>("name");
            if (blacklist.name.empty()) {
                continue;
            }
            blacklist.bantime = bl->Get<time_t>("time", "4h");
            blacklist.reason = bl->Get<Anope::string>("reason");

            for (int j = 0; j < bl->CountBlock("reply"); ++j) {
                Configuration::Block *reply = bl->GetBlock("reply", j);
                Blacklist::Reply r;

                r.code = reply->Get<int>("code");
                r.reason = reply->Get<Anope::string>("reason");
                r.allow_account = reply->Get<bool>("allow_account");

                blacklist.replies.push_back(r);
            }

            this->blacklists.push_back(blacklist);
        }

        this->exempts.clear();
        for (int i = 0; i < block->CountBlock("exempt"); ++i) {
            Configuration::Block *bl = block->GetBlock("exempt", i);
            this->exempts.insert(bl->Get<Anope::string>("ip"));
        }
    }

    void OnUserConnect(User *user, bool &exempt) anope_override {
        if (exempt || user->Quitting() || (!this->check_on_connect && !Me->IsSynced()) || !dnsmanager) {
            return;
        }

        if (!this->check_on_netburst && !user->server->IsSynced()) {
            return;
        }

        if (!user->ip.valid())
            /* User doesn't have a valid IP (spoof/etc) */
        {
            return;
        }

        if (this->blacklists.empty()) {
            return;
        }

        if (this->exempts.count(user->ip.addr())) {
            Log(LOG_DEBUG) << "User " << user->nick << " is exempt from dnsbl check - ip: "
                           << user->ip.addr();
            return;
        }

        Anope::string reverse = user->ip.reverse();

        for (unsigned i = 0; i < this->blacklists.size(); ++i) {
            const Blacklist &b = this->blacklists[i];

            Anope::string dnsbl_host = reverse + "." + b.name;
            DNSBLResolver *res = NULL;
            try {
                res = new DNSBLResolver(this, user, b, dnsbl_host, this->add_to_akill);
                dnsmanager->Process(res);
            } catch (const SocketException &ex) {
                delete res;
                Log(this) << ex.GetReason();
            }
        }
    }
};

MODULE_INIT(ModuleDNSBL)
