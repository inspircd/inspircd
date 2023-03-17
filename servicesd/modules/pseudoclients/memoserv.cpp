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

class MemoServCore : public Module, public MemoServService {
    Reference<BotInfo> MemoServ;

    bool SendMemoMail(NickCore *nc, MemoInfo *mi, Memo *m) {
        Anope::string subject = Language::Translate(nc,
                                Config->GetBlock("mail")->Get<const Anope::string>("memo_subject").c_str()),
                                message = Language::Translate(
                                              Config->GetBlock("mail")->Get<const Anope::string>("memo_message").c_str());

        subject = subject.replace_all_cs("%n", nc->display);
        subject = subject.replace_all_cs("%s", m->sender);
        subject = subject.replace_all_cs("%d", stringify(mi->GetIndex(m) + 1));
        subject = subject.replace_all_cs("%t", m->text);
        subject = subject.replace_all_cs("%N",
                                         Config->GetBlock("networkinfo")->Get<const Anope::string>("networkname"));

        message = message.replace_all_cs("%n", nc->display);
        message = message.replace_all_cs("%s", m->sender);
        message = message.replace_all_cs("%d", stringify(mi->GetIndex(m) + 1));
        message = message.replace_all_cs("%t", m->text);
        message = message.replace_all_cs("%N",
                                         Config->GetBlock("networkinfo")->Get<const Anope::string>("networkname"));

        return Mail::Send(nc, subject, message);
    }

  public:
    MemoServCore(const Anope::string &modname,
                 const Anope::string &creator) : Module(modname, creator, PSEUDOCLIENT | VENDOR),
        MemoServService(this) {
    }

    MemoResult Send(const Anope::string &source, const Anope::string &target,
                    const Anope::string &message, bool force) anope_override {
        bool ischan;
        MemoInfo *mi = MemoInfo::GetMemoInfo(target, ischan);

        if (mi == NULL) {
            return MEMO_INVALID_TARGET;
        }

        Anope::string sender_display = source;

        User *sender = User::Find(source, true);
        if (sender != NULL) {
            if (!sender->HasPriv("memoserv/no-limit") && !force) {
                time_t send_delay = Config->GetModule("memoserv")->Get<time_t>("senddelay");
                if (send_delay > 0 && sender->lastmemosend + send_delay > Anope::CurTime) {
                    return MEMO_TOO_FAST;
                } else if (!mi->memomax) {
                    return MEMO_TARGET_FULL;
                } else if (mi->memomax > 0
                           && mi->memos->size() >= static_cast<unsigned>(mi->memomax)) {
                    return MEMO_TARGET_FULL;
                } else if (mi->HasIgnore(sender)) {
                    return MEMO_SUCCESS;
                }
            }

            NickCore *acc = sender->Account();
            if (acc != NULL) {
                sender_display = acc->display;
            }
        }

        if (sender != NULL) {
            sender->lastmemosend = Anope::CurTime;
        }

        Memo *m = new Memo();
        m->mi = mi;
        mi->memos->push_back(m);
        m->owner = target;
        m->sender = sender_display;
        m->time = Anope::CurTime;
        m->text = message;
        m->unread = true;

        FOREACH_MOD(OnMemoSend, (source, target, mi, m));

        if (ischan) {
            ChannelInfo *ci = ChannelInfo::Find(target);

            if (ci->c) {
                for (Channel::ChanUserList::iterator it = ci->c->users.begin(),
                        it_end = ci->c->users.end(); it != it_end; ++it) {
                    ChanUserContainer *cu = it->second;

                    if (ci->AccessFor(cu->user).HasPriv("MEMO")) {
                        if (cu->user->Account() && cu->user->Account()->HasExt("MEMO_RECEIVE")) {
                            cu->user->SendMessage(MemoServ, MEMO_NEW_X_MEMO_ARRIVED, ci->name.c_str(),
                                                  Config->StrictPrivmsg.c_str(), MemoServ->nick.c_str(), ci->name.c_str(),
                                                  mi->memos->size());
                        }
                    }
                }
            }
        } else {
            NickCore *nc = NickAlias::Find(target)->nc;

            if (nc->HasExt("MEMO_RECEIVE")) {
                for (unsigned i = 0; i < nc->aliases->size(); ++i) {
                    const NickAlias *na = nc->aliases->at(i);
                    User *user = User::Find(na->nick, true);
                    if (user && user->IsIdentified()) {
                        user->SendMessage(MemoServ, MEMO_NEW_MEMO_ARRIVED, source.c_str(),
                                          Config->StrictPrivmsg.c_str(), MemoServ->nick.c_str(), mi->memos->size());
                    }
                }
            }

            /* let's get out the mail if set in the nickcore - certus */
            if (nc->HasExt("MEMO_MAIL")) {
                SendMemoMail(nc, mi, m);
            }
        }

        return MEMO_SUCCESS;
    }

    void Check(User *u) anope_override {
        const NickCore *nc = u->Account();
        if (!nc) {
            return;
        }

        unsigned i = 0, end = nc->memos.memos->size(), newcnt = 0;
        for (; i < end; ++i)
            if (nc->memos.GetMemo(i)->unread) {
                ++newcnt;
            }
        if (newcnt > 0) {
            u->SendMessage(MemoServ,
                           newcnt == 1 ? _("You have 1 new memo.") : _("You have %d new memos."), newcnt);
        }
        if (nc->memos.memomax > 0 && nc->memos.memos->size() >= static_cast<unsigned>(nc->memos.memomax)) {
            if (nc->memos.memos->size() > static_cast<unsigned>(nc->memos.memomax)) {
                u->SendMessage(MemoServ,
                               _("You are over your maximum number of memos (%d). You will be unable to receive any new memos until you delete some of your current ones."),
                               nc->memos.memomax);
            } else {
                u->SendMessage(MemoServ,
                               _("You have reached your maximum number of memos (%d). You will be unable to receive any new memos until you delete some of your current ones."),
                               nc->memos.memomax);
            }
        }
    }

    void OnReload(Configuration::Conf *conf) anope_override {
        const Anope::string &msnick = conf->GetModule(this)->Get<const Anope::string>("client");

        if (msnick.empty()) {
            throw ConfigException(Module::name + ": <client> must be defined");
        }

        BotInfo *bi = BotInfo::Find(msnick, true);
        if (!bi) {
            throw ConfigException(Module::name + ": no bot named " + msnick);
        }

        MemoServ = bi;
    }

    void OnNickCoreCreate(NickCore *nc) anope_override {
        nc->memos.memomax = Config->GetModule(this)->Get<int>("maxmemos");
    }

    void OnCreateChan(ChannelInfo *ci) anope_override {
        ci->memos.memomax = Config->GetModule(this)->Get<int>("maxmemos");
    }

    void OnBotDelete(BotInfo *bi) anope_override {
        if (bi == MemoServ) {
            MemoServ = NULL;
        }
    }

    void OnNickIdentify(User *u) anope_override {
        this->Check(u);
    }

    void OnJoinChannel(User *u, Channel *c) anope_override {
        if (c->ci && !c->ci->memos.memos->empty() && c->ci->AccessFor(u).HasPriv("MEMO")) {
            if (c->ci->memos.memos->size() == 1) {
                u->SendMessage(MemoServ, _("There is \002%d\002 memo on channel %s."),
                               c->ci->memos.memos->size(), c->ci->name.c_str());
            } else {
                u->SendMessage(MemoServ, _("There are \002%d\002 memos on channel %s."),
                               c->ci->memos.memos->size(), c->ci->name.c_str());
            }
        }
    }

    void OnUserAway(User *u, const Anope::string &message) anope_override {
        if (message.empty()) {
            this->Check(u);
        }
    }

    void OnNickUpdate(User *u) anope_override {
        this->Check(u);
    }

    EventReturn OnPreHelp(CommandSource &source,
                          const std::vector<Anope::string> &params) anope_override {
        if (!params.empty() || source.c || source.service != *MemoServ) {
            return EVENT_CONTINUE;
        }
        source.Reply(_("\002%s\002 is a utility allowing IRC users to send short\n"
                       "messages to other IRC users, whether they are online at\n"
                       "the time or not, or to channels(*). Both the sender's\n"
                       "nickname and the target nickname or channel must be\n"
                       "registered in order to send a memo.\n"
                       "%s's commands include:"), MemoServ->nick.c_str(), MemoServ->nick.c_str());
        return EVENT_CONTINUE;
    }

    void OnPostHelp(CommandSource &source,
                    const std::vector<Anope::string> &params) anope_override {
        if (!params.empty() || source.c || source.service != *MemoServ) {
            return;
        }
        source.Reply(_(" \n"
                       "Type \002%s%s HELP \037command\037\002 for help on any of the\n"
                       "above commands."), Config->StrictPrivmsg.c_str(), MemoServ->nick.c_str());
    }
};

MODULE_INIT(MemoServCore)
