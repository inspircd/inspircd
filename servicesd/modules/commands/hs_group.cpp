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

class CommandHSGroup : public Command {
    bool setting;

  public:
    void Sync(const NickAlias *na) {
        if (setting) {
            return;
        }

        if (!na || !na->HasVhost()) {
            return;
        }

        setting = true;
        for (unsigned i = 0; i < na->nc->aliases->size(); ++i) {
            NickAlias *nick = na->nc->aliases->at(i);
            if (nick) {
                nick->SetVhost(na->GetVhostIdent(), na->GetVhostHost(), na->GetVhostCreator());
                FOREACH_MOD(OnSetVhost, (nick));
            }
        }
        setting = false;
    }

    CommandHSGroup(Module *creator) : Command(creator, "hostserv/group", 0, 0),
        setting(false) {
        this->SetDesc(_("Syncs the vhost for all nicks in a group"));
    }

    void Execute(CommandSource &source,
                 const std::vector<Anope::string> &params) anope_override {
        if (Anope::ReadOnly) {
            source.Reply(READ_ONLY_MODE);
            return;
        }

        NickAlias *na = NickAlias::Find(source.GetNick());
        if (na && source.GetAccount() == na->nc && na->HasVhost()) {
            this->Sync(na);
            if (!na->GetVhostIdent().empty()) {
                source.Reply(
                    _("All vhosts in the group \002%s\002 have been set to \002%s\002@\002%s\002."),
                    source.nc->display.c_str(), na->GetVhostIdent().c_str(),
                    na->GetVhostHost().c_str());
            } else {
                source.Reply(
                    _("All vhosts in the group \002%s\002 have been set to \002%s\002."),
                    source.nc->display.c_str(), na->GetVhostHost().c_str());
            }
        } else {
            source.Reply(HOST_NOT_ASSIGNED);
        }

        return;
    }

    bool OnHelp(CommandSource &source,
                const Anope::string &subcommand) anope_override {
        this->SendSyntax(source);
        source.Reply(" ");
        source.Reply(_("This command allows users to set the vhost of their\n"
                       "CURRENT nick to be the vhost for all nicks in the same\n"
                       "group."));
        return true;
    }
};

class HSGroup : public Module {
    CommandHSGroup commandhsgroup;
    bool syncongroup;
    bool synconset;

  public:
    HSGroup(const Anope::string &modname,
            const Anope::string &creator) : Module(modname, creator, VENDOR),
        commandhsgroup(this) {
        if (!IRCD || !IRCD->CanSetVHost) {
            throw ModuleException("Your IRCd does not support vhosts");
        }
    }

    void OnSetVhost(NickAlias *na) anope_override {
        if (!synconset) {
            return;
        }

        commandhsgroup.Sync(na);
    }

    void OnNickGroup(User *u, NickAlias *na) anope_override {
        if (!syncongroup) {
            return;
        }

        commandhsgroup.Sync(na);
    }

    void OnReload(Configuration::Conf *conf) anope_override {
        Configuration::Block *block = conf->GetModule(this);
        syncongroup = block->Get<bool>("syncongroup");
        synconset = block->Get<bool>("synconset");
    }
};

MODULE_INIT(HSGroup)
