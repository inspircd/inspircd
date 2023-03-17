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

class CommandMSInfo : public Command {
  public:
    CommandMSInfo(Module *creator) : Command(creator, "memoserv/info", 0, 1) {
        this->SetDesc(_("Displays information about your memos"));
        this->SetSyntax(_("[\037nick\037 | \037channel\037]"));
    }

    void Execute(CommandSource &source,
                 const std::vector<Anope::string> &params) anope_override {
        NickCore *nc = source.nc;
        const MemoInfo *mi;
        const NickAlias *na = NULL;
        ChannelInfo *ci = NULL;
        const Anope::string &nname = !params.empty() ? params[0] : "";
        bool hardmax;

        if (!nname.empty() && nname[0] != '#' && source.HasPriv("memoserv/info")) {
            na = NickAlias::Find(nname);
            if (!na) {
                source.Reply(NICK_X_NOT_REGISTERED, nname.c_str());
                return;
            }
            mi = &na->nc->memos;
            hardmax = na->nc->HasExt("MEMO_HARDMAX");
        } else if (!nname.empty() && nname[0] == '#') {
            ci = ChannelInfo::Find(nname);
            if (!ci) {
                source.Reply(CHAN_X_NOT_REGISTERED, nname.c_str());
                return;
            } else if (!source.AccessFor(ci).HasPriv("MEMO")) {
                source.Reply(ACCESS_DENIED);
                return;
            }
            mi = &ci->memos;
            hardmax = ci->HasExt("MEMO_HARDMAX");
        } else if (!nname.empty()) { /* It's not a chan and we aren't services admin */
            source.Reply(ACCESS_DENIED);
            return;
        } else {
            mi = &nc->memos;
            hardmax = nc->HasExt("MEMO_HARDMAX");
        }

        if (!nname.empty() && (ci || na->nc != nc)) {
            if (mi->memos->empty()) {
                source.Reply(_("%s currently has no memos."), nname.c_str());
            } else if (mi->memos->size() == 1) {
                if (mi->GetMemo(0)->unread) {
                    source.Reply(
                        _("%s currently has \0021\002 memo, and it has not yet been read."),
                        nname.c_str());
                } else {
                    source.Reply(_("%s currently has \0021\002 memo."), nname.c_str());
                }
            } else {
                unsigned count = 0, i, end;
                for (i = 0, end = mi->memos->size(); i < end; ++i)
                    if (mi->GetMemo(i)->unread) {
                        ++count;
                    }
                if (count == mi->memos->size()) {
                    source.Reply(_("%s currently has \002%d\002 memos; all of them are unread."),
                                 nname.c_str(), count);
                } else if (!count) {
                    source.Reply(_("%s currently has \002%d\002 memos."), nname.c_str(),
                                 mi->memos->size());
                } else if (count == 1) {
                    source.Reply(
                        _("%s currently has \002%d\002 memos, of which \0021\002 is unread."),
                        nname.c_str(), mi->memos->size());
                } else {
                    source.Reply(
                        _("%s currently has \002%d\002 memos, of which \002%d\002 are unread."),
                        nname.c_str(), mi->memos->size(), count);
                }
            }
            if (!mi->memomax) {
                if (hardmax) {
                    source.Reply(_("%s's memo limit is \002%d\002, and may not be changed."),
                                 nname.c_str(), mi->memomax);
                } else {
                    source.Reply(_("%s's memo limit is \002%d\002."), nname.c_str(), mi->memomax);
                }
            } else if (mi->memomax > 0) {
                if (hardmax) {
                    source.Reply(_("%s's memo limit is \002%d\002, and may not be changed."),
                                 nname.c_str(), mi->memomax);
                } else {
                    source.Reply(_("%s's memo limit is \002%d\002."), nname.c_str(), mi->memomax);
                }
            } else {
                source.Reply(_("%s has no memo limit."), nname.c_str());
            }

            /* I ripped this code out of ircservices 4.4.5, since I didn't want
               to rewrite the whole thing (it pisses me off). */
            if (na) {
                if (na->nc->HasExt("MEMO_RECEIVE") && na->nc->HasExt("MEMO_SIGNON")) {
                    source.Reply(_("%s is notified of new memos at logon and when they arrive."),
                                 nname.c_str());
                } else if (na->nc->HasExt("MEMO_RECEIVE")) {
                    source.Reply(_("%s is notified when new memos arrive."), nname.c_str());
                } else if (na->nc->HasExt("MEMO_SIGNON")) {
                    source.Reply(_("%s is notified of new memos at logon."), nname.c_str());
                } else {
                    source.Reply(_("%s is not notified of new memos."), nname.c_str());
                }
            }
        } else { /* !nname || (!ci || na->nc == nc) */
            if (mi->memos->empty()) {
                source.Reply(_("You currently have no memos."));
            } else if (mi->memos->size() == 1) {
                if (mi->GetMemo(0)->unread) {
                    source.Reply(
                        _("You currently have \0021\002 memo, and it has not yet been read."));
                } else {
                    source.Reply(_("You currently have \0021\002 memo."));
                }
            } else {
                unsigned count = 0, i, end;
                for (i = 0, end = mi->memos->size(); i < end; ++i)
                    if (mi->GetMemo(i)->unread) {
                        ++count;
                    }
                if (count == mi->memos->size()) {
                    source.Reply(_("You currently have \002%d\002 memos; all of them are unread."),
                                 count);
                } else if (!count) {
                    source.Reply(_("You currently have \002%d\002 memos."), mi->memos->size());
                } else if (count == 1) {
                    source.Reply(
                        _("You currently have \002%d\002 memos, of which \0021\002 is unread."),
                        mi->memos->size());
                } else {
                    source.Reply(
                        _("You currently have \002%d\002 memos, of which \002%d\002 are unread."),
                        mi->memos->size(), count);
                }
            }

            if (!mi->memomax) {
                if (!source.IsServicesOper() && hardmax) {
                    source.Reply(
                        _("Your memo limit is \0020\002; you will not receive any new memos. You cannot change this limit."));
                } else {
                    source.Reply(
                        _("Your memo limit is \0020\002; you will not receive any new memos."));
                }
            } else if (mi->memomax > 0) {
                if (!source.IsServicesOper() && hardmax) {
                    source.Reply(_("Your memo limit is \002%d\002, and may not be changed."),
                                 mi->memomax);
                } else {
                    source.Reply(_("Your memo limit is \002%d\002."), mi->memomax);
                }
            } else {
                source.Reply(_("You have no limit on the number of memos you may keep."));
            }

            bool memo_mail = nc->HasExt("MEMO_MAIL");
            if (nc->HasExt("MEMO_RECEIVE") && nc->HasExt("MEMO_SIGNON")) {
                if (memo_mail) {
                    source.Reply(
                        _("You will be notified of new memos at logon and when they arrive, and by mail when they arrive."));
                } else {
                    source.Reply(
                        _("You will be notified of new memos at logon and when they arrive."));
                }
            } else if (nc->HasExt("MEMO_RECEIVE")) {
                if (memo_mail) {
                    source.Reply(
                        _("You will be notified by message and by mail when new memos arrive."));
                } else {
                    source.Reply(_("You will be notified when new memos arrive."));
                }
            } else if (nc->HasExt("MEMO_SIGNON")) {
                if (memo_mail) {
                    source.Reply(
                        _("You will be notified of new memos at logon, and by mail when they arrive."));
                } else {
                    source.Reply(_("You will be notified of new memos at logon."));
                }
            } else {
                source.Reply(_("You will not be notified of new memos."));
            }
        }
    }

    bool OnHelp(CommandSource &source,
                const Anope::string &subcommand) anope_override {
        this->SendSyntax(source);
        source.Reply(" ");
        source.Reply(_("Without a parameter, displays information on the number of\n"
                       "memos you have, how many of them are unread, and how many\n"
                       "total memos you can receive.\n"
                       " \n"
                       "With a channel parameter, displays the same information for\n"
                       "the given channel.\n"
                       " \n"
                       "With a nickname parameter, displays the same information\n"
                       "for the given nickname. This is limited to \002Services\002\n"
                       "\002Operators\002."));

        return true;
    }
};

class MSInfo : public Module {
    CommandMSInfo commandmsinfo;

  public:
    MSInfo(const Anope::string &modname,
           const Anope::string &creator) : Module(modname, creator, VENDOR),
        commandmsinfo(this) {

    }
};

MODULE_INIT(MSInfo)
