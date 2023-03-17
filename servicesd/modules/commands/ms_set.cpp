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

class CommandMSSet : public Command {
  private:
    void DoNotify(CommandSource &source, const std::vector<Anope::string> &params,
                  MemoInfo *mi) {
        const Anope::string &param = params[1];
        NickCore *nc = source.nc;
        BotInfo *MemoServ = Config->GetClient("MemoServ");

        if (!MemoServ) {
            return;
        }

        if (param.equals_ci("ON")) {
            nc->Extend<bool>("MEMO_SIGNON");
            nc->Extend<bool>("MEMO_RECEIVE");
            source.Reply(
                _("%s will now notify you of memos when you log on and when they are sent to you."),
                MemoServ->nick.c_str());
        } else if (param.equals_ci("LOGON")) {
            nc->Extend<bool>("MEMO_SIGNON");
            nc->Shrink<bool>("MEMO_RECEIVE");
            source.Reply(
                _("%s will now notify you of memos when you log on or unset /AWAY."),
                MemoServ->nick.c_str());
        } else if (param.equals_ci("NEW")) {
            nc->Shrink<bool>("MEMO_SIGNON");
            nc->Extend<bool>("MEMO_RECEIVE");
            source.Reply(_("%s will now notify you of memos when they are sent to you."),
                         MemoServ->nick.c_str());
        } else if (param.equals_ci("MAIL")) {
            if (!nc->email.empty()) {
                nc->Extend<bool>("MEMO_MAIL");
                source.Reply(_("You will now be informed about new memos via email."));
            } else {
                source.Reply(_("There's no email address set for your nick."));
            }
        } else if (param.equals_ci("NOMAIL")) {
            nc->Shrink<bool>("MEMO_MAIL");
            source.Reply(_("You will no longer be informed via email."));
        } else if (param.equals_ci("OFF")) {
            nc->Shrink<bool>("MEMO_SIGNON");
            nc->Shrink<bool>("MEMO_RECEIVE");
            nc->Shrink<bool>("MEMO_MAIL");
            source.Reply(_("%s will not send you any notification of memos."),
                         MemoServ->nick.c_str());
        } else {
            this->OnSyntaxError(source, "");
        }
    }

    void DoLimit(CommandSource &source, const std::vector<Anope::string> &params,
                 MemoInfo *mi) {

        Anope::string p1 = params[1];
        Anope::string p2 = params.size() > 2 ? params[2] : "";
        Anope::string p3 = params.size() > 3 ? params[3] : "";
        Anope::string user, chan;
        int16_t limit;
        NickCore *nc = source.nc;
        ChannelInfo *ci = NULL;
        bool is_servadmin = source.HasPriv("memoserv/set-limit");

        if (p1[0] == '#') {
            chan = p1;
            p1 = p2;
            p2 = p3;
            p3 = params.size() > 4 ? params[4] : "";

            ci = ChannelInfo::Find(chan);
            if (!ci) {
                source.Reply(CHAN_X_NOT_REGISTERED, chan.c_str());
                return;
            } else if (!is_servadmin && !source.AccessFor(ci).HasPriv("MEMO")) {
                source.Reply(ACCESS_DENIED);
                return;
            }
            mi = &ci->memos;
        }
        if (is_servadmin) {
            if (!p2.empty() && !p2.equals_ci("HARD") && chan.empty()) {
                const NickAlias *na;
                if (!(na = NickAlias::Find(p1))) {
                    source.Reply(NICK_X_NOT_REGISTERED, p1.c_str());
                    return;
                }
                user = p1;
                mi = &na->nc->memos;
                nc = na->nc;
                p1 = p2;
                p2 = p3;
            } else if (p1.empty() || (!p1.is_pos_number_only() && !p1.equals_ci("NONE"))
                       || (!p2.empty() && !p2.equals_ci("HARD"))) {
                this->OnSyntaxError(source, "");
                return;
            }
            if (!chan.empty()) {
                if (!p2.empty()) {
                    ci->Extend<bool>("MEMO_HARDMAX");
                } else {
                    ci->Shrink<bool>("MEMO_HARDMAX");
                }
            } else {
                if (!p2.empty()) {
                    nc->Extend<bool>("MEMO_HARDMAX");
                } else {
                    nc->Shrink<bool>("MEMO_HARDMAX");
                }
            }
            limit = -1;
            try {
                limit = convertTo<int16_t>(p1);
            } catch (const ConvertException &) { }
        } else {
            if (p1.empty() || !p2.empty() || !isdigit(p1[0])) {
                this->OnSyntaxError(source, "");
                return;
            }
            if (!chan.empty() && ci->HasExt("MEMO_HARDMAX")) {
                source.Reply(_("The memo limit for %s may not be changed."), chan.c_str());
                return;
            } else if (chan.empty() && nc->HasExt("MEMO_HARDMAX")) {
                source.Reply(_("You are not permitted to change your memo limit."));
                return;
            }
            int max_memos = Config->GetModule("memoserv")->Get<int>("maxmemos");
            limit = -1;
            try {
                limit = convertTo<int16_t>(p1);
            } catch (const ConvertException &) { }
            /* The first character is a digit, but we could still go negative
             * from overflow... watch out! */
            if (limit < 0 || (max_memos > 0 && limit > max_memos)) {
                if (!chan.empty()) {
                    source.Reply(_("You cannot set the memo limit for %s higher than %d."),
                                 chan.c_str(), max_memos);
                } else {
                    source.Reply(_("You cannot set your memo limit higher than %d."), max_memos);
                }
                return;
            }
        }
        mi->memomax = limit;
        if (limit > 0) {
            if (chan.empty() && nc == source.nc) {
                source.Reply(_("Your memo limit has been set to \002%d\002."), limit);
            } else {
                source.Reply(_("Memo limit for %s set to \002%d\002."),
                             !chan.empty() ? chan.c_str() : user.c_str(), limit);
            }
        } else if (!limit) {
            if (chan.empty() && nc == source.nc) {
                source.Reply(_("You will no longer be able to receive memos."));
            } else {
                source.Reply(_("Memo limit for %s set to \0020\002."),
                             !chan.empty() ? chan.c_str() : user.c_str());
            }
        } else {
            if (chan.empty() && nc == source.nc) {
                source.Reply(_("Your memo limit has been disabled."));
            } else {
                source.Reply(_("Memo limit \002disabled\002 for %s."),
                             !chan.empty() ? chan.c_str() : user.c_str());
            }
        }
        return;
    }
  public:
    CommandMSSet(Module *creator) : Command(creator, "memoserv/set", 2, 5) {
        this->SetDesc(_("Set options related to memos"));
        this->SetSyntax(_("\037option\037 \037parameters\037"));
    }

    void Execute(CommandSource &source,
                 const std::vector<Anope::string> &params) anope_override {
        const Anope::string &cmd = params[0];
        MemoInfo *mi = &source.nc->memos;

        if (Anope::ReadOnly) {
            source.Reply(_("Sorry, memo option setting is temporarily disabled."));
        } else if (cmd.equals_ci("NOTIFY")) {
            return this->DoNotify(source, params, mi);
        } else if (cmd.equals_ci("LIMIT")) {
            return this->DoLimit(source, params, mi);
        } else {
            this->OnSyntaxError(source, "");
        }

        return;
    }

    bool OnHelp(CommandSource &source,
                const Anope::string &subcommand) anope_override {
        if (subcommand.empty()) {
            this->SendSyntax(source);
            source.Reply(" ");
            source.Reply(_("Sets various memo options.  \037option\037 can be one of:\n"
                           " \n"
                           "    NOTIFY      Changes when you will be notified about\n"
                           "                   new memos (only for nicknames)\n"
                           "    LIMIT       Sets the maximum number of memos you can\n"
                           "                   receive\n"
                           " \n"
                           "Type \002%s%s HELP %s \037option\037\002 for more information\n"
                           "on a specific option."), Config->StrictPrivmsg.c_str(),
                         source.service->nick.c_str(), source.command.c_str());
        } else if (subcommand.equals_ci("NOTIFY"))
            source.Reply(_("Syntax: \002NOTIFY {ON | LOGON | NEW | MAIL | NOMAIL | OFF}\002\n"
                           " \n"
                           "Changes when you will be notified about new memos:\n"
                           " \n"
                           "    ON      You will be notified of memos when you log on,\n"
                           "               when you unset /AWAY, and when they are sent\n"
                           "               to you.\n"
                           "    LOGON   You will only be notified of memos when you log\n"
                           "               on or when you unset /AWAY.\n"
                           "    NEW     You will only be notified of memos when they\n"
                           "               are sent to you.\n"
                           "    MAIL    You will be notified of memos by email as well as\n"
                           "               any other settings you have.\n"
                           "    NOMAIL  You will not be notified of memos by email.\n"
                           "    OFF     You will not receive any notification of memos.\n"
                           " \n"
                           "\002ON\002 is essentially \002LOGON\002 and \002NEW\002 combined."));
        else if (subcommand.equals_ci("LIMIT")) {
            int max_memos = Config->GetModule("memoserv")->Get<int>("maxmemos");
            if (source.IsServicesOper())
                source.Reply(
                    _("Syntax: \002LIMIT [\037user\037 | \037channel\037] {\037limit\037 | NONE} [HARD]\002\n"
                      " \n"
                      "Sets the maximum number of memos a user or channel is\n"
                      "allowed to have.  Setting the limit to 0 prevents the user\n"
                      "from receiving any memos; setting it to \002NONE\002 allows the\n"
                      "user to receive and keep as many memos as they want.  If\n"
                      "you do not give a nickname or channel, your own limit is\n"
                      "set.\n"
                      " \n"
                      "Adding \002HARD\002 prevents the user from changing the limit.  Not\n"
                      "adding \002HARD\002 has the opposite effect, allowing the user to\n"
                      "change the limit (even if a previous limit was set with\n"
                      "\002HARD\002).\n"
                      " \n"
                      "This use of the \002SET LIMIT\002 command is limited to \002Services\002\n"
                      "\002Operators\002.  Other users may only enter a limit for themselves\n"
                      "or a channel on which they have such privileges, may not\n"
                      "remove their limit, may not set a limit above %d, and may\n"
                      "not set a hard limit."), max_memos);
            else
                source.Reply(_("Syntax: \002LIMIT [\037channel\037] \037limit\037\002\n"
                               " \n"
                               "Sets the maximum number of memos you (or the given channel)\n"
                               "are allowed to have. If you set this to 0, no one will be\n"
                               "able to send any memos to you.  However, you cannot set\n"
                               "this any higher than %d."), max_memos);
        } else {
            return false;
        }

        return true;
    }
};

class MSSet : public Module {
    CommandMSSet commandmsset;
    SerializableExtensibleItem<bool> memo_signon, memo_receive, memo_mail,
                               memo_hardmax;

  public:
    MSSet(const Anope::string &modname,
          const Anope::string &creator) : Module(modname, creator, VENDOR),
        commandmsset(this), memo_signon(this, "MEMO_SIGNON"), memo_receive(this,
                "MEMO_RECEIVE"), memo_mail(this, "MEMO_MAIL"),
        memo_hardmax(this, "MEMO_HARDMAX") {

    }
};

MODULE_INIT(MSSet)
