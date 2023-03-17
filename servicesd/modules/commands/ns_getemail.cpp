/* NickServ core functions
 *
 * (C) 2003-2023 Anope Team
 * Contact us at team@anope.org
 *
 * Please read COPYING and README for further details.
 *
 * Based on the original code of Epona by Lara.
 * Based on the original code of Services by Andy Church.
 *
 * A simple call to check for all emails that a user may have registered
 * with. It returns the nicks that match the email you provide. Wild
 * Cards are not excepted. Must use user@email-host.
 */

#include "module.h"

class CommandNSGetEMail : public Command {
  public:
    CommandNSGetEMail(Module *creator) : Command(creator, "nickserv/getemail", 1,
                1) {
        this->SetDesc(
            _("Matches and returns all users that registered using given email"));
        this->SetSyntax(_("\037email\037"));
    }

    void Execute(CommandSource &source,
                 const std::vector<Anope::string> &params) anope_override {
        const Anope::string &email = params[0];
        int j = 0;

        Log(LOG_ADMIN, source, this) << "on " << email;

        for (nickcore_map::const_iterator it = NickCoreList->begin(), it_end = NickCoreList->end(); it != it_end; ++it) {
            const NickCore *nc = it->second;

            if (!nc->email.empty() && Anope::Match(nc->email, email)) {
                ++j;
                source.Reply(_("Email matched: \002%s\002 (\002%s\002) to \002%s\002."),
                             nc->display.c_str(), nc->email.c_str(), email.c_str());
            }
        }

        if (j <= 0) {
            source.Reply(_("No registrations matching \002%s\002 were found."),
                         email.c_str());
            return;
        }

        return;
    }

    bool OnHelp(CommandSource &source,
                const Anope::string &subcommand) anope_override {
        this->SendSyntax(source);
        source.Reply(" ");
        source.Reply(_("Returns the matching accounts that used given email."));
        return true;
    }
};

class NSGetEMail : public Module {
    CommandNSGetEMail commandnsgetemail;
  public:
    NSGetEMail(const Anope::string &modname,
               const Anope::string &creator) : Module(modname, creator, VENDOR),
        commandnsgetemail(this) {

    }
};

MODULE_INIT(NSGetEMail)
