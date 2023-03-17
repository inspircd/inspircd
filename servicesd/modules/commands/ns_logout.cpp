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

static ServiceReference<NickServService> NickServService("NickServService",
        "NickServ");

class CommandNSLogout : public Command {
  public:
    CommandNSLogout(Module *creator) : Command(creator, "nickserv/logout", 0, 2) {
        this->SetDesc(_("Reverses the effect of the IDENTIFY command"));
        this->SetSyntax(_("[\037nickname\037 [REVALIDATE]]"));
    }

    void Execute(CommandSource &source,
                 const std::vector<Anope::string> &params) anope_override {

        const Anope::string &nick = !params.empty() ? params[0] : "";
        const Anope::string &param = params.size() > 1 ? params[1] : "";

        User *u2;
        if (!source.IsServicesOper() && !nick.empty()) {
            this->OnSyntaxError(source, "");
        } else if (!(u2 = (!nick.empty() ? User::Find(nick, true) : source.GetUser()))) {
            source.Reply(NICK_X_NOT_IN_USE,
                         !nick.empty() ? nick.c_str() : source.GetNick().c_str());
        } else if (!nick.empty() && u2->IsServicesOper()) {
            source.Reply(_("You can't logout %s, they are a Services Operator."),
                         nick.c_str());
        } else {
            if (!nick.empty() && !param.empty() && param.equals_ci("REVALIDATE")
                    && NickServService) {
                NickServService->Validate(u2);
            }

            u2->super_admin = false; /* Don't let people logout and remain a SuperAdmin */
            Log(LOG_COMMAND, source, this) << "to logout " << u2->nick;

            /* Remove founder status from this user in all channels */
            if (!nick.empty()) {
                source.Reply(_("Nick %s has been logged out."), nick.c_str());
            } else {
                source.Reply(_("Your nick has been logged out."));
            }

            IRCD->SendLogout(u2);
            u2->RemoveMode(source.service, "REGISTERED");
            u2->Logout();

            /* Send out an event */
            FOREACH_MOD(OnNickLogout, (u2));
        }
        return;
    }

    bool OnHelp(CommandSource &source,
                const Anope::string &subcommand) anope_override {
        this->SendSyntax(source);
        source.Reply(" ");
        source.Reply(_("Without a parameter, reverses the effect of the \002IDENTIFY\002\n"
                       "command, i.e. make you not recognized as the real owner of the nick\n"
                       "anymore. Note, however, that you won't be asked to reidentify\n"
                       "yourself.\n"
                       " \n"
                       "With a parameter, does the same for the given nick. If you\n"
                       "specify \002REVALIDATE\002 as well, Services will ask the given nick\n"
                       "to re-identify. This is limited to \002Services Operators\002."));

        return true;
    }
};

class NSLogout : public Module {
    CommandNSLogout commandnslogout;

  public:
    NSLogout(const Anope::string &modname,
             const Anope::string &creator) : Module(modname, creator, VENDOR),
        commandnslogout(this) {

    }
};

MODULE_INIT(NSLogout)
