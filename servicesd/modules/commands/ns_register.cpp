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

static bool SendRegmail(User *u, const NickAlias *na, BotInfo *bi);

class CommandNSConfirm : public Command {
  public:
    CommandNSConfirm(Module *creator) : Command(creator, "nickserv/confirm", 1, 2) {
        this->SetDesc(_("Confirm a passcode"));
        this->SetSyntax(_("\037passcode\037"));
        this->AllowUnregistered(true);
    }

    void Execute(CommandSource &source,
                 const std::vector<Anope::string> &params) anope_override {
        const Anope::string &passcode = params[0];

        if (source.nc && (!source.nc->HasExt("UNCONFIRMED") || source.IsOper()) && source.HasPriv("nickserv/confirm")) {
            NickAlias *na = NickAlias::Find(passcode);
            if (na == NULL) {
                source.Reply(NICK_X_NOT_REGISTERED, passcode.c_str());
            } else if (na->nc->HasExt("UNCONFIRMED") == false) {
                source.Reply(_("Nick \002%s\002 is already confirmed."), na->nick.c_str());
            } else {
                na->nc->Shrink<bool>("UNCONFIRMED");
                FOREACH_MOD(OnNickConfirm, (source.GetUser(), na->nc));
                Log(LOG_ADMIN, source, this) << "to confirm nick " << na->nick << " (" <<
                                             na->nc->display << ")";
                source.Reply(_("Nick \002%s\002 has been confirmed."), na->nick.c_str());

                /* Login the users online already */
                for (std::list<User *>::iterator it = na->nc->users.begin();
                        it != na->nc->users.end(); ++it) {
                    User *u = *it;

                    IRCD->SendLogin(u, na);

                    NickAlias *u_na = NickAlias::Find(u->nick);

                    /* Set +r if they're on a nick in the group */
                    if (!Config->GetModule("nickserv")->Get<bool>("nonicknameownership") && u_na
                            && *u_na->nc == *na->nc) {
                        u->SetMode(source.service, "REGISTERED");
                    }
                }
            }
        } else if (source.nc) {
            Anope::string *code = source.nc->GetExt<Anope::string>("passcode");
            if (code != NULL && *code == passcode) {
                NickCore *nc = source.nc;
                nc->Shrink<Anope::string>("passcode");
                Log(LOG_COMMAND, source, this) << "to confirm their email";
                source.Reply(_("Your email address of \002%s\002 has been confirmed."),
                             source.nc->email.c_str());
                nc->Shrink<bool>("UNCONFIRMED");
                FOREACH_MOD(OnNickConfirm, (source.GetUser(), nc));

                if (source.GetUser()) {
                    NickAlias *na = NickAlias::Find(source.GetNick());
                    if (na) {
                        IRCD->SendLogin(source.GetUser(), na);
                        if (!Config->GetModule("nickserv")->Get<bool>("nonicknameownership")
                                && na->nc == source.GetAccount() && !na->nc->HasExt("UNCONFIRMED")) {
                            source.GetUser()->SetMode(source.service, "REGISTERED");
                        }
                    }
                }
            } else {
                source.Reply(_("Invalid passcode."));
            }
        } else {
            source.Reply(_("Invalid passcode."));
        }

        return;
    }

    bool OnHelp(CommandSource &source,
                const Anope::string &subcommand) anope_override {
        this->SendSyntax(source);
        source.Reply(" ");
        source.Reply(_("This command is used by several commands as a way to confirm\n"
                       "changes made to your account.\n"
                       " \n"
                       "This is most commonly used to confirm your email address once\n"
                       "you register or change it.\n"
                       " \n"
                       "This is also used after the RESETPASS command has been used to\n"
                       "force identify you to your nick so you may change your password."));
        if (source.HasPriv("nickserv/confirm"))
            source.Reply(_("Additionally, Services Operators with the \037nickserv/confirm\037 permission can\n"
                           "replace \037passcode\037 with a users nick to force validate them."));
        return true;
    }

    void OnSyntaxError(CommandSource &source,
                       const Anope::string &subcommand) anope_override {
        source.Reply(NICK_CONFIRM_INVALID);
    }
};

class CommandNSRegister : public Command {
  public:
    CommandNSRegister(Module *creator) : Command(creator, "nickserv/register", 1,
                2) {
        this->SetDesc(_("Register a nickname"));
        if (Config->GetModule("nickserv")->Get<bool>("forceemail", "yes")) {
            this->SetSyntax(_("\037password\037 \037email\037"));
        } else {
            this->SetSyntax(_("\037password\037 \037[email]\037"));
        }
        this->AllowUnregistered(true);
    }

    void Execute(CommandSource &source,
                 const std::vector<Anope::string> &params) anope_override {
        User *u = source.GetUser();
        Anope::string u_nick = source.GetNick();
        size_t nicklen = u_nick.length();
        Anope::string pass = params[0];
        Anope::string email = params.size() > 1 ? params[1] : "";
        const Anope::string &nsregister = Config->GetModule(this->owner)->Get<const Anope::string>("registration");

        if (Anope::ReadOnly) {
            source.Reply(_("Sorry, nickname registration is temporarily disabled."));
            return;
        }

        if (nsregister.equals_ci("disable")) {
            source.Reply(_("Registration is currently disabled."));
            return;
        }

        time_t nickregdelay = Config->GetModule(this->owner)->Get<time_t>("nickregdelay");
        time_t reg_delay = Config->GetModule("nickserv")->Get<time_t>("regdelay");
        if (u && !u->HasMode("OPER") && nickregdelay && Anope::CurTime - u->timestamp < nickregdelay) {
            source.Reply(
                _("You must have been using this nick for at least %d seconds to register."),
                nickregdelay);
            return;
        }

        /* Prevent "Guest" nicks from being registered. -TheShadow */

        /* Guest nick can now have a series of between 1 and 7 digits.
         *   --lara
         */
        const Anope::string &guestnick = Config->GetModule("nickserv")->Get<const Anope::string>("guestnickprefix", "Guest");
        if (nicklen <= guestnick.length() + 7 && nicklen >= guestnick.length() + 1 && !u_nick.find_ci(guestnick) && u_nick.substr(guestnick.length()).find_first_not_of("1234567890") == Anope::string::npos) {
            source.Reply(NICK_CANNOT_BE_REGISTERED, u_nick.c_str());
            return;
        }

        if (!IRCD->IsNickValid(u_nick)) {
            source.Reply(NICK_CANNOT_BE_REGISTERED, u_nick.c_str());
            return;
        }

        if (BotInfo::Find(u_nick, true)) {
            source.Reply(NICK_CANNOT_BE_REGISTERED, u_nick.c_str());
            return;
        }

        if (Config->GetModule("nickserv")->Get<bool>("restrictopernicks")) {
            for (unsigned i = 0; i < Oper::opers.size(); ++i) {
                Oper *o = Oper::opers[i];

                if (!source.IsOper() && u_nick.find_ci(o->name) != Anope::string::npos) {
                    source.Reply(NICK_CANNOT_BE_REGISTERED, u_nick.c_str());
                    return;
                }
            }
        }

        unsigned int passlen = Config->GetModule("nickserv")->Get<unsigned>("passlen", "32");

        if (Config->GetModule("nickserv")->Get<bool>("forceemail", "yes") && email.empty()) {
            this->OnSyntaxError(source, "");
        } else if (u && Anope::CurTime < u->lastnickreg + reg_delay) {
            source.Reply(
                _("Please wait %d seconds before using the REGISTER command again."),
                (u->lastnickreg + reg_delay) - Anope::CurTime);
        } else if (NickAlias::Find(u_nick) != NULL) {
            source.Reply(NICK_ALREADY_REGISTERED, u_nick.c_str());
        } else if (pass.equals_ci(u_nick) || (Config->GetBlock("options")->Get<bool>("strictpasswords") && pass.length() < 5)) {
            source.Reply(MORE_OBSCURE_PASSWORD);
        } else if (pass.length() > passlen) {
            source.Reply(PASSWORD_TOO_LONG, passlen);
        } else if (!email.empty() && !Mail::Validate(email)) {
            source.Reply(MAIL_X_INVALID, email.c_str());
        } else {
            NickCore *nc = new NickCore(u_nick);
            NickAlias *na = new NickAlias(u_nick, nc);
            Anope::Encrypt(pass, nc->pass);
            if (!email.empty()) {
                nc->email = email;
            }

            if (u) {
                na->last_usermask = u->GetIdent() + "@" + u->GetDisplayedHost();
                na->last_realname = u->realname;
            } else {
                na->last_realname = source.GetNick();
            }

            Log(LOG_COMMAND, source,
                this) << "to register " << na->nick << " (email: " << (!na->nc->email.empty() ?
                        na->nc->email : "none") << ")";

            if (na->nc->GetAccessCount()) {
                source.Reply(_("Nickname \002%s\002 registered under your user@host-mask: %s"),
                             u_nick.c_str(), na->nc->GetAccess(0).c_str());
            } else {
                source.Reply(_("Nickname \002%s\002 registered."), u_nick.c_str());
            }

            Anope::string tmp_pass;
            if (Anope::Decrypt(na->nc->pass, tmp_pass) == 1) {
                source.Reply(_("Your password is \002%s\002 - remember this for later use."),
                             tmp_pass.c_str());
            }

            if (nsregister.equals_ci("admin")) {
                nc->Extend<bool>("UNCONFIRMED");
            } else if (nsregister.equals_ci("mail")) {
                if (!email.empty()) {
                    nc->Extend<bool>("UNCONFIRMED");
                    SendRegmail(NULL, na, source.service);
                }
            }

            FOREACH_MOD(OnNickRegister, (source.GetUser(), na, pass));

            if (u) {
                // This notifies the user that if their registration is unconfirmed
                u->Identify(na);
                u->lastnickreg = Anope::CurTime;
            } else if (nc->HasExt("UNCONFIRMED")) {
                if (nsregister.equals_ci("admin")) {
                    source.Reply(
                        _("All new accounts must be validated by an administrator. Please wait for your registration to be confirmed."));
                } else if (nsregister.equals_ci("mail")) {
                    source.Reply(
                        _("Your email address is not confirmed. To confirm it, follow the instructions that were emailed to you."));
                }
            }
        }
    }

    bool OnHelp(CommandSource &source,
                const Anope::string &subcommand) anope_override {
        this->SendSyntax(source);
        source.Reply(" ");
        source.Reply(_("Registers your nickname in the %s database. Once\n"
                       "your nick is registered, you can use the \002SET\002 and \002ACCESS\002\n"
                       "commands to configure your nick's settings as you like\n"
                       "them. Make sure you remember the password you use when\n"
                       "registering - you'll need it to make changes to your nick\n"
                       "later. (Note that \002case matters!\002 \037ANOPE\037, \037Anope\037, and\n"
                       "\037anope\037 are all different passwords!)\n"
                       " \n"
                       "Guidelines on choosing passwords:\n"
                       " \n"
                       "Passwords should not be easily guessable. For example,\n"
                       "using your real name as a password is a bad idea. Using\n"
                       "your nickname as a password is a much worse idea ;) and,\n"
                       "in fact, %s will not allow it. Also, short\n"
                       "passwords are vulnerable to trial-and-error searches, so\n"
                       "you should choose a password at least 5 characters long.\n"
                       "Finally, the space character cannot be used in passwords."),
                     source.service->nick.c_str(), source.service->nick.c_str());

        if (!Config->GetModule("nickserv")->Get<bool>("forceemail", "yes")) {
            source.Reply(" ");
            source.Reply(
                _("The \037email\037 parameter is optional and will set the email\n"
                  "for your nick immediately.\n"
                  "Your privacy is respected; this e-mail won't be given to\n"
                  "any third-party person. You may also wish to \002SET HIDE\002 it\n"
                  "after registering if it isn't the default setting already."));
        }

        source.Reply(" ");
        source.Reply(_("This command also creates a new group for your nickname,\n"
                       "that will allow you to register other nicks later sharing\n"
                       "the same configuration, the same set of memos and the\n"
                       "same channel privileges."));
        return true;
    }
};

class CommandNSResend : public Command {
  public:
    CommandNSResend(Module *creator) : Command(creator, "nickserv/resend", 0, 0) {
        this->SetDesc(_("Resend registration confirmation email"));
    }

    void Execute(CommandSource &source,
                 const std::vector<Anope::string> &params) anope_override {
        if (!Config->GetModule(this->owner)->Get<const Anope::string>("registration").equals_ci("mail")) {
            source.Reply(ACCESS_DENIED);
            return;
        }

        const NickAlias *na = NickAlias::Find(source.GetNick());

        if (na == NULL) {
            source.Reply(NICK_NOT_REGISTERED);
        } else if (na->nc != source.GetAccount() || !source.nc->HasExt("UNCONFIRMED")) {
            source.Reply(_("Your account is already confirmed."));
        } else {
            if (Anope::CurTime < source.nc->lastmail + Config->GetModule(
                        this->owner)->Get<time_t>("resenddelay")) {
                source.Reply(_("Cannot send mail now; please retry a little later."));
            } else if (SendRegmail(source.GetUser(), na, source.service)) {
                na->nc->lastmail = Anope::CurTime;
                source.Reply(_("Your passcode has been re-sent to %s."), na->nc->email.c_str());
                Log(LOG_COMMAND, source, this) << "to resend registration verification code";
            } else {
                Log(this->owner) << "Unable to resend registration verification code for " <<
                                 source.GetNick();
            }
        }

        return;
    }

    bool OnHelp(CommandSource &source,
                const Anope::string &subcommand) anope_override {
        if (!Config->GetModule(this->owner)->Get<const Anope::string>("registration").equals_ci("mail")) {
            return false;
        }

        this->SendSyntax(source);
        source.Reply(" ");
        source.Reply(_("This command will resend you the registration confirmation email."));
        return true;
    }

    void OnServHelp(CommandSource &source) anope_override {
        if (Config->GetModule(this->owner)->Get<const Anope::string>("registration").equals_ci("mail")) {
            Command::OnServHelp(source);
        }
    }
};

class NSRegister : public Module {
    CommandNSRegister commandnsregister;
    CommandNSConfirm commandnsconfirm;
    CommandNSResend commandnsrsend;

    SerializableExtensibleItem<bool> unconfirmed;
    SerializableExtensibleItem<Anope::string> passcode;

  public:
    NSRegister(const Anope::string &modname,
               const Anope::string &creator) : Module(modname, creator, VENDOR),
        commandnsregister(this), commandnsconfirm(this), commandnsrsend(this),
        unconfirmed(this, "UNCONFIRMED"),
        passcode(this, "passcode") {
        if (Config->GetModule(
                    this)->Get<const Anope::string>("registration").equals_ci("disable")) {
            throw ModuleException("Module " + this->name +
                                  " will not load with registration disabled.");
        }
    }

    void OnNickIdentify(User *u) anope_override {
        BotInfo *NickServ;
        if (unconfirmed.HasExt(u->Account()) && (NickServ = Config->GetClient("NickServ"))) {
            const Anope::string &nsregister = Config->GetModule(
                this)->Get<const Anope::string>("registration");
            if (nsregister.equals_ci("admin")) {
                u->SendMessage(NickServ,
                               _("All new accounts must be validated by an administrator. Please wait for your registration to be confirmed."));
            } else {
                u->SendMessage(NickServ,
                               _("Your email address is not confirmed. To confirm it, follow the instructions that were emailed to you."));
            }
            const NickAlias *this_na = NickAlias::Find(u->Account()->display);
            time_t time_registered = Anope::CurTime - this_na->time_registered;
            time_t unconfirmed_expire = Config->GetModule(
                                            this)->Get<time_t>("unconfirmedexpire", "1d");
            if (unconfirmed_expire > time_registered) {
                u->SendMessage(NickServ,
                               _("Your account will expire, if not confirmed, in %s."),
                               Anope::Duration(unconfirmed_expire - time_registered, u->Account()).c_str());
            }
        }
    }

    void OnPreNickExpire(NickAlias *na, bool &expire) anope_override {
        if (unconfirmed.HasExt(na->nc)) {
            time_t unconfirmed_expire = Config->GetModule(
                this)->Get<time_t>("unconfirmedexpire", "1d");
            if (unconfirmed_expire
                    && Anope::CurTime - na->time_registered >= unconfirmed_expire) {
                expire = true;
            }
        }
    }
};

static bool SendRegmail(User *u, const NickAlias *na, BotInfo *bi) {
    NickCore *nc = na->nc;

    Anope::string *code = na->nc->GetExt<Anope::string>("passcode");
    if (code == NULL) {
        code = na->nc->Extend<Anope::string>("passcode");
        *code = Anope::Random(9);
    }

    Anope::string subject = Language::Translate(na->nc,
                            Config->GetBlock("mail")->Get<const Anope::string>("registration_subject").c_str()),
                            message = Language::Translate(na->nc,
                                      Config->GetBlock("mail")->Get<const Anope::string>("registration_message").c_str());

    subject = subject.replace_all_cs("%n", na->nick);
    subject = subject.replace_all_cs("%N",
                                     Config->GetBlock("networkinfo")->Get<const Anope::string>("networkname"));
    subject = subject.replace_all_cs("%c", *code);

    message = message.replace_all_cs("%n", na->nick);
    message = message.replace_all_cs("%N",
                                     Config->GetBlock("networkinfo")->Get<const Anope::string>("networkname"));
    message = message.replace_all_cs("%c", *code);

    return Mail::Send(u, nc, bi, subject, message);
}

MODULE_INIT(NSRegister)
