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

class CommandMSCancel : public Command {
  public:
    CommandMSCancel(Module *creator) : Command(creator, "memoserv/cancel", 1, 1) {
        this->SetDesc(_("Cancel the last memo you sent"));
        this->SetSyntax(_("{\037nick\037 | \037channel\037}"));
    }

    void Execute(CommandSource &source,
                 const std::vector<Anope::string> &params) anope_override {
        if (Anope::ReadOnly) {
            source.Reply(READ_ONLY_MODE);
            return;
        }

        const Anope::string &nname = params[0];

        bool ischan;
        MemoInfo *mi = MemoInfo::GetMemoInfo(nname, ischan);

        if (mi == NULL) {
            source.Reply(ischan ? CHAN_X_NOT_REGISTERED : NICK_X_NOT_REGISTERED,
                         nname.c_str());
            return;
        }

        ChannelInfo *ci = NULL;
        NickAlias *na = NULL;
        if (ischan) {
            ci = ChannelInfo::Find(nname);

            if (ci == NULL) {
                return;    // can't happen
            }
        } else {
            na = NickAlias::Find(nname);

            if (na == NULL) {
                return;    // can't happen
            }
        }

        for (int i = mi->memos->size() - 1; i >= 0; --i) {
            Memo *m = mi->GetMemo(i);

            if (!m->unread) {
                continue;
            }

            NickAlias *sender = NickAlias::Find(m->sender);

            if (sender && sender->nc == source.GetAccount()) {
                FOREACH_MOD(OnMemoDel, (ischan ? ci->name : na->nc->display, mi, m));
                mi->Del(i);
                source.Reply(_("Last memo to \002%s\002 has been cancelled."),
                             (ischan ? ci->name : na->nc->display).c_str());
                return;
            }
        }

        source.Reply(_("No memo was cancelable."));
    }

    bool OnHelp(CommandSource &source,
                const Anope::string &subcommand) anope_override {
        this->SendSyntax(source);
        source.Reply(" ");
        source.Reply(_("Cancels the last memo you sent to the given nick or channel,\n"
                       "provided it has not been read at the time you use the command."));
        return true;
    }
};

class MSCancel : public Module {
    CommandMSCancel commandmscancel;

  public:
    MSCancel(const Anope::string &modname,
             const Anope::string &creator) : Module(modname, creator, VENDOR),
        commandmscancel(this) {
    }
};

MODULE_INIT(MSCancel)
