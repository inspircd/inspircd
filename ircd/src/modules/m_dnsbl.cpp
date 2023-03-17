/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2018-2020 Matt Schatz <genius3000@g3k.solutions>
 *   Copyright (C) 2018-2019 linuxdaemon <linuxdaemon.irc@gmail.com>
 *   Copyright (C) 2013, 2017-2022 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2013, 2015-2016 Adam <Adam@anope.org>
 *   Copyright (C) 2012-2016 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012, 2018 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2007 Craig Edwards <brain@inspircd.org>
 *   Copyright (C) 2006-2009 Robin Burchell <robin+git@viroteck.net>
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


#include "inspircd.h"
#include "xline.h"
#include "modules/dns.h"
#include "modules/stats.h"

/* Class holding data for a single entry */
class DNSBLConfEntry : public refcountbase {
  public:
    enum EnumBanaction { I_UNKNOWN, I_KILL, I_ZLINE, I_KLINE, I_GLINE, I_MARK };
    enum EnumType { A_RECORD, A_BITMASK };
    std::string name, ident, host, domain, reason;
    EnumBanaction banaction;
    EnumType type;
    unsigned long duration;
    unsigned int bitmask;
    unsigned int timeout;
    unsigned char records[256];
    unsigned long stats_hits, stats_misses, stats_errors;
    DNSBLConfEntry()
        : type(A_BITMASK)
        , duration(86400)
        , bitmask(0)
        , timeout(0)
        , stats_hits(0)
        , stats_misses(0)
        , stats_errors(0) {
    }
};

typedef SimpleExtItem<std::vector<std::string> > MarkExtItem;

/** Resolver for CGI:IRC hostnames encoded in ident/real name
 */
class DNSBLResolver : public DNS::Request {
  private:
    irc::sockets::sockaddrs theirsa;
    std::string theiruid;
    MarkExtItem& nameExt;
    LocalIntExt& countExt;
    reference<DNSBLConfEntry> ConfEntry;

  public:
    DNSBLResolver(DNS::Manager *mgr, Module *me, MarkExtItem& match,
                  LocalIntExt& ctr, const std::string &hostname, LocalUser* u,
                  reference<DNSBLConfEntry> conf)
        : DNS::Request(mgr, me, hostname, DNS::QUERY_A, true, conf->timeout)
        , theirsa(u->client_sa)
        , theiruid(u->uuid)
        , nameExt(match)
        , countExt(ctr)
        , ConfEntry(conf) {
    }

    /* Note: This may be called multiple times for multiple A record results */
    void OnLookupComplete(const DNS::Query *r) CXX11_OVERRIDE {
        /* Check the user still exists */
        LocalUser* them = IS_LOCAL(ServerInstance->FindUUID(theiruid));
        if (!them || them->client_sa != theirsa) {
            ConfEntry->stats_misses++;
            return;
        }

        int i = countExt.get(them);
        if (i) {
            countExt.set(them, i - 1);
        }

        // The DNSBL reply must contain an A result.
        const DNS::ResourceRecord* const ans_record = r->FindAnswerOfType(DNS::QUERY_A);
        if (!ans_record) {
            ConfEntry->stats_errors++;
            ServerInstance->SNO->WriteGlobalSno('d',
                                                "%s returned an result with no IPv4 address.",
                                                ConfEntry->name.c_str());
            return;
        }

        // The DNSBL reply must be a valid IPv4 address.
        in_addr resultip;
        if (inet_pton(AF_INET, ans_record->rdata.c_str(), &resultip) != 1) {
            ConfEntry->stats_errors++;
            ServerInstance->SNO->WriteGlobalSno('d',
                                                "%s returned an invalid IPv4 address: %s",
                                                ConfEntry->name.c_str(), ans_record->rdata.c_str());
            return;
        }

        // The DNSBL reply should be in the 127.0.0.0/8 range.
        if ((resultip.s_addr & 0xFF) != 127) {
            ConfEntry->stats_errors++;
            ServerInstance->SNO->WriteGlobalSno('d',
                                                "%s returned an IPv4 address which is outside of the 127.0.0.0/8 subnet: %s",
                                                ConfEntry->name.c_str(), ans_record->rdata.c_str());
            return;
        }

        bool match = false;
        unsigned int result = 0;
        switch (ConfEntry->type) {
        case DNSBLConfEntry::A_BITMASK: {
            result = (resultip.s_addr >> 24) & ConfEntry->bitmask;
            match = (result != 0);
            break;
        }
        case DNSBLConfEntry::A_RECORD: {
            result = resultip.s_addr >> 24;
            match = (ConfEntry->records[result] == 1);
            break;
        }
        }

        if (match) {
            std::string reason = ConfEntry->reason;
            std::string::size_type x = reason.find("%ip%");
            while (x != std::string::npos) {
                reason.erase(x, 4);
                reason.insert(x, them->GetIPString());
                x = reason.find("%ip%");
            }

            ConfEntry->stats_hits++;

            switch (ConfEntry->banaction) {
            case DNSBLConfEntry::I_KILL: {
                ServerInstance->Users->QuitUser(them, "Killed (" + reason + ")");
                break;
            }
            case DNSBLConfEntry::I_MARK: {
                if (!ConfEntry->ident.empty()) {
                    them->WriteNotice("Your ident has been set to " + ConfEntry->ident +
                                      " because you matched " + reason);
                    them->ChangeIdent(ConfEntry->ident);
                }

                if (!ConfEntry->host.empty()) {
                    them->WriteNotice("Your host has been set to " + ConfEntry->host +
                                      " because you matched " + reason);
                    them->ChangeDisplayedHost(ConfEntry->host);
                }

                std::vector<std::string>* marks = nameExt.get(them);
                if (!marks) {
                    marks = new std::vector<std::string>();
                    nameExt.set(them, marks);
                }
                marks->push_back(ConfEntry->name);
                break;
            }
            case DNSBLConfEntry::I_KLINE: {
                KLine* kl = new KLine(ServerInstance->Time(), ConfEntry->duration,
                                      MODNAME "@" + ServerInstance->Config->ServerName, reason,
                                      them->GetBanIdent(), them->GetIPString());
                if (ServerInstance->XLines->AddLine(kl,NULL)) {
                    ServerInstance->SNO->WriteToSnoMask('x',
                                                        "%s added a timed K-line on %s, expires in %s (on %s): %s",
                                                        kl->source.c_str(), kl->Displayable().c_str(),
                                                        InspIRCd::DurationString(kl->duration).c_str(),
                                                        InspIRCd::TimeString(kl->expiry).c_str(), kl->reason.c_str());
                    ServerInstance->XLines->ApplyLines();
                } else {
                    delete kl;
                    return;
                }
                break;
            }
            case DNSBLConfEntry::I_GLINE: {
                GLine* gl = new GLine(ServerInstance->Time(), ConfEntry->duration,
                                      MODNAME "@" + ServerInstance->Config->ServerName, reason,
                                      them->GetBanIdent(), them->GetIPString());
                if (ServerInstance->XLines->AddLine(gl,NULL)) {
                    ServerInstance->SNO->WriteToSnoMask('x',
                                                        "%s added a timed G-line on %s, expires in %s (on %s): %s",
                                                        gl->source.c_str(), gl->Displayable().c_str(),
                                                        InspIRCd::DurationString(gl->duration).c_str(),
                                                        InspIRCd::TimeString(gl->expiry).c_str(), gl->reason.c_str());
                    ServerInstance->XLines->ApplyLines();
                } else {
                    delete gl;
                    return;
                }
                break;
            }
            case DNSBLConfEntry::I_ZLINE: {
                ZLine* zl = new ZLine(ServerInstance->Time(), ConfEntry->duration,
                                      MODNAME "@" + ServerInstance->Config->ServerName, reason,
                                      them->GetIPString());
                if (ServerInstance->XLines->AddLine(zl,NULL)) {
                    ServerInstance->SNO->WriteToSnoMask('x',
                                                        "%s added a timed Z-line on %s, expires in %s (on %s): %s",
                                                        zl->source.c_str(), zl->Displayable().c_str(),
                                                        InspIRCd::DurationString(zl->duration).c_str(),
                                                        InspIRCd::TimeString(zl->expiry).c_str(), zl->reason.c_str());
                    ServerInstance->XLines->ApplyLines();
                } else {
                    delete zl;
                    return;
                }
                break;
            }
            case DNSBLConfEntry::I_UNKNOWN:
            default:
                break;
            }

            ServerInstance->SNO->WriteGlobalSno('d',
                                                "Connecting user %s (%s) detected as being on the '%s' DNS blacklist with result %d",
                                                them->GetFullRealHost().c_str(), them->GetIPString().c_str(),
                                                ConfEntry->name.c_str(), result);
        } else {
            ConfEntry->stats_misses++;
        }
    }

    void OnError(const DNS::Query *q) CXX11_OVERRIDE {
        bool is_miss = true;
        switch (q->error) {
        case DNS::ERROR_NO_RECORDS:
        case DNS::ERROR_DOMAIN_NOT_FOUND:
            ConfEntry->stats_misses++;
            break;

        default:
            ConfEntry->stats_errors++;
            is_miss = false;
            break;
        }

        LocalUser* them = IS_LOCAL(ServerInstance->FindUUID(theiruid));
        if (!them || them->client_sa != theirsa) {
            return;
        }

        int i = countExt.get(them);
        if (i) {
            countExt.set(them, i - 1);
        }

        if (is_miss) {
            return;
        }

        ServerInstance->SNO->WriteGlobalSno('d', "An error occurred whilst checking whether %s (%s) is on the '%s' DNS blacklist: %s",
                                            them->GetFullRealHost().c_str(), them->GetIPString().c_str(), ConfEntry->name.c_str(), this->manager->GetErrorStr(q->error).c_str());
    }
};

typedef std::vector<reference<DNSBLConfEntry> > DNSBLConfList;

class ModuleDNSBL : public Module, public Stats::EventListener {
    DNSBLConfList DNSBLConfEntries;
    dynamic_reference<DNS::Manager> DNS;
    MarkExtItem nameExt;
    LocalIntExt countExt;

    /*
     *  Convert a string to EnumBanaction
     */
    DNSBLConfEntry::EnumBanaction str2banaction(const std::string &action) {
        if (stdalgo::string::equalsci(action, "kill")) {
            return DNSBLConfEntry::I_KILL;
        }
        if (stdalgo::string::equalsci(action, "kline")) {
            return DNSBLConfEntry::I_KLINE;
        }
        if (stdalgo::string::equalsci(action, "zline")) {
            return DNSBLConfEntry::I_ZLINE;
        }
        if (stdalgo::string::equalsci(action, "gline")) {
            return DNSBLConfEntry::I_GLINE;
        }
        if (stdalgo::string::equalsci(action, "mark")) {
            return DNSBLConfEntry::I_MARK;
        }
        return DNSBLConfEntry::I_UNKNOWN;
    }
  public:
    ModuleDNSBL()
        : Stats::EventListener(this)
        , DNS(this, "DNS")
        , nameExt("dnsbl_match", ExtensionItem::EXT_USER, this)
        , countExt("dnsbl_pending", ExtensionItem::EXT_USER, this) {
    }

    void init() CXX11_OVERRIDE {
        ServerInstance->SNO->EnableSnomask('d', "DNSBL");
    }

    void Prioritize() CXX11_OVERRIDE {
        Module* corexline = ServerInstance->Modules->Find("core_xline");
        ServerInstance->Modules->SetPriority(this, I_OnSetUserIP, PRIORITY_AFTER, corexline);
    }

    Version GetVersion() CXX11_OVERRIDE {
        return Version("Allows the server administrator to check the IP address of connecting users against a DNSBL.", VF_VENDOR);
    }

    /** Fill our conf vector with data
     */
    void ReadConfig(ConfigStatus& status) CXX11_OVERRIDE {
        DNSBLConfList newentries;

        ConfigTagList dnsbls = ServerInstance->Config->ConfTags("dnsbl");
        for(ConfigIter i = dnsbls.first; i != dnsbls.second; ++i) {
            ConfigTag* tag = i->second;
            reference<DNSBLConfEntry> e = new DNSBLConfEntry();

            e->name = tag->getString("name");
            e->ident = tag->getString("ident");
            e->host = tag->getString("host");
            e->reason = tag->getString("reason", "Your IP has been blacklisted.", 1);
            e->domain = tag->getString("domain");
            e->timeout = tag->getDuration("timeout", 0);

            if (stdalgo::string::equalsci(tag->getString("type"), "bitmask")) {
                e->type = DNSBLConfEntry::A_BITMASK;
                e->bitmask = tag->getUInt("bitmask", 0, 0, UINT_MAX);
            } else {
                memset(e->records, 0, sizeof(e->records));
                e->type = DNSBLConfEntry::A_RECORD;
                irc::portparser portrange(tag->getString("records"), false);
                long item = -1;
                while ((item = portrange.GetToken())) {
                    e->records[item] = 1;
                }
            }

            e->banaction = str2banaction(tag->getString("action"));
            e->duration = tag->getDuration("duration", 60, 1);

            /* Use portparser for record replies */

            /* yeah, logic here is a little messy */
            if ((e->bitmask <= 0) && (DNSBLConfEntry::A_BITMASK == e->type)) {
                throw ModuleException("Invalid <dnsbl:bitmask> at " + tag->getTagLocation());
            } else if (e->name.empty()) {
                throw ModuleException("Empty <dnsbl:name> at " + tag->getTagLocation());
            } else if (e->domain.empty()) {
                throw ModuleException("Empty <dnsbl:domain> at " + tag->getTagLocation());
            } else if (e->banaction == DNSBLConfEntry::I_UNKNOWN) {
                throw ModuleException("Unknown <dnsbl:action> at " + tag->getTagLocation());
            } else {
                /* add it, all is ok */
                newentries.push_back(e);
            }
        }

        DNSBLConfEntries.swap(newentries);
    }

    void OnSetUserIP(LocalUser* user) CXX11_OVERRIDE {
        if (user->exempt || user->quitting || !DNS) {
            return;
        }

        // Clients can't be in a DNSBL if they aren't connected via IPv4 or IPv6.
        if (user->client_sa.family() != AF_INET && user->client_sa.family() != AF_INET6) {
            return;
        }

        if (user->MyClass) {
            if (!user->MyClass->config->getBool("usednsbl", true)) {
                return;
            }
        } else {
            ServerInstance->Logs->Log(MODNAME, LOG_DEBUG,
                                      "User has no connect class in OnSetUserIP");
            return;
        }

        std::string reversedip;
        if (user->client_sa.family() == AF_INET) {
            unsigned int a, b, c, d;
            d = (unsigned int) (user->client_sa.in4.sin_addr.s_addr >> 24) & 0xFF;
            c = (unsigned int) (user->client_sa.in4.sin_addr.s_addr >> 16) & 0xFF;
            b = (unsigned int) (user->client_sa.in4.sin_addr.s_addr >> 8) & 0xFF;
            a = (unsigned int) user->client_sa.in4.sin_addr.s_addr & 0xFF;

            reversedip = ConvToStr(d) + "." + ConvToStr(c) + "." + ConvToStr(
                             b) + "." + ConvToStr(a);
        } else if (user->client_sa.family() == AF_INET6) {
            const unsigned char* ip = user->client_sa.in6.sin6_addr.s6_addr;

            std::string buf = BinToHex(ip, 16);
            for (std::string::const_reverse_iterator it = buf.rbegin(); it != buf.rend();
                    ++it) {
                reversedip.push_back(*it);
                reversedip.push_back('.');
            }
            reversedip.erase(reversedip.length() - 1, 1);
        } else {
            return;
        }

        ServerInstance->Logs->Log(MODNAME, LOG_DEBUG, "Reversed IP %s -> %s", user->GetIPString().c_str(), reversedip.c_str());

        countExt.set(user, DNSBLConfEntries.size());

        // For each DNSBL, we will run through this lookup
        for (unsigned i = 0; i < DNSBLConfEntries.size(); ++i) {
            // Fill hostname with a dnsbl style host (d.c.b.a.domain.tld)
            std::string hostname = reversedip + "." + DNSBLConfEntries[i]->domain;

            /* now we'd need to fire off lookups for `hostname'. */
            DNSBLResolver *r = new DNSBLResolver(*this->DNS, this, nameExt, countExt,
                                                 hostname, user, DNSBLConfEntries[i]);
            try {
                this->DNS->Process(r);
            } catch (DNS::Exception &ex) {
                delete r;
                ServerInstance->Logs->Log(MODNAME, LOG_DEBUG, ex.GetReason());
            }

            if (user->quitting) {
                break;
            }
        }
    }

    ModResult OnSetConnectClass(LocalUser* user,
                                ConnectClass* myclass) CXX11_OVERRIDE {
        std::string dnsbl;
        if (!myclass->config->readString("dnsbl", dnsbl)) {
            return MOD_RES_PASSTHRU;
        }

        std::vector<std::string>* match = nameExt.get(user);
        if (!match) {
            ServerInstance->Logs->Log("CONNECTCLASS", LOG_DEBUG,
                                      "The %s connect class is not suitable as it requires a DNSBL mark",
                                      myclass->GetName().c_str());
            return MOD_RES_DENY;
        }

        for (std::vector<std::string>::const_iterator it = match->begin(); it != match->end(); ++it) {
            if (InspIRCd::Match(*it, dnsbl)) {
                return MOD_RES_PASSTHRU;
            }
        }

        const std::string marks = stdalgo::string::join(dnsbl);
        ServerInstance->Logs->Log("CONNECTCLASS", LOG_DEBUG, "The %s connect class is not suitable as the DNSBL marks (%s) do not match %s",
                                  myclass->GetName().c_str(), marks.c_str(), dnsbl.c_str());
        return MOD_RES_DENY;
    }

    ModResult OnCheckReady(LocalUser *user) CXX11_OVERRIDE {
        if (countExt.get(user)) {
            return MOD_RES_DENY;
        }
        return MOD_RES_PASSTHRU;
    }

    ModResult OnStats(Stats::Context& stats) CXX11_OVERRIDE {
        if (stats.GetSymbol() != 'd') {
            return MOD_RES_PASSTHRU;
        }

        unsigned long total_hits = 0;
        unsigned long total_misses = 0;
        unsigned long total_errors = 0;
        for (std::vector<reference<DNSBLConfEntry> >::const_iterator i = DNSBLConfEntries.begin(); i != DNSBLConfEntries.end(); ++i) {
            total_hits += (*i)->stats_hits;
            total_misses += (*i)->stats_misses;
            total_errors += (*i)->stats_errors;

            stats.AddRow(304,
                         InspIRCd::Format("DNSBLSTATS \"%s\" had %lu hits, %lu misses, and %lu errors",
                                          (*i)->name.c_str(), (*i)->stats_hits, (*i)->stats_misses, (*i)->stats_errors));
        }

        stats.AddRow(304, "DNSBLSTATS Total hits: " + ConvToStr(total_hits));
        stats.AddRow(304, "DNSBLSTATS Total misses: " + ConvToStr(total_misses));
        stats.AddRow(304, "DNSBLSTATS Total errors: " + ConvToStr(total_errors));
        return MOD_RES_PASSTHRU;
    }
};

MODULE_INIT(ModuleDNSBL)
