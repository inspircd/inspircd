/* MemoServ core functions
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

class CommandMSCheck : public Command {
  public:
    CommandMSCheck(Module *creator) : Command(creator, "memoserv/check", 1, 1) {
        this->SetDesc(_("Checks if last memo to a nick was read"));
        this->SetSyntax(_("\037nick\037"));
    }

    void Execute(CommandSource &source,
                 const std::vector<Anope::string> &params) anope_override {

        const Anope::string &recipient = params[0];

        bool found = false;

        const NickAlias *na = NickAlias::Find(recipient);
        if (!na) {
            source.Reply(NICK_X_NOT_REGISTERED, recipient.c_str());
            return;
        }

        MemoInfo *mi = &na->nc->memos;

        /* Okay, I know this looks strange but we want to get the LAST memo, so we
            have to loop backwards */

        for (unsigned i = mi->memos->size(); i > 0; --i) {
            Memo *m = mi->GetMemo(i - 1);
            NickAlias *na2 = NickAlias::Find(m->sender);

            if (na2 != NULL && na2->nc == source.GetAccount()) {
                found = true; /* Yes, we've found the memo */

                if (m->unread) {
                    source.Reply(
                        _("The last memo you sent to %s (sent on %s) has not yet been read."),
                        na->nick.c_str(), Anope::strftime(m->time, source.GetAccount()).c_str());
                } else {
                    source.Reply(_("The last memo you sent to %s (sent on %s) has been read."),
                                 na->nick.c_str(), Anope::strftime(m->time, source.GetAccount()).c_str());
                }
                break;
            }
        }

        if (!found) {
            source.Reply(_("Nick %s doesn't have a memo from you."), na->nick.c_str());
        }

        return;
    }

    bool OnHelp(CommandSource &source,
                const Anope::string &subcommand) anope_override {
        this->SendSyntax(source);
        source.Reply(" ");
        source.Reply(_("Checks whether the _last_ memo you sent to \037nick\037 has been read\n"
                       "or not. Note that this only works with nicks, not with channels."));
        return true;
    }
};

class MSCheck : public Module {
    CommandMSCheck commandmscheck;

  public:
    MSCheck(const Anope::string &modname,
            const Anope::string &creator) : Module(modname, creator, VENDOR),
        commandmscheck(this) {

    }
};

MODULE_INIT(MSCheck)
