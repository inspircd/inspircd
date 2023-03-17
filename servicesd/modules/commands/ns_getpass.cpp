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

class CommandNSGetPass : public Command {
  public:
    CommandNSGetPass(Module *creator) : Command(creator, "nickserv/getpass", 1, 1) {
        this->SetDesc(_("Retrieve the password for a nickname"));
        this->SetSyntax(_("\037nickname\037"));
    }

    void Execute(CommandSource &source,
                 const std::vector<Anope::string> &params) anope_override {
        const Anope::string &nick = params[0];
        Anope::string tmp_pass;
        const NickAlias *na;

        if (!(na = NickAlias::Find(nick))) {
            source.Reply(NICK_X_NOT_REGISTERED, nick.c_str());
        } else if (Config->GetModule("nickserv")->Get<bool>("secureadmins", "yes") && na->nc->IsServicesOper()) {
            source.Reply(_("You may not get the password of other Services Operators."));
        } else {
            if (Anope::Decrypt(na->nc->pass, tmp_pass) == 1) {
                Log(LOG_ADMIN, source, this) << "for " << nick;
                source.Reply(_("Password for %s is \002%s\002."), nick.c_str(),
                             tmp_pass.c_str());
            } else {
                source.Reply(_("GETPASS command unavailable because encryption is in use."));
            }
        }
        return;
    }

    bool OnHelp(CommandSource &source,
                const Anope::string &subcommand) anope_override {
        this->SendSyntax(source);
        source.Reply(" ");
        source.Reply(_("Returns the password for the given nickname.  \002Note\002 that\n"
                       "whenever this command is used, a message including the\n"
                       "person who issued the command and the nickname it was used\n"
                       "on will be logged and sent out as a WALLOPS/GLOBOPS."));
        return true;
    }
};

class NSGetPass : public Module {
    CommandNSGetPass commandnsgetpass;

  public:
    NSGetPass(const Anope::string &modname,
              const Anope::string &creator) : Module(modname, creator, VENDOR),
        commandnsgetpass(this) {

        Anope::string tmp_pass = "plain:tmp";
        if (!Anope::Decrypt(tmp_pass, tmp_pass)) {
            throw ModuleException("Incompatible with the encryption module being used");
        }

    }
};

MODULE_INIT(NSGetPass)
