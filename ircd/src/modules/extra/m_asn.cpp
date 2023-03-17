/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2021 Sadie Powell <sadie@witchery.services>
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

/// $ModAuthor: Sadie Powell
/// $ModAuthorMail: sadie@witchery.services
/// $ModDepends: core 3
/// $ModDesc: Allows banning users based on Autonomous System number.


#include "inspircd.h"
#include "modules/dns.h"
#include "modules/stats.h"
#include "modules/whois.h"

enum {
    // InspIRCd-specific.
    RPL_WHOISASN = 569,
    RPL_STATSASN = 800
};

class ASNExt CXX11_FINAL
    : public LocalIntExt {
  public:
    ASNExt(Module* Creator)
        : LocalIntExt("asn", ExtensionItem::EXT_USER, Creator) {
    }

    void FromNetwork(Extensible* container,
                     const std::string& value) CXX11_OVERRIDE {
        return FromInternal(container, value);
    }

    std::string ToNetwork(const Extensible* container,
                          void* item) const CXX11_OVERRIDE {
        return ToInternal(container, item);
    }
};

class ASNResolver CXX11_FINAL
    : public DNS::Request {
  private:
    irc::sockets::sockaddrs theirsa;
    std::string theiruuid;
    ASNExt& asnext;
    LocalIntExt& asnpendingext;

    std::string GetDNS(LocalUser* user) {
        std::stringstream buffer;
        switch (user->client_sa.family()) {
        case AF_INET: {
            unsigned int d = (unsigned int) (user->client_sa.in4.sin_addr.s_addr >> 24) &
                             0xFF;
            unsigned int c = (unsigned int) (user->client_sa.in4.sin_addr.s_addr >> 16) &
                             0xFF;
            unsigned int b = (unsigned int) (user->client_sa.in4.sin_addr.s_addr >> 8) &
                             0xFF;
            unsigned int a = (unsigned int) user->client_sa.in4.sin_addr.s_addr & 0xFF;
            buffer << d << '.' << c << '.' << b << '.' << a << ".origin.asn.cymru.com";
            break;
        }
        case AF_INET6: {
            const std::string hexip = BinToHex(user->client_sa.in6.sin6_addr.s6_addr, 16);
            for (std::string::const_reverse_iterator it = hexip.rbegin();
                    it != hexip.rend(); ++it) {
                buffer << *it << '.';
            }
            buffer << "origin6.asn.cymru.com";
            break;
        }
        default:
            break;
        }
        return buffer.str();
    }


  public:
    ASNResolver(DNS::Manager* dns, Module* Creator, LocalUser* user, ASNExt& asn,
                LocalIntExt& asnpending)
        : DNS::Request(dns, Creator, GetDNS(user), DNS::QUERY_TXT, true)
        , theirsa(user->client_sa)
        , theiruuid(user->uuid)
        , asnext(asn)
        , asnpendingext(asnpending) {
    }

    void OnLookupComplete(const DNS::Query* result) CXX11_OVERRIDE {
        LocalUser* them = IS_LOCAL(ServerInstance->FindUUID(theiruuid));
        if (!them || them->client_sa != theirsa) {
            return;
        }

        // The DNS reply must contain an TXT result.
        const DNS::ResourceRecord* record = result->FindAnswerOfType(DNS::QUERY_TXT);
        if (!record) {
            asnpendingext.unset(them);
            return;
        }

        size_t pos = record->rdata.find_first_not_of("0123456789");
        intptr_t asn = ConvToNum<uintptr_t>(record->rdata.substr(0, pos));
        asnext.set(them, asn);
        asnpendingext.unset(them);
        ServerInstance->Logs->Log(MODNAME, LOG_DEBUG, "ASN for %s (%s) is %ld",
                                  them->uuid.c_str(), them->GetIPString().c_str(), asn);
    }

    void OnError(const DNS::Query* query) CXX11_OVERRIDE {
        LocalUser* them = IS_LOCAL(ServerInstance->FindUUID(theiruuid));
        if (!them || them->client_sa != theirsa) {
            return;
        }

        asnpendingext.unset(them);
        ServerInstance->SNO->WriteGlobalSno('a', "ASN lookup error for %s: %s",
                                            them->GetIPString().c_str(), manager->GetErrorStr(query->error).c_str());
    }
};

class ModuleASN CXX11_FINAL
    : public Module
    , public Stats::EventListener
    , public Whois::EventListener {
  private:
    ASNExt asnext;
    LocalIntExt asnpendingext;
    dynamic_reference<DNS::Manager> dns;

  public:
    ModuleASN()
        : Stats::EventListener(this)
        , Whois::EventListener(this)
        , asnext(this)
        , asnpendingext("asn-pending", ExtensionItem::EXT_USER, this)
        , dns(this, "DNS") {
    }

    Version GetVersion() CXX11_OVERRIDE {
        return Version("Allows banning users based on Autonomous System number.", VF_OPTCOMMON);
    }

    void On005Numeric(std::map<std::string, std::string>& tokens) CXX11_OVERRIDE {
        tokens["EXTBAN"].push_back('b');
    }

    ModResult OnCheckBan(User* user, Channel*,
                         const std::string& mask) CXX11_OVERRIDE {
        if ((mask.length() > 2) && (mask[0] == 'b') && (mask[1] == ':')) {
            // Does this user match against the ban?
            const std::string asnstr = ConvToStr(asnext.get(user));
            if (stdalgo::string::equalsci(asnstr, mask.substr(2))) {
                return MOD_RES_PASSTHRU;
            }
        }
        return MOD_RES_PASSTHRU;
    }

    ModResult OnCheckReady(LocalUser* user) CXX11_OVERRIDE {
        // Block until ASN info is available.
        return asnpendingext.get(user) ? MOD_RES_DENY : MOD_RES_PASSTHRU;
    }

    void OnSetUserIP(LocalUser* user) CXX11_OVERRIDE {
        if (user->quitting) {
            return;
        }

        if (!user->MyClass || !user->MyClass->config->getBool("useasn", true)) {
            return;
        }
        asnext.unset(user);
        if (user->client_sa.family() != AF_INET && user->client_sa.family() != AF_INET6) {
            return;
        }

        ASNResolver* resolver = new ASNResolver(*dns, this, user, asnext, asnpendingext);
        try {
            asnpendingext.set(user, 1);
            dns->Process(resolver);
        } catch (DNS::Exception& error) {
            asnpendingext.unset(user);
            delete resolver;
            ServerInstance->SNO->WriteGlobalSno('a', "ASN lookup error for %s: %s",
                                                user->GetIPString().c_str(), error.GetReason().c_str());
        }
    }

    ModResult OnSetConnectClass(LocalUser* user,
                                ConnectClass* myclass) CXX11_OVERRIDE {
        const std::string asn = myclass->config->getString("asn");
        if (asn.empty()) {
            return MOD_RES_PASSTHRU;
        }

        const std::string asnstr = ConvToStr(asnext.get(user));
        irc::spacesepstream asnstream(asn);
        for (std::string token; asnstream.GetToken(token); ) {
            // If the user matches this ASN then they can use this connect class.
            if (stdalgo::string::equalsci(asnstr, token)) {
                return MOD_RES_PASSTHRU;
            }
        }

        // A list of ASNs were specified but the user didn't match any of them.
        ServerInstance->Logs->Log("CONNECTCLASS", LOG_DEBUG, "The %s connect class is not suitable as the origin ASN (%s) is not any of %s",
                                  myclass->GetName().c_str(), asnstr.c_str(), asn.c_str());
        return MOD_RES_DENY;
    }

    ModResult OnStats(Stats::Context& stats) CXX11_OVERRIDE {
        if (stats.GetSymbol() != 'b') {
            return MOD_RES_PASSTHRU;
        }

        std::map<intptr_t, size_t> counts;
        const user_hash& list = ServerInstance->Users.GetUsers();
        for (user_hash::const_iterator iter = list.begin(); iter != list.end(); ++iter) {
            intptr_t asn = asnext.get(iter->second);
            if (!counts.insert(std::make_pair(asn, 1)).second) {
                counts[asn]++;
            }
        }

        for (std::map<intptr_t, size_t>::const_iterator iter = counts.begin(); iter != counts.end(); ++iter) {
            stats.AddRow(RPL_STATSASN, iter->first, iter->second);
        }
        return MOD_RES_DENY;
    }

    void OnWhois(Whois::Context& whois) CXX11_OVERRIDE {
        if (whois.GetTarget()->server->IsULine()) {
            return;
        }

        intptr_t asn = asnext.get(whois.GetTarget());
        if (asn) {
            whois.SendLine(RPL_WHOISASN, asn, "is connecting from AS" + ConvToStr(asn));
        } else {
            whois.SendLine(RPL_WHOISASN, "*",
                           "is connecting from an unknown autonomous system");
        }
    }
};

MODULE_INIT(ModuleASN)
