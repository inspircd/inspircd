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

class CommandOSLogin : public Command {
  public:
    CommandOSLogin(Module *creator) : Command(creator, "operserv/login", 1, 1) {
        this->SetSyntax(_("\037password\037"));
        this->RequireUser(true);
    }

    void Execute(CommandSource &source,
                 const std::vector<Anope::string> &params) anope_override {
        const Anope::string &password = params[0];

        User *u = source.GetUser();
        Oper *o = source.nc->o;
        if (o == NULL) {
            source.Reply(_("No oper block for your nick."));
        } else if (o->password.empty()) {
            source.Reply(_("Your oper block doesn't require logging in."));
        } else if (u->HasExt("os_login")) {
            source.Reply(_("You are already identified."));
        } else if (o->password != password) {
            source.Reply(PASSWORD_INCORRECT);
            u->BadPassword();
        } else {
            Log(LOG_ADMIN, source, this) << "and successfully identified to " <<
                                         source.service->nick;
            u->Extend<bool>("os_login");
            source.Reply(_("Password accepted."));
        }
    }

    bool OnHelp(CommandSource &source,
                const Anope::string &subcommand) anope_override {
        this->SendSyntax(source);
        source.Reply(" ");
        source.Reply(_("Logs you in to %s so you gain Services Operator privileges.\n"
                       "This command may be unnecessary if your oper block is\n"
                       "configured without a password."), source.service->nick.c_str());
        return true;
    }

    const Anope::string GetDesc(CommandSource &source) const anope_override {
        return Anope::printf(Language::Translate(source.GetAccount(), _("Login to %s")),
                             source.service->nick.c_str());
    }
};

class CommandOSLogout : public Command {
  public:
    CommandOSLogout(Module *creator) : Command(creator, "operserv/logout", 0, 0) {
        this->RequireUser(true);
    }

    void Execute(CommandSource &source,
                 const std::vector<Anope::string> &params) anope_override {
        User *u = source.GetUser();
        Oper *o = source.nc->o;
        if (o == NULL) {
            source.Reply(_("No oper block for your nick."));
        } else if (o->password.empty()) {
            source.Reply(_("Your oper block doesn't require logging in."));
        } else if (!u->HasExt("os_login")) {
            source.Reply(_("You are not identified."));
        } else {
            Log(LOG_ADMIN, source, this);
            u->Shrink<bool>("os_login");
            source.Reply(_("You have been logged out."));
        }
    }

    bool OnHelp(CommandSource &source,
                const Anope::string &subcommand) anope_override {
        this->SendSyntax(source);
        source.Reply(" ");
        source.Reply(_("Logs you out from %s so you lose Services Operator privileges.\n"
                       "This command is only useful if your oper block is configured\n"
                       "with a password."), source.service->nick.c_str());
        return true;
    }

    const Anope::string GetDesc(CommandSource &source) const anope_override {
        return Anope::printf(Language::Translate(source.GetAccount(),
                             _("Logout from %s")), source.service->nick.c_str());
    }
};

class OSLogin : public Module {
    CommandOSLogin commandoslogin;
    CommandOSLogout commandoslogout;
    ExtensibleItem<bool> os_login;

  public:
    OSLogin(const Anope::string &modname,
            const Anope::string &creator) : Module(modname, creator, VENDOR),
        commandoslogin(this), commandoslogout(this), os_login(this, "os_login") {

    }

    EventReturn IsServicesOper(User *u) anope_override {
        if (!u->Account()->o->password.empty()) {
            if (os_login.HasExt(u)) {
                return EVENT_ALLOW;
            }
            return EVENT_STOP;
        }

        return EVENT_CONTINUE;
    }
};

MODULE_INIT(OSLogin)
