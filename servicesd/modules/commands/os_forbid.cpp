/* OperServ core functions
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
#include "modules/os_forbid.h"

static ServiceReference<NickServService> nickserv("NickServService",
        "NickServ");

struct ForbidDataImpl : ForbidData, Serializable {
    ForbidDataImpl() : Serializable("ForbidData") { }
    void Serialize(Serialize::Data &data) const anope_override;
    static Serializable* Unserialize(Serializable *obj, Serialize::Data &data);
};

void ForbidDataImpl::Serialize(Serialize::Data &data) const {
    data["mask"] << this->mask;
    data["creator"] << this->creator;
    data["reason"] << this->reason;
    data["created"] << this->created;
    data["expires"] << this->expires;
    data["type"] << this->type;
}

Serializable* ForbidDataImpl::Unserialize(Serializable *obj,
        Serialize::Data &data) {
    if (!forbid_service) {
        return NULL;
    }

    ForbidDataImpl *fb;
    if (obj) {
        fb = anope_dynamic_static_cast<ForbidDataImpl *>(obj);
    } else {
        fb = new ForbidDataImpl();
    }

    data["mask"] >> fb->mask;
    data["creator"] >> fb->creator;
    data["reason"] >> fb->reason;
    data["created"] >> fb->created;
    data["expires"] >> fb->expires;
    unsigned int t;
    data["type"] >> t;
    fb->type = static_cast<ForbidType>(t);

    if (t > FT_SIZE - 1) {
        return NULL;
    }

    if (!obj) {
        forbid_service->AddForbid(fb);
    }
    return fb;
}

class MyForbidService : public ForbidService {
    Serialize::Checker<std::vector<ForbidData *>[FT_SIZE - 1]> forbid_data;

    inline std::vector<ForbidData *>& forbids(unsigned t) {
        return (*this->forbid_data)[t - 1];
    }

  public:
    MyForbidService(Module *m) : ForbidService(m), forbid_data("ForbidData") { }

    ~MyForbidService() {
        std::vector<ForbidData *> f = GetForbids();
        for (unsigned i = 0; i < f.size(); ++i) {
            delete f[i];
        }
    }

    void AddForbid(ForbidData *d) anope_override {
        this->forbids(d->type).push_back(d);
    }

    void RemoveForbid(ForbidData *d) anope_override {
        std::vector<ForbidData *>::iterator it = std::find(this->forbids(d->type).begin(), this->forbids(d->type).end(), d);
        if (it != this->forbids(d->type).end()) {
            this->forbids(d->type).erase(it);
        }
        delete d;
    }

    ForbidData *CreateForbid() anope_override {
        return new ForbidDataImpl();
    }

    ForbidData *FindForbid(const Anope::string &mask,
                           ForbidType ftype) anope_override {
        for (unsigned i = this->forbids(ftype).size(); i > 0; --i) {
            ForbidData *d = this->forbids(ftype)[i - 1];

            if (Anope::Match(mask, d->mask, false, true)) {
                return d;
            }
        }
        return NULL;
    }

    ForbidData *FindForbidExact(const Anope::string &mask,
                                ForbidType ftype) anope_override {
        for (unsigned i = this->forbids(ftype).size(); i > 0; --i) {
            ForbidData *d = this->forbids(ftype)[i - 1];

            if (d->mask.equals_ci(mask)) {
                return d;
            }
        }
        return NULL;
    }

    std::vector<ForbidData *> GetForbids() anope_override {
        std::vector<ForbidData *> f;
        for (unsigned j = FT_NICK; j < FT_SIZE; ++j)
            for (unsigned i = this->forbids(j).size(); i > 0; --i) {
                ForbidData *d = this->forbids(j).at(i - 1);

                if (d->expires && !Anope::NoExpire && Anope::CurTime >= d->expires) {
                    Anope::string ftype = "none";
                    if (d->type == FT_NICK) {
                        ftype = "nick";
                    } else if (d->type == FT_CHAN) {
                        ftype = "chan";
                    } else if (d->type == FT_EMAIL) {
                        ftype = "email";
                    }

                    Log(LOG_NORMAL, "expire/forbid",
                        Config->GetClient("OperServ")) << "Expiring forbid for " << d->mask << " type "
                                                       << ftype;
                    this->forbids(j).erase(this->forbids(j).begin() + i - 1);
                    delete d;
                } else {
                    f.push_back(d);
                }
            }

        return f;
    }
};

class CommandOSForbid : public Command {
    ServiceReference<ForbidService> fs;
  public:
    CommandOSForbid(Module *creator) : Command(creator, "operserv/forbid", 1, 5),
        fs("ForbidService", "forbid") {
        this->SetDesc(_("Forbid usage of nicknames, channels, and emails"));
        this->SetSyntax(
            _("ADD {NICK|CHAN|EMAIL|REGISTER} [+\037expiry\037] \037entry\037 \037reason\037"));
        this->SetSyntax(_("DEL {NICK|CHAN|EMAIL|REGISTER} \037entry\037"));
        this->SetSyntax("LIST [NICK|CHAN|EMAIL|REGISTER]");
    }

    void Execute(CommandSource &source,
                 const std::vector<Anope::string> &params) anope_override {
        if (!this->fs) {
            return;
        }

        const Anope::string &command = params[0];
        const Anope::string &subcommand = params.size() > 1 ? params[1] : "";

        ForbidType ftype = FT_SIZE;
        if (subcommand.equals_ci("NICK")) {
            ftype = FT_NICK;
        } else if (subcommand.equals_ci("CHAN")) {
            ftype = FT_CHAN;
        } else if (subcommand.equals_ci("EMAIL")) {
            ftype = FT_EMAIL;
        } else if (subcommand.equals_ci("REGISTER")) {
            ftype = FT_REGISTER;
        }

        if (command.equals_ci("ADD") && params.size() > 3 && ftype != FT_SIZE) {
            const Anope::string &expiry = params[2][0] == '+' ? params[2] : "";
            const Anope::string &entry = !expiry.empty() ? params[3] : params[2];
            Anope::string reason;
            if (expiry.empty()) {
                reason = params[3] + " ";
            }
            if (params.size() > 4) {
                reason += params[4];
            }
            reason.trim();

            if (entry.replace_all_cs("?*", "").empty()) {
                source.Reply(_("The mask must contain at least one non wildcard character."));
                return;
            }

            time_t expiryt = 0;

            if (!expiry.empty()) {
                expiryt = Anope::DoTime(expiry);
                if (expiryt < 0) {
                    source.Reply(BAD_EXPIRY_TIME);
                    return;
                } else if (expiryt) {
                    expiryt += Anope::CurTime;
                }
            }

            NickAlias *target = NickAlias::Find(entry);
            if (target != NULL
                    && Config->GetModule("nickserv")->Get<bool>("secureadmins", "yes")
                    && target->nc->IsServicesOper()) {
                source.Reply(ACCESS_DENIED);
                return;
            }

            ForbidData *d = this->fs->FindForbidExact(entry, ftype);
            bool created = false;
            if (d == NULL) {
                d = new ForbidDataImpl();
                created = true;
            }

            d->mask = entry;
            d->creator = source.GetNick();
            d->reason = reason;
            d->created = Anope::CurTime;
            d->expires = expiryt;
            d->type = ftype;
            if (created) {
                this->fs->AddForbid(d);
            }

            if (Anope::ReadOnly) {
                source.Reply(READ_ONLY_MODE);
            }

            Log(LOG_ADMIN, source, this) << "to add a forbid on " << entry << " of type " <<
                                         subcommand;
            source.Reply(_("Added a forbid on %s of type %s to expire on %s."),
                         entry.c_str(), subcommand.lower().c_str(),
                         d->expires ? Anope::strftime(d->expires,
                                 source.GetAccount()).c_str() : "never");

            /* apply forbid */
            switch (ftype) {
            case FT_NICK: {
                int na_matches = 0;

                for (user_map::const_iterator it = UserListByNick.begin();
                        it != UserListByNick.end(); ++it) {
                    module->OnUserNickChange(it->second, "");
                }

                for (nickalias_map::const_iterator it = NickAliasList->begin(),
                        it_end = NickAliasList->end(); it != it_end;) {
                    NickAlias *na = it->second;
                    ++it;

                    d = this->fs->FindForbid(na->nick, FT_NICK);
                    if (d == NULL) {
                        continue;
                    }

                    ++na_matches;

                    delete na;
                }

                source.Reply(_("\002%d\002 nickname(s) dropped."), na_matches);
                break;
            }
            case FT_CHAN: {
                int chan_matches = 0, ci_matches = 0;

                for (channel_map::const_iterator it = ChannelList.begin(),
                        it_end = ChannelList.end(); it != it_end;) {
                    Channel *c = it->second;
                    ++it;

                    d = this->fs->FindForbid(c->name, FT_CHAN);
                    if (d == NULL) {
                        continue;
                    }

                    ServiceReference<ChanServService> chanserv("ChanServService", "ChanServ");
                    BotInfo *OperServ = Config->GetClient("OperServ");
                    if (IRCD->CanSQLineChannel && OperServ) {
                        time_t inhabit = Config->GetModule("chanserv")->Get<time_t>("inhabit", "15s");
                        XLine x(c->name, OperServ->nick, Anope::CurTime + inhabit, d->reason);
                        IRCD->SendSQLine(NULL, &x);
                    } else if (chanserv) {
                        chanserv->Hold(c);
                    }

                    ++chan_matches;

                    for (Channel::ChanUserList::const_iterator cit = c->users.begin(),
                            cit_end = c->users.end(); cit != cit_end;) {
                        User *u = cit->first;
                        ++cit;

                        if (u->server == Me || u->HasMode("OPER")) {
                            continue;
                        }

                        reason = Anope::printf(Language::Translate(u,
                                               _("This channel has been forbidden: %s")), d->reason.c_str());

                        c->Kick(source.service, u, "%s", reason.c_str());
                    }
                }

                for (registered_channel_map::const_iterator it = RegisteredChannelList->begin();
                        it != RegisteredChannelList->end();) {
                    ChannelInfo *ci = it->second;
                    ++it;

                    d = this->fs->FindForbid(ci->name, FT_CHAN);
                    if (d == NULL) {
                        continue;
                    }

                    ++ci_matches;

                    delete ci;
                }

                source.Reply(
                    _("\002%d\002 channel(s) cleared, and \002%d\002 channel(s) dropped."),
                    chan_matches, ci_matches);

                break;
            }
            default:
                break;
            }

        } else if (command.equals_ci("DEL") && params.size() > 2 && ftype != FT_SIZE) {
            const Anope::string &entry = params[2];

            ForbidData *d = this->fs->FindForbidExact(entry, ftype);
            if (d != NULL) {
                if (Anope::ReadOnly) {
                    source.Reply(READ_ONLY_MODE);
                }

                Log(LOG_ADMIN, source, this) << "to remove forbid on " << d->mask << " of type "
                                             << subcommand;
                source.Reply(_("%s deleted from the %s forbid list."), d->mask.c_str(),
                             subcommand.c_str());
                this->fs->RemoveForbid(d);
            } else {
                source.Reply(_("Forbid on %s was not found."), entry.c_str());
            }
        } else if (command.equals_ci("LIST")) {
            const std::vector<ForbidData *> &forbids = this->fs->GetForbids();
            if (forbids.empty()) {
                source.Reply(_("Forbid list is empty."));
            } else {
                ListFormatter list(source.GetAccount());
                list.AddColumn(_("Mask")).AddColumn(_("Type")).AddColumn(
                    _("Creator")).AddColumn(_("Expires")).AddColumn(_("Reason"));

                unsigned shown = 0;
                for (unsigned i = 0; i < forbids.size(); ++i) {
                    ForbidData *d = forbids[i];

                    if (ftype != FT_SIZE && ftype != d->type) {
                        continue;
                    }

                    Anope::string stype;
                    if (d->type == FT_NICK) {
                        stype = "NICK";
                    } else if (d->type == FT_CHAN) {
                        stype = "CHAN";
                    } else if (d->type == FT_EMAIL) {
                        stype = "EMAIL";
                    } else if (d->type == FT_REGISTER) {
                        stype = "REGISTER";
                    } else {
                        continue;
                    }

                    ListFormatter::ListEntry entry;
                    entry["Mask"] = d->mask;
                    entry["Type"] = stype;
                    entry["Creator"] = d->creator;
                    entry["Expires"] = d->expires ? Anope::strftime(d->expires, NULL,
                                       true).c_str() : Language::Translate(source.GetAccount(), _("Never"));
                    entry["Reason"] = d->reason;
                    list.AddEntry(entry);
                    ++shown;
                }

                if (!shown) {
                    source.Reply(_("There are no forbids of type %s."), subcommand.upper().c_str());
                } else {
                    source.Reply(_("Forbid list:"));

                    std::vector<Anope::string> replies;
                    list.Process(replies);

                    for (unsigned i = 0; i < replies.size(); ++i) {
                        source.Reply(replies[i]);
                    }

                    if (shown >= forbids.size()) {
                        source.Reply(_("End of forbid list."));
                    } else {
                        source.Reply(_("End of forbid list - %d/%d entries shown."), shown,
                                     forbids.size());
                    }
                }
            }
        } else {
            this->OnSyntaxError(source, command);
        }

        return;
    }

    bool OnHelp(CommandSource &source,
                const Anope::string &subcommand) anope_override {
        this->SendSyntax(source);
        source.Reply(" ");
        source.Reply(_("Forbid allows you to forbid usage of certain nicknames, channels,\n"
                       "and email addresses. Wildcards are accepted for all entries."));

        const Anope::string &regexengine = Config->GetBlock("options")->Get<const Anope::string>("regexengine");
        if (!regexengine.empty()) {
            source.Reply(" ");
            source.Reply(_("Regex matches are also supported using the %s engine.\n"
                           "Enclose your pattern in // if this is desired."), regexengine.c_str());
        }

        return true;
    }
};

class OSForbid : public Module {
    MyForbidService forbidService;
    Serialize::Type forbiddata_type;
    CommandOSForbid commandosforbid;

  public:
    OSForbid(const Anope::string &modname,
             const Anope::string &creator) : Module(modname, creator, VENDOR),
        forbidService(this), forbiddata_type("ForbidData", ForbidDataImpl::Unserialize),
        commandosforbid(this) {

    }

    void OnUserConnect(User *u, bool &exempt) anope_override {
        if (u->Quitting() || exempt) {
            return;
        }

        this->OnUserNickChange(u, "");
    }

    void OnUserNickChange(User *u, const Anope::string &) anope_override {
        if (u->HasMode("OPER")) {
            return;
        }

        ForbidData *d = this->forbidService.FindForbid(u->nick, FT_NICK);
        if (d != NULL) {
            BotInfo *bi = Config->GetClient("NickServ");
            if (!bi) {
                bi = Config->GetClient("OperServ");
            }
            if (bi) {
                u->SendMessage(bi, _("This nickname has been forbidden: %s"),
                               d->reason.c_str());
            }
            if (nickserv) {
                nickserv->Collide(u, NULL);
            }
        }
    }

    EventReturn OnCheckKick(User *u, Channel *c, Anope::string &mask,
                            Anope::string &reason) anope_override {
        BotInfo *OperServ = Config->GetClient("OperServ");
        if (u->HasMode("OPER") || !OperServ) {
            return EVENT_CONTINUE;
        }

        ForbidData *d = this->forbidService.FindForbid(c->name, FT_CHAN);
        if (d != NULL) {
            ServiceReference<ChanServService> chanserv("ChanServService", "ChanServ");
            if (IRCD->CanSQLineChannel) {
                time_t inhabit = Config->GetModule("chanserv")->Get<time_t>("inhabit", "15s");
                XLine x(c->name, OperServ->nick, Anope::CurTime + inhabit, d->reason);
                IRCD->SendSQLine(NULL, &x);
            } else if (chanserv) {
                chanserv->Hold(c);
            }

            reason = Anope::printf(Language::Translate(u,
                                   _("This channel has been forbidden: %s")), d->reason.c_str());

            return EVENT_STOP;
        }

        return EVENT_CONTINUE;
    }

    EventReturn OnPreCommand(CommandSource &source, Command *command,
                             std::vector<Anope::string> &params) anope_override {
        if (command->name == "nickserv/info" && params.size() > 0) {
            ForbidData *d = this->forbidService.FindForbid(params[0], FT_NICK);
            if (d != NULL) {
                if (source.IsOper()) {
                    source.Reply(_("Nick \002%s\002 is forbidden by %s: %s"), params[0].c_str(),
                                 d->creator.c_str(), d->reason.c_str());
                } else {
                    source.Reply(_("Nick \002%s\002 is forbidden."), params[0].c_str());
                }
                return EVENT_STOP;
            }
        } else if (command->name == "chanserv/info" && params.size() > 0) {
            ForbidData *d = this->forbidService.FindForbid(params[0], FT_CHAN);
            if (d != NULL) {
                if (source.IsOper()) {
                    source.Reply(_("Channel \002%s\002 is forbidden by %s: %s"), params[0].c_str(),
                                 d->creator.c_str(), d->reason.c_str());
                } else {
                    source.Reply(_("Channel \002%s\002 is forbidden."), params[0].c_str());
                }
                return EVENT_STOP;
            }
        } else if (source.IsOper()) {
            return EVENT_CONTINUE;
        } else if (command->name == "nickserv/register" && params.size() > 1) {
            ForbidData *d = this->forbidService.FindForbid(source.GetNick(), FT_REGISTER);
            if (d != NULL) {
                source.Reply(NICK_CANNOT_BE_REGISTERED, source.GetNick().c_str());
                return EVENT_STOP;
            }

            d = this->forbidService.FindForbid(params[1], FT_EMAIL);
            if (d != NULL) {
                source.Reply(_("Your email address is not allowed, choose a different one."));
                return EVENT_STOP;
            }
        } else if (command->name == "nickserv/set/email" && params.size() > 0) {
            ForbidData *d = this->forbidService.FindForbid(params[0], FT_EMAIL);
            if (d != NULL) {
                source.Reply(_("Your email address is not allowed, choose a different one."));
                return EVENT_STOP;
            }
        } else if (command->name == "chanserv/register" && !params.empty()) {
            ForbidData *d = this->forbidService.FindForbid(params[0], FT_REGISTER);
            if (d != NULL) {
                source.Reply(CHAN_X_INVALID, params[0].c_str());
                return EVENT_STOP;
            }
        }

        return EVENT_CONTINUE;
    }
};

MODULE_INIT(OSForbid)
