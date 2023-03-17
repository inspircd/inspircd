/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2021 Herman <GermanAizek@yandex.ru>
 *   Copyright (C) 2017, 2019 B00mX0r <b00mx0r@aureus.pw>
 *   Copyright (C) 2017 Sheogorath <sheogorath@shivering-isles.com>
 *   Copyright (C) 2016 Adam <Adam@anope.org>
 *   Copyright (C) 2014 Thomas Fargeix <t.fargeix@gmail.com>
 *   Copyright (C) 2013, 2018 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2013, 2016-2021 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2011 jackmcbarn <jackmcbarn@inspircd.org>
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2007-2009 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2007-2008 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2007 John Brooks <special@inspircd.org>
 *   Copyright (C) 2006 Oliver Lupton <om@inspircd.org>
 *   Copyright (C) 2003, 2007-2008 Craig Edwards <brain@inspircd.org>
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
#include "modules/hash.h"

enum CloakMode {
    /** 2.0 cloak of "half" of the hostname plus the full IP hash */
    MODE_HALF_CLOAK,

    /** 2.0 cloak of IP hash, split at 2 common CIDR range points */
    MODE_OPAQUE
};

// lowercase-only encoding similar to base64, used for hash output
static const char base32[] = "0123456789abcdefghijklmnopqrstuv";

// The minimum length of a cloak key.
static const size_t minkeylen = 30;

struct CloakInfo {
    // The method used for cloaking users.
    CloakMode mode;

    // The number of parts of the hostname shown when using half cloaking.
    unsigned int domainparts;

    // Whether to ignore the case of a hostname when cloaking it.
    bool ignorecase;

    // The secret used for generating cloaks.
    std::string key;

    // The prefix for cloaks (e.g. MyNet-).
    std::string prefix;

    // The suffix for IP cloaks (e.g. .IP).
    std::string suffix;

    CloakInfo(CloakMode Mode, const std::string& Key, const std::string& Prefix,
              const std::string& Suffix, bool IgnoreCase, unsigned int DomainParts = 0)
        : mode(Mode)
        , domainparts(DomainParts)
        , ignorecase(IgnoreCase)
        , key(Key)
        , prefix(Prefix)
        , suffix(Suffix) {
    }
};

typedef std::vector<std::string> CloakList;

class CloakExtItem : public SimpleExtItem<CloakList> {
  public:
    CloakExtItem(Module* Creator)
        : SimpleExtItem("cloaks", ExtensionItem::EXT_USER, Creator) {
    }

    std::string ToHuman(const Extensible* container,
                        void* item) const CXX11_OVERRIDE {
        return stdalgo::string::join(*static_cast<CloakList*>(item), ' ');
    }
};

class CloakUser : public ModeHandler {
  public:
    bool active;
    CloakExtItem ext;
    std::string debounce_uid;
    time_t debounce_ts;
    int debounce_count;

    CloakUser(Module* source)
        : ModeHandler(source, "cloak", 'x', PARAM_NONE, MODETYPE_USER)
        , active(false)
        , ext(source)
        , debounce_ts(0)
        , debounce_count(0) {
    }

    ModeAction OnModeChange(User* source, User* dest, Channel* channel,
                            std::string& parameter, bool adding) CXX11_OVERRIDE {
        LocalUser* user = IS_LOCAL(dest);

        /* For remote clients, we don't take any action, we just allow it.
         * The local server where they are will set their cloak instead.
         * This is fine, as we will receive it later.
         */
        if (!user) {
            // Remote setters broadcast mode before host while local setters do the opposite, so this takes that into account
            active = IS_LOCAL(source) ? adding : !adding;
            dest->SetMode(this, adding);
            return MODEACTION_ALLOW;
        }

        if (user->uuid == debounce_uid && debounce_ts == ServerInstance->Time()) {
            // prevent spamming using /mode user +x-x+x-x+x-x
            if (++debounce_count > 2) {
                return MODEACTION_DENY;
            }
        } else {
            debounce_uid = user->uuid;
            debounce_count = 1;
            debounce_ts = ServerInstance->Time();
        }

        if (adding == user->IsModeSet(this)) {
            return MODEACTION_DENY;
        }

        /* don't allow this user to spam modechanges */
        if (source == dest) {
            user->CommandFloodPenalty += 5000;
        }

        if (adding) {
            // assume this is more correct
            if (user->registered != REG_ALL
                    && user->GetRealHost() != user->GetDisplayedHost()) {
                return MODEACTION_DENY;
            }

            CloakList* cloaks = ext.get(user);
            if (!cloaks) {
                /* Force creation of missing cloak */
                try {
                    creator->OnUserConnect(user);
                    cloaks = ext.get(user);
                } catch (CoreException& modexcept) {
                    ServerInstance->Logs->Log(MODNAME, LOG_DEFAULT,
                                              "Exception caught when generating cloak: " + modexcept.GetReason());
                    return MODEACTION_DENY;
                }
            }

            // If we have a cloak then set the hostname.
            if (cloaks && !cloaks->empty()) {
                user->ChangeDisplayedHost(cloaks->front());
                user->SetMode(this, true);
                return MODEACTION_ALLOW;
            } else {
                return MODEACTION_DENY;
            }
        } else {
            /* User is removing the mode, so restore their real host
             * and make it match the displayed one.
             */
            user->SetMode(this, false);
            user->ChangeDisplayedHost(user->GetRealHost());
            return MODEACTION_ALLOW;
        }
    }
};

class CommandCloak : public Command {
  public:
    CommandCloak(Module* Creator) : Command(Creator, "CLOAK", 1) {
        flags_needed = 'o';
        syntax = "<host>";
    }

    CmdResult Handle(User* user, const Params& parameters) CXX11_OVERRIDE;
};

class ModuleCloaking : public Module {
  public:
    CloakUser cu;
    CommandCloak ck;
    std::vector<CloakInfo> cloaks;
    dynamic_reference<HashProvider> Hash;

    ModuleCloaking()
        : cu(this)
        , ck(this)
        , Hash(this, "hash/md5") {
    }

    /** Takes a domain name and retrieves the subdomain which should be visible.
     * This is usually the last \p domainparts labels but if not enough are
     * present then all but the most specific label are used. If the domain name
     * consists of one label only then none are used.
     *
     * Here are some examples for how domain names will be shortened assuming
     * \p domainparts is set to the default of 3.
     *
     *   "this.is.an.example.com"  =>  ".an.example.com"
     *   "an.example.com"          =>  ".example.com"
     *   "example.com"             =>  ".com"
     *   "localhost"               =>  ""
     *
     * @param host The hostname to cloak.
     * @param domainparts The number of domain labels that should be visible.
     * @return The visible segment of the hostname.
     */
    std::string VisibleDomainParts(const std::string& host,
                                   unsigned int domainparts) {
        // The position at which we found the last dot.
        std::string::const_reverse_iterator dotpos;

        // The number of dots we have seen so far.
        unsigned int seendots = 0;

        for (std::string::const_reverse_iterator iter = host.rbegin();
                iter != host.rend(); ++iter) {
            if (*iter != '.') {
                continue;
            }

            // We have found a dot!
            dotpos = iter;
            seendots += 1;

            // Do we have enough segments to stop?
            if (seendots >= domainparts) {
                break;
            }
        }

        // We only returns a domain part if more than one label is
        // present. See above for a full explanation.
        if (!seendots) {
            return "";
        }
        return std::string(dotpos.base() - 1, host.end());
    }

    /**
     * 2.0-style cloaking function
     * @param item The item to cloak (part of an IP or hostname)
     * @param id A unique ID for this type of item (to make it unique if the item matches)
     * @param len The length of the output. Maximum for MD5 is 16 characters.
     */
    std::string SegmentCloak(const CloakInfo& info, const std::string& item,
                             char id, size_t len) {
        std::string input;
        input.reserve(info.key.length() + 3 + item.length());
        input.append(1, id);
        input.append(info.key);
        input.append(1, '\0'); // null does not terminate a C++ string
        if (info.ignorecase) {
            std::transform(item.begin(), item.end(), std::back_inserter(input), ::tolower);
        } else {
            input.append(item);
        }

        std::string rv = Hash->GenerateRaw(input).substr(0,len);
        for(size_t i = 0; i < len; i++) {
            // this discards 3 bits per byte. We have an
            // overabundance of bits in the hash output, doesn't
            // matter which ones we are discarding.
            rv[i] = base32[rv[i] & 0x1F];
        }
        return rv;
    }

    std::string SegmentIP(const CloakInfo& info, const irc::sockets::sockaddrs& ip,
                          bool full) {
        std::string bindata;
        size_t hop1, hop2, hop3;
        size_t len1, len2;
        std::string rv;
        if (ip.family() == AF_INET6) {
            bindata = std::string((const char*)ip.in6.sin6_addr.s6_addr, 16);
            hop1 = 8;
            hop2 = 6;
            hop3 = 4;
            len1 = 6;
            len2 = 4;
            // pfx s1.s2.s3. (xxxx.xxxx or s4) sfx
            //     6  4  4    9/6
            rv.reserve(info.prefix.length() + 26 + info.suffix.length());
        } else {
            bindata = std::string((const char*)&ip.in4.sin_addr, 4);
            hop1 = 3;
            hop2 = 0;
            hop3 = 2;
            len1 = len2 = 3;
            // pfx s1.s2. (xxx.xxx or s3) sfx
            rv.reserve(info.prefix.length() + 15 + info.suffix.length());
        }

        rv.append(info.prefix);
        rv.append(SegmentCloak(info, bindata, 10, len1));
        rv.append(1, '.');
        bindata.erase(hop1);
        rv.append(SegmentCloak(info, bindata, 11, len2));
        if (hop2) {
            rv.append(1, '.');
            bindata.erase(hop2);
            rv.append(SegmentCloak(info, bindata, 12, len2));
        }

        if (full) {
            rv.append(1, '.');
            bindata.erase(hop3);
            rv.append(SegmentCloak(info, bindata, 13, 6));
            rv.append(info.suffix);
        } else {
            if (ip.family() == AF_INET6) {
                rv.append(InspIRCd::Format(".%02x%02x.%02x%02x%s",
                                           ip.in6.sin6_addr.s6_addr[2], ip.in6.sin6_addr.s6_addr[3],
                                           ip.in6.sin6_addr.s6_addr[0], ip.in6.sin6_addr.s6_addr[1], info.suffix.c_str()));
            } else {
                const unsigned char* ip4 = (const unsigned char*)&ip.in4.sin_addr;
                rv.append(InspIRCd::Format(".%d.%d%s", ip4[1], ip4[0], info.suffix.c_str()));
            }
        }
        return rv;
    }

    ModResult OnCheckBan(User* user, Channel* chan,
                         const std::string& mask) CXX11_OVERRIDE {
        LocalUser* lu = IS_LOCAL(user);
        if (!lu) {
            return MOD_RES_PASSTHRU;
        }

        // Force the creation of cloaks if not already set.
        OnUserConnect(lu);

        // If the user has no cloaks (i.e. UNIX socket) then we do nothing here.
        CloakList* cloaklist = cu.ext.get(user);
        if (!cloaklist || cloaklist->empty()) {
            return MOD_RES_PASSTHRU;
        }

        // Check if they have a cloaked host but are not using it.
        for (CloakList::const_iterator iter = cloaklist->begin(); iter != cloaklist->end(); ++iter) {
            const std::string& cloak = *iter;
            if (cloak != user->GetDisplayedHost()) {
                const std::string cloakMask = user->nick + "!" + user->ident + "@" + cloak;
                if (InspIRCd::Match(cloakMask, mask)) {
                    return MOD_RES_DENY;
                }
            }
        }
        return MOD_RES_PASSTHRU;
    }

    void Prioritize() CXX11_OVERRIDE {
        /* Needs to be after m_banexception etc. */
        ServerInstance->Modules->SetPriority(this, I_OnCheckBan, PRIORITY_LAST);
    }

    // this unsets umode +x on every host change. If we are actually doing a +x
    // mode change, we will call SetMode back to true AFTER the host change is done.
    void OnChangeHost(User* u, const std::string& host) CXX11_OVERRIDE {
        if (u->IsModeSet(cu) && !cu.active) {
            u->SetMode(cu, false);

            LocalUser* luser = IS_LOCAL(u);
            if (!luser) {
                return;
            }

            Modes::ChangeList modechangelist;
            modechangelist.push_remove(&cu);
            ClientProtocol::Events::Mode modeevent(ServerInstance->FakeClient, NULL, u,
                                                   modechangelist);
            luser->Send(modeevent);
        }
        cu.active = false;
    }

    Version GetVersion() CXX11_OVERRIDE {
        std::string testcloak = "broken";
        if (Hash && !cloaks.empty()) {
            const CloakInfo& info = cloaks.front();
            switch (info.mode) {
            case MODE_HALF_CLOAK:
                // Use old cloaking verification to stay compatible with 2.0
                // But verify domainparts and ignorecase when use 3.0-only features
                if (info.domainparts == 3 && !info.ignorecase) {
                    testcloak = info.prefix + SegmentCloak(info, "*", 3, 8) + info.suffix;
                } else {
                    irc::sockets::sockaddrs sa;
                    testcloak = GenCloak(info, sa, "",
                                         testcloak + ConvToStr(info.domainparts)) + (info.ignorecase ? "-ci" : "");
                }
                break;
            case MODE_OPAQUE:
                testcloak = info.prefix + SegmentCloak(info, "*", 4,
                                                       8) + info.suffix + (info.ignorecase ? "-ci" : "");
            }
        }
        return Version("Adds user mode x (cloak) which allows user hostnames to be hidden.", VF_COMMON|VF_VENDOR, testcloak);
    }

    void ReadConfig(ConfigStatus& status) CXX11_OVERRIDE {
        ConfigTagList tags = ServerInstance->Config->ConfTags("cloak");
        if (tags.first == tags.second) {
            throw ModuleException("You have loaded the cloaking module but not configured any <cloak> tags!");
        }

        std::vector<CloakInfo> newcloaks;
        for (ConfigIter i = tags.first; i != tags.second; ++i) {
            ConfigTag* tag = i->second;

            // Ensure that we have the <cloak:key> parameter.
            const std::string key = tag->getString("key");
            if (key.empty()) {
                throw ModuleException("You have not defined a cloaking key. Define <cloak:key> as a "
                                      + ConvToStr(minkeylen) + "+ character network-wide secret, at " +
                                      tag->getTagLocation());
            }

            // If we are the first cloak method then mandate a strong key.
            if (i == tags.first && key.length() < minkeylen) {
                throw ModuleException("Your cloaking key is not secure. It should be at least "
                                      + ConvToStr(minkeylen) + " characters long, at " + tag->getTagLocation());
            }

            const bool ignorecase = tag->getBool("ignorecase");
            const std::string mode = tag->getString("mode");
            const std::string prefix = tag->getString("prefix");
            const std::string suffix = tag->getString("suffix", ".IP");
            if (stdalgo::string::equalsci(mode, "half")) {
                unsigned int domainparts = tag->getUInt("domainparts", 3, 1, 10);
                newcloaks.push_back(CloakInfo(MODE_HALF_CLOAK, key, prefix, suffix, ignorecase,
                                              domainparts));
            } else if (stdalgo::string::equalsci(mode, "full")) {
                newcloaks.push_back(CloakInfo(MODE_OPAQUE, key, prefix, suffix, ignorecase));
            } else {
                throw ModuleException(mode +
                                      " is an invalid value for <cloak:mode>; acceptable values are 'half' and 'full', at "
                                      + tag->getTagLocation());
            }
        }

        // The cloak configuration was valid so we can apply it.
        cloaks.swap(newcloaks);
    }

    std::string GenCloak(const CloakInfo& info, const irc::sockets::sockaddrs& ip,
                         const std::string& ipstr, const std::string& host) {
        std::string chost;

        irc::sockets::sockaddrs hostip;
        bool host_is_ip = irc::sockets::aptosa(host, ip.port(), hostip) && hostip == ip;

        switch (info.mode) {
        case MODE_HALF_CLOAK: {
            if (!host_is_ip) {
                chost = info.prefix + SegmentCloak(info, host, 1, 6) + VisibleDomainParts(host,
                        info.domainparts);
            }
            if (chost.empty() || chost.length() > 50) {
                chost = SegmentIP(info, ip, false);
            }
            break;
        }
        case MODE_OPAQUE:
        default:
            chost = SegmentIP(info, ip, true);
        }
        return chost;
    }

    void OnSetUserIP(LocalUser* user) CXX11_OVERRIDE {
        // Connecting users are handled in OnUserConnect not here.
        if (user->registered != REG_ALL || user->quitting) {
            return;
        }

        // Remove the cloaks and generate new ones.
        cu.ext.unset(user);
        OnUserConnect(user);

        // If a user is using a cloak then update it.
        if (user->IsModeSet(cu)) {
            CloakList* cloaklist = cu.ext.get(user);
            user->ChangeDisplayedHost(cloaklist->front());
        }
    }

    void OnUserConnect(LocalUser* dest) CXX11_OVERRIDE {
        if (cu.ext.get(dest)) {
            return;
        }

        // TODO: decide how we are going to cloak AF_UNIX hostnames.
        if (dest->client_sa.family() != AF_INET && dest->client_sa.family() != AF_INET6) {
            return;
        }

        CloakList cloaklist;
        for (std::vector<CloakInfo>::const_iterator iter = cloaks.begin(); iter != cloaks.end(); ++iter) {
            cloaklist.push_back(GenCloak(*iter, dest->client_sa, dest->GetIPString(),
                                         dest->GetRealHost()));
        }
        cu.ext.set(dest, cloaklist);
    }
};

CmdResult CommandCloak::Handle(User* user, const Params& parameters) {
    ModuleCloaking* mod = (ModuleCloaking*)(Module*)creator;

    // If we're cloaking an IP address we pass it in the IP field too.
    irc::sockets::sockaddrs sa;
    const char* ipaddr = irc::sockets::aptosa(parameters[0], 0,
                         sa) ? parameters[0].c_str() : "";

    unsigned int id = 0;
    for (std::vector<CloakInfo>::const_iterator iter = mod->cloaks.begin();
            iter != mod->cloaks.end(); ++iter) {
        const std::string cloak = mod->GenCloak(*iter, sa, ipaddr, parameters[0]);
        user->WriteNotice(InspIRCd::Format("*** Cloak #%u for %s is %s", ++id,
                                           parameters[0].c_str(), cloak.c_str()));
    }
    return CMD_SUCCESS;
}

MODULE_INIT(ModuleCloaking)
