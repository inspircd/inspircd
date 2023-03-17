/* NickServ core functions
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

class CommandNSSet : public Command {
  public:
    CommandNSSet(Module *creator) : Command(creator, "nickserv/set", 1, 3) {
        this->SetDesc(_("Set options, including kill protection"));
        this->SetSyntax(_("\037option\037 \037parameters\037"));
    }

    void Execute(CommandSource &source,
                 const std::vector<Anope::string> &params) anope_override {
        this->OnSyntaxError(source, "");
        return;
    }

    bool OnHelp(CommandSource &source,
                const Anope::string &subcommand) anope_override {
        this->SendSyntax(source);
        source.Reply(" ");
        source.Reply(_("Sets various nickname options. \037option\037 can be one of:"));

        Anope::string this_name = source.command;
        bool hide_privileged_commands = Config->GetBlock("options")->Get<bool>("hideprivilegedcommands"),
        hide_registered_commands = Config->GetBlock("options")->Get<bool>("hideregisteredcommands");
        for (CommandInfo::map::const_iterator it = source.service->commands.begin(), it_end = source.service->commands.end(); it != it_end; ++it) {
            const Anope::string &c_name = it->first;
            const CommandInfo &info = it->second;

            if (c_name.find_ci(this_name + " ") == 0) {
                if (info.hide) {
                    continue;
                }

                ServiceReference<Command> c("Command", info.name);
                // XXX dup
                if (!c) {
                    continue;
                } else if (hide_registered_commands && !c->AllowUnregistered()
                           && !source.GetAccount()) {
                    continue;
                } else if (hide_privileged_commands && !info.permission.empty()
                           && !source.HasCommand(info.permission)) {
                    continue;
                }

                source.command = c_name;
                c->OnServHelp(source);
            }
        }

        source.Reply(_("Type \002%s%s HELP %s \037option\037\002 for more information\n"
                       "on a specific option."), Config->StrictPrivmsg.c_str(), source.service->nick.c_str(), this_name.c_str());

        return true;
    }
};

class CommandNSSASet : public Command {
  public:
    CommandNSSASet(Module *creator) : Command(creator, "nickserv/saset", 2, 4) {
        this->SetDesc(_("Set SET-options on another nickname"));
        this->SetSyntax(_("\037option\037 \037nickname\037 \037parameters\037"));
    }

    void Execute(CommandSource &source,
                 const std::vector<Anope::string> &params) anope_override {
        this->OnSyntaxError(source, "");
        return;
    }

    bool OnHelp(CommandSource &source,
                const Anope::string &subcommand) anope_override {
        this->SendSyntax(source);
        source.Reply(" ");
        source.Reply(_("Sets various nickname options. \037option\037 can be one of:"));

        Anope::string this_name = source.command;
        for (CommandInfo::map::const_iterator it = source.service->commands.begin(), it_end = source.service->commands.end(); it != it_end; ++it) {
            const Anope::string &c_name = it->first;
            const CommandInfo &info = it->second;

            if (c_name.find_ci(this_name + " ") == 0) {
                ServiceReference<Command> command("Command", info.name);
                if (command) {
                    source.command = c_name;
                    command->OnServHelp(source);
                }
            }
        }

        source.Reply(_("Type \002%s%s HELP %s \037option\037\002 for more information\n"
                       "on a specific option. The options will be set on the given\n"
                       "\037nickname\037."), Config->StrictPrivmsg.c_str(), source.service->nick.c_str(), this_name.c_str());
        return true;
    }
};

class CommandNSSetPassword : public Command {
  public:
    CommandNSSetPassword(Module *creator) : Command(creator,
                "nickserv/set/password", 1) {
        this->SetDesc(_("Set your nickname password"));
        this->SetSyntax(_("\037new-password\037"));
    }

    void Execute(CommandSource &source,
                 const std::vector<Anope::string> &params) anope_override {
        const Anope::string &param = params[0];
        unsigned len = param.length();

        if (Anope::ReadOnly) {
            source.Reply(READ_ONLY_MODE);
            return;
        }

        if (source.GetNick().equals_ci(param) || (Config->GetBlock("options")->Get<bool>("strictpasswords") && len < 5)) {
            source.Reply(MORE_OBSCURE_PASSWORD);
            return;
        }

        unsigned int passlen = Config->GetModule("nickserv")->Get<unsigned>("passlen", "32");
        if (len > passlen) {
            source.Reply(PASSWORD_TOO_LONG, passlen);
            return;
        }

        Log(LOG_COMMAND, source, this) << "to change their password";

        Anope::Encrypt(param, source.nc->pass);
        Anope::string tmp_pass;
        if (Anope::Decrypt(source.nc->pass, tmp_pass) == 1) {
            source.Reply(_("Password for \002%s\002 changed to \002%s\002."),
                         source.nc->display.c_str(), tmp_pass.c_str());
        } else {
            source.Reply(_("Password for \002%s\002 changed."),
                         source.nc->display.c_str());
        }
    }

    bool OnHelp(CommandSource &source, const Anope::string &) anope_override {
        this->SendSyntax(source);
        source.Reply(" ");
        source.Reply(_("Changes the password used to identify you as the nick's\n"
                       "owner."));
        return true;
    }
};

class CommandNSSASetPassword : public Command {
  public:
    CommandNSSASetPassword(Module *creator) : Command(creator,
                "nickserv/saset/password", 2, 2) {
        this->SetDesc(_("Set the nickname password"));
        this->SetSyntax(_("\037nickname\037 \037new-password\037"));
    }

    void Execute(CommandSource &source,
                 const std::vector<Anope::string> &params) anope_override {
        if (Anope::ReadOnly) {
            source.Reply(READ_ONLY_MODE);
            return;
        }

        const NickAlias *setter_na = NickAlias::Find(params[0]);
        if (setter_na == NULL) {
            source.Reply(NICK_X_NOT_REGISTERED, params[0].c_str());
            return;
        }
        NickCore *nc = setter_na->nc;

        size_t len = params[1].length();

        if (Config->GetModule("nickserv")->Get<bool>("secureadmins", "yes") && source.nc != nc && nc->IsServicesOper()) {
            source.Reply(_("You may not change the password of other Services Operators."));
            return;
        }

        if (nc->display.equals_ci(params[1]) || (Config->GetBlock("options")->Get<bool>("strictpasswords") && len < 5)) {
            source.Reply(MORE_OBSCURE_PASSWORD);
            return;
        }

        unsigned int passlen = Config->GetModule("nickserv")->Get<unsigned>("passlen", "32");
        if (len > passlen) {
            source.Reply(PASSWORD_TOO_LONG, passlen);
            return;
        }

        Log(LOG_ADMIN, source, this) << "to change the password of " << nc->display;

        Anope::Encrypt(params[1], nc->pass);
        Anope::string tmp_pass;
        if (Anope::Decrypt(nc->pass, tmp_pass) == 1) {
            source.Reply(_("Password for \002%s\002 changed to \002%s\002."),
                         nc->display.c_str(), tmp_pass.c_str());
        } else {
            source.Reply(_("Password for \002%s\002 changed."), nc->display.c_str());
        }
    }

    bool OnHelp(CommandSource &source, const Anope::string &) anope_override {
        this->SendSyntax(source);
        source.Reply(" ");
        source.Reply(_("Changes the password used to identify as the nick's owner."));
        return true;
    }
};

class CommandNSSetAutoOp : public Command {
  public:
    CommandNSSetAutoOp(Module *creator,
                       const Anope::string &sname = "nickserv/set/autoop",
                       size_t min = 1) : Command(creator, sname, min, min + 1) {
        this->SetDesc(
            _("Sets whether services should set channel status modes on you automatically."));
        this->SetSyntax("{ON | OFF}");
    }

    void Run(CommandSource &source, const Anope::string &user,
             const Anope::string &param) {
        if (Anope::ReadOnly) {
            source.Reply(READ_ONLY_MODE);
            return;
        }

        const NickAlias *na = NickAlias::Find(user);
        if (na == NULL) {
            source.Reply(NICK_X_NOT_REGISTERED, user.c_str());
            return;
        }
        NickCore *nc = na->nc;

        EventReturn MOD_RESULT;
        FOREACH_RESULT(OnSetNickOption, MOD_RESULT, (source, this, nc, param));
        if (MOD_RESULT == EVENT_STOP) {
            return;
        }

        if (param.equals_ci("ON")) {
            Log(nc == source.GetAccount() ? LOG_COMMAND : LOG_ADMIN, source,
                this) << "to enable autoop for " << na->nc->display;
            nc->Extend<bool>("AUTOOP");
            source.Reply(_("Services will from now on set status modes on %s in channels."),
                         nc->display.c_str());
        } else if (param.equals_ci("OFF")) {
            Log(nc == source.GetAccount() ? LOG_COMMAND : LOG_ADMIN, source,
                this) << "to disable autoop for " << na->nc->display;
            nc->Shrink<bool>("AUTOOP");
            source.Reply(_("Services will no longer set status modes on %s in channels."),
                         nc->display.c_str());
        } else {
            this->OnSyntaxError(source, "AUTOOP");
        }
    }

    void Execute(CommandSource &source,
                 const std::vector<Anope::string> &params) anope_override {
        this->Run(source, source.nc->display, params[0]);
    }

    bool OnHelp(CommandSource &source, const Anope::string &) anope_override {
        BotInfo *bi = Config->GetClient("ChanServ");
        this->SendSyntax(source);
        source.Reply(" ");
        source.Reply(_("Sets whether you will be given your channel status modes automatically.\n"
                       "Set to \002ON\002 to allow %s to set status modes on you automatically\n"
                       "when entering channels. Note that depending on channel settings some modes\n"
                       "may not get set automatically."), bi ? bi->nick.c_str() : "ChanServ");
        return true;
    }
};

class CommandNSSASetAutoOp : public CommandNSSetAutoOp {
  public:
    CommandNSSASetAutoOp(Module *creator) : CommandNSSetAutoOp(creator,
                "nickserv/saset/autoop", 2) {
        this->ClearSyntax();
        this->SetSyntax(_("\037nickname\037 {ON | OFF}"));
    }

    void Execute(CommandSource &source,
                 const std::vector<Anope::string> &params) anope_override {
        this->Run(source, params[0], params[1]);
    }

    bool OnHelp(CommandSource &source, const Anope::string &) anope_override {
        BotInfo *bi = Config->GetClient("ChanServ");
        this->SendSyntax(source);
        source.Reply(" ");
        source.Reply(_("Sets whether the given nickname will be given its status modes\n"
                       "in channels automatically. Set to \002ON\002 to allow %s\n"
                       "to set status modes on the given nickname automatically when it\n"
                       "is entering channels. Note that depending on channel settings\n"
                       "some modes may not get set automatically."), bi ? bi->nick.c_str() : "ChanServ");
        return true;
    }
};

class CommandNSSetDisplay : public Command {
  public:
    CommandNSSetDisplay(Module *creator,
                        const Anope::string &sname = "nickserv/set/display",
                        size_t min = 1) : Command(creator, sname, min, min + 1) {
        this->SetDesc(_("Set the display of your group in Services"));
        this->SetSyntax(_("\037new-display\037"));
    }

    void Run(CommandSource &source, const Anope::string &user,
             const Anope::string &param) {
        if (Anope::ReadOnly) {
            source.Reply(READ_ONLY_MODE);
            return;
        }

        NickAlias *user_na = NickAlias::Find(user), *na = NickAlias::Find(param);

        if (Config->GetModule("nickserv")->Get<bool>("nonicknameownership")) {
            source.Reply(
                _("This command may not be used on this network because nickname ownership is disabled."));
            return;
        }
        if (user_na == NULL) {
            source.Reply(NICK_X_NOT_REGISTERED, user.c_str());
            return;
        } else if (!na || *na->nc != *user_na->nc) {
            source.Reply(_("The new display MUST be a nickname of the nickname group %s."),
                         user_na->nc->display.c_str());
            return;
        }

        EventReturn MOD_RESULT;
        FOREACH_RESULT(OnSetNickOption, MOD_RESULT, (source, this, user_na->nc, param));
        if (MOD_RESULT == EVENT_STOP) {
            return;
        }

        Log(user_na->nc == source.GetAccount() ? LOG_COMMAND : LOG_ADMIN, source,
            this) << "to change the display of " << user_na->nc->display << " to " <<
                  na->nick;

        user_na->nc->SetDisplay(na);

        /* Send updated account name */
        for (std::list<User *>::iterator it = user_na->nc->users.begin();
                it != user_na->nc->users.end(); ++it) {
            User *u = *it;
            IRCD->SendLogin(u, user_na);
        }

        source.Reply(NICK_SET_DISPLAY_CHANGED, user_na->nc->display.c_str());
    }

    void Execute(CommandSource &source,
                 const std::vector<Anope::string> &params) anope_override {
        this->Run(source, source.nc->display, params[0]);
    }

    bool OnHelp(CommandSource &source, const Anope::string &) anope_override {
        this->SendSyntax(source);
        source.Reply(" ");
        source.Reply(_("Changes the display used to refer to your nickname group in\n"
                       "Services. The new display MUST be a nick of your group."));
        return true;
    }
};

class CommandNSSASetDisplay : public CommandNSSetDisplay {
  public:
    CommandNSSASetDisplay(Module *creator) : CommandNSSetDisplay(creator,
                "nickserv/saset/display", 2) {
        this->ClearSyntax();
        this->SetSyntax(_("\037nickname\037 \037new-display\037"));
    }

    void Execute(CommandSource &source,
                 const std::vector<Anope::string> &params) anope_override {
        this->Run(source, params[0], params[1]);
    }

    bool OnHelp(CommandSource &source, const Anope::string &) anope_override {
        this->SendSyntax(source);
        source.Reply(" ");
        source.Reply(_("Changes the display used to refer to the nickname group in\n"
                       "Services. The new display MUST be a nick of the group."));
        return true;
    }
};

class CommandNSSetEmail : public Command {
    static bool SendConfirmMail(User *u, NickCore *nc, BotInfo *bi,
                                const Anope::string &new_email) {
        Anope::string code = Anope::Random(9);

        std::pair<Anope::string, Anope::string> *n =
            nc->Extend<std::pair<Anope::string, Anope::string> >("ns_set_email");
        n->first = new_email;
        n->second = code;

        Anope::string subject =
            Config->GetBlock("mail")->Get<const Anope::string>("emailchange_subject"),
            message = Config->GetBlock("mail")->Get<const Anope::string>("emailchange_message");

        subject = subject.replace_all_cs("%e", nc->email);
        subject = subject.replace_all_cs("%E", new_email);
        subject = subject.replace_all_cs("%n", nc->display);
        subject = subject.replace_all_cs("%N",
                                         Config->GetBlock("networkinfo")->Get<const Anope::string>("networkname"));
        subject = subject.replace_all_cs("%c", code);

        message = message.replace_all_cs("%e", nc->email);
        message = message.replace_all_cs("%E", new_email);
        message = message.replace_all_cs("%n", nc->display);
        message = message.replace_all_cs("%N",
                                         Config->GetBlock("networkinfo")->Get<const Anope::string>("networkname"));
        message = message.replace_all_cs("%c", code);

        Anope::string old = nc->email;
        nc->email = new_email;
        bool b = Mail::Send(u, nc, bi, subject, message);
        nc->email = old;
        return b;
    }

  public:
    CommandNSSetEmail(Module *creator,
                      const Anope::string &cname = "nickserv/set/email",
                      size_t min = 0) : Command(creator, cname, min, min + 1) {
        this->SetDesc(_("Associate an E-mail address with your nickname"));
        this->SetSyntax(_("\037address\037"));
    }

    void Run(CommandSource &source, const Anope::string &user,
             const Anope::string &param) {
        if (Anope::ReadOnly) {
            source.Reply(READ_ONLY_MODE);
            return;
        }

        const NickAlias *na = NickAlias::Find(user);
        if (!na) {
            source.Reply(NICK_X_NOT_REGISTERED, user.c_str());
            return;
        }
        NickCore *nc = na->nc;

        if (nc->HasExt("UNCONFIRMED")) {
            source.Reply(_("You may not change the email of an unconfirmed account."));
            return;
        }

        if (param.empty()
                && Config->GetModule("nickserv")->Get<bool>("forceemail", "yes")) {
            source.Reply(_("You cannot unset the e-mail on this network."));
            return;
        } else if (Config->GetModule("nickserv")->Get<bool>("secureadmins", "yes")
                   && source.nc != nc && nc->IsServicesOper()) {
            source.Reply(_("You may not change the e-mail of other Services Operators."));
            return;
        } else if (!param.empty() && !Mail::Validate(param)) {
            source.Reply(MAIL_X_INVALID, param.c_str());
            return;
        }

        EventReturn MOD_RESULT;
        FOREACH_RESULT(OnSetNickOption, MOD_RESULT, (source, this, nc, param));
        if (MOD_RESULT == EVENT_STOP) {
            return;
        }

        if (!param.empty()
                && Config->GetModule("nickserv")->Get<bool>("confirmemailchanges")
                && !source.IsServicesOper()) {
            if (SendConfirmMail(source.GetUser(), source.GetAccount(), source.service,
                                param)) {
                Log(LOG_COMMAND, source,
                    this) << "to request changing the email of " << nc->display << " to " << param;
                source.Reply(
                    _("A confirmation e-mail has been sent to \002%s\002. Follow the instructions in it to change your e-mail address."),
                    param.c_str());
            }
        } else {
            if (!param.empty()) {
                Log(nc == source.GetAccount() ? LOG_COMMAND : LOG_ADMIN, source,
                    this) << "to change the email of " << nc->display << " to " << param;
                nc->email = param;
                source.Reply(_("E-mail address for \002%s\002 changed to \002%s\002."),
                             nc->display.c_str(), param.c_str());
            } else {
                Log(nc == source.GetAccount() ? LOG_COMMAND : LOG_ADMIN, source,
                    this) << "to unset the email of " << nc->display;
                nc->email.clear();
                source.Reply(_("E-mail address for \002%s\002 unset."), nc->display.c_str());
            }
        }
    }

    void Execute(CommandSource &source,
                 const std::vector<Anope::string> &params) anope_override {
        this->Run(source, source.nc->display, params.size() ? params[0] : "");
    }

    bool OnHelp(CommandSource &source, const Anope::string &) anope_override {
        this->SendSyntax(source);
        source.Reply(" ");
        source.Reply(_("Associates the given E-mail address with your nickname.\n"
                       "This address will be displayed whenever someone requests\n"
                       "information on the nickname with the \002INFO\002 command."));
        return true;
    }
};

class CommandNSSASetEmail : public CommandNSSetEmail {
  public:
    CommandNSSASetEmail(Module *creator) : CommandNSSetEmail(creator,
                "nickserv/saset/email", 2) {
        this->ClearSyntax();
        this->SetSyntax(_("\037nickname\037 \037address\037"));
    }

    void Execute(CommandSource &source,
                 const std::vector<Anope::string> &params) anope_override {
        this->Run(source, params[0], params.size() > 1 ? params[1] : "");
    }

    bool OnHelp(CommandSource &source, const Anope::string &) anope_override {
        this->SendSyntax(source);
        source.Reply(" ");
        source.Reply(_("Associates the given E-mail address with the nickname."));
        return true;
    }
};

class CommandNSSetKeepModes : public Command {
  public:
    CommandNSSetKeepModes(Module *creator,
                          const Anope::string &sname = "nickserv/set/keepmodes",
                          size_t min = 1) : Command(creator, sname, min, min + 1) {
        this->SetDesc(_("Enable or disable keep modes"));
        this->SetSyntax("{ON | OFF}");
    }

    void Run(CommandSource &source, const Anope::string &user,
             const Anope::string &param) {
        if (Anope::ReadOnly) {
            source.Reply(READ_ONLY_MODE);
            return;
        }

        const NickAlias *na = NickAlias::Find(user);
        if (!na) {
            source.Reply(NICK_X_NOT_REGISTERED, user.c_str());
            return;
        }
        NickCore *nc = na->nc;

        EventReturn MOD_RESULT;
        FOREACH_RESULT(OnSetNickOption, MOD_RESULT, (source, this, nc, param));
        if (MOD_RESULT == EVENT_STOP) {
            return;
        }

        if (param.equals_ci("ON")) {
            Log(nc == source.GetAccount() ? LOG_COMMAND : LOG_ADMIN, source,
                this) << "to enable keepmodes for " << nc->display;
            nc->Extend<bool>("NS_KEEP_MODES");
            source.Reply(_("Keep modes for %s is now \002on\002."), nc->display.c_str());
        } else if (param.equals_ci("OFF")) {
            Log(nc == source.GetAccount() ? LOG_COMMAND : LOG_ADMIN, source,
                this) << "to disable keepmodes for " << nc->display;
            nc->Shrink<bool>("NS_KEEP_MODES");
            source.Reply(_("Keep modes for %s is now \002off\002."), nc->display.c_str());
        } else {
            this->OnSyntaxError(source, "");
        }
    }

    void Execute(CommandSource &source,
                 const std::vector<Anope::string> &params) anope_override {
        this->Run(source, source.nc->display, params[0]);
    }

    bool OnHelp(CommandSource &source, const Anope::string &) anope_override {
        this->SendSyntax(source);
        source.Reply(" ");
        source.Reply(_("Enables or disables keepmodes for your nick. If keep\n"
                       "modes is enabled, services will remember your usermodes\n"
                       "and attempt to re-set them the next time you authenticate."));
        return true;
    }
};

class CommandNSSASetKeepModes : public CommandNSSetKeepModes {
  public:
    CommandNSSASetKeepModes(Module *creator) : CommandNSSetKeepModes(creator,
                "nickserv/saset/keepmodes", 2) {
        this->ClearSyntax();
        this->SetSyntax(_("\037nickname\037 {ON | OFF}"));
    }

    void Execute(CommandSource &source,
                 const std::vector<Anope::string> &params) anope_override {
        this->Run(source, params[0], params[1]);
    }

    bool OnHelp(CommandSource &source, const Anope::string &) anope_override {
        this->SendSyntax(source);
        source.Reply(" ");
        source.Reply(_("Enables or disables keepmodes for the given nick. If keep\n"
                       "modes is enabled, services will remember users' usermodes\n"
                       "and attempt to re-set them the next time they authenticate."));
        return true;
    }
};

class CommandNSSetKill : public Command {
  public:
    CommandNSSetKill(Module *creator,
                     const Anope::string &sname = "nickserv/set/kill",
                     size_t min = 1) : Command(creator, sname, min, min + 1) {
        this->SetDesc(_("Turn protection on or off"));
        this->SetSyntax("{ON | QUICK | IMMED | OFF}");
    }

    void Run(CommandSource &source, const Anope::string &user,
             const Anope::string &param) {
        if (Anope::ReadOnly) {
            source.Reply(READ_ONLY_MODE);
            return;
        }

        if (Config->GetModule("nickserv")->Get<bool>("nonicknameownership")) {
            source.Reply(
                _("This command may not be used on this network because nickname ownership is disabled."));
            return;
        }

        const NickAlias *na = NickAlias::Find(user);
        if (!na) {
            source.Reply(NICK_X_NOT_REGISTERED, user.c_str());
            return;
        }
        NickCore *nc = na->nc;

        EventReturn MOD_RESULT;
        FOREACH_RESULT(OnSetNickOption, MOD_RESULT, (source, this, nc, param));
        if (MOD_RESULT == EVENT_STOP) {
            return;
        }

        if (param.equals_ci("ON")) {
            nc->Extend<bool>("KILLPROTECT");
            nc->Shrink<bool>("KILL_QUICK");
            nc->Shrink<bool>("KILL_IMMED");
            Log(nc == source.GetAccount() ? LOG_COMMAND : LOG_ADMIN, source,
                this) << "to set kill on for " << nc->display;
            source.Reply(_("Protection is now \002on\002 for \002%s\002."),
                         nc->display.c_str());
        } else if (param.equals_ci("QUICK")) {
            nc->Extend<bool>("KILLPROTECT");
            nc->Extend<bool>("KILL_QUICK");
            nc->Shrink<bool>("KILL_IMMED");
            Log(nc == source.GetAccount() ? LOG_COMMAND : LOG_ADMIN, source,
                this) << "to set kill quick for " << nc->display;
            source.Reply(
                _("Protection is now \002on\002 for \002%s\002, with a reduced delay."),
                nc->display.c_str());
        } else if (param.equals_ci("IMMED")) {
            if (Config->GetModule(this->owner)->Get<bool>("allowkillimmed")) {
                nc->Extend<bool>("KILLPROTECT");
                nc->Shrink<bool>("KILL_QUICK");
                nc->Extend<bool>("KILL_IMMED");
                Log(nc == source.GetAccount() ? LOG_COMMAND : LOG_ADMIN, source,
                    this) << "to set kill immed for " << nc->display;
                source.Reply(_("Protection is now \002on\002 for \002%s\002, with no delay."),
                             nc->display.c_str());
            } else {
                source.Reply(_("The \002IMMED\002 option is not available on this network."));
            }
        } else if (param.equals_ci("OFF")) {
            nc->Shrink<bool>("KILLPROTECT");
            nc->Shrink<bool>("KILL_QUICK");
            nc->Shrink<bool>("KILL_IMMED");
            Log(nc == source.GetAccount() ? LOG_COMMAND : LOG_ADMIN, source,
                this) << "to disable kill for " << nc->display;
            source.Reply(_("Protection is now \002off\002 for \002%s\002."),
                         nc->display.c_str());
        } else {
            this->OnSyntaxError(source, "KILL");
        }

        return;
    }

    void Execute(CommandSource &source,
                 const std::vector<Anope::string> &params) anope_override {
        this->Run(source, source.nc->display, params[0]);
    }

    bool OnHelp(CommandSource &source, const Anope::string &) anope_override {
        this->SendSyntax(source);
        source.Reply(" ");
        source.Reply(_("Turns the automatic protection option for your nick\n"
                       "on or off. With protection on, if another user\n"
                       "tries to take your nick, they will be given one minute to\n"
                       "change to another nick, after which %s will forcibly change\n"
                       "their nick.\n"
                       " \n"
                       "If you select \002QUICK\002, the user will be given only 20 seconds\n"
                       "to change nicks instead of the usual 60. If you select\n"
                       "\002IMMED\002, the user's nick will be changed immediately \037without\037 being\n"
                       "warned first or given a chance to change their nick; please\n"
                       "do not use this option unless necessary. Also, your\n"
                       "network's administrators may have disabled this option."), source.service->nick.c_str());
        return true;
    }
};

class CommandNSSASetKill : public CommandNSSetKill {
  public:
    CommandNSSASetKill(Module *creator) : CommandNSSetKill(creator,
                "nickserv/saset/kill", 2) {
        this->ClearSyntax();
        this->SetSyntax(_("\037nickname\037 {ON | QUICK | IMMED | OFF}"));
    }

    void Execute(CommandSource &source,
                 const std::vector<Anope::string> &params) anope_override {
        this->Run(source, params[0], params[1]);
    }

    bool OnHelp(CommandSource &source, const Anope::string &) anope_override {
        this->SendSyntax(source);
        source.Reply(" ");
        source.Reply(_("Turns the automatic protection option for the nick\n"
                       "on or off. With protection on, if another user\n"
                       "tries to take the nick, they will be given one minute to\n"
                       "change to another nick, after which %s will forcibly change\n"
                       "their nick.\n"
                       " \n"
                       "If you select \002QUICK\002, the user will be given only 20 seconds\n"
                       "to change nicks instead of the usual 60. If you select\n"
                       "\002IMMED\002, the user's nick will be changed immediately \037without\037 being\n"
                       "warned first or given a chance to change their nick; please\n"
                       "do not use this option unless necessary. Also, your\n"
                       "network's administrators may have disabled this option."), source.service->nick.c_str());
        return true;
    }
};

class CommandNSSetLanguage : public Command {
  public:
    CommandNSSetLanguage(Module *creator,
                         const Anope::string &sname = "nickserv/set/language",
                         size_t min = 1) : Command(creator, sname, min, min + 1) {
        this->SetDesc(_("Set the language Services will use when messaging you"));
        this->SetSyntax(_("\037language\037"));
    }

    void Run(CommandSource &source, const Anope::string &user,
             const Anope::string &param) {
        if (Anope::ReadOnly) {
            source.Reply(READ_ONLY_MODE);
            return;
        }

        const NickAlias *na = NickAlias::Find(user);
        if (!na) {
            source.Reply(NICK_X_NOT_REGISTERED, user.c_str());
            return;
        }
        NickCore *nc = na->nc;

        EventReturn MOD_RESULT;
        FOREACH_RESULT(OnSetNickOption, MOD_RESULT, (source, this, nc, param));
        if (MOD_RESULT == EVENT_STOP) {
            return;
        }

        if (param != "en_US")
            for (unsigned j = 0; j < Language::Languages.size(); ++j) {
                if (Language::Languages[j] == param) {
                    break;
                } else if (j + 1 == Language::Languages.size()) {
                    this->OnSyntaxError(source, "");
                    return;
                }
            }

        Log(nc == source.GetAccount() ? LOG_COMMAND : LOG_ADMIN, source,
            this) << "to change the language of " << nc->display << " to " << param;

        nc->language = param;
        if (source.GetAccount() == nc) {
            source.Reply(_("Language changed to \002English\002."));
        } else {
            source.Reply(_("Language for \002%s\002 changed to \002%s\002."),
                         nc->display.c_str(), Language::Translate(param.c_str(), _("English")));
        }
    }

    void Execute(CommandSource &source,
                 const std::vector<Anope::string> &param) anope_override {
        this->Run(source, source.nc->display, param[0]);
    }

    bool OnHelp(CommandSource &source, const Anope::string &) anope_override {
        this->SendSyntax(source);
        source.Reply(" ");
        source.Reply(_("Changes the language Services uses when sending messages to\n"
                       "you (for example, when responding to a command you send).\n"
                       "\037language\037 should be chosen from the following list of\n"
                       "supported languages:"));

        source.Reply("         en_US (English)");
        for (unsigned j = 0; j < Language::Languages.size(); ++j) {
            const Anope::string &langname = Language::Translate(
                Language::Languages[j].c_str(), _("English"));
            if (langname == "English") {
                continue;
            }
            source.Reply("         %s (%s)", Language::Languages[j].c_str(),
                         langname.c_str());
        }

        return true;
    }
};

class CommandNSSASetLanguage : public CommandNSSetLanguage {
  public:
    CommandNSSASetLanguage(Module *creator) : CommandNSSetLanguage(creator,
                "nickserv/saset/language", 2) {
        this->ClearSyntax();
        this->SetSyntax(_("\037nickname\037 \037language\037"));
    }

    void Execute(CommandSource &source,
                 const std::vector<Anope::string> &params) anope_override {
        this->Run(source, params[0], params[1]);
    }

    bool OnHelp(CommandSource &source, const Anope::string &) anope_override {
        this->SendSyntax(source);
        source.Reply(" ");
        source.Reply(_("Changes the language Services uses when sending messages to\n"
                       "the given user (for example, when responding to a command they send).\n"
                       "\037language\037 should be chosen from the following list of\n"
                       "supported languages:"));
        source.Reply("         en (English)");
        for (unsigned j = 0; j < Language::Languages.size(); ++j) {
            const Anope::string &langname = Language::Translate(
                Language::Languages[j].c_str(), _("English"));
            if (langname == "English") {
                continue;
            }
            source.Reply("         %s (%s)", Language::Languages[j].c_str(),
                         langname.c_str());
        }
        return true;
    }
};

class CommandNSSetMessage : public Command {
  public:
    CommandNSSetMessage(Module *creator,
                        const Anope::string &sname = "nickserv/set/message",
                        size_t min = 1) : Command(creator, sname, min, min + 1) {
        this->SetDesc(_("Change the communication method of Services"));
        this->SetSyntax("{ON | OFF}");
    }

    void Run(CommandSource &source, const Anope::string &user,
             const Anope::string &param) {
        if (Anope::ReadOnly) {
            source.Reply(READ_ONLY_MODE);
            return;
        }

        const NickAlias *na = NickAlias::Find(user);
        if (!na) {
            source.Reply(NICK_X_NOT_REGISTERED, user.c_str());
            return;
        }
        NickCore *nc = na->nc;

        if (!Config->GetBlock("options")->Get<bool>("useprivmsg")) {
            source.Reply(_("You cannot %s on this network."), source.command.c_str());
            return;
        }

        EventReturn MOD_RESULT;
        FOREACH_RESULT(OnSetNickOption, MOD_RESULT, (source, this, nc, param));
        if (MOD_RESULT == EVENT_STOP) {
            return;
        }

        if (param.equals_ci("ON")) {
            Log(nc == source.GetAccount() ? LOG_COMMAND : LOG_ADMIN, source,
                this) << "to enable " << source.command << " for " << nc->display;
            nc->Extend<bool>("MSG");
            source.Reply(_("Services will now reply to \002%s\002 with \002messages\002."),
                         nc->display.c_str());
        } else if (param.equals_ci("OFF")) {
            Log(nc == source.GetAccount() ? LOG_COMMAND : LOG_ADMIN, source,
                this) << "to disable " << source.command << " for " << nc->display;
            nc->Shrink<bool>("MSG");
            source.Reply(_("Services will now reply to \002%s\002 with \002notices\002."),
                         nc->display.c_str());
        } else {
            this->OnSyntaxError(source, "MSG");
        }
    }

    void Execute(CommandSource &source,
                 const std::vector<Anope::string> &params) anope_override {
        this->Run(source, source.nc->display, params[0]);
    }

    bool OnHelp(CommandSource &source, const Anope::string &) anope_override {
        Anope::string cmd = source.command;
        size_t i = cmd.find_last_of(' ');
        if (i != Anope::string::npos) {
            cmd = cmd.substr(i + 1);
        }

        this->SendSyntax(source);
        source.Reply(" ");
        source.Reply(_("Allows you to choose the way Services are communicating with\n"
                       "you. With \002%s\002 set, Services will use messages, else they'll\n"
                       "use notices."), cmd.upper().c_str());
        return true;
    }

    void OnServHelp(CommandSource &source) anope_override {
        if (Config->GetBlock("options")->Get<bool>("useprivmsg")) {
            Command::OnServHelp(source);
        }
    }
};

class CommandNSSASetMessage : public CommandNSSetMessage {
  public:
    CommandNSSASetMessage(Module *creator) : CommandNSSetMessage(creator,
                "nickserv/saset/message", 2) {
        this->ClearSyntax();
        this->SetSyntax(_("\037nickname\037 {ON | OFF}"));
    }

    bool OnHelp(CommandSource &source, const Anope::string &) anope_override {
        this->SendSyntax(source);
        source.Reply(" ");
        source.Reply(_("Allows you to choose the way Services are communicating with\n"
                       "the given user. With \002MSG\002 set, Services will use messages,\n"
                       "else they'll use notices."));
        return true;
    }

    void Execute(CommandSource &source,
                 const std::vector<Anope::string> &params) anope_override {
        this->Run(source, params[0], params[1]);
    }
};

class CommandNSSetSecure : public Command {
  public:
    CommandNSSetSecure(Module *creator,
                       const Anope::string &sname = "nickserv/set/secure",
                       size_t min = 1) : Command(creator, sname, min, min + 1) {
        this->SetDesc(_("Turn nickname security on or off"));
        this->SetSyntax("{ON | OFF}");
    }

    void Run(CommandSource &source, const Anope::string &user,
             const Anope::string &param) {
        if (Anope::ReadOnly) {
            source.Reply(READ_ONLY_MODE);
            return;
        }

        const NickAlias *na = NickAlias::Find(user);
        if (!na) {
            source.Reply(NICK_X_NOT_REGISTERED, user.c_str());
            return;
        }
        NickCore *nc = na->nc;

        EventReturn MOD_RESULT;
        FOREACH_RESULT(OnSetNickOption, MOD_RESULT, (source, this, nc, param));
        if (MOD_RESULT == EVENT_STOP) {
            return;
        }

        if (param.equals_ci("ON")) {
            Log(nc == source.GetAccount() ? LOG_COMMAND : LOG_ADMIN, source,
                this) << "to enable secure for " << nc->display;
            nc->Extend<bool>("NS_SECURE");
            source.Reply(_("Secure option is now \002on\002 for \002%s\002."),
                         nc->display.c_str());
        } else if (param.equals_ci("OFF")) {
            Log(nc == source.GetAccount() ? LOG_COMMAND : LOG_ADMIN, source,
                this) << "to disable secure for " << nc->display;
            nc->Shrink<bool>("NS_SECURE");
            source.Reply(_("Secure option is now \002off\002 for \002%s\002."),
                         nc->display.c_str());
        } else {
            this->OnSyntaxError(source, "SECURE");
        }
    }

    void Execute(CommandSource &source,
                 const std::vector<Anope::string> &params) anope_override {
        this->Run(source, source.nc->display, params[0]);
    }

    bool OnHelp(CommandSource &source, const Anope::string &) anope_override {
        this->SendSyntax(source);
        source.Reply(" ");
        source.Reply(_("Turns %s's security features on or off for your\n"
                       "nick. With \002SECURE\002 set, you must enter your password\n"
                       "before you will be recognized as the owner of the nick,\n"
                       "regardless of whether your address is on the access\n"
                       "list. However, if you are on the access list, %s\n"
                       "will not auto-kill you regardless of the setting of the\n"
                       "\002KILL\002 option."), source.service->nick.c_str(), source.service->nick.c_str());
        return true;
    }
};

class CommandNSSASetSecure : public CommandNSSetSecure {
  public:
    CommandNSSASetSecure(Module *creator) : CommandNSSetSecure(creator,
                "nickserv/saset/secure", 2) {
        this->ClearSyntax();
        this->SetSyntax(_("\037nickname\037 {ON | OFF}"));
    }

    void Execute(CommandSource &source,
                 const std::vector<Anope::string> &params) anope_override {
        this->Run(source, params[0], params[1]);
    }

    bool OnHelp(CommandSource &source, const Anope::string &) anope_override {
        this->SendSyntax(source);
        source.Reply(" ");
        source.Reply(_("Turns %s's security features on or off for your\n"
                       "nick. With \002SECURE\002 set, you must enter your password\n"
                       "before you will be recognized as the owner of the nick,\n"
                       "regardless of whether your address is on the access\n"
                       "list. However, if you are on the access list, %s\n"
                       "will not auto-kill you regardless of the setting of the\n"
                       "\002KILL\002 option."), source.service->nick.c_str(), source.service->nick.c_str());
        return true;
    }
};

class CommandNSSASetNoexpire : public Command {
  public:
    CommandNSSASetNoexpire(Module *creator) : Command(creator,
                "nickserv/saset/noexpire", 1, 2) {
        this->SetDesc(_("Prevent the nickname from expiring"));
        this->SetSyntax(_("\037nickname\037 {ON | OFF}"));
    }

    void Execute(CommandSource &source,
                 const std::vector<Anope::string> &params) anope_override {
        if (Anope::ReadOnly) {
            source.Reply(READ_ONLY_MODE);
            return;
        }

        NickAlias *na = NickAlias::Find(params[0]);
        if (na == NULL) {
            source.Reply(NICK_X_NOT_REGISTERED, params[0].c_str());
            return;
        }

        Anope::string param = params.size() > 1 ? params[1] : "";

        if (param.equals_ci("ON")) {
            Log(LOG_ADMIN, source, this) << "to enable noexpire for " << na->nick << " (" <<
                                         na->nc->display << ")";
            na->Extend<bool>("NS_NO_EXPIRE");
            source.Reply(_("Nick %s \002will not\002 expire."), na->nick.c_str());
        } else if (param.equals_ci("OFF")) {
            Log(LOG_ADMIN, source, this) << "to disable noexpire for " << na->nick << " ("
                                         << na->nc->display << ")";
            na->Shrink<bool>("NS_NO_EXPIRE");
            source.Reply(_("Nick %s \002will\002 expire."), na->nick.c_str());
        } else {
            this->OnSyntaxError(source, "NOEXPIRE");
        }
    }

    bool OnHelp(CommandSource &source, const Anope::string &) anope_override {
        this->SendSyntax(source);
        source.Reply(" ");
        source.Reply(_("Sets whether the given nickname will expire.  Setting this\n"
                       "to \002ON\002 prevents the nickname from expiring."));
        return true;
    }
};

class NSSet : public Module {
    CommandNSSet commandnsset;
    CommandNSSASet commandnssaset;

    CommandNSSetAutoOp commandnssetautoop;
    CommandNSSASetAutoOp commandnssasetautoop;

    CommandNSSetDisplay commandnssetdisplay;
    CommandNSSASetDisplay commandnssasetdisplay;

    CommandNSSetEmail commandnssetemail;
    CommandNSSASetEmail commandnssasetemail;

    CommandNSSetKeepModes commandnssetkeepmodes;
    CommandNSSASetKeepModes commandnssasetkeepmodes;

    CommandNSSetKill commandnssetkill;
    CommandNSSASetKill commandnssasetkill;

    CommandNSSetLanguage commandnssetlanguage;
    CommandNSSASetLanguage commandnssasetlanguage;

    CommandNSSetMessage commandnssetmessage;
    CommandNSSASetMessage commandnssasetmessage;

    CommandNSSetPassword commandnssetpassword;
    CommandNSSASetPassword commandnssasetpassword;

    CommandNSSetSecure commandnssetsecure;
    CommandNSSASetSecure commandnssasetsecure;

    CommandNSSASetNoexpire commandnssasetnoexpire;

    SerializableExtensibleItem<bool> autoop, killprotect, kill_quick, kill_immed,
                               message, secure, noexpire;

    struct KeepModes : SerializableExtensibleItem<bool> {
        KeepModes(Module *m, const Anope::string &n) : SerializableExtensibleItem<bool>
            (m, n) { }

        void ExtensibleSerialize(const Extensible *e, const Serializable *s,
                                 Serialize::Data &data) const anope_override {
            SerializableExtensibleItem<bool>::ExtensibleSerialize(e, s, data);

            if (s->GetSerializableType()->GetName() != "NickCore") {
                return;
            }

            const NickCore *nc = anope_dynamic_static_cast<const NickCore *>(s);
            Anope::string modes;
            for (User::ModeList::const_iterator it = nc->last_modes.begin();
                    it != nc->last_modes.end(); ++it) {
                if (!modes.empty()) {
                    modes += " ";
                }
                modes += it->first;
                if (!it->second.empty()) {
                    modes += "," + it->second;
                }
            }
            data["last_modes"] << modes;
        }

        void ExtensibleUnserialize(Extensible *e, Serializable *s,
                                   Serialize::Data &data) anope_override {
            SerializableExtensibleItem<bool>::ExtensibleUnserialize(e, s, data);

            if (s->GetSerializableType()->GetName() != "NickCore") {
                return;
            }

            NickCore *nc = anope_dynamic_static_cast<NickCore *>(s);
            Anope::string modes;
            data["last_modes"] >> modes;
            nc->last_modes.clear();
            for (spacesepstream sep(modes); sep.GetToken(modes);) {
                size_t c = modes.find(',');
                if (c == Anope::string::npos) {
                    nc->last_modes.insert(std::make_pair(modes, ""));
                } else {
                    nc->last_modes.insert(std::make_pair(modes.substr(0, c), modes.substr(c + 1)));
                }
            }
        }
    } keep_modes;

    /* email, passcode */
    PrimitiveExtensibleItem<std::pair<Anope::string, Anope::string > > ns_set_email;

  public:
    NSSet(const Anope::string &modname,
          const Anope::string &creator) : Module(modname, creator, VENDOR),
        commandnsset(this), commandnssaset(this),
        commandnssetautoop(this), commandnssasetautoop(this),
        commandnssetdisplay(this), commandnssasetdisplay(this),
        commandnssetemail(this), commandnssasetemail(this),
        commandnssetkeepmodes(this), commandnssasetkeepmodes(this),
        commandnssetkill(this), commandnssasetkill(this),
        commandnssetlanguage(this), commandnssasetlanguage(this),
        commandnssetmessage(this), commandnssasetmessage(this),
        commandnssetpassword(this), commandnssasetpassword(this),
        commandnssetsecure(this), commandnssasetsecure(this),
        commandnssasetnoexpire(this),

        autoop(this, "AUTOOP"),
        killprotect(this, "KILLPROTECT"), kill_quick(this, "KILL_QUICK"),
        kill_immed(this, "KILL_IMMED"), message(this, "MSG"),
        secure(this, "NS_SECURE"), noexpire(this, "NS_NO_EXPIRE"),

        keep_modes(this, "NS_KEEP_MODES"), ns_set_email(this, "ns_set_email") {

    }

    EventReturn OnPreCommand(CommandSource &source, Command *command,
                             std::vector<Anope::string> &params) anope_override {
        NickCore *uac = source.nc;

        if (command->name == "nickserv/confirm" && !params.empty() && uac) {
            std::pair<Anope::string, Anope::string> *n = ns_set_email.Get(uac);
            if (n) {
                if (params[0] == n->second) {
                    uac->email = n->first;
                    Log(LOG_COMMAND, source,
                        command) << "to confirm their email address change to " << uac->email;
                    source.Reply(_("Your email address has been changed to \002%s\002."),
                                 uac->email.c_str());
                    ns_set_email.Unset(uac);
                    return EVENT_STOP;
                }
            }
        }

        return EVENT_CONTINUE;
    }

    void OnSetCorrectModes(User *user, Channel *chan, AccessGroup &access,
                           bool &give_modes, bool &take_modes) anope_override {
        if (chan->ci) {
            /* Only give modes if autoop is set */
            give_modes &= !user->Account() || autoop.HasExt(user->Account());
        }
    }

    void OnPreNickExpire(NickAlias *na, bool &expire) anope_override {
        if (noexpire.HasExt(na)) {
            expire = false;
        }
    }

    void OnNickInfo(CommandSource &source, NickAlias *na, InfoFormatter &info,
                    bool show_hidden) anope_override {
        if (!show_hidden) {
            return;
        }

        if (kill_immed.HasExt(na->nc)) {
            info.AddOption(_("Immediate protection"));
        } else if (kill_quick.HasExt(na->nc)) {
            info.AddOption(_("Quick protection"));
        } else if (killprotect.HasExt(na->nc)) {
            info.AddOption(_("Protection"));
        }
        if (secure.HasExt(na->nc)) {
            info.AddOption(_("Security"));
        }
        if (message.HasExt(na->nc)) {
            info.AddOption(_("Message mode"));
        }
        if (autoop.HasExt(na->nc)) {
            info.AddOption(_("Auto-op"));
        }
        if (noexpire.HasExt(na)) {
            info.AddOption(_("No expire"));
        }
        if (keep_modes.HasExt(na->nc)) {
            info.AddOption(_("Keep modes"));
        }
    }

    void OnUserModeSet(const MessageSource &setter, User *u,
                       const Anope::string &mname) anope_override {
        if (u->Account() && setter.GetUser() == u) {
            u->Account()->last_modes = u->GetModeList();
        }
    }

    void OnUserModeUnset(const MessageSource &setter, User *u,
                         const Anope::string &mname) anope_override {
        if (u->Account() && setter.GetUser() == u) {
            u->Account()->last_modes = u->GetModeList();
        }
    }

    void OnUserLogin(User *u) anope_override {
        if (keep_modes.HasExt(u->Account())) {
            User::ModeList modes = u->Account()->last_modes;
            for (User::ModeList::iterator it = modes.begin(); it != modes.end(); ++it) {
                UserMode *um = ModeManager::FindUserModeByName(it->first);
                /* if the null user can set the mode, then it's probably safe */
                if (um && um->CanSet(NULL)) {
                    u->SetMode(NULL, it->first, it->second);
                }
            }
        }
    }
};

MODULE_INIT(NSSet)
