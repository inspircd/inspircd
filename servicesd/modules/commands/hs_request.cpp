/* hs_request.c - Add request and activate functionality to HostServ
 *
 * (C) 2003-2023 Anope Team
 * Contact us at team@anope.org
 *
 * Based on the original module by Rob <rob@anope.org>
 * Included in the Anope module pack since Anope 1.7.11
 * Anope Coder: GeniusDex <geniusdex@anope.org>
 *
 * Please read COPYING and README for further details.
 *
 * Send bug reports to the Anope Coder instead of the module
 * author, because any changes since the inclusion into anope
 * are not supported by the original author.
 */

#include "module.h"

static ServiceReference<MemoServService> memoserv("MemoServService",
        "MemoServ");

static void req_send_memos(Module *me, CommandSource &source,
                           const Anope::string &vIdent, const Anope::string &vHost);

struct HostRequest : Serializable {
    Anope::string nick;
    Anope::string ident;
    Anope::string host;
    time_t time;

    HostRequest(Extensible *) : Serializable("HostRequest") { }

    void Serialize(Serialize::Data &data) const anope_override {
        data["nick"] << this->nick;
        data["ident"] << this->ident;
        data["host"] << this->host;
        data.SetType("time", Serialize::Data::DT_INT);
        data["time"] << this->time;
    }

    static Serializable* Unserialize(Serializable *obj, Serialize::Data &data) {
        Anope::string snick;
        data["nick"] >> snick;

        NickAlias *na = NickAlias::Find(snick);
        if (na == NULL) {
            return NULL;
        }

        HostRequest *req;
        if (obj) {
            req = anope_dynamic_static_cast<HostRequest *>(obj);
        } else {
            req = na->Extend<HostRequest>("hostrequest");
        }
        if (req) {
            req->nick = na->nick;
            data["ident"] >> req->ident;
            data["host"] >> req->host;
            data["time"] >> req->time;
        }

        return req;
    }
};

class CommandHSRequest : public Command {
    bool isvalidchar(char c) {
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')
                || c == '.' || c == '-') {
            return true;
        }
        return false;
    }

  public:
    CommandHSRequest(Module *creator) : Command(creator, "hostserv/request", 1, 1) {
        this->SetDesc(_("Request a vHost for your nick"));
        this->SetSyntax(_("vhost"));
    }

    void Execute(CommandSource &source,
                 const std::vector<Anope::string> &params) anope_override {
        if (Anope::ReadOnly) {
            source.Reply(READ_ONLY_MODE);
            return;
        }

        User *u = source.GetUser();
        NickAlias *na = NickAlias::Find(source.GetNick());
        if (!na || na->nc != source.GetAccount()) {
            source.Reply(ACCESS_DENIED);
            return;
        }

        if (source.GetAccount()->HasExt("UNCONFIRMED")) {
            source.Reply(
                _("You must confirm your account before you may request a vhost."));
            return;
        }

        Anope::string rawhostmask = params[0];

        Anope::string user, host;
        size_t a = rawhostmask.find('@');

        if (a == Anope::string::npos) {
            host = rawhostmask;
        } else {
            user = rawhostmask.substr(0, a);
            host = rawhostmask.substr(a + 1);
        }

        if (host.empty()) {
            this->OnSyntaxError(source, "");
            return;
        }

        if (!user.empty()) {
            if (user.length() > Config->GetBlock("networkinfo")->Get<unsigned>("userlen")) {
                source.Reply(HOST_SET_IDENTTOOLONG,
                             Config->GetBlock("networkinfo")->Get<unsigned>("userlen"));
                return;
            } else if (!IRCD->CanSetVIdent) {
                source.Reply(HOST_NO_VIDENT);
                return;
            }
            for (Anope::string::iterator s = user.begin(), s_end = user.end(); s != s_end;
                    ++s)
                if (!isvalidchar(*s)) {
                    source.Reply(HOST_SET_IDENT_ERROR);
                    return;
                }
        }

        if (host.length() > Config->GetBlock("networkinfo")->Get<unsigned>("hostlen")) {
            source.Reply(HOST_SET_TOOLONG,
                         Config->GetBlock("networkinfo")->Get<unsigned>("hostlen"));
            return;
        }

        if (!IRCD->IsHostValid(host)) {
            source.Reply(HOST_SET_ERROR);
            return;
        }

        time_t send_delay = Config->GetModule("memoserv")->Get<time_t>("senddelay");
        if (Config->GetModule(this->owner)->Get<bool>("memooper") && send_delay > 0 && u && u->lastmemosend + send_delay > Anope::CurTime) {
            source.Reply(_("Please wait %d seconds before requesting a new vHost."),
                         send_delay);
            u->lastmemosend = Anope::CurTime;
            return;
        }

        HostRequest req(na);
        req.nick = source.GetNick();
        req.ident = user;
        req.host = host;
        req.time = Anope::CurTime;
        na->Extend<HostRequest>("hostrequest", req);

        source.Reply(_("Your vHost has been requested."));
        req_send_memos(owner, source, user, host);
        Log(LOG_COMMAND, source, this) << "to request new vhost " << (!user.empty() ? user + "@" : "") << host;
    }

    bool OnHelp(CommandSource &source,
                const Anope::string &subcommand) anope_override {
        this->SendSyntax(source);
        source.Reply(" ");
        source.Reply(_("Request the given vHost to be activated for your nick by the\n"
                       "network administrators. Please be patient while your request\n"
                       "is being considered."));
        return true;
    }
};

class CommandHSActivate : public Command {
  public:
    CommandHSActivate(Module *creator) : Command(creator, "hostserv/activate", 1,
                1) {
        this->SetDesc(_("Approve the requested vHost of a user"));
        this->SetSyntax(_("\037nick\037"));
    }

    void Execute(CommandSource &source,
                 const std::vector<Anope::string> &params) anope_override {
        if (Anope::ReadOnly) {
            source.Reply(READ_ONLY_MODE);
            return;
        }

        const Anope::string &nick = params[0];

        NickAlias *na = NickAlias::Find(nick);
        HostRequest *req = na ? na->GetExt<HostRequest>("hostrequest") : NULL;
        if (req) {
            na->SetVhost(req->ident, req->host, source.GetNick(), req->time);
            FOREACH_MOD(OnSetVhost, (na));

            if (Config->GetModule(this->owner)->Get<bool>("memouser") && memoserv) {
                memoserv->Send(source.service->nick, na->nick,
                               _("[auto memo] Your requested vHost has been approved."), true);
            }

            source.Reply(_("vHost for %s has been activated."), na->nick.c_str());
            Log(LOG_COMMAND, source,
                this) << "for " << na->nick << " for vhost " << (!req->ident.empty() ?
                        req->ident + "@" : "") << req->host;
            na->Shrink<HostRequest>("hostrequest");
        } else {
            source.Reply(_("No request for nick %s found."), nick.c_str());
        }
    }

    bool OnHelp(CommandSource &source,
                const Anope::string &subcommand) anope_override {
        this->SendSyntax(source);
        source.Reply(" ");
        source.Reply(_("Activate the requested vHost for the given nick."));
        if (Config->GetModule(this->owner)->Get<bool>("memouser")) {
            source.Reply(_("A memo informing the user will also be sent."));
        }

        return true;
    }
};

class CommandHSReject : public Command {
  public:
    CommandHSReject(Module *creator) : Command(creator, "hostserv/reject", 1, 2) {
        this->SetDesc(_("Reject the requested vHost of a user"));
        this->SetSyntax(_("\037nick\037 [\037reason\037]"));
    }

    void Execute(CommandSource &source,
                 const std::vector<Anope::string> &params) anope_override {
        if (Anope::ReadOnly) {
            source.Reply(READ_ONLY_MODE);
            return;
        }

        const Anope::string &nick = params[0];
        const Anope::string &reason = params.size() > 1 ? params[1] : "";

        NickAlias *na = NickAlias::Find(nick);
        HostRequest *req = na ? na->GetExt<HostRequest>("hostrequest") : NULL;
        if (req) {
            na->Shrink<HostRequest>("hostrequest");

            if (Config->GetModule(this->owner)->Get<bool>("memouser") && memoserv) {
                Anope::string message;
                if (!reason.empty()) {
                    message = Anope::printf(
                        _("[auto memo] Your requested vHost has been rejected. Reason: %s"),
                        reason.c_str());
                } else {
                    message = _("[auto memo] Your requested vHost has been rejected.");
                }

                memoserv->Send(source.service->nick, nick,
                               Language::Translate(source.GetAccount(), message.c_str()), true);
            }

            source.Reply(_("vHost for %s has been rejected."), nick.c_str());
            Log(LOG_COMMAND, source,
                this) << "to reject vhost for " << nick << " (" << (!reason.empty() ? reason :
                        "no reason") << ")";
        } else {
            source.Reply(_("No request for nick %s found."), nick.c_str());
        }
    }

    bool OnHelp(CommandSource &source,
                const Anope::string &subcommand) anope_override {
        this->SendSyntax(source);
        source.Reply(" ");
        source.Reply(_("Reject the requested vHost for the given nick."));
        if (Config->GetModule(this->owner)->Get<bool>("memouser")) {
            source.Reply(
                _("A memo informing the user will also be sent, which includes the reason for the rejection if supplied."));
        }

        return true;
    }
};

class CommandHSWaiting : public Command {
  public:
    CommandHSWaiting(Module *creator) : Command(creator, "hostserv/waiting", 0, 0) {
        this->SetDesc(_("Retrieves the vhost requests"));
    }

    void Execute(CommandSource &source,
                 const std::vector<Anope::string> &params) anope_override {
        unsigned counter = 0;
        unsigned display_counter = 0, listmax = Config->GetModule(this->owner)->Get<unsigned>("listmax");
        ListFormatter list(source.GetAccount());

        list.AddColumn(_("Number")).AddColumn(_("Nick")).AddColumn(_("Vhost")).AddColumn(_("Created"));

        for (nickalias_map::const_iterator it = NickAliasList->begin(), it_end = NickAliasList->end(); it != it_end; ++it) {
            const NickAlias *na = it->second;
            HostRequest *hr = na->GetExt<HostRequest>("hostrequest");
            if (!hr) {
                continue;
            }

            if (!listmax || display_counter < listmax) {
                ++display_counter;

                ListFormatter::ListEntry entry;
                entry["Number"] = stringify(display_counter);
                entry["Nick"] = it->first;
                if (!hr->ident.empty()) {
                    entry["Vhost"] = hr->ident + "@" + hr->host;
                } else {
                    entry["Vhost"] = hr->host;
                }
                entry["Created"] = Anope::strftime(hr->time, NULL, true);
                list.AddEntry(entry);
            }
            ++counter;
        }

        std::vector<Anope::string> replies;
        list.Process(replies);

        for (unsigned i = 0; i < replies.size(); ++i) {
            source.Reply(replies[i]);
        }

        source.Reply(_("Displayed \002%d\002 records (\002%d\002 total)."), display_counter, counter);
    }

    bool OnHelp(CommandSource &source,
                const Anope::string &subcommand) anope_override {
        this->SendSyntax(source);
        source.Reply(" ");
        source.Reply(_("This command retrieves the vhost requests."));

        return true;
    }
};

class HSRequest : public Module {
    CommandHSRequest commandhsrequest;
    CommandHSActivate commandhsactive;
    CommandHSReject commandhsreject;
    CommandHSWaiting commandhswaiting;
    ExtensibleItem<HostRequest> hostrequest;
    Serialize::Type request_type;

  public:
    HSRequest(const Anope::string &modname,
              const Anope::string &creator) : Module(modname, creator, VENDOR),
        commandhsrequest(this), commandhsactive(this),
        commandhsreject(this), commandhswaiting(this), hostrequest(this, "hostrequest"),
        request_type("HostRequest", HostRequest::Unserialize) {
        if (!IRCD || !IRCD->CanSetVHost) {
            throw ModuleException("Your IRCd does not support vhosts");
        }
    }
};

static void req_send_memos(Module *me, CommandSource &source,
                           const Anope::string &vIdent, const Anope::string &vHost) {
    Anope::string host;
    std::list<std::pair<Anope::string, Anope::string> >::iterator it, it_end;

    if (!vIdent.empty()) {
        host = vIdent + "@" + vHost;
    } else {
        host = vHost;
    }

    if (Config->GetModule(me)->Get<bool>("memooper") && memoserv)
        for (unsigned i = 0; i < Oper::opers.size(); ++i) {
            Oper *o = Oper::opers[i];

            const NickAlias *na = NickAlias::Find(o->name);
            if (!na) {
                continue;
            }

            Anope::string message = Anope::printf(
                                        _("[auto memo] vHost \002%s\002 has been requested by %s."), host.c_str(),
                                        source.GetNick().c_str());

            memoserv->Send(source.service->nick, na->nick, message, true);
        }
}

MODULE_INIT(HSRequest)
