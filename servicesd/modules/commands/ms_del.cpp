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

class MemoDelCallback : public NumberList {
    CommandSource &source;
    Command *cmd;
    ChannelInfo *ci;
    MemoInfo *mi;
  public:
    MemoDelCallback(CommandSource &_source, Command *c, ChannelInfo *_ci,
                    MemoInfo *_mi, const Anope::string &list) : NumberList(list, true),
        source(_source), cmd(c), ci(_ci), mi(_mi) {
    }

    void HandleNumber(unsigned number) anope_override {
        if (!number || number > mi->memos->size()) {
            return;
        }

        FOREACH_MOD(OnMemoDel, (ci ? ci->name : source.nc->display, mi, mi->GetMemo(number - 1)));

        mi->Del(number - 1);
        source.Reply(_("Memo %d has been deleted."), number);
        if (ci) {
            Log(LOG_COMMAND, source, cmd, ci) << "on memo " << number;
        }
    }
};

class CommandMSDel : public Command {
  public:
    CommandMSDel(Module *creator) : Command(creator, "memoserv/del", 0, 2) {
        this->SetDesc(_("Delete a memo or memos"));
        this->SetSyntax(
            _("[\037channel\037] {\037num\037 | \037list\037 | LAST | ALL}"));
    }

    void Execute(CommandSource &source,
                 const std::vector<Anope::string> &params) anope_override {
        if (Anope::ReadOnly) {
            source.Reply(READ_ONLY_MODE);
            return;
        }

        MemoInfo *mi;
        ChannelInfo *ci = NULL;
        Anope::string numstr = !params.empty() ? params[0] : "", chan;

        if (!numstr.empty() && numstr[0] == '#') {
            chan = numstr;
            numstr = params.size() > 1 ? params[1] : "";

            ci = ChannelInfo::Find(chan);
            if (!ci) {
                source.Reply(CHAN_X_NOT_REGISTERED, chan.c_str());
                return;
            } else if (!source.AccessFor(ci).HasPriv("MEMO")) {
                source.Reply(ACCESS_DENIED);
                return;
            }
            mi = &ci->memos;
        } else {
            mi = &source.nc->memos;
        }
        if (numstr.empty() || (!isdigit(numstr[0]) && !numstr.equals_ci("ALL") && !numstr.equals_ci("LAST"))) {
            this->OnSyntaxError(source, numstr);
        } else if (mi->memos->empty()) {
            if (!chan.empty()) {
                source.Reply(MEMO_X_HAS_NO_MEMOS, chan.c_str());
            } else {
                source.Reply(MEMO_HAVE_NO_MEMOS);
            }
        } else {
            if (isdigit(numstr[0])) {
                MemoDelCallback list(source, this, ci, mi, numstr);
                list.Process();
            } else if (numstr.equals_ci("LAST")) {
                /* Delete last memo. */
                FOREACH_MOD(OnMemoDel, (ci ? ci->name : source.nc->display, mi,
                                        mi->GetMemo(mi->memos->size() - 1)));
                mi->Del(mi->memos->size() - 1);
                source.Reply(_("Memo %d has been deleted."), mi->memos->size() + 1);
                if (ci) {
                    Log(LOG_COMMAND, source, this, ci) << "on LAST memo";
                }
            } else {
                /* Delete all memos. */
                for (unsigned i = mi->memos->size(); i > 0; --i) {
                    FOREACH_MOD(OnMemoDel, (ci ? ci->name : source.nc->display, mi,
                                            mi->GetMemo(i)));
                    mi->Del(i - 1);
                }
                if (!chan.empty()) {
                    source.Reply(_("All memos for channel %s have been deleted."), chan.c_str());
                    Log(LOG_COMMAND, source, this, ci) << "on ALL memos";
                } else {
                    source.Reply(_("All of your memos have been deleted."));
                }
            }
        }
        return;
    }

    bool OnHelp(CommandSource &source,
                const Anope::string &subcommand) anope_override {
        this->SendSyntax(source);
        source.Reply(" ");
        source.Reply(_("Deletes the specified memo or memos. You can supply\n"
                       "multiple memo numbers or ranges of numbers instead of a\n"
                       "single number, as in the second example below.\n"
                       " \n"
                       "If \002LAST\002 is given, the last memo will be deleted.\n"
                       "If \002ALL\002 is given, deletes all of your memos.\n"
                       " \n"
                       "Examples:\n"
                       " \n"
                       "   \002DEL 1\002\n"
                       "      Deletes your first memo.\n"
                       " \n"
                       "   \002DEL 2-5,7-9\002\n"
                       "      Deletes memos numbered 2 through 5 and 7 through 9."));
        return true;
    }
};

class MSDel : public Module {
    CommandMSDel commandmsdel;

  public:
    MSDel(const Anope::string &modname,
          const Anope::string &creator) : Module(modname, creator, VENDOR),
        commandmsdel(this) {

    }
};

MODULE_INIT(MSDel)
