/* InspIRCd 3.0 functions
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
#include "modules/cs_mode.h"
#include "modules/sasl.h"

typedef std::map<char, unsigned> ListLimits;

struct SASLUser {
    Anope::string uid;
    Anope::string acc;
    time_t created;
};

static std::list<SASLUser> saslusers;

static Anope::string rsquit_server, rsquit_id;

class InspIRCd3Proto : public IRCDProto {
  private:
    void SendChgIdentInternal(const Anope::string &nick,
                              const Anope::string &vIdent) {
        if (!Servers::Capab.count("CHGIDENT")) {
            Log() << "CHGIDENT not loaded!";
        } else {
            UplinkSocket::Message(Me) << "CHGIDENT " << nick << " " << vIdent;
        }
    }

    void SendChgHostInternal(const Anope::string &nick,
                             const Anope::string &vhost) {
        if (!Servers::Capab.count("CHGHOST")) {
            Log() << "CHGHOST not loaded!";
        } else {
            UplinkSocket::Message(Me) << "CHGHOST " << nick << " " << vhost;
        }
    }

    void SendAddLine(const Anope::string &xtype, const Anope::string &mask,
                     time_t duration, const Anope::string &addedby, const Anope::string &reason) {
        UplinkSocket::Message(Me) << "ADDLINE " << xtype << " " << mask << " " <<
                                  addedby << " " << Anope::CurTime << " " << duration << " :" << reason;
    }

    void SendDelLine(const Anope::string &xtype, const Anope::string &mask) {
        UplinkSocket::Message(Me) << "DELLINE " << xtype << " " << mask;
    }

  public:
    PrimitiveExtensibleItem<ListLimits> maxlist;

    InspIRCd3Proto(Module *creator) : IRCDProto(creator, "InspIRCd 3"),
        maxlist(creator, "maxlist") {
        DefaultPseudoclientModes = "+oI";
        CanSVSNick = true;
        CanSVSJoin = true;
        CanSetVHost = true;
        CanSetVIdent = true;
        CanSQLine = true;
        CanSQLineChannel = true;
        CanSZLine = true;
        CanSVSHold = true;
        CanCertFP = true;
        RequiresID = true;
        MaxModes = 20;
        MaxLine = 4096;
    }

    unsigned GetMaxListFor(Channel *c, ChannelMode *cm) anope_override {
        ListLimits *limits = maxlist.Get(c);
        if (limits) {
            ListLimits::const_iterator limit = limits->find(cm->mchar);
            if (limit != limits->end()) {
                return limit->second;
            }
        }

        // Fall back to the config limit if we can't find the mode.
        return IRCDProto::GetMaxListFor(c, cm);
    }

    void SendConnect() anope_override {
        UplinkSocket::Message() << "CAPAB START 1205";
        UplinkSocket::Message() << "CAPAB CAPABILITIES :CASEMAPPING=" << Config->GetBlock("options")->Get<const Anope::string>("casemap", "ascii");
        UplinkSocket::Message() << "CAPAB END";
        UplinkSocket::Message() << "SERVER " << Me->GetName() << " " << Config->Uplinks[Anope::CurrentUplink].password << " 0 " << Me->GetSID() << " :" << Me->GetDescription();
    }

    void SendSASLMechanisms(std::vector<Anope::string> &mechanisms) anope_override {
        Anope::string mechlist;
        for (unsigned i = 0; i < mechanisms.size(); ++i) {
            mechlist += "," + mechanisms[i];
        }

        UplinkSocket::Message(Me) << "METADATA * saslmechlist :" << (mechanisms.empty() ? "" : mechlist.substr(1));
    }

    void SendSVSKillInternal(const MessageSource &source, User *user,
                             const Anope::string &buf) anope_override {
        IRCDProto::SendSVSKillInternal(source, user, buf);
        user->KillInternal(source, buf);
    }

    void SendGlobalNotice(BotInfo *bi, const Server *dest,
                          const Anope::string &msg) anope_override {
        UplinkSocket::Message(bi) << "NOTICE $" << dest->GetName() << " :" << msg;
    }

    void SendGlobalPrivmsg(BotInfo *bi, const Server *dest,
                           const Anope::string &msg) anope_override {
        UplinkSocket::Message(bi) << "PRIVMSG $" << dest->GetName() << " :" << msg;
    }

    void SendPong(const Anope::string &servname,
                  const Anope::string &who) anope_override {
        Server *serv = servname.empty() ? NULL : Server::Find(servname);
        if (!serv) {
            serv = Me;
        }

        UplinkSocket::Message(serv) << "PONG " << who;
    }

    void SendAkillDel(const XLine *x) anope_override {
        {
            /* InspIRCd may support regex bans
             * Mask is expected in format: 'n!u@h\sr' and spaces as '\s'
             * We remove the '//' and replace '#' and any ' ' with '\s'
             */
            if (x->IsRegex() && Servers::Capab.count("RLINE")) {
                Anope::string mask = x->mask;
                if (mask.length() >= 2 && mask[0] == '/' && mask[mask.length() - 1] == '/') {
                    mask = mask.substr(1, mask.length() - 2);
                }
                size_t h = mask.find('#');
                if (h != Anope::string::npos) {
                    mask = mask.replace(h, 1, "\\s");
                    mask = mask.replace_all_cs(" ", "\\s");
                }
                SendDelLine("R", mask);
                return;
            } else if (x->IsRegex() || x->HasNickOrReal()) {
                return;
            }

            /* ZLine if we can instead */
            if (x->GetUser() == "*") {
                cidr addr(x->GetHost());
                if (addr.valid()) {
                    IRCD->SendSZLineDel(x);
                    return;
                }
            }

            SendDelLine("G", x->GetUser() + "@" + x->GetHost());
        }
    }

    void SendInvite(const MessageSource &source, const Channel *c,
                    User *u) anope_override {
        UplinkSocket::Message(source) << "INVITE " << u->GetUID() << " " << c->name << " " << c->creation_time;
    }

    void SendTopic(const MessageSource &source, Channel *c) anope_override {
        if (Servers::Capab.count("SVSTOPIC")) {
            UplinkSocket::Message(c->ci->WhoSends()) << "SVSTOPIC " << c->name << " " <<
                    c->topic_ts << " " << c->topic_setter << " :" << c->topic;
        } else {
            /* If the last time a topic was set is after the TS we want for this topic we must bump this topic's timestamp to now */
            time_t ts = c->topic_ts;
            if (c->topic_time > ts) {
                ts = Anope::CurTime;
            }
            /* But don't modify c->topic_ts, it should remain set to the real TS we want as ci->last_topic_time pulls from it */
            UplinkSocket::Message(source) << "FTOPIC " << c->name << " " << c->creation_time
                                          << " " << ts << " " << c->topic_setter << " :" << c->topic;
        }
    }

    void SendVhostDel(User *u) anope_override {
        UserMode *um = ModeManager::FindUserModeByName("CLOAK");

        if (um && !u->HasMode(um->name))
            // Just set +x if we can
        {
            u->SetMode(NULL, um);
        } else
            // Try to restore cloaked host
        {
            this->SendChgHostInternal(u->nick, u->chost);
        }
    }

    void SendAkill(User *u, XLine *x) anope_override {
        // Calculate the time left before this would expire, capping it at 2 days
        time_t timeleft = x->expires - Anope::CurTime;
        if (timeleft > 172800 || !x->expires) {
            timeleft = 172800;
        }

        /* InspIRCd may support regex bans, if they do we can send this and forget about it
         * Mask is expected in format: 'n!u@h\sr' and spaces as '\s'
         * We remove the '//' and replace '#' and any ' ' with '\s'
         */
        if (x->IsRegex() && Servers::Capab.count("RLINE")) {
            Anope::string mask = x->mask;
            if (mask.length() >= 2 && mask[0] == '/' && mask[mask.length() - 1] == '/') {
                mask = mask.substr(1, mask.length() - 2);
            }
            size_t h = mask.find('#');
            if (h != Anope::string::npos) {
                mask = mask.replace(h, 1, "\\s");
                mask = mask.replace_all_cs(" ", "\\s");
            }
            SendAddLine("R", mask, timeleft, x->by, x->GetReason());
            return;
        } else if (x->IsRegex() || x->HasNickOrReal()) {
            if (!u) {
                /* No user (this akill was just added), and contains nick and/or realname. Find users that match and ban them */
                for (user_map::const_iterator it = UserListByNick.begin();
                        it != UserListByNick.end(); ++it)
                    if (x->manager->Check(it->second, x)) {
                        this->SendAkill(it->second, x);
                    }
                return;
            }

            const XLine *old = x;

            if (old->manager->HasEntry("*@" + u->host)) {
                return;
            }

            /* We can't akill x as it has a nick and/or realname included, so create a new akill for *@host */
            x = new XLine("*@" + u->host, old->by, old->expires, old->reason, old->id);
            old->manager->AddXLine(x);

            Log(Config->GetClient("OperServ"),
                "akill") << "AKILL: Added an akill for " << x->mask << " because " <<
                         u->GetMask() << "#" << u->realname << " matches " << old->mask;
        }

        /* ZLine if we can instead */
        if (x->GetUser() == "*") {
            cidr addr(x->GetHost());
            if (addr.valid()) {
                IRCD->SendSZLine(u, x);
                return;
            }
        }

        SendAddLine("G", x->GetUser() + "@" + x->GetHost(), timeleft, x->by, x->GetReason());
    }

    void SendNumericInternal(int numeric, const Anope::string &dest,
                             const Anope::string &buf) anope_override {
        UplinkSocket::Message() << "NUM " << Me->GetSID() << " " << dest << " " << numeric << " " << buf;
    }

    void SendModeInternal(const MessageSource &source, const Channel *dest,
                          const Anope::string &buf) anope_override {
        UplinkSocket::Message(source) << "FMODE " << dest->name << " " << dest->creation_time << " " << buf;
    }

    void SendClientIntroduction(User *u) anope_override {
        Anope::string modes = "+" + u->GetModes();
        UplinkSocket::Message(Me) << "UID " << u->GetUID() << " " << u->timestamp << " " << u->nick << " " << u->host << " " << u->host << " " << u->GetIdent() << " 0.0.0.0 " << u->timestamp << " " << modes << " :" << u->realname;

        if (modes.find('o') != Anope::string::npos) {
            // Mark as introduced so we can send an oper type.
            BotInfo *bi = BotInfo::Find(u->nick, true);
            if (bi) {
                bi->introduced = true;
            }

            UplinkSocket::Message(u) << "OPERTYPE :service";
        }
    }

    void SendServer(const Server *server) anope_override {
        /* if rsquit is set then we are waiting on a squit */
        if (rsquit_id.empty() && rsquit_server.empty()) {
            UplinkSocket::Message() << "SERVER " << server->GetName() << " " <<
                                    server->GetSID() << " :" << server->GetDescription();
        }
    }

    void SendSquit(Server *s, const Anope::string &message) anope_override {
        if (s != Me) {
            rsquit_id = s->GetSID();
            rsquit_server = s->GetName();
            UplinkSocket::Message() << "RSQUIT " << s->GetName() << " :" << message;
        } else {
            UplinkSocket::Message() << "SQUIT " << s->GetName() << " :" << message;
        }
    }

    void SendJoin(User *user, Channel *c,
                  const ChannelStatus *status) anope_override {
        UplinkSocket::Message(Me) << "FJOIN " << c->name << " " << c->creation_time << " +" << c->GetModes(true, true) << " :," << user->GetUID();
        /* Note that we can send this with the FJOIN but choose not to
         * because the mode stacker will handle this and probably will
         * merge these modes with +nrt and other mlocked modes
         */
        if (status) {
            /* First save the channel status incase uc->Status == status */
            ChannelStatus cs = *status;
            /* If the user is internally on the channel with flags, kill them so that
             * the stacker will allow this.
             */
            ChanUserContainer *uc = c->FindUser(user);
            if (uc != NULL) {
                uc->status.Clear();
            }

            BotInfo *setter = BotInfo::Find(user->GetUID());
            for (size_t i = 0; i < cs.Modes().length(); ++i) {
                c->SetMode(setter, ModeManager::FindChannelModeByChar(cs.Modes()[i]),
                           user->GetUID(), false);
            }

            if (uc != NULL) {
                uc->status = cs;
            }
        }
    }

    void SendSQLineDel(const XLine *x) anope_override {
        if (IRCD->CanSQLineChannel && (x->mask[0] == '#')) {
            SendDelLine("CBAN", x->mask);
        } else {
            SendDelLine("Q", x->mask);
        }
    }

    void SendSQLine(User *u, const XLine *x) anope_override {
        // Calculate the time left before this would expire, capping it at 2 days
        time_t timeleft = x->expires - Anope::CurTime;
        if (timeleft > 172800 || !x->expires) {
            timeleft = 172800;
        }

        if (IRCD->CanSQLineChannel && (x->mask[0] == '#')) {
            SendAddLine("CBAN", x->mask, timeleft, x->by, x->GetReason());
        } else {
            SendAddLine("Q", x->mask, timeleft, x->by, x->GetReason());
        }
    }

    void SendVhost(User *u, const Anope::string &vIdent,
                   const Anope::string &vhost) anope_override {
        if (!vIdent.empty()) {
            this->SendChgIdentInternal(u->nick, vIdent);
        }
        if (!vhost.empty()) {
            this->SendChgHostInternal(u->nick, vhost);
        }
    }

    void SendSVSHold(const Anope::string &nick, time_t t) anope_override {
        UplinkSocket::Message(Config->GetClient("NickServ")) << "SVSHOLD " << nick << " " << t << " :Being held for registered user";
    }

    void SendSVSHoldDel(const Anope::string &nick) anope_override {
        UplinkSocket::Message(Config->GetClient("NickServ")) << "SVSHOLD " << nick;
    }

    void SendSZLineDel(const XLine *x) anope_override {
        SendDelLine("Z", x->GetHost());
    }

    void SendSZLine(User *u, const XLine *x) anope_override {
        // Calculate the time left before this would expire, capping it at 2 days
        time_t timeleft = x->expires - Anope::CurTime;
        if (timeleft > 172800 || !x->expires) {
            timeleft = 172800;
        }
        SendAddLine("Z", x->GetHost(), timeleft, x->by, x->GetReason());
    }

    void SendSVSJoin(const MessageSource &source, User *u,
                     const Anope::string &chan, const Anope::string &other) anope_override {
        UplinkSocket::Message(source) << "SVSJOIN " << u->GetUID() << " " << chan;
    }

    void SendSVSPart(const MessageSource &source, User *u,
                     const Anope::string &chan, const Anope::string &param) anope_override {
        if (!param.empty()) {
            UplinkSocket::Message(source) << "SVSPART " << u->GetUID() << " " << chan <<
                                          " :" << param;
        } else {
            UplinkSocket::Message(source) << "SVSPART " << u->GetUID() << " " << chan;
        }
    }

    void SendSWhois(const MessageSource &bi, const Anope::string &who,
                    const Anope::string &mask) anope_override {
        User *u = User::Find(who);

        UplinkSocket::Message(Me) << "METADATA " << u->GetUID() << " swhois :" << mask;
    }

    void SendBOB() anope_override {
        UplinkSocket::Message(Me) << "BURST " << Anope::CurTime;
        Module *enc = ModuleManager::FindFirstOf(ENCRYPTION);
        UplinkSocket::Message(Me) << "SINFO version :Anope-" << Anope::Version() << " " << Me->GetName() << " :" << IRCD->GetProtocolName() << " - (" << (enc ? enc->name : "none") << ") -- " << Anope::VersionBuildString();
        UplinkSocket::Message(Me) << "SINFO fullversion :Anope-" << Anope::Version() << " " << Me->GetName() << " :[" << Me->GetSID() << "] " << IRCD->GetProtocolName() << " - (" << (enc ? enc->name : "none") << ") -- " << Anope::VersionBuildString();
        UplinkSocket::Message(Me) << "SINFO rawversion :Anope-" << Anope::VersionShort();
    }

    void SendEOB() anope_override {
        UplinkSocket::Message(Me) << "ENDBURST";
    }

    void SendGlobopsInternal(const MessageSource &source,
                             const Anope::string &buf) anope_override {
        if (Servers::Capab.count("GLOBOPS")) {
            UplinkSocket::Message(source) << "SNONOTICE g :" << buf;
        } else {
            UplinkSocket::Message(source) << "SNONOTICE A :" << buf;
        }
    }

    void SendLogin(User *u, NickAlias *na) anope_override {
        /* InspIRCd uses an account to bypass chmode +R, not umode +r, so we can't send this here */
        if (na->nc->HasExt("UNCONFIRMED")) {
            return;
        }

        UplinkSocket::Message(Me) << "METADATA " << u->GetUID() << " accountid :" << na->nc->GetId();
        UplinkSocket::Message(Me) << "METADATA " << u->GetUID() << " accountname :" << na->nc->display;
    }

    void SendLogout(User *u) anope_override {
        UplinkSocket::Message(Me) << "METADATA " << u->GetUID() << " accountid :";
        UplinkSocket::Message(Me) << "METADATA " << u->GetUID() << " accountname :";
    }

    void SendChannel(Channel *c) anope_override {
        UplinkSocket::Message(Me) << "FJOIN " << c->name << " " << c->creation_time << " +" << c->GetModes(true, true) << " :";
    }

    void SendSASLMessage(const SASL::Message &message) anope_override {
        UplinkSocket::Message(Me) << "ENCAP " << message.target.substr(0, 3) << " SASL " << message.source << " " << message.target << " " << message.type << " " << message.data << (message.ext.empty() ? "" : (" " + message.ext));
    }

    void SendSVSLogin(const Anope::string &uid, const Anope::string &acc,
                      const Anope::string &vident, const Anope::string &vhost) anope_override {
        // TODO: in 2.1 this function should take a NickAlias instead of strings.
        NickCore *nc = NickCore::Find(acc);
        if (!nc) {
            return;
        }

        UplinkSocket::Message(Me) << "METADATA " << uid << " accountid :" << nc->GetId();
        UplinkSocket::Message(Me) << "METADATA " << uid << " accountname :" << acc;

        if (!vident.empty()) {
            UplinkSocket::Message(Me) << "ENCAP " << uid.substr(0,
                                      3) << " CHGIDENT " << uid << " " << vident;
        }
        if (!vhost.empty()) {
            UplinkSocket::Message(Me) << "ENCAP " << uid.substr(0,
                                      3) << " CHGHOST " << uid << " " << vhost;
        }

        SASLUser su;
        su.uid = uid;
        su.acc = acc;
        su.created = Anope::CurTime;

        for (std::list<SASLUser>::iterator it = saslusers.begin(); it != saslusers.end();) {
            SASLUser &u = *it;

            if (u.created + 30 < Anope::CurTime || u.uid == uid) {
                it = saslusers.erase(it);
            } else {
                ++it;
            }
        }

        saslusers.push_back(su);
    }

    bool IsExtbanValid(const Anope::string &mask) anope_override {
        return mask.length() >= 3 && mask[1] == ':';
    }

    bool IsIdentValid(const Anope::string &ident) anope_override {
        if (ident.empty() || ident.length() > Config->GetBlock("networkinfo")->Get<unsigned>("userlen")) {
            return false;
        }

        for (unsigned i = 0; i < ident.length(); ++i) {
            const char &c = ident[i];

            if (c >= 'A' && c <= '}') {
                continue;
            }

            if ((c >= '0' && c <= '9') || c == '-' || c == '.') {
                continue;
            }

            return false;
        }

        return true;
    }
};

class InspIRCdAutoOpMode : public ChannelModeList {
  public:
    InspIRCdAutoOpMode(char mode) : ChannelModeList("AUTOOP", mode) {
    }

    bool IsValid(Anope::string &mask) const anope_override {
        // We can not validate this because we don't know about the
        // privileges of the setter so just reject attempts to set it.
        return false;
    }
};

class InspIRCdExtBan : public ChannelModeVirtual<ChannelModeList> {
    char ext;

  public:
    InspIRCdExtBan(const Anope::string &mname, const Anope::string &basename,
                   char extban) : ChannelModeVirtual<ChannelModeList>(mname, basename)
        , ext(extban) {
    }

    ChannelMode *Wrap(Anope::string &param) anope_override {
        param = Anope::string(ext) + ":" + param;
        return ChannelModeVirtual<ChannelModeList>::Wrap(param);
    }

    ChannelMode *Unwrap(ChannelMode *cm, Anope::string &param) anope_override {
        if (cm->type != MODE_LIST || param.length() < 3 || param[0] != ext || param[1] != ':') {
            return cm;
        }

        param = param.substr(2);
        return this;
    }
};

namespace InspIRCdExtban {
class EntryMatcher : public InspIRCdExtBan {
  public:
    EntryMatcher(const Anope::string &mname, const Anope::string &mbase,
                 char c) : InspIRCdExtBan(mname, mbase, c) {
    }

    bool Matches(User *u, const Entry *e) anope_override {
        const Anope::string &mask = e->GetMask();
        Anope::string real_mask = mask.substr(3);

        return Entry(this->name, real_mask).Matches(u);
    }
};

class ChannelMatcher : public InspIRCdExtBan {
  public:
    ChannelMatcher(const Anope::string &mname, const Anope::string &mbase,
                   char c) : InspIRCdExtBan(mname, mbase, c) {
    }

    bool Matches(User *u, const Entry *e) anope_override {
        const Anope::string &mask = e->GetMask();

        Anope::string channel = mask.substr(3);

        ChannelMode *cm = NULL;
        if (channel[0] != '#') {
            char modeChar = ModeManager::GetStatusChar(channel[0]);
            channel.erase(channel.begin());
            cm = ModeManager::FindChannelModeByChar(modeChar);
            if (cm != NULL && cm->type != MODE_STATUS) {
                cm = NULL;
            }
        }

        Channel *c = Channel::Find(channel);
        if (c != NULL) {
            ChanUserContainer *uc = c->FindUser(u);
            if (uc != NULL)
                if (cm == NULL || uc->status.HasMode(cm->mchar)) {
                    return true;
                }
        }

        return false;
    }
};

class AccountMatcher : public InspIRCdExtBan {
  public:
    AccountMatcher(const Anope::string &mname, const Anope::string &mbase,
                   char c) : InspIRCdExtBan(mname, mbase, c) {
    }

    bool Matches(User *u, const Entry *e) anope_override {
        const Anope::string &mask = e->GetMask();
        Anope::string real_mask = mask.substr(2);

        return u->IsIdentified() && real_mask.equals_ci(u->Account()->display);
    }
};

class RealnameMatcher : public InspIRCdExtBan {
  public:
    RealnameMatcher(const Anope::string &mname, const Anope::string &mbase,
                    char c) : InspIRCdExtBan(mname, mbase, c) {
    }

    bool Matches(User *u, const Entry *e) anope_override {
        const Anope::string &mask = e->GetMask();
        Anope::string real_mask = mask.substr(2);
        return Anope::Match(u->realname, real_mask);
    }
};

class ServerMatcher : public InspIRCdExtBan {
  public:
    ServerMatcher(const Anope::string &mname, const Anope::string &mbase,
                  char c) : InspIRCdExtBan(mname, mbase, c) {
    }

    bool Matches(User *u, const Entry *e) anope_override {
        const Anope::string &mask = e->GetMask();
        Anope::string real_mask = mask.substr(2);
        return Anope::Match(u->server->GetName(), real_mask);
    }
};

class FingerprintMatcher : public InspIRCdExtBan {
  public:
    FingerprintMatcher(const Anope::string &mname, const Anope::string &mbase,
                       char c) : InspIRCdExtBan(mname, mbase, c) {
    }

    bool Matches(User *u, const Entry *e) anope_override {
        const Anope::string &mask = e->GetMask();
        Anope::string real_mask = mask.substr(2);
        return !u->fingerprint.empty() && Anope::Match(u->fingerprint, real_mask);
    }
};

class UnidentifiedMatcher : public InspIRCdExtBan {
  public:
    UnidentifiedMatcher(const Anope::string &mname, const Anope::string &mbase,
                        char c) : InspIRCdExtBan(mname, mbase, c) {
    }

    bool Matches(User *u, const Entry *e) anope_override {
        const Anope::string &mask = e->GetMask();
        Anope::string real_mask = mask.substr(2);
        return !u->Account() && Entry("BAN", real_mask).Matches(u);
    }
};
}

class ColonDelimitedParamMode : public ChannelModeParam {
  public:
    ColonDelimitedParamMode(const Anope::string &modename,
                            char modeChar) : ChannelModeParam(modename, modeChar, true) { }

    bool IsValid(Anope::string &value) const anope_override {
        return IsValid(value, false);
    }

    bool IsValid(const Anope::string &value, bool historymode) const {
        if (value.empty()) {
            return false;    // empty param is never valid
        }

        Anope::string::size_type pos = value.find(':');
        if ((pos == Anope::string::npos) || (pos == 0)) {
            return false;    // no ':' or it's the first char, both are invalid
        }

        Anope::string rest;
        try {
            if (convertTo<int>(value, rest, false) <= 0) {
                return false;    // negative numbers and zero are invalid
            }

            rest = rest.substr(1);
            int n;
            if (historymode) {
                // For the history mode, the part after the ':' is a duration and it
                // can be in the user friendly "1d3h20m" format, make sure we accept that
                n = Anope::DoTime(rest);
            } else {
                n = convertTo<int>(rest);
            }

            if (n <= 0) {
                return false;
            }
        } catch (const ConvertException &e) {
            // conversion error, invalid
            return false;
        }

        return true;
    }
};

class SimpleNumberParamMode : public ChannelModeParam {
  public:
    SimpleNumberParamMode(const Anope::string &modename,
                          char modeChar) : ChannelModeParam(modename, modeChar, true) { }

    bool IsValid(Anope::string &value) const anope_override {
        if (value.empty()) {
            return false;    // empty param is never valid
        }

        try {
            if (convertTo<int>(value) <= 0) {
                return false;
            }
        } catch (const ConvertException &e) {
            // conversion error, invalid
            return false;
        }

        return true;
    }
};

class ChannelModeFlood : public ColonDelimitedParamMode {
  public:
    ChannelModeFlood(char modeChar) : ColonDelimitedParamMode("FLOOD", modeChar) { }

    bool IsValid(Anope::string &value) const anope_override {
        // The parameter of this mode is a bit different, it may begin with a '*',
        // ignore it if that's the case
        Anope::string v = value[0] == '*' ? value.substr(1) : value;
        return ((!value.empty()) && (ColonDelimitedParamMode::IsValid(v)));
    }
};

class ChannelModeHistory : public ColonDelimitedParamMode {
  public:
    ChannelModeHistory(char modeChar) : ColonDelimitedParamMode("HISTORY",
                modeChar) { }

    bool IsValid(Anope::string &value) const anope_override {
        return (ColonDelimitedParamMode::IsValid(value, true));
    }
};

class ChannelModeRedirect : public ChannelModeParam {
  public:
    ChannelModeRedirect(char modeChar) : ChannelModeParam("REDIRECT", modeChar,
                true) { }

    bool IsValid(Anope::string &value) const anope_override {
        // The parameter of this mode is a channel, and channel names start with '#'
        return ((!value.empty()) && (value[0] == '#'));
    }
};

struct IRCDMessageAway : Message::Away {
    IRCDMessageAway(Module *creator) : Message::Away(creator, "AWAY") {
        SetFlag(IRCDMESSAGE_REQUIRE_USER);
    }

    void Run(MessageSource &source,
             const std::vector<Anope::string> &params) anope_override {
        std::vector<Anope::string> newparams(params);
        if (newparams.size() > 1) {
            newparams.erase(newparams.begin());
        }

        Message::Away::Run(source, newparams);
    }
};

struct IRCDMessageCapab : Message::Capab {
    struct ModeInfo {
        // The letter assigned to the mode (e.g. o).
        char letter;

        // If a prefix mode then the rank of the prefix.
        unsigned level;

        // The name of the mode.
        Anope::string name;

        // If a prefix mode then the symbol associated with the prefix.
        char symbol;

        // The type of mode.
        Anope::string type;

        ModeInfo() : letter(0), level(0), symbol(0) { }
    };

    static bool ParseMode(const Anope::string& token, ModeInfo& mode) {
        // list:ban=b  param-set:limit=l  param:key=k  prefix:30000:op=@o  simple:noextmsg=n
        //     A   C            A     C        A   C         A     B  C          A        C
        Anope::string::size_type a = token.find(':');
        if (a == Anope::string::npos) {
            return false;
        }

        // If the mode is a prefix mode then it also has a rank.
        mode.type = token.substr(0, a);
        if (mode.type == "prefix") {
            Anope::string::size_type b = token.find(':', a + 1);
            if (b == Anope::string::npos) {
                return false;
            }

            const Anope::string modelevel = token.substr(a + 1, b - a - 1);
            mode.level = modelevel.is_pos_number_only() ? convertTo<unsigned>
                         (modelevel) : 0;
            a = b;
        }

        Anope::string::size_type c = token.find('=', a + 1);
        if (c == Anope::string::npos) {
            return false;
        }

        mode.name = token.substr(a + 1, c - a - 1);
        switch (token.length() - c) {
        case 2:
            mode.letter = token[c + 1];
            break;
        case 3:
            mode.symbol = token[c + 1];
            mode.letter = token[c + 2];
            break;
        default:
            return false;
        }

        Log(LOG_DEBUG) << "Parsed mode: " << "type=" << mode.type << " name=" <<
                       mode.name << " level="
                       << mode.level << " symbol=" << mode.symbol << " letter=" << mode.letter;
        return true;
    }

    IRCDMessageCapab(Module *creator) : Message::Capab(creator, "CAPAB") {
        SetFlag(IRCDMESSAGE_SOFT_LIMIT);
    }

    void Run(MessageSource &source,
             const std::vector<Anope::string> &params) anope_override {
        if (params[0].equals_cs("START")) {
            unsigned int spanningtree_proto_ver = 0;
            if (params.size() >= 2) {
                spanningtree_proto_ver = params[1].is_pos_number_only() ? convertTo<unsigned>
                (params[1]) : 0;
            }

            if (spanningtree_proto_ver < 1205) {
                UplinkSocket::Message() <<
                                        "ERROR :Protocol mismatch, no or invalid protocol version given in CAPAB START";
                Anope::QuitReason =
                    "Protocol mismatch, no or invalid protocol version given in CAPAB START";
                Anope::Quitting = true;
                return;
            }

            /* reset CAPAB */
            Servers::Capab.insert("SERVERS");
            Servers::Capab.insert("TOPICLOCK");
            IRCD->CanSQLineChannel = false;
            IRCD->CanSVSHold = false;
            IRCD->DefaultPseudoclientModes = "+oI";
        } else if (params[0].equals_cs("CHANMODES") && params.size() > 1) {
            spacesepstream ssep(params[1]);
            Anope::string capab;

            while (ssep.GetToken(capab)) {
                ModeInfo mode;
                if (!ParseMode(capab, mode)) {
                    continue;
                }

                ChannelMode *cm = NULL;
                if (mode.name.equals_cs("admin")) {
                    cm = new ChannelModeStatus("PROTECT", mode.letter, mode.symbol, mode.level);
                } else if (mode.name.equals_cs("allowinvite")) {
                    cm = new ChannelMode("ALLINVITE", mode.letter);
                    ModeManager::AddChannelMode(new InspIRCdExtban::EntryMatcher("INVITEBAN", "BAN",
                                                'A'));
                } else if (mode.name.equals_cs("auditorium")) {
                    cm = new ChannelMode("AUDITORIUM", mode.letter);
                } else if (mode.name.equals_cs("autoop")) {
                    cm = new InspIRCdAutoOpMode(mode.letter);
                } else if (mode.name.equals_cs("ban")) {
                    cm = new ChannelModeList("BAN", mode.letter);
                } else if (mode.name.equals_cs("banexception")) {
                    cm = new ChannelModeList("EXCEPT", mode.letter);
                } else if (mode.name.equals_cs("blockcaps")) {
                    cm = new ChannelMode("BLOCKCAPS", mode.letter);
                    ModeManager::AddChannelMode(new InspIRCdExtban::EntryMatcher("BLOCKCAPSBAN",
                                                "BAN", 'B'));
                } else if (mode.name.equals_cs("blockcolor")) {
                    cm = new ChannelMode("BLOCKCOLOR", mode.letter);
                    ModeManager::AddChannelMode(new InspIRCdExtban::EntryMatcher("BLOCKCOLORBAN",
                                                "BAN", 'c'));
                } else if (mode.name.equals_cs("c_registered")) {
                    cm = new ChannelModeNoone("REGISTERED", mode.letter);
                } else if (mode.name.equals_cs("censor")) {
                    cm = new ChannelMode("CENSOR", mode.letter);
                } else if (mode.name.equals_cs("delayjoin")) {
                    cm = new ChannelMode("DELAYEDJOIN", mode.letter);
                } else if (mode.name.equals_cs("delaymsg")) {
                    cm = new SimpleNumberParamMode("DELAYMSG", mode.letter);
                } else if (mode.name.equals_cs("filter")) {
                    cm = new ChannelModeList("FILTER", mode.letter);
                } else if (mode.name.equals_cs("flood")) {
                    cm = new ChannelModeFlood(mode.letter);
                } else if (mode.name.equals_cs("founder")) {
                    cm = new ChannelModeStatus("OWNER", mode.letter, mode.symbol, mode.level);
                } else if (mode.name.equals_cs("halfop")) {
                    cm = new ChannelModeStatus("HALFOP", mode.letter, mode.symbol, mode.level);
                } else if (mode.name.equals_cs("history")) {
                    cm = new ChannelModeHistory(mode.letter);
                } else if (mode.name.equals_cs("invex")) {
                    cm = new ChannelModeList("INVITEOVERRIDE", mode.letter);
                } else if (mode.name.equals_cs("inviteonly")) {
                    cm = new ChannelMode("INVITE", mode.letter);
                } else if (mode.name.equals_cs("joinflood")) {
                    cm = new ColonDelimitedParamMode("JOINFLOOD", mode.letter);
                } else if (mode.name.equals_cs("key")) {
                    cm = new ChannelModeKey(mode.letter);
                } else if (mode.name.equals_cs("kicknorejoin")) {
                    cm = new SimpleNumberParamMode("NOREJOIN", mode.letter);
                } else if (mode.name.equals_cs("limit")) {
                    cm = new ChannelModeParam("LIMIT", mode.letter, true);
                } else if (mode.name.equals_cs("moderated")) {
                    cm = new ChannelMode("MODERATED", mode.letter);
                } else if (mode.name.equals_cs("nickflood")) {
                    cm = new ColonDelimitedParamMode("NICKFLOOD", mode.letter);
                } else if (mode.name.equals_cs("noctcp")) {
                    cm = new ChannelMode("NOCTCP", mode.letter);
                    ModeManager::AddChannelMode(new InspIRCdExtban::EntryMatcher("NOCTCPBAN", "BAN",
                                                'C'));
                } else if (mode.name.equals_cs("noextmsg")) {
                    cm = new ChannelMode("NOEXTERNAL", mode.letter);
                } else if (mode.name.equals_cs("nokick")) {
                    cm = new ChannelMode("NOKICK", mode.letter);
                    ModeManager::AddChannelMode(new InspIRCdExtban::EntryMatcher("NOKICKBAN", "BAN",
                                                'Q'));
                } else if (mode.name.equals_cs("noknock")) {
                    cm = new ChannelMode("NOKNOCK", mode.letter);
                } else if (mode.name.equals_cs("nonick")) {
                    cm = new ChannelMode("NONICK", mode.letter);
                    ModeManager::AddChannelMode(new InspIRCdExtban::EntryMatcher("NONICKBAN", "BAN",
                                                'N'));
                } else if (mode.name.equals_cs("nonotice")) {
                    cm = new ChannelMode("NONOTICE", mode.letter);
                    ModeManager::AddChannelMode(new InspIRCdExtban::EntryMatcher("NONOTICEBAN",
                                                "BAN", 'T'));
                } else if (mode.name.equals_cs("official-join")) {
                    cm = new ChannelModeStatus("OFFICIALJOIN", mode.letter, mode.symbol,
                                               mode.level);
                } else if (mode.name.equals_cs("op")) {
                    cm = new ChannelModeStatus("OP", mode.letter, mode.symbol, mode.level);
                } else if (mode.name.equals_cs("operonly")) {
                    cm = new ChannelModeOperOnly("OPERONLY", mode.letter);
                } else if (mode.name.equals_cs("operprefix")) {
                    cm = new ChannelModeStatus("OPERPREFIX", mode.letter, mode.symbol, mode.level);
                } else if (mode.name.equals_cs("permanent")) {
                    cm = new ChannelMode("PERM", mode.letter);
                } else if (mode.name.equals_cs("private")) {
                    cm = new ChannelMode("PRIVATE", mode.letter);
                } else if (mode.name.equals_cs("redirect")) {
                    cm = new ChannelModeRedirect(mode.letter);
                } else if (mode.name.equals_cs("reginvite")) {
                    cm = new ChannelMode("REGISTEREDONLY", mode.letter);
                } else if (mode.name.equals_cs("regmoderated")) {
                    cm = new ChannelMode("REGMODERATED", mode.letter);
                } else if (mode.name.equals_cs("secret")) {
                    cm = new ChannelMode("SECRET", mode.letter);
                } else if (mode.name.equals_cs("sslonly")) {
                    cm = new ChannelMode("SSL", mode.letter);
                    ModeManager::AddChannelMode(new InspIRCdExtban::FingerprintMatcher("SSLBAN",
                                                "BAN", 'z'));
                } else if (mode.name.equals_cs("stripcolor")) {
                    cm = new ChannelMode("STRIPCOLOR", mode.letter);
                    ModeManager::AddChannelMode(new InspIRCdExtban::EntryMatcher("STRIPCOLORBAN",
                                                "BAN", 'S'));
                } else if (mode.name.equals_cs("topiclock")) {
                    cm = new ChannelMode("TOPIC", mode.letter);
                } else if (mode.name.equals_cs("voice")) {
                    cm = new ChannelModeStatus("VOICE", mode.letter, mode.symbol, mode.level);
                }

                // Handle unknown modes.
                else if (mode.type.equals_cs("list")) {
                    cm = new ChannelModeList(mode.name.upper(), mode.letter);
                } else if (mode.type.equals_cs("param-set")) {
                    cm = new ChannelModeParam(mode.name.upper(), mode.letter, true);
                } else if (mode.type.equals_cs("param")) {
                    cm = new ChannelModeParam(mode.name.upper(), mode.letter, false);
                } else if (mode.type.equals_cs("prefix")) {
                    cm = new ChannelModeStatus(mode.name.upper(), mode.letter, mode.symbol,
                                               mode.level);
                } else if (mode.type.equals_cs("simple")) {
                    cm = new ChannelMode(mode.name.upper(), mode.letter);
                } else {
                    Log(LOG_DEBUG) << "Unknown channel mode: " << capab;
                }

                if (cm) {
                    ModeManager::AddChannelMode(cm);
                }
            }
        }
        if (params[0].equals_cs("USERMODES") && params.size() > 1) {
            spacesepstream ssep(params[1]);
            Anope::string capab;

            while (ssep.GetToken(capab)) {
                ModeInfo mode;
                if (!ParseMode(capab, mode)) {
                    continue;
                }

                UserMode *um = NULL;
                if (mode.name.equals_cs("bot")) {
                    um = new UserMode("BOT", mode.letter);
                    IRCD->DefaultPseudoclientModes += mode.letter;
                } else if (mode.name.equals_cs("callerid")) {
                    um = new UserMode("CALLERID", mode.letter);
                } else if (mode.name.equals_cs("cloak")) {
                    um = new UserMode("CLOAK", mode.letter);
                } else if (mode.name.equals_cs("deaf")) {
                    um = new UserMode("DEAF", mode.letter);
                } else if (mode.name.equals_cs("deaf_commonchan")) {
                    um = new UserMode("COMMONCHANS", mode.letter);
                } else if (mode.name.equals_cs("helpop")) {
                    um = new UserModeOperOnly("HELPOP", mode.letter);
                } else if (mode.name.equals_cs("hidechans")) {
                    um = new UserMode("PRIV", mode.letter);
                } else if (mode.name.equals_cs("hideoper")) {
                    um = new UserModeOperOnly("HIDEOPER", mode.letter);
                } else if (mode.name.equals_cs("invisible")) {
                    um = new UserMode("INVIS", mode.letter);
                } else if (mode.name.equals_cs("invis-oper")) {
                    um = new UserModeOperOnly("INVISIBLE_OPER", mode.letter);
                } else if (mode.name.equals_cs("oper")) {
                    um = new UserModeOperOnly("OPER", mode.letter);
                } else if (mode.name.equals_cs("regdeaf")) {
                    um = new UserMode("REGPRIV", mode.letter);
                } else if (mode.name.equals_cs("servprotect")) {
                    um = new UserModeNoone("PROTECTED", mode.letter);
                    IRCD->DefaultPseudoclientModes += mode.letter;
                } else if (mode.name.equals_cs("showwhois")) {
                    um = new UserMode("WHOIS", mode.letter);
                } else if (mode.name.equals_cs("u_censor")) {
                    um = new UserMode("CENSOR", mode.letter);
                } else if (mode.name.equals_cs("u_registered")) {
                    um = new UserModeNoone("REGISTERED", mode.letter);
                } else if (mode.name.equals_cs("u_stripcolor")) {
                    um = new UserMode("STRIPCOLOR", mode.letter);
                } else if (mode.name.equals_cs("wallops")) {
                    um = new UserMode("WALLOPS", mode.letter);
                }

                // Handle unknown modes.
                else if (mode.type.equals_cs("param-set")) {
                    um = new UserModeParam(mode.name.upper(), mode.letter);
                } else if (mode.type.equals_cs("simple")) {
                    um = new UserMode(mode.name.upper(), mode.letter);
                } else {
                    Log(LOG_DEBUG) << "Unknown user mode: " << capab;
                }

                if (um) {
                    ModeManager::AddUserMode(um);
                }
            }
        } else if (params[0].equals_cs("MODULES") && params.size() > 1) {
            spacesepstream ssep(params[1]);
            Anope::string module;

            while (ssep.GetToken(module)) {
                if (module.equals_cs("m_svshold.so")) {
                    IRCD->CanSVSHold = true;
                } else if (module.find("m_rline.so") == 0) {
                    Servers::Capab.insert("RLINE");
                    const Anope::string &regexengine =
                        Config->GetBlock("options")->Get<const Anope::string>("regexengine");
                    if (!regexengine.empty() && module.length() > 11
                            && regexengine != module.substr(11)) {
                        Log() << "Warning: InspIRCd is using regex engine " << module.substr(
                                  11) << ", but we have " << regexengine << ". This may cause inconsistencies.";
                    }
                } else if (module.equals_cs("m_topiclock.so")) {
                    Servers::Capab.insert("TOPICLOCK");
                } else if (module.equals_cs("m_cban.so=glob")) {
                    IRCD->CanSQLineChannel = true;
                }
            }
        } else if (params[0].equals_cs("MODSUPPORT") && params.size() > 1) {
            spacesepstream ssep(params[1]);
            Anope::string module;

            while (ssep.GetToken(module)) {
                if (module.equals_cs("m_services_account.so")) {
                    Servers::Capab.insert("SERVICES");
                    ModeManager::AddChannelMode(new InspIRCdExtban::AccountMatcher("ACCOUNTBAN",
                                                "BAN", 'R'));
                    ModeManager::AddChannelMode(new
                                                InspIRCdExtban::UnidentifiedMatcher("UNREGISTEREDBAN", "BAN", 'U'));
                } else if (module.equals_cs("m_chghost.so")) {
                    Servers::Capab.insert("CHGHOST");
                } else if (module.equals_cs("m_chgident.so")) {
                    Servers::Capab.insert("CHGIDENT");
                } else if (module == "m_channelban.so") {
                    ModeManager::AddChannelMode(new InspIRCdExtban::ChannelMatcher("CHANNELBAN",
                                                "BAN", 'j'));
                } else if (module == "m_gecosban.so") {
                    ModeManager::AddChannelMode(new InspIRCdExtban::RealnameMatcher("REALNAMEBAN",
                                                "BAN", 'r'));
                } else if (module == "m_nopartmessage.so") {
                    ModeManager::AddChannelMode(new InspIRCdExtban::EntryMatcher("PARTMESSAGEBAN",
                                                "BAN", 'p'));
                } else if (module == "m_serverban.so") {
                    ModeManager::AddChannelMode(new InspIRCdExtban::ServerMatcher("SERVERBAN",
                                                "BAN", 's'));
                } else if (module == "m_muteban.so") {
                    ModeManager::AddChannelMode(new InspIRCdExtban::EntryMatcher("QUIET", "BAN",
                                                'm'));
                }
            }
        } else if (params[0].equals_cs("CAPABILITIES") && params.size() > 1) {
            spacesepstream ssep(params[1]);
            Anope::string capab;
            while (ssep.GetToken(capab)) {
                if (capab.find("MAXMODES=") != Anope::string::npos) {
                    Anope::string maxmodes(capab.begin() + 9, capab.end());
                    IRCD->MaxModes = maxmodes.is_pos_number_only() ? convertTo<unsigned>
                                     (maxmodes) : 3;
                } else if (capab == "GLOBOPS=1") {
                    Servers::Capab.insert("GLOBOPS");
                }
            }
        } else if (params[0].equals_cs("END")) {
            if (!Servers::Capab.count("SERVICES")) {
                UplinkSocket::Message() <<
                                        "ERROR :The services_account module is not loaded. This is required by Anope";
                Anope::QuitReason =
                    "ERROR: Remote server does not have the services_account module loaded, and this is required.";
                Anope::Quitting = true;
                return;
            }
            if (!ModeManager::FindUserModeByName("PRIV")) {
                UplinkSocket::Message() <<
                                        "ERROR :The hidechans module is not loaded. This is required by Anope";
                Anope::QuitReason =
                    "ERROR: Remote server does not have the hidechans module loaded, and this is required.";
                Anope::Quitting = true;
                return;
            }
            if (!IRCD->CanSVSHold) {
                Log() << "SVSHOLD missing, Usage disabled until module is loaded.";
            }
            if (!Servers::Capab.count("CHGHOST")) {
                Log() << "CHGHOST missing, Usage disabled until module is loaded.";
            }
            if (!Servers::Capab.count("CHGIDENT")) {
                Log() << "CHGIDENT missing, Usage disabled until module is loaded.";
            }
        }

        Message::Capab::Run(source, params);
    }
};

struct IRCDMessageEncap : IRCDMessage {
    IRCDMessageEncap(Module *creator) : IRCDMessage(creator, "ENCAP", 4) {
        SetFlag(IRCDMESSAGE_SOFT_LIMIT);
    }

    void Run(MessageSource &source,
             const std::vector<Anope::string> &params) anope_override {
        if (!Anope::Match(Me->GetSID(), params[0]) && !Anope::Match(Me->GetName(), params[0])) {
            return;
        }

        if (params[1] == "CHGIDENT") {
            User *u = User::Find(params[2]);
            if (!u || u->server != Me) {
                return;
            }

            u->SetIdent(params[3]);
            UplinkSocket::Message(u) << "FIDENT :" << params[3];
        } else if (params[1] == "CHGHOST") {
            User *u = User::Find(params[2]);
            if (!u || u->server != Me) {
                return;
            }

            u->SetDisplayedHost(params[3]);
            UplinkSocket::Message(u) << "FHOST :" << params[3];
        } else if (params[1] == "CHGNAME") {
            User *u = User::Find(params[2]);
            if (!u || u->server != Me) {
                return;
            }

            u->SetRealname(params[3]);
            UplinkSocket::Message(u) << "FNAME :" << params[3];
        } else if (SASL::sasl && params[1] == "SASL" && params.size() >= 6) {
            SASL::Message m;
            m.source = params[2];
            m.target = params[3];
            m.type = params[4];
            m.data = params[5];
            m.ext = params.size() > 6 ? params[6] : "";

            SASL::sasl->ProcessMessage(m);
        }
    }
};

struct IRCDMessageFHost : IRCDMessage {
    IRCDMessageFHost(Module *creator) : IRCDMessage(creator, "FHOST", 1) {
        SetFlag(IRCDMESSAGE_REQUIRE_USER);
    }

    void Run(MessageSource &source,
             const std::vector<Anope::string> &params) anope_override {
        User *u = source.GetUser();
        if (u->HasMode("CLOAK")) {
            u->RemoveModeInternal(source, ModeManager::FindUserModeByName("CLOAK"));
        }
        u->SetDisplayedHost(params[0]);
    }
};

struct IRCDMessageFIdent : IRCDMessage {
    IRCDMessageFIdent(Module *creator) : IRCDMessage(creator, "FIDENT", 1) {
        SetFlag(IRCDMESSAGE_REQUIRE_USER);
    }

    void Run(MessageSource &source,
             const std::vector<Anope::string> &params) anope_override {
        source.GetUser()->SetIdent(params[0]);
    }
};

struct IRCDMessageKick : IRCDMessage {
    IRCDMessageKick(Module *creator) : IRCDMessage(creator, "KICK", 3) {
        SetFlag(IRCDMESSAGE_SOFT_LIMIT);
    }

    void Run(MessageSource &source,
             const std::vector<Anope::string> &params) anope_override {
        // Received: :715AAAAAA KICK #chan 715AAAAAD :reason
        // Received: :715AAAAAA KICK #chan 628AAAAAA 4 :reason
        Channel *c = Channel::Find(params[0]);
        if (!c) {
            return;
        }

        const Anope::string &reason = params.size() > 3 ? params[3] : params[2];
        c->KickInternal(source, params[1], reason);
    }
};

struct IRCDMessageSave : IRCDMessage {
    time_t last_collide;

    IRCDMessageSave(Module *creator) : IRCDMessage(creator, "SAVE", 2),
        last_collide(0) { }

    void Run(MessageSource &source,
             const std::vector<Anope::string> &params) anope_override {
        User *targ = User::Find(params[0]);
        time_t ts;

        try {
            ts = convertTo<time_t>(params[1]);
        } catch (const ConvertException &) {
            return;
        }

        if (!targ || targ->timestamp != ts) {
            return;
        }

        BotInfo *bi;
        if (targ->server == Me && (bi = dynamic_cast<BotInfo *>(targ))) {
            if (last_collide == Anope::CurTime) {
                Anope::QuitReason = "Nick collision fight on " + targ->nick;
                Anope::Quitting = true;
                return;
            }

            IRCD->SendKill(Me, targ->nick, "Nick collision");
            IRCD->SendNickChange(targ, targ->nick);
            last_collide = Anope::CurTime;
        } else {
            targ->ChangeNick(targ->GetUID());
        }
    }
};

class IRCDMessageMetadata : IRCDMessage {
    const bool &do_topiclock;
    const bool &do_mlock;
    PrimitiveExtensibleItem<ListLimits> &maxlist;

  public:
    IRCDMessageMetadata(Module *creator, const bool &handle_topiclock,
                        const bool &handle_mlock,
                        PrimitiveExtensibleItem<ListLimits> &listlimits) : IRCDMessage(creator,
                                    "METADATA", 3), do_topiclock(handle_topiclock), do_mlock(handle_mlock),
        maxlist(listlimits) {
        SetFlag(IRCDMESSAGE_REQUIRE_SERVER);
        SetFlag(IRCDMESSAGE_SOFT_LIMIT);
    }

    void Run(MessageSource &source,
             const std::vector<Anope::string> &params) anope_override {
        // We deliberately ignore non-bursting servers to avoid pseudoserver fights
        // Channel METADATA has an additional parameter: the channel TS
        // Received: :715 METADATA #chan 1572026333 mlock :nt
        if ((params[0][0] == '#') && (params.size() > 3) && (!source.GetServer()->IsSynced())) {
            Channel *c = Channel::Find(params[0]);
            if (c) {
                if ((c->ci) && (do_mlock) && (params[2] == "mlock")) {
                    ModeLocks *modelocks = c->ci->GetExt<ModeLocks>("modelocks");
                    Anope::string modes;
                    if (modelocks) {
                        modes = modelocks->GetMLockAsString(false).replace_all_cs("+",
                                "").replace_all_cs("-", "");
                    }

                    // Mode lock string is not what we say it is?
                    if (modes != params[3]) {
                        UplinkSocket::Message(Me) << "METADATA " << c->name << " " << c->creation_time
                                                  << " mlock :" << modes;
                    }
                } else if ((c->ci) && (do_topiclock) && (params[2] == "topiclock")) {
                    bool mystate = c->ci->HasExt("TOPICLOCK");
                    bool serverstate = (params[3] == "1");
                    if (mystate != serverstate) {
                        UplinkSocket::Message(Me) << "METADATA " << c->name << " " << c->creation_time
                                                  << " topiclock :" << (mystate ? "1" : "");
                    }
                } else if (params[2] == "maxlist") {
                    ListLimits limits;
                    spacesepstream limitstream(params[3]);
                    Anope::string modechr, modelimit;
                    while (limitstream.GetToken(modechr) && limitstream.GetToken(modelimit)) {
                        limits.insert(std::make_pair(modechr[0], convertTo<unsigned>(modelimit)));
                    }
                    maxlist.Set(c, limits);
                }
            }
        } else if (isdigit(params[0][0])) {
            if (params[1].equals_cs("accountname")) {
                User *u = User::Find(params[0]);
                NickCore *nc = NickCore::Find(params[2]);
                if (u && nc) {
                    u->Login(nc);
                }
            }

            /*
             *   possible incoming ssl_cert messages:
             *   Received: :409 METADATA 409AAAAAA ssl_cert :vTrSe c38070ce96e41cc144ed6590a68d45a6 <...> <...>
             *   Received: :409 METADATA 409AAAAAC ssl_cert :vTrSE Could not get peer certificate: error:00000000:lib(0):func(0):reason(0)
             */
            else if (params[1].equals_cs("ssl_cert")) {
                User *u = User::Find(params[0]);
                if (!u) {
                    return;
                }
                u->Extend<bool>("ssl");
                Anope::string data = params[2].c_str();
                size_t pos1 = data.find(' ') + 1;
                size_t pos2 = data.find(' ', pos1);
                if ((pos2 - pos1) >=
                        32) { // inspircd supports md5 and sha1 fingerprint hashes -> size 32 or 40 bytes.
                    u->fingerprint = data.substr(pos1, pos2 - pos1);
                }
                FOREACH_MOD(OnFingerprint, (u));
            }
        } else if (params[0] == "*") {
            // Wed Oct  3 15:40:27 2012: S[14] O :20D METADATA * modules :-m_svstopic.so

            if (params[1].equals_cs("modules") && !params[2].empty()) {
                // only interested when it comes from our uplink
                Server* server = source.GetServer();
                if (!server || server->GetUplink() != Me) {
                    return;
                }

                bool plus = (params[2][0] == '+');
                if (!plus && params[2][0] != '-') {
                    return;
                }

                bool required = false;
                Anope::string capab, module = params[2].substr(1);

                if (module.equals_cs("m_services_account.so")) {
                    required = true;
                } else if (module.equals_cs("m_hidechans.so")) {
                    required = true;
                } else if (module.equals_cs("m_chghost.so")) {
                    capab = "CHGHOST";
                } else if (module.equals_cs("m_chgident.so")) {
                    capab = "CHGIDENT";
                } else if (module.equals_cs("m_svshold.so")) {
                    capab = "SVSHOLD";
                } else if (module.equals_cs("m_rline.so")) {
                    capab = "RLINE";
                } else if (module.equals_cs("m_topiclock.so")) {
                    capab = "TOPICLOCK";
                } else {
                    return;
                }

                if (required) {
                    if (!plus) {
                        Log() << "Warning: InspIRCd unloaded module " << module <<
                              ", Anope won't function correctly without it";
                    }
                } else {
                    if (plus) {
                        Servers::Capab.insert(capab);
                    } else {
                        Servers::Capab.erase(capab);
                    }

                    Log() << "InspIRCd " << (plus ? "loaded" : "unloaded") << " module " << module
                          << ", adjusted functionality";
                }

            }
        }
    }
};

struct IRCDMessageEndburst : IRCDMessage {
    IRCDMessageEndburst(Module *creator) : IRCDMessage(creator, "ENDBURST", 0) {
        SetFlag(IRCDMESSAGE_REQUIRE_SERVER);
    }

    void Run(MessageSource &source,
             const std::vector<Anope::string> &params) anope_override {
        Server *s = source.GetServer();

        Log(LOG_DEBUG) << "Processed ENDBURST for " << s->GetName();

        s->Sync(true);
    }
};

struct IRCDMessageFJoin : IRCDMessage {
    IRCDMessageFJoin(Module *creator) : IRCDMessage(creator, "FJOIN", 2) {
        SetFlag(IRCDMESSAGE_REQUIRE_SERVER);
        SetFlag(IRCDMESSAGE_SOFT_LIMIT);
    }

    void Run(MessageSource &source,
             const std::vector<Anope::string> &params) anope_override {
        Anope::string modes;
        if (params.size() >= 3) {
            for (unsigned i = 2; i < params.size() - 1; ++i) {
                modes += " " + params[i];
            }
            if (!modes.empty()) {
                modes.erase(modes.begin());
            }
        }

        std::list<Message::Join::SJoinUser> users;

        spacesepstream sep(params[params.size() - 1]);
        Anope::string buf;
        while (sep.GetToken(buf)) {
            Message::Join::SJoinUser sju;

            /* Loop through prefixes and find modes for them */
            for (char c; (c = buf[0]) != ',' && c;) {
                buf.erase(buf.begin());
                sju.first.AddMode(c);
            }
            /* Erase the , */
            if (!buf.empty()) {
                buf.erase(buf.begin());
            }

            /* Erase the :membid */
            if (!buf.empty()) {
                Anope::string::size_type membid = buf.find(':');
                if (membid != Anope::string::npos) {
                    buf.erase(membid, Anope::string::npos);
                }
            }

            sju.second = User::Find(buf);
            if (!sju.second) {
                Log(LOG_DEBUG) << "FJOIN for nonexistent user " << buf << " on " << params[0];
                continue;
            }

            users.push_back(sju);
        }

        time_t ts = Anope::string(params[1]).is_pos_number_only() ? convertTo<time_t>(params[1]) : Anope::CurTime;
        Message::Join::SJoin(source, params[0], ts, modes, users);
    }
};

struct IRCDMessageFMode : IRCDMessage {
    IRCDMessageFMode(Module *creator) : IRCDMessage(creator, "FMODE", 3) {
        SetFlag(IRCDMESSAGE_SOFT_LIMIT);
    }

    void Run(MessageSource &source,
             const std::vector<Anope::string> &params) anope_override {
        /* :source FMODE #test 12345678 +nto foo */

        Anope::string modes = params[2];
        for (unsigned n = 3; n < params.size(); ++n) {
            modes += " " + params[n];
        }

        Channel *c = Channel::Find(params[0]);
        time_t ts;

        try {
            ts = convertTo<time_t>(params[1]);
        } catch (const ConvertException &) {
            ts = 0;
        }

        if (c) {
            c->SetModesInternal(source, modes, ts);
        }
    }
};

struct IRCDMessageFTopic : IRCDMessage {
    IRCDMessageFTopic(Module *creator) : IRCDMessage(creator, "FTOPIC", 4) {
        SetFlag(IRCDMESSAGE_SOFT_LIMIT);
    }

    void Run(MessageSource &source,
             const std::vector<Anope::string> &params) anope_override {
        // :source FTOPIC channel ts topicts :topic
        // :source FTOPIC channel ts topicts setby :topic (burst or RESYNC)

        const Anope::string &setby = params.size() > 4 ? params[3] : source.GetName();
        const Anope::string &topic = params.size() > 4 ? params[4] : params[3];

        Channel *c = Channel::Find(params[0]);
        if (c) {
            c->ChangeTopicInternal(NULL, setby, topic,
                                   params[2].is_pos_number_only() ? convertTo<time_t>(params[2]) :
                                   Anope::CurTime);
        }
    }
};

struct IRCDMessageIdle : IRCDMessage {
    IRCDMessageIdle(Module *creator) : IRCDMessage(creator, "IDLE", 1) { }

    void Run(MessageSource &source,
             const std::vector<Anope::string> &params) anope_override {
        BotInfo *bi = BotInfo::Find(params[0]);
        if (bi) {
            UplinkSocket::Message(bi) << "IDLE " << source.GetSource() << " " <<
                                      Anope::StartTime << " " << (Anope::CurTime - bi->lastmsg);
        } else {
            User *u = User::Find(params[0]);
            if (u && u->server == Me) {
                UplinkSocket::Message(u) <<  "IDLE " << source.GetSource() << " " <<
                                         Anope::StartTime << " 0";
            }
        }
    }
};

struct IRCDMessageIJoin : IRCDMessage {
    IRCDMessageIJoin(Module *creator) : IRCDMessage(creator, "IJOIN", 2) {
        SetFlag(IRCDMESSAGE_REQUIRE_USER);
        SetFlag(IRCDMESSAGE_SOFT_LIMIT);
    }

    void Run(MessageSource &source,
             const std::vector<Anope::string> &params) anope_override {
        // :<uid> IJOIN <chan> <membid> [<ts> [<flags>]]
        Channel *c = Channel::Find(params[0]);
        if (!c) {
            // When receiving an IJOIN, first check if the target channel exists. If it does not exist,
            // ignore the join (that is, do not create the channel) and send a RESYNC back to the source.
            UplinkSocket::Message(Me) <<  "RESYNC :" << params[0];
            return;
        }

        // If the channel does exist then join the user, and in case any prefix modes were sent (found in
        // the 3rd parameter), compare the TS of the channel to the TS in the IJOIN (2nd parameter). If
        // the timestamps match, set the modes on the user, otherwise ignore the modes.
        Message::Join::SJoinUser user;
        user.second = source.GetUser();

        time_t chants = Anope::CurTime;
        if (params.size() >= 4) {
            chants = params[2].is_pos_number_only() ? convertTo<unsigned>(params[2]) : 0;
            for (unsigned i = 0; i < params[3].length(); ++i) {
                user.first.AddMode(params[3][i]);
            }
        }

        std::list<Message::Join::SJoinUser> users;
        users.push_back(user);
        Message::Join::SJoin(source, params[0], chants, "", users);
    }
};

struct IRCDMessageMode : IRCDMessage {
    IRCDMessageMode(Module *creator) : IRCDMessage(creator, "MODE", 2) {
        SetFlag(IRCDMESSAGE_SOFT_LIMIT);
    }

    void Run(MessageSource &source,
             const std::vector<Anope::string> &params) anope_override {
        if (IRCD->IsChannelValid(params[0])) {
            Channel *c = Channel::Find(params[0]);

            Anope::string modes = params[1];
            for (unsigned n = 2; n < params.size(); ++n) {
                modes += " " + params[n];
            }

            if (c) {
                c->SetModesInternal(source, modes);
            }
        } else {
            /* InspIRCd lets opers change another
               users modes, we have to kludge this
               as it slightly breaks RFC1459
             */
            User *u = User::Find(params[0]);
            if (u) {
                u->SetModesInternal(source, "%s", params[1].c_str());
            }
        }
    }
};

struct IRCDMessageNick : IRCDMessage {
    IRCDMessageNick(Module *creator) : IRCDMessage(creator, "NICK", 2) {
        SetFlag(IRCDMESSAGE_REQUIRE_USER);
    }

    void Run(MessageSource &source,
             const std::vector<Anope::string> &params) anope_override {
        source.GetUser()->ChangeNick(params[0]);
    }
};

struct IRCDMessageOperType : IRCDMessage {
    IRCDMessageOperType(Module *creator) : IRCDMessage(creator, "OPERTYPE", 0) {
        SetFlag(IRCDMESSAGE_SOFT_LIMIT);
        SetFlag(IRCDMESSAGE_REQUIRE_USER);
    }

    void Run(MessageSource &source,
             const std::vector<Anope::string> &params) anope_override {
        /* opertype is equivalent to mode +o because servers
           don't do this directly */
        User *u = source.GetUser();
        if (!u->HasMode("OPER")) {
            u->SetModesInternal(source, "+o");
        }
    }
};

struct IRCDMessagePing : IRCDMessage {
    IRCDMessagePing(Module *creator) : IRCDMessage(creator, "PING", 1) {
        SetFlag(IRCDMESSAGE_SOFT_LIMIT);
        SetFlag(IRCDMESSAGE_REQUIRE_SERVER);
    }

    void Run(MessageSource &source,
             const std::vector<Anope::string> &params) anope_override {
        if (params[0] == Me->GetSID()) {
            IRCD->SendPong(params[0], source.GetServer()->GetSID());
        }
    }
};

struct IRCDMessageRSQuit : IRCDMessage {
    IRCDMessageRSQuit(Module *creator) : IRCDMessage(creator, "RSQUIT", 1) {
        SetFlag(IRCDMESSAGE_SOFT_LIMIT);
    }

    void Run(MessageSource &source,
             const std::vector<Anope::string> &params) anope_override {
        Server *s = Server::Find(params[0]);
        const Anope::string &reason = params.size() > 1 ? params[1] : "";
        if (!s) {
            return;
        }

        UplinkSocket::Message(Me) << "SQUIT " << s->GetSID() << " :" << reason;
        s->Delete(s->GetName() + " " + s->GetUplink()->GetName());
    }
};

struct IRCDMessageServer : IRCDMessage {
    IRCDMessageServer(Module *creator) : IRCDMessage(creator, "SERVER", 3) {
        SetFlag(IRCDMESSAGE_REQUIRE_SERVER);
        SetFlag(IRCDMESSAGE_SOFT_LIMIT);
    }

    void Run(MessageSource &source,
             const std::vector<Anope::string> &params) anope_override {
        if (!source.GetServer() && params.size() == 5) {
            /*
             * SERVER testnet.inspircd.org hunter7 0 123 :InspIRCd Test Network
             * 0: name
             * 1: pass
             * 2: hops
             * 3: numeric
             * 4: desc
             */
            unsigned int hops = Anope::string(params[2]).is_pos_number_only() ?
            convertTo<unsigned>(params[2]) : 0;
            new Server(Me, params[0], hops, params[4], params[3]);
        } else if (source.GetServer()) {
            /*
             * SERVER testnet.inspircd.org 123 burst=1234 hidden=0 :InspIRCd Test Network
             * 0: name
             * 1: numeric
             * 2 to N-1: various key=value pairs.
             * N: desc
             */
            new Server(source.GetServer(), params[0], 1, params.back(), params[1]);
        }
    }
};

struct IRCDMessageSQuit : Message::SQuit {
    IRCDMessageSQuit(Module *creator) : Message::SQuit(creator) { }

    void Run(MessageSource &source,
             const std::vector<Anope::string> &params) anope_override {
        if (params[0] == rsquit_id || params[0] == rsquit_server) {
            /* squit for a recently squit server, introduce the juped server now */
            Server *s = Server::Find(rsquit_server);

            rsquit_id.clear();
            rsquit_server.clear();

            if (s && s->IsJuped()) {
                IRCD->SendServer(s);
            }
        } else {
            Message::SQuit::Run(source, params);
        }
    }
};

struct IRCDMessageTime : IRCDMessage {
    IRCDMessageTime(Module *creator) : IRCDMessage(creator, "TIME", 2) { }

    void Run(MessageSource &source,
             const std::vector<Anope::string> &params) anope_override {
        UplinkSocket::Message(Me) << "TIME " << source.GetSource() << " " << params[1] << " " << Anope::CurTime;
    }
};

struct IRCDMessageUID : IRCDMessage {
    IRCDMessageUID(Module *creator) : IRCDMessage(creator, "UID", 8) {
        SetFlag(IRCDMESSAGE_REQUIRE_SERVER);
        SetFlag(IRCDMESSAGE_SOFT_LIMIT);
    }

    /*
     * [Nov 03 22:09:58.176252 2009] debug: Received: :964 UID 964AAAAAC 1225746297 w00t2 localhost testnet.user w00t 127.0.0.1 1225746302 +iosw +ACGJKLNOQcdfgjklnoqtx :Robin Burchell <w00t@inspircd.org>
     * 0: uid
     * 1: ts
     * 2: nick
     * 3: host
     * 4: dhost
     * 5: ident
     * 6: ip
     * 7: signon
     * 8+: modes and params -- IMPORTANT, some modes (e.g. +s) may have parameters. So don't assume a fixed position of realname!
     * last: realname
     */
    void Run(MessageSource &source,
             const std::vector<Anope::string> &params) anope_override {
        time_t ts = convertTo<time_t>(params[1]);

        Anope::string modes = params[8];
        for (unsigned i = 9; i < params.size() - 1; ++i) {
            modes += " " + params[i];
        }

        NickAlias *na = NULL;
        if (SASL::sasl)
            for (std::list<SASLUser>::iterator it = saslusers.begin(); it != saslusers.end();) {
                SASLUser &u = *it;

                if (u.created + 30 < Anope::CurTime) {
                    it = saslusers.erase(it);
                } else if (u.uid == params[0]) {
                    na = NickAlias::Find(u.acc);
                    it = saslusers.erase(it);
                } else {
                    ++it;
                }
            }

        User *u = User::OnIntroduce(params[2], params[5], params[3], params[4], params[6], source.GetServer(), params[params.size() - 1], ts, modes, params[0], na ? *na->nc : NULL);
        if (u) {
            u->signon = convertTo<time_t>(params[7]);
        }
    }
};

class ProtoInspIRCd3 : public Module {
    InspIRCd3Proto ircd_proto;
    ExtensibleItem<bool> ssl;

    /* Core message handlers */
    Message::Error message_error;
    Message::Invite message_invite;
    Message::Kill message_kill;
    Message::MOTD message_motd;
    Message::Notice message_notice;
    Message::Part message_part;
    Message::Privmsg message_privmsg;
    Message::Quit message_quit;
    Message::Stats message_stats;

    /* Our message handlers */
    IRCDMessageAway message_away;
    IRCDMessageCapab message_capab;
    IRCDMessageEncap message_encap;
    IRCDMessageEndburst message_endburst;
    IRCDMessageFHost message_fhost;
    IRCDMessageFIdent message_fident;
    IRCDMessageFJoin message_fjoin;
    IRCDMessageFMode message_fmode;
    IRCDMessageFTopic message_ftopic;
    IRCDMessageIdle message_idle;
    IRCDMessageIJoin message_ijoin;
    IRCDMessageKick message_kick;
    IRCDMessageMetadata message_metadata;
    IRCDMessageMode message_mode;
    IRCDMessageNick message_nick;
    IRCDMessageOperType message_opertype;
    IRCDMessagePing message_ping;
    IRCDMessageRSQuit message_rsquit;
    IRCDMessageSave message_save;
    IRCDMessageServer message_server;
    IRCDMessageSQuit message_squit;
    IRCDMessageTime message_time;
    IRCDMessageUID message_uid;

    bool use_server_side_topiclock, use_server_side_mlock;

    void SendChannelMetadata(Channel *c, const Anope::string &metadataname,
                             const Anope::string &value) {
        UplinkSocket::Message(Me) << "METADATA " << c->name << " " << c->creation_time
                                  << " " << metadataname << " :" << value;
    }

  public:
    ProtoInspIRCd3(const Anope::string &modname,
                   const Anope::string &creator) : Module(modname, creator, PROTOCOL | VENDOR),
        ircd_proto(this), ssl(this, "ssl"),
        message_error(this), message_invite(this), message_kill(this),
        message_motd(this), message_notice(this),
        message_part(this), message_privmsg(this), message_quit(this),
        message_stats(this),
        message_away(this), message_capab(this), message_encap(this),
        message_endburst(this), message_fhost(this),
        message_fident(this), message_fjoin(this), message_fmode(this),
        message_ftopic(this), message_idle(this),
        message_ijoin(this), message_kick(this), message_metadata(this,
                use_server_side_topiclock, use_server_side_mlock, ircd_proto.maxlist),
        message_mode(this), message_nick(this), message_opertype(this),
        message_ping(this), message_rsquit(this),
        message_save(this), message_server(this), message_squit(this),
        message_time(this), message_uid(this) {
    }

    void OnReload(Configuration::Conf *conf) anope_override {
        use_server_side_topiclock = conf->GetModule(this)->Get<bool>("use_server_side_topiclock");
        use_server_side_mlock = conf->GetModule(this)->Get<bool>("use_server_side_mlock");
    }

    void OnUserNickChange(User *u, const Anope::string &) anope_override {
        u->RemoveModeInternal(Me, ModeManager::FindUserModeByName("REGISTERED"));
    }

    void OnChannelSync(Channel *c) anope_override {
        if (c->ci) {
            this->OnChanRegistered(c->ci);
        }
    }

    void OnChanRegistered(ChannelInfo *ci) anope_override {
        ModeLocks *modelocks = ci->GetExt<ModeLocks>("modelocks");
        if (use_server_side_mlock && ci->c && modelocks && !modelocks->GetMLockAsString(false).empty()) {
            Anope::string modes = modelocks->GetMLockAsString(false).replace_all_cs("+",
                    "").replace_all_cs("-", "");
            SendChannelMetadata(ci->c, "mlock", modes);
        }

        if (use_server_side_topiclock && Servers::Capab.count("TOPICLOCK") && ci->c) {
            if (ci->HasExt("TOPICLOCK")) {
                SendChannelMetadata(ci->c, "topiclock", "1");
            }
        }
    }

    void OnDelChan(ChannelInfo *ci) anope_override {
        if (use_server_side_mlock && ci->c) {
            SendChannelMetadata(ci->c, "mlock", "");
        }

        if (use_server_side_topiclock && Servers::Capab.count("TOPICLOCK") && ci->c) {
            SendChannelMetadata(ci->c, "topiclock", "");
        }
    }

    EventReturn OnMLock(ChannelInfo *ci, ModeLock *lock) anope_override {
        ModeLocks *modelocks = ci->GetExt<ModeLocks>("modelocks");
        ChannelMode *cm = ModeManager::FindChannelModeByName(lock->name);
        if (use_server_side_mlock && cm && ci->c && modelocks && (cm->type == MODE_REGULAR || cm->type == MODE_PARAM)) {
            Anope::string modes = modelocks->GetMLockAsString(false).replace_all_cs("+",
                    "").replace_all_cs("-", "") + cm->mchar;
            SendChannelMetadata(ci->c, "mlock", modes);
        }

        return EVENT_CONTINUE;
    }

    EventReturn OnUnMLock(ChannelInfo *ci, ModeLock *lock) anope_override {
        ModeLocks *modelocks = ci->GetExt<ModeLocks>("modelocks");
        ChannelMode *cm = ModeManager::FindChannelModeByName(lock->name);
        if (use_server_side_mlock && cm && ci->c && modelocks && (cm->type == MODE_REGULAR || cm->type == MODE_PARAM)) {
            Anope::string modes = modelocks->GetMLockAsString(false).replace_all_cs("+",
                    "").replace_all_cs("-", "").replace_all_cs(cm->mchar, "");
            SendChannelMetadata(ci->c, "mlock", modes);
        }

        return EVENT_CONTINUE;
    }

    EventReturn OnSetChannelOption(CommandSource &source, Command *cmd,
                                   ChannelInfo *ci, const Anope::string &setting) anope_override {
        if (cmd->name == "chanserv/topic" && ci->c) {
            if (setting == "topiclock on") {
                SendChannelMetadata(ci->c, "topiclock", "1");
            } else if (setting == "topiclock off") {
                SendChannelMetadata(ci->c, "topiclock", "0");
            }
        }

        return EVENT_CONTINUE;
    }
};

MODULE_INIT(ProtoInspIRCd3)
