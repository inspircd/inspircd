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

class CommandHSDel : public Command {
  public:
    CommandHSDel(Module *creator) : Command(creator, "hostserv/del", 1, 1) {
        this->SetDesc(_("Delete the vhost of another user"));
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
        if (na) {
            Log(LOG_ADMIN, source, this) << "for user " << na->nick;
            FOREACH_MOD(OnDeleteVhost, (na));
            na->RemoveVhost();
            source.Reply(_("Vhost for \002%s\002 removed."), nick.c_str());
        } else {
            source.Reply(NICK_X_NOT_REGISTERED, nick.c_str());
        }
    }

    bool OnHelp(CommandSource &source,
                const Anope::string &subcommand) anope_override {
        this->SendSyntax(source);
        source.Reply(" ");
        source.Reply(_("Deletes the vhost assigned to the given nick from the\n"
                       "database."));
        return true;
    }
};

class CommandHSDelAll : public Command {
  public:
    CommandHSDelAll(Module *creator) : Command(creator, "hostserv/delall", 1, 1) {
        this->SetDesc(_("Deletes the vhost for all nicks in a group"));
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
        if (na) {
            FOREACH_MOD(OnDeleteVhost, (na));
            const NickCore *nc = na->nc;
            for (unsigned i = 0; i < nc->aliases->size(); ++i) {
                na = nc->aliases->at(i);
                na->RemoveVhost();
            }
            Log(LOG_ADMIN, source, this) << "for all nicks in group " << nc->display;
            source.Reply(_("vhosts for group \002%s\002 have been removed."),
                         nc->display.c_str());
        } else {
            source.Reply(NICK_X_NOT_REGISTERED, nick.c_str());
        }
    }

    bool OnHelp(CommandSource &source,
                const Anope::string &subcommand) anope_override {
        this->SendSyntax(source);
        source.Reply(" ");
        source.Reply(_("Deletes the vhost for all nicks in the same group as\n"
                       "that of the given nick."));
        return true;
    }
};

class HSDel : public Module {
    CommandHSDel commandhsdel;
    CommandHSDelAll commandhsdelall;

  public:
    HSDel(const Anope::string &modname,
          const Anope::string &creator) : Module(modname, creator, VENDOR),
        commandhsdel(this), commandhsdelall(this) {
        if (!IRCD || !IRCD->CanSetVHost) {
            throw ModuleException("Your IRCd does not support vhosts");
        }
    }
};

MODULE_INIT(HSDel)
