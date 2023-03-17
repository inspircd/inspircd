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

class CommandHSOff : public Command {
  public:
    CommandHSOff(Module *creator) : Command(creator, "hostserv/off", 0, 0) {
        this->SetDesc(_("Deactivates your assigned vhost"));
        this->RequireUser(true);
    }

    void Execute(CommandSource &source,
                 const std::vector<Anope::string> &params) anope_override {
        User *u = source.GetUser();

        const NickAlias *na = NickAlias::Find(u->nick);
        if (!na || na->nc != u->Account() || !na->HasVhost()) {
            na = NickAlias::Find(u->Account()->display);
        }

        if (!na || !na->HasVhost()) {
            source.Reply(HOST_NOT_ASSIGNED);
        } else {
            u->vhost.clear();
            IRCD->SendVhostDel(u);
            u->UpdateHost();
            Log(LOG_COMMAND, source, this) << "to disable their vhost";
            source.Reply(_("Your vhost was removed and the normal cloaking restored."));
        }

        return;
    }

    bool OnHelp(CommandSource &source,
                const Anope::string &subcommand) anope_override {
        this->SendSyntax(source);
        source.Reply(" ");
        source.Reply(_("Deactivates the vhost currently assigned to the nick in use.\n"
                       "When you use this command any user who performs a /whois\n"
                       "on you will see your real host/IP address."));
        return true;
    }
};

class HSOff : public Module {
    CommandHSOff commandhsoff;

  public:
    HSOff(const Anope::string &modname,
          const Anope::string &creator) : Module(modname, creator, VENDOR),
        commandhsoff(this) {
        if (!IRCD || !IRCD->CanSetVHost) {
            throw ModuleException("Your IRCd does not support vhosts");
        }
    }
};

MODULE_INIT(HSOff)
