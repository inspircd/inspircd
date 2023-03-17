/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2019 linuxdaemon <linuxdaemon.irc@gmail.com>
 *   Copyright (C) 2014 md_5 <git@md-5.net>
 *   Copyright (C) 2014 Googolplexed <googol@googolplexed.net>
 *   Copyright (C) 2013, 2017-2018, 2020-2021 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2013 Adam <Adam@anope.org>
 *   Copyright (C) 2012-2013, 2015 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012, 2019 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2009 Uli Schlachter <psychon@inspircd.org>
 *   Copyright (C) 2007-2009 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2006-2007, 2010 Craig Edwards <brain@inspircd.org>
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
#include "modules/ssl.h"
#include "modules/webirc.h"
#include "modules/whois.h"

// One or more hostmask globs or CIDR ranges.
typedef std::vector<std::string> MaskList;

// Encapsulates information about an ident host.
class IdentHost {
  private:
    MaskList hostmasks;
    std::string newident;

  public:
    IdentHost(const MaskList& masks, const std::string& ident)
        : hostmasks(masks)
        , newident(ident) {
    }

    const std::string& GetIdent() const {
        return newident;
    }

    bool Matches(LocalUser* user) const {
        for (MaskList::const_iterator iter = hostmasks.begin(); iter != hostmasks.end();
                ++iter) {
            // Does the user's hostname match this hostmask?
            if (InspIRCd::Match(user->GetRealHost(), *iter, ascii_case_insensitive_map)) {
                return true;
            }

            // Does the user's IP address match this hostmask?
            if (InspIRCd::MatchCIDR(user->GetIPString(), *iter,
                                    ascii_case_insensitive_map)) {
                return true;
            }
        }

        // The user didn't match any hostmasks.
        return false;
    }
};

// Encapsulates information about a WebIRC host.
class WebIRCHost {
  private:
    MaskList hostmasks;
    std::string fingerprint;
    std::string password;
    std::string passhash;
    TokenList trustedflags;

  public:
    WebIRCHost(const MaskList& masks, const std::string& fp,
               const std::string& pass, const std::string& hash, const std::string& flags)
        : hostmasks(masks)
        , fingerprint(fp)
        , password(pass)
        , passhash(hash) {
        trustedflags.AddList(flags);
    }

    bool IsFlagTrusted(const std::string& flag) const {
        return trustedflags.Contains(flag);
    }

    bool Matches(LocalUser* user, const std::string& pass,
                 UserCertificateAPI& sslapi) const {
        // Did the user send a valid password?
        if (!password.empty()
                && !ServerInstance->PassCompare(user, password, pass, passhash)) {
            return false;
        }

        // Does the user have a valid fingerprint?
        const std::string fp = sslapi ? sslapi->GetFingerprint(user) : "";
        if (!fingerprint.empty() && !InspIRCd::TimingSafeCompare(fp, fingerprint)) {
            return false;
        }

        for (MaskList::const_iterator iter = hostmasks.begin(); iter != hostmasks.end();
                ++iter) {
            // Does the user's hostname match this hostmask?
            if (InspIRCd::Match(user->GetRealHost(), *iter, ascii_case_insensitive_map)) {
                return true;
            }

            // Does the user's IP address match this hostmask?
            if (InspIRCd::MatchCIDR(user->GetIPString(), *iter,
                                    ascii_case_insensitive_map)) {
                return true;
            }
        }

        // The user didn't match any hostmasks.
        return false;
    }
};

class CommandHexIP : public SplitCommand {
  public:
    CommandHexIP(Module* Creator)
        : SplitCommand(Creator, "HEXIP", 1) {
        allow_empty_last_param = false;
        Penalty = 2;
        syntax = "<hex-ip|raw-ip>";
    }

    CmdResult HandleLocal(LocalUser* user,
                          const Params& parameters) CXX11_OVERRIDE {
        irc::sockets::sockaddrs sa;
        if (irc::sockets::aptosa(parameters[0], 0, sa)) {
            if (sa.family() != AF_INET) {
                user->WriteNotice("*** HEXIP: You can only hex encode an IPv4 address!");
                return CMD_FAILURE;
            }

            uint32_t addr = sa.in4.sin_addr.s_addr;
            user->WriteNotice(InspIRCd::Format("*** HEXIP: %s encodes to %02x%02x%02x%02x.",
                                               sa.addr().c_str(), (addr & 0xFF), ((addr >> 8) & 0xFF), ((addr >> 16) & 0xFF),
                                               ((addr >> 24) & 0xFF)));
            return CMD_SUCCESS;
        }

        if (ParseIP(parameters[0], sa)) {
            user->WriteNotice(InspIRCd::Format("*** HEXIP: %s decodes to %s.",
                                               parameters[0].c_str(), sa.addr().c_str()));
            return CMD_SUCCESS;
        }

        user->WriteNotice(InspIRCd::Format("*** HEXIP: %s is not a valid raw or hex encoded IPv4 address.",
                                           parameters[0].c_str()));
        return CMD_FAILURE;
    }

    static bool ParseIP(const std::string& in, irc::sockets::sockaddrs& out) {
        const char* ident = NULL;
        if (in.length() == 8) {
            // The ident is an IPv4 address encoded in hexadecimal with two characters
            // per address segment.
            ident = in.c_str();
        } else if (in.length() == 9 && in[0] == '~') {
            // The same as above but m_ident got to this user before we did. Strip the
            // ident prefix and continue as normal.
            ident = in.c_str() + 1;
        } else {
            // The user either does not have an IPv4 in their ident or the gateway server
            // is also running an identd. In the latter case there isn't really a lot we
            // can do so we just assume that the client in question is not connecting via
            // an ident gateway.
            return false;
        }

        // Try to convert the IP address to a string. If this fails then the user
        // does not have an IPv4 address in their ident.
        errno = 0;
        unsigned long address = strtoul(ident, NULL, 16);
        if (errno) {
            return false;
        }

        out.in4.sin_family = AF_INET;
        out.in4.sin_addr.s_addr = htonl(address);
        return true;
    }
};

class CommandWebIRC : public SplitCommand {
  public:
    std::vector<WebIRCHost> hosts;
    bool notify;
    StringExtItem gateway;
    StringExtItem realhost;
    StringExtItem realip;
    UserCertificateAPI sslapi;
    Events::ModuleEventProvider webircevprov;

    CommandWebIRC(Module* Creator)
        : SplitCommand(Creator, "WEBIRC", 4)
        , gateway("cgiirc_gateway", ExtensionItem::EXT_USER, Creator)
        , realhost("cgiirc_realhost", ExtensionItem::EXT_USER, Creator)
        , realip("cgiirc_realip", ExtensionItem::EXT_USER, Creator)
        , sslapi(Creator)
        , webircevprov(Creator, "event/webirc") {
        allow_empty_last_param = false;
        works_before_reg = true;
        this->syntax = "<password> <gateway> <hostname> <ip> [<flags>]";
    }

    CmdResult HandleLocal(LocalUser* user,
                          const Params& parameters) CXX11_OVERRIDE {
        if (user->registered == REG_ALL || realhost.get(user)) {
            return CMD_FAILURE;
        }

        for (std::vector<WebIRCHost>::const_iterator iter = hosts.begin(); iter != hosts.end(); ++iter) {
            // If we don't match the host then skip to the next host.
            if (!iter->Matches(user, parameters[0], sslapi)) {
                continue;
            }

            irc::sockets::sockaddrs ipaddr;
            if (!irc::sockets::aptosa(parameters[3], user->client_sa.port(), ipaddr)) {
                WriteLog("Connecting user %s (%s) tried to use WEBIRC but gave an invalid IP address.",
                         user->uuid.c_str(), user->GetIPString().c_str());
                ServerInstance->Users->QuitUser(user,
                                                "WEBIRC: IP address is invalid: " + parameters[3]);
                return CMD_FAILURE;
            }

            // The user matched a WebIRC block!
            gateway.set(user, parameters[1]);
            realhost.set(user, user->GetRealHost());
            realip.set(user, user->GetIPString());

            WriteLog("Connecting user %s is using the %s WebIRC gateway; changing their IP from %s to %s.",
                     user->uuid.c_str(), parameters[1].c_str(),
                     user->GetIPString().c_str(), parameters[3].c_str());

            // If we have custom flags then deal with them.
            WebIRC::FlagMap flags;
            const bool hasflags = (parameters.size() > 4);
            if (hasflags) {
                std::string flagname;
                std::string flagvalue;

                // Parse the flags.
                irc::spacesepstream flagstream(parameters[4]);
                for (std::string flag; flagstream.GetToken(flag); ) {
                    // Does this flag have a value?
                    const size_t separator = flag.find('=');
                    if (separator == std::string::npos) {
                        // It does not; just use the flag.
                        flagname = flag;
                        flagvalue.clear();
                    } else {
                        // It does; extract the value.
                        flagname = flag.substr(0, separator);
                        flagvalue = flag.substr(separator + 1);
                    }

                    if (iter->IsFlagTrusted(flagname)) {
                        flags[flagname] = flagvalue;
                    }
                }
            }

            // Inform modules about the WebIRC attempt.
            FOREACH_MOD_CUSTOM(webircevprov, WebIRC::EventListener, OnWebIRCAuth, (user,
                               (hasflags ? &flags : NULL)));

            // Set the IP address sent via WEBIRC. We ignore the hostname and lookup
            // instead do our own DNS lookups because of unreliable gateways.
            user->SetClientIP(ipaddr);
            return CMD_SUCCESS;
        }

        WriteLog("Connecting user %s (%s) tried to use WEBIRC but didn't match any configured WebIRC hosts.",
                 user->uuid.c_str(), user->GetIPString().c_str());
        ServerInstance->Users->QuitUser(user, "WEBIRC: you don't match any configured WebIRC hosts.");
        return CMD_FAILURE;
    }

    void WriteLog(const char* message, ...) CUSTOM_PRINTF(2, 3) {
        std::string buffer;
        VAFORMAT(buffer, message, message);

        // If we are sending a snotice then the message will already be
        // written to the logfile.
        if (notify) {
            ServerInstance->SNO->WriteGlobalSno('w', buffer);
        } else {
            ServerInstance->Logs->Log(MODNAME, LOG_DEFAULT, buffer);
        }
    }
};

class ModuleCgiIRC
    : public Module
    , public WebIRC::EventListener
    , public Whois::EventListener {
  private:
    CommandHexIP cmdhexip;
    CommandWebIRC cmdwebirc;
    std::vector<IdentHost> hosts;

  public:
    ModuleCgiIRC()
        : WebIRC::EventListener(this)
        , Whois::EventListener(this)
        , cmdhexip(this)
        , cmdwebirc(this) {
    }

    void init() CXX11_OVERRIDE {
        ServerInstance->SNO->EnableSnomask('w', "CGIIRC");
    }

    void On005Numeric(std::map<std::string, std::string>& tokens) CXX11_OVERRIDE {
        tokens["EXTBAN"].push_back('w');
    }

    ModResult OnCheckBan(User* user, Channel*,
                         const std::string& mask) CXX11_OVERRIDE {
        if (mask.length() <= 2 || mask[0] != 'w' || mask[1] != ':') {
            return MOD_RES_PASSTHRU;
        }

        const std::string* gateway = cmdwebirc.gateway.get(user);
        if (!gateway) {
            return MOD_RES_PASSTHRU;
        }

        if (InspIRCd::Match(*gateway, mask.substr(2))) {
            return MOD_RES_DENY;
        }

        return MOD_RES_PASSTHRU;
    }

    void ReadConfig(ConfigStatus& status) CXX11_OVERRIDE {
        std::vector<IdentHost> identhosts;
        std::vector<WebIRCHost> webirchosts;

        ConfigTagList tags = ServerInstance->Config->ConfTags("cgihost");
        for (ConfigIter i = tags.first; i != tags.second; ++i) {
            ConfigTag* tag = i->second;

            MaskList masks;
            irc::spacesepstream maskstream(tag->getString("mask"));
            for (std::string mask; maskstream.GetToken(mask); ) {
                masks.push_back(mask);
            }

            // Ensure that we have the <cgihost:mask> parameter.
            if (masks.empty()) {
                throw ModuleException("<cgihost:mask> is a mandatory field, at " +
                                      tag->getTagLocation());
            }

            // Determine what lookup type this host uses.
            const std::string type = tag->getString("type");
            if (stdalgo::string::equalsci(type, "ident")) {
                // The IP address should be looked up from the hex IP address.
                const std::string newident = tag->getString("newident", "gateway",
                                             ServerInstance->IsIdent);
                identhosts.push_back(IdentHost(masks, newident));
            } else if (stdalgo::string::equalsci(type, "webirc")) {
                // The IP address will be received via the WEBIRC command.
                const std::string fingerprint = tag->getString("fingerprint");
                const std::string password = tag->getString("password");
                const std::string passwordhash = tag->getString("hash", "plaintext", 1);
                const std::string trustedflags = tag->getString("trustedflags", "*", 1);

                // WebIRC blocks require a password.
                if (fingerprint.empty() && password.empty()) {
                    throw ModuleException("When using <cgihost type=\"webirc\"> either the fingerprint or password field is required, at "
                                          + tag->getTagLocation());
                }

                if (!password.empty() && stdalgo::string::equalsci(passwordhash, "plaintext")) {
                    ServerInstance->Logs->Log(MODNAME, LOG_DEFAULT,
                                              "<cgihost> tag at %s contains an plain text password, this is insecure!",
                                              tag->getTagLocation().c_str());
                }

                webirchosts.push_back(WebIRCHost(masks, fingerprint, password, passwordhash,
                                                 trustedflags));
            } else {
                throw ModuleException(type + " is an invalid <cgihost:mask> type, at " +
                                      tag->getTagLocation());
            }
        }

        // The host configuration was valid so we can apply it.
        hosts.swap(identhosts);
        cmdwebirc.hosts.swap(webirchosts);

        // Do we send an oper notice when a m_cgiirc client has their IP changed?
        cmdwebirc.notify = ServerInstance->Config->ConfValue("cgiirc")->getBool("opernotice", true);
    }

    ModResult OnSetConnectClass(LocalUser* user,
                                ConnectClass* myclass) CXX11_OVERRIDE {
        // If <connect:webirc> is not set then we have nothing to do.
        const std::string webirc = myclass->config->getString("webirc");
        if (webirc.empty()) {
            return MOD_RES_PASSTHRU;
        }

        // If the user is not connecting via a WebIRC gateway then they
        // cannot match this connect class.
        const std::string* gateway = cmdwebirc.gateway.get(user);
        if (!gateway) {
            ServerInstance->Logs->Log("CONNECTCLASS", LOG_DEBUG,
                                      "The %s connect class is not suitable as it requires a connection via a WebIRC gateway",
                                      myclass->GetName().c_str());
            return MOD_RES_DENY;
        }

        // If the gateway matches the <connect:webirc> constraint then
        // allow the check to continue. Otherwise, reject it.
        if (!InspIRCd::Match(*gateway, webirc)) {
            ServerInstance->Logs->Log("CONNECTCLASS", LOG_DEBUG,
                                      "The %s connect class is not suitable as the WebIRC gateway name (%s) does not match %s",
                                      myclass->GetName().c_str(), gateway->c_str(), webirc.c_str());
            return MOD_RES_DENY;
        }

        return MOD_RES_PASSTHRU;
    }

    ModResult OnUserRegister(LocalUser* user) CXX11_OVERRIDE {
        // There is no need to check for gateways if one is already being used.
        if (cmdwebirc.realhost.get(user)) {
            return MOD_RES_PASSTHRU;
        }

        for (std::vector<IdentHost>::const_iterator iter = hosts.begin(); iter != hosts.end(); ++iter) {
            // If we don't match the host then skip to the next host.
            if (!iter->Matches(user)) {
                continue;
            }

            // We have matched an <cgihost> block! Try to parse the encoded IPv4 address
            // out of the ident.
            irc::sockets::sockaddrs address(user->client_sa);
            if (!CommandHexIP::ParseIP(user->ident, address)) {
                return MOD_RES_PASSTHRU;
            }

            // Store the hostname and IP of the gateway for later use.
            cmdwebirc.realhost.set(user, user->GetRealHost());
            cmdwebirc.realip.set(user, user->GetIPString());

            const std::string& newident = iter->GetIdent();
            cmdwebirc.WriteLog("Connecting user %s is using an ident gateway; changing their IP from %s to %s and their ident from %s to %s.",
                               user->uuid.c_str(), user->GetIPString().c_str(), address.addr().c_str(),
                               user->ident.c_str(), newident.c_str());

            user->ChangeIdent(newident);
            user->SetClientIP(address);
            break;
        }
        return MOD_RES_PASSTHRU;
    }

    void OnWebIRCAuth(LocalUser* user,
                      const WebIRC::FlagMap* flags) CXX11_OVERRIDE {
        // We are only interested in connection flags. If none have been
        // given then we have nothing to do.
        if (!flags) {
            return;
        }

        WebIRC::FlagMap::const_iterator cport = flags->find("remote-port");
        if (cport != flags->end()) {
            // If we can't parse the port then just give up.
            uint16_t port = ConvToNum<uint16_t>(cport->second);
            if (port) {
                switch (user->client_sa.family()) {
                case AF_INET:
                    user->client_sa.in4.sin_port = htons(port);
                    break;

                case AF_INET6:
                    user->client_sa.in6.sin6_port = htons(port);
                    break;

                default:
                    // If we have reached this point then we have encountered a bug.
                    ServerInstance->Logs->Log(MODNAME, LOG_DEBUG,
                                              "BUG: OnWebIRCAuth(%s): socket type %d is unknown!",
                                              user->uuid.c_str(), user->client_sa.family());
                    return;
                }
            }
        }

        WebIRC::FlagMap::const_iterator sport = flags->find("local-port");
        if (sport != flags->end()) {
            // If we can't parse the port then just give up.
            uint16_t port = ConvToNum<uint16_t>(sport->second);
            if (port) {
                switch (user->server_sa.family()) {
                case AF_INET:
                    user->server_sa.in4.sin_port = htons(port);
                    break;

                case AF_INET6:
                    user->server_sa.in6.sin6_port = htons(port);
                    break;

                default:
                    // If we have reached this point then we have encountered a bug.
                    ServerInstance->Logs->Log(MODNAME, LOG_DEBUG,
                                              "BUG: OnWebIRCAuth(%s): socket type %d is unknown!",
                                              user->uuid.c_str(), user->server_sa.family());
                    return;
                }
            }
        }
    }

    void OnWhois(Whois::Context& whois) CXX11_OVERRIDE {
        // If these fields are not set then the client is not using a gateway.
        std::string* realhost = cmdwebirc.realhost.get(whois.GetTarget());
        std::string* realip = cmdwebirc.realip.get(whois.GetTarget());
        if (!realhost || !realip) {
            return;
        }

        // If the source doesn't have the right privs then only show the gateway name.
        std::string hidden = "*";
        if (!whois.GetSource()->HasPrivPermission("users/auspex")) {
            realhost = realip = &hidden;
        }

        const std::string* gateway = cmdwebirc.gateway.get(whois.GetTarget());
        if (gateway) {
            whois.SendLine(RPL_WHOISGATEWAY, *realhost, *realip,
                           "is connected via the " + *gateway + " WebIRC gateway");
        } else {
            whois.SendLine(RPL_WHOISGATEWAY, *realhost, *realip,
                           "is connected via an ident gateway");
        }
    }

    Version GetVersion() CXX11_OVERRIDE {
        return Version("Adds the ability for IRC gateways to forward the real IP address of users connecting through them.", VF_VENDOR);
    }
};

MODULE_INIT(ModuleCgiIRC)
