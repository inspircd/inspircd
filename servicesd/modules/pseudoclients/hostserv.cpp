/* HostServ core functions
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

class HostServCore : public Module {
    Reference<BotInfo> HostServ;
  public:
    HostServCore(const Anope::string &modname,
                 const Anope::string &creator) : Module(modname, creator,
                             PSEUDOCLIENT | VENDOR) {
        if (!IRCD || !IRCD->CanSetVHost) {
            throw ModuleException("Your IRCd does not support vhosts");
        }
    }

    void OnReload(Configuration::Conf *conf) anope_override {
        const Anope::string &hsnick = conf->GetModule(this)->Get<const Anope::string>("client");

        if (hsnick.empty()) {
            throw ConfigException(Module::name + ": <client> must be defined");
        }

        BotInfo *bi = BotInfo::Find(hsnick, true);
        if (!bi) {
            throw ConfigException(Module::name + ": no bot named " + hsnick);
        }

        HostServ = bi;
    }

    void OnUserLogin(User *u) anope_override {
        if (!IRCD->CanSetVHost) {
            return;
        }

        const NickAlias *na = NickAlias::Find(u->nick);
        if (!na || na->nc != u->Account() || !na->HasVhost()) {
            na = NickAlias::Find(u->Account()->display);
        }
        if (!na || !na->HasVhost()) {
            return;
        }

        if (u->vhost.empty() || !u->vhost.equals_cs(na->GetVhostHost()) || (!na->GetVhostIdent().empty() && !u->GetVIdent().equals_cs(na->GetVhostIdent()))) {
            IRCD->SendVhost(u, na->GetVhostIdent(), na->GetVhostHost());

            u->vhost = na->GetVhostHost();
            u->UpdateHost();

            if (IRCD->CanSetVIdent && !na->GetVhostIdent().empty()) {
                u->SetVIdent(na->GetVhostIdent());
            }

            if (HostServ) {
                if (!na->GetVhostIdent().empty()) {
                    u->SendMessage(HostServ,
                                   _("Your vhost of \002%s\002@\002%s\002 is now activated."),
                                   na->GetVhostIdent().c_str(), na->GetVhostHost().c_str());
                } else {
                    u->SendMessage(HostServ, _("Your vhost of \002%s\002 is now activated."),
                                   na->GetVhostHost().c_str());
                }
            }
        }
    }

    void OnNickDrop(CommandSource &source, NickAlias *na) anope_override {
        if (na->HasVhost()) {
            FOREACH_MOD(OnDeleteVhost, (na));
            na->RemoveVhost();
        }
    }

    void OnNickUpdate(User *u) anope_override {
        this->OnUserLogin(u);
    }

    EventReturn OnPreHelp(CommandSource &source,
                          const std::vector<Anope::string> &params) anope_override {
        if (!params.empty() || source.c || source.service != *HostServ) {
            return EVENT_CONTINUE;
        }
        source.Reply(_("%s commands:"), HostServ->nick.c_str());
        return EVENT_CONTINUE;
    }

    void OnSetVhost(NickAlias *na) anope_override {
        if (Config->GetModule(this)->Get<bool>("activate_on_set")) {
            User *u = User::Find(na->nick);

            if (u && u->Account() == na->nc) {
                IRCD->SendVhost(u, na->GetVhostIdent(), na->GetVhostHost());

                u->vhost = na->GetVhostHost();
                u->UpdateHost();

                if (IRCD->CanSetVIdent && !na->GetVhostIdent().empty()) {
                    u->SetVIdent(na->GetVhostIdent());
                }

                if (HostServ) {
                    if (!na->GetVhostIdent().empty()) {
                        u->SendMessage(HostServ,
                                       _("Your vhost of \002%s\002@\002%s\002 is now activated."),
                                       na->GetVhostIdent().c_str(), na->GetVhostHost().c_str());
                    } else {
                        u->SendMessage(HostServ, _("Your vhost of \002%s\002 is now activated."),
                                       na->GetVhostHost().c_str());
                    }
                }
            }
        }
    }

    void OnDeleteVhost(NickAlias *na) anope_override {
        if (Config->GetModule(this)->Get<bool>("activate_on_set")) {
            User *u = User::Find(na->nick);

            if (u && u->Account() == na->nc) {
                IRCD->SendVhostDel(u);
            }
        }
    }
};

MODULE_INIT(HostServCore)
