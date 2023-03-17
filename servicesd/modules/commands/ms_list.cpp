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

class CommandMSList : public Command {
  public:
    CommandMSList(Module *creator) : Command(creator, "memoserv/list", 0, 2) {
        this->SetDesc(_("List your memos"));
        this->SetSyntax(_("[\037channel\037] [\037list\037 | NEW]"));
    }

    void Execute(CommandSource &source,
                 const std::vector<Anope::string> &params) anope_override {

        Anope::string param = !params.empty() ? params[0] : "", chan;
        ChannelInfo *ci = NULL;
        const MemoInfo *mi;

        if (!param.empty() && param[0] == '#') {
            chan = param;
            param = params.size() > 1 ? params[1] : "";

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

        if (!param.empty() && !isdigit(param[0]) && !param.equals_ci("NEW")) {
            this->OnSyntaxError(source, param);
        } else if (!mi->memos->size()) {
            if (!chan.empty()) {
                source.Reply(MEMO_X_HAS_NO_MEMOS, chan.c_str());
            } else {
                source.Reply(MEMO_HAVE_NO_MEMOS);
            }
        } else {
            ListFormatter list(source.GetAccount());

            list.AddColumn(_("Number")).AddColumn(_("Sender")).AddColumn(_("Date/Time"));

            if (!param.empty() && isdigit(param[0])) {
                class MemoListCallback : public NumberList {
                    ListFormatter &list;
                    CommandSource &source;
                    const MemoInfo *mi;
                  public:
                    MemoListCallback(ListFormatter &_list, CommandSource &_source,
                                     const MemoInfo *_mi, const Anope::string &numlist) : NumberList(numlist, false),
                        list(_list), source(_source), mi(_mi) {
                    }

                    void HandleNumber(unsigned number) anope_override {
                        if (!number || number > mi->memos->size()) {
                            return;
                        }

                        const Memo *m = mi->GetMemo(number - 1);

                        ListFormatter::ListEntry entry;
                        entry["Number"] = (m->unread ? "* " : "  ") + stringify(number);
                        entry["Sender"] = m->sender;
                        entry["Date/Time"] = Anope::strftime(m->time, source.GetAccount());
                        this->list.AddEntry(entry);
                    }
                }
                mlc(list, source, mi, param);
                mlc.Process();
            } else {
                if (!param.empty()) {
                    unsigned i, end;
                    for (i = 0, end = mi->memos->size(); i < end; ++i)
                        if (mi->GetMemo(i)->unread) {
                            break;
                        }
                    if (i == end) {
                        if (!chan.empty()) {
                            source.Reply(MEMO_X_HAS_NO_NEW_MEMOS, chan.c_str());
                        } else {
                            source.Reply(MEMO_HAVE_NO_NEW_MEMOS);
                        }
                        return;
                    }
                }

                for (unsigned i = 0, end = mi->memos->size(); i < end; ++i) {
                    if (!param.empty() && !mi->GetMemo(i)->unread) {
                        continue;
                    }

                    const Memo *m = mi->GetMemo(i);

                    ListFormatter::ListEntry entry;
                    entry["Number"] = (m->unread ? "* " : "  ") + stringify(i + 1);
                    entry["Sender"] = m->sender;
                    entry["Date/Time"] = Anope::strftime(m->time, source.GetAccount());
                    list.AddEntry(entry);
                }
            }

            std::vector<Anope::string> replies;
            list.Process(replies);

            source.Reply(_("Memos for %s:"),
                         ci ? ci->name.c_str() : source.GetNick().c_str());
            for (unsigned i = 0; i < replies.size(); ++i) {
                source.Reply(replies[i]);
            }
        }
        return;
    }

    bool OnHelp(CommandSource &source,
                const Anope::string &subcommand) anope_override {
        this->SendSyntax(source);
        source.Reply(" ");
        source.Reply(_("Lists any memos you currently have.  With \002NEW\002, lists only\n"
                       "new (unread) memos. Unread memos are marked with a \"*\"\n"
                       "to the left of the memo number. You can also specify a list\n"
                       "of numbers, as in the example below:\n"
                       "   \002LIST 2-5,7-9\002\n"
                       "      Lists memos numbered 2 through 5 and 7 through 9."));
        return true;
    }
};

class MSList : public Module {
    CommandMSList commandmslist;

  public:
    MSList(const Anope::string &modname,
           const Anope::string &creator) : Module(modname, creator, VENDOR),
        commandmslist(this) {

    }
};

MODULE_INIT(MSList)
