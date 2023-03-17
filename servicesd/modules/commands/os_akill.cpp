/* OperServ core functions
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

static ServiceReference<XLineManager> akills("XLineManager",
        "xlinemanager/sgline");

class AkillDelCallback : public NumberList {
    CommandSource &source;
    unsigned deleted;
    Command *cmd;
  public:
    AkillDelCallback(CommandSource &_source, const Anope::string &numlist,
                     Command *c) : NumberList(numlist, true), source(_source), deleted(0), cmd(c) {
    }

    ~AkillDelCallback() {
        if (!deleted) {
            source.Reply(_("No matching entries on the AKILL list."));
        } else if (deleted == 1) {
            source.Reply(_("Deleted 1 entry from the AKILL list."));
        } else {
            source.Reply(_("Deleted %d entries from the AKILL list."), deleted);
        }
    }

    void HandleNumber(unsigned number) anope_override {
        if (!number) {
            return;
        }

        XLine *x = akills->GetEntry(number - 1);

        if (!x) {
            return;
        }

        Log(LOG_ADMIN, source, cmd) << "to remove " << x->mask << " from the list";

        ++deleted;
        DoDel(source, x);
    }

    static void DoDel(CommandSource &source, XLine *x) {
        akills->DelXLine(x);
    }
};

class CommandOSAKill : public Command {
  private:
    void DoAdd(CommandSource &source, const std::vector<Anope::string> &params) {
        Anope::string expiry, mask;

        if (params.size() < 2) {
            this->OnSyntaxError(source, "ADD");
            return;
        }

        spacesepstream sep(params[1]);
        sep.GetToken(mask);

        if (mask[0] == '+') {
            expiry = mask;
            sep.GetToken(mask);
        }

        time_t expires = !expiry.empty() ? Anope::DoTime(expiry) :
                         Config->GetModule("operserv")->Get<time_t>("autokillexpiry", "30d");
        /* If the expiry given does not contain a final letter, it's in days,
         * said the doc. Ah well.
         */
        if (!expiry.empty() && isdigit(expiry[expiry.length() - 1])) {
            expires *= 86400;
        }
        /* Do not allow less than a minute expiry time */
        if (expires && expires < 60) {
            source.Reply(BAD_EXPIRY_TIME);
            return;
        } else if (expires > 0) {
            expires += Anope::CurTime;
        }

        if (sep.StreamEnd()) {
            this->OnSyntaxError(source, "ADD");
            return;
        }

        Anope::string reason;
        if (mask.find('#') != Anope::string::npos) {
            Anope::string remaining = sep.GetRemaining();

            size_t co = remaining[0] == ':' ? 0 : remaining.rfind(" :");
            if (co == Anope::string::npos) {
                this->OnSyntaxError(source, "ADD");
                return;
            }

            if (co != 0) {
                ++co;
            }

            reason = remaining.substr(co + 1);
            mask += " " + remaining.substr(0, co);
            mask.trim();
        } else {
            reason = sep.GetRemaining();
        }

        if (mask[0] == '/' && mask[mask.length() - 1] == '/') {
            const Anope::string &regexengine =
                Config->GetBlock("options")->Get<const Anope::string>("regexengine");

            if (regexengine.empty()) {
                source.Reply(_("Regex is disabled."));
                return;
            }

            ServiceReference<RegexProvider> provider("Regex", regexengine);
            if (!provider) {
                source.Reply(_("Unable to find regex engine %s."), regexengine.c_str());
                return;
            }

            try {
                Anope::string stripped_mask = mask.substr(1, mask.length() - 2);
                delete provider->Compile(stripped_mask);
            } catch (const RegexException &ex) {
                source.Reply("%s", ex.GetReason().c_str());
                return;
            }
        }

        User *targ = User::Find(mask, true);
        if (targ) {
            mask = "*@" + targ->host;
        }

        if (Config->GetModule("operserv")->Get<bool>("addakiller", "yes")
                && !source.GetNick().empty()) {
            reason = "[" + source.GetNick() + "] " + reason;
        }

        if (mask.find_first_not_of("/~@.*?") == Anope::string::npos) {
            source.Reply(USERHOST_MASK_TOO_WIDE, mask.c_str());
            return;
        } else if (mask.find('@') == Anope::string::npos) {
            source.Reply(BAD_USERHOST_MASK);
            return;
        }

        XLine *x = new XLine(mask, source.GetNick(), expires, reason);
        if (Config->GetModule("operserv")->Get<bool>("akillids")) {
            x->id = XLineManager::GenerateUID();
        }

        unsigned int affected = 0;
        for (user_map::const_iterator it = UserListByNick.begin();
                it != UserListByNick.end(); ++it)
            if (akills->Check(it->second, x)) {
                ++affected;
            }
        float percent = static_cast<float>(affected) / static_cast<float>
                        (UserListByNick.size()) * 100.0;

        if (percent > 95) {
            source.Reply(USERHOST_MASK_TOO_WIDE, mask.c_str());
            Log(LOG_ADMIN, source, this) << "tried to akill " << percent <<
                                         "% of the network (" << affected << " users)";
            delete x;
            return;
        }

        if (!akills->CanAdd(source, mask, expires, reason)) {
            return;
        }

        EventReturn MOD_RESULT;
        FOREACH_RESULT(OnAddXLine, MOD_RESULT, (source, x, akills));
        if (MOD_RESULT == EVENT_STOP) {
            delete x;
            return;
        }

        akills->AddXLine(x);
        if (Config->GetModule("operserv")->Get<bool>("akillonadd")) {
            akills->Send(NULL, x);
        }

        source.Reply(_("\002%s\002 added to the AKILL list."), mask.c_str());

        Log(LOG_ADMIN, source, this) << "on " << mask << " (" << x->reason <<
                                     "), expires in " << (expires ? Anope::Duration(expires - Anope::CurTime) :
                                             "never") << " [affects " << affected << " user(s) (" << percent << "%)]";
        if (Anope::ReadOnly) {
            source.Reply(READ_ONLY_MODE);
        }
    }

    void DoDel(CommandSource &source, const std::vector<Anope::string> &params) {
        const Anope::string &mask = params.size() > 1 ? params[1] : "";

        if (mask.empty()) {
            this->OnSyntaxError(source, "DEL");
            return;
        }

        if (akills->GetList().empty()) {
            source.Reply(_("AKILL list is empty."));
            return;
        }

        if (isdigit(mask[0])
                && mask.find_first_not_of("1234567890,-") == Anope::string::npos) {
            AkillDelCallback list(source, mask, this);
            list.Process();
        } else {
            XLine *x = akills->HasEntry(mask);

            if (!x) {
                source.Reply(_("\002%s\002 not found on the AKILL list."), mask.c_str());
                return;
            }

            do {
                FOREACH_MOD(OnDelXLine, (source, x, akills));

                Log(LOG_ADMIN, source, this) << "to remove " << x->mask << " from the list";
                source.Reply(_("\002%s\002 deleted from the AKILL list."), x->mask.c_str());
                AkillDelCallback::DoDel(source, x);
            } while ((x = akills->HasEntry(mask)));

        }

        if (Anope::ReadOnly) {
            source.Reply(READ_ONLY_MODE);
        }

        return;
    }

    void ProcessList(CommandSource &source,
                     const std::vector<Anope::string> &params, ListFormatter &list) {
        const Anope::string &mask = params.size() > 1 ? params[1] : "";

        if (!mask.empty() && isdigit(mask[0])
                && mask.find_first_not_of("1234567890,-") == Anope::string::npos) {
            class ListCallback : public NumberList {
                CommandSource &source;
                ListFormatter &list;
              public:
                ListCallback(CommandSource &_source, ListFormatter &_list,
                             const Anope::string &numstr) : NumberList(numstr, false), source(_source),
                    list(_list) {
                }

                void HandleNumber(unsigned number) anope_override {
                    if (!number) {
                        return;
                    }

                    const XLine *x = akills->GetEntry(number - 1);

                    if (!x) {
                        return;
                    }

                    ListFormatter::ListEntry entry;
                    entry["Number"] = stringify(number);
                    entry["Mask"] = x->mask;
                    entry["Creator"] = x->by;
                    entry["Created"] = Anope::strftime(x->created, NULL, true);
                    entry["Expires"] = Anope::Expires(x->expires, source.nc);
                    entry["ID"] = x->id;
                    entry["Reason"] = x->reason;
                    this->list.AddEntry(entry);
                }
            }
            nl_list(source, list, mask);
            nl_list.Process();
        } else {
            for (unsigned i = 0, end = akills->GetCount(); i < end; ++i) {
                const XLine *x = akills->GetEntry(i);

                if (mask.empty() || mask.equals_ci(x->mask) || mask == x->id
                        || Anope::Match(x->mask, mask, false, true)) {
                    ListFormatter::ListEntry entry;
                    entry["Number"] = stringify(i + 1);
                    entry["Mask"] = x->mask;
                    entry["Creator"] = x->by;
                    entry["Created"] = Anope::strftime(x->created, NULL, true);
                    entry["Expires"] = Anope::Expires(x->expires, source.nc);
                    entry["ID"] = x->id;
                    entry["Reason"] = x->reason;
                    list.AddEntry(entry);
                }
            }
        }

        if (list.IsEmpty()) {
            source.Reply(_("No matching entries on the AKILL list."));
        } else {
            source.Reply(_("Current AKILL list:"));

            std::vector<Anope::string> replies;
            list.Process(replies);

            for (unsigned i = 0; i < replies.size(); ++i) {
                source.Reply(replies[i]);
            }

            source.Reply(_("End of AKILL list."));
        }
    }

    void DoList(CommandSource &source, const std::vector<Anope::string> &params) {
        if (akills->GetList().empty()) {
            source.Reply(_("AKILL list is empty."));
            return;
        }

        ListFormatter list(source.GetAccount());
        list.AddColumn(_("Number")).AddColumn(_("Mask")).AddColumn(_("Reason"));

        this->ProcessList(source, params, list);
    }

    void DoView(CommandSource &source, const std::vector<Anope::string> &params) {
        if (akills->GetList().empty()) {
            source.Reply(_("AKILL list is empty."));
            return;
        }

        ListFormatter list(source.GetAccount());
        list.AddColumn(_("Number")).AddColumn(_("Mask")).AddColumn(
            _("Creator")).AddColumn(_("Created")).AddColumn(_("Expires"));
        if (Config->GetModule("operserv")->Get<bool>("akillids")) {
            list.AddColumn(_("ID"));
        }
        list.AddColumn(_("Reason"));

        this->ProcessList(source, params, list);
    }

    void DoClear(CommandSource &source) {

        for (unsigned i = akills->GetCount(); i > 0; --i) {
            XLine *x = akills->GetEntry(i - 1);
            FOREACH_MOD(OnDelXLine, (source, x, akills));
            akills->DelXLine(x);
        }

        Log(LOG_ADMIN, source, this) << "to CLEAR the list";
        source.Reply(_("The AKILL list has been cleared."));

        if (Anope::ReadOnly) {
            source.Reply(READ_ONLY_MODE);
        }
    }
  public:
    CommandOSAKill(Module *creator) : Command(creator, "operserv/akill", 1, 2) {
        this->SetDesc(_("Manipulate the AKILL list"));
        this->SetSyntax(_("ADD [+\037expiry\037] \037mask\037 \037reason\037"));
        this->SetSyntax(
            _("DEL {\037mask\037 | \037entry-num\037 | \037list\037 | \037id\037}"));
        this->SetSyntax(_("LIST [\037mask\037 | \037list\037 | \037id\037]"));
        this->SetSyntax(_("VIEW [\037mask\037 | \037list\037 | \037id\037]"));
        this->SetSyntax("CLEAR");
    }

    void Execute(CommandSource &source,
                 const std::vector<Anope::string> &params) anope_override {
        const Anope::string &cmd = params[0];

        if (!akills) {
            return;
        }

        if (cmd.equals_ci("ADD")) {
            return this->DoAdd(source, params);
        } else if (cmd.equals_ci("DEL")) {
            return this->DoDel(source, params);
        } else if (cmd.equals_ci("LIST")) {
            return this->DoList(source, params);
        } else if (cmd.equals_ci("VIEW")) {
            return this->DoView(source, params);
        } else if (cmd.equals_ci("CLEAR")) {
            return this->DoClear(source);
        } else {
            this->OnSyntaxError(source, "");
        }

        return;
    }

    bool OnHelp(CommandSource &source,
                const Anope::string &subcommand) anope_override {
        this->SendSyntax(source);
        source.Reply(" ");
        source.Reply(_("Allows Services Operators to manipulate the AKILL list. If\n"
                       "a user matching an AKILL mask attempts to connect, Services\n"
                       "will issue a KILL for that user and, on supported server\n"
                       "types, will instruct all servers to add a ban for the mask\n"
                       "which the user matched.\n"
                       " \n"
                       "\002AKILL ADD\002 adds the given mask to the AKILL\n"
                       "list for the given reason, which \002must\002 be given.\n"
                       "Mask should be in the format of nick!user@host#real name,\n"
                       "though all that is required is user@host. If a real name is specified,\n"
                       "the reason must be prepended with a :.\n"
                       "\037expiry\037 is specified as an integer followed by one of \037d\037\n"
                       "(days), \037h\037 (hours), or \037m\037 (minutes). Combinations (such as\n"
                       "\0371h30m\037) are not permitted. If a unit specifier is not\n"
                       "included, the default is days (so \037+30\037 by itself means 30\n"
                       "days). To add an AKILL which does not expire, use \037+0\037. If the\n"
                       "usermask to be added starts with a \037+\037, an expiry time must\n"
                       "be given, even if it is the same as the default. The\n"
                       "current AKILL default expiry time can be found with the\n"
                       "\002STATS AKILL\002 command."));
        const Anope::string &regexengine = Config->GetBlock("options")->Get<const Anope::string>("regexengine");
        if (!regexengine.empty()) {
            source.Reply(" ");
            source.Reply(_("Regex matches are also supported using the %s engine.\n"
                           "Enclose your mask in // if this is desired."), regexengine.c_str());
        }
        source.Reply(_(
                         " \n"
                         "The \002AKILL DEL\002 command removes the given mask from the\n"
                         "AKILL list if it is present.  If a list of entry numbers is\n"
                         "given, those entries are deleted.  (See the example for LIST\n"
                         "below.)\n"
                         " \n"
                         "The \002AKILL LIST\002 command displays the AKILL list.\n"
                         "If a wildcard mask is given, only those entries matching the\n"
                         "mask are displayed.  If a list of entry numbers is given,\n"
                         "only those entries are shown; for example:\n"
                         "   \002AKILL LIST 2-5,7-9\002\n"
                         "      Lists AKILL entries numbered 2 through 5 and 7\n"
                         "      through 9.\n"
                         "      \n"
                         "\002AKILL VIEW\002 is a more verbose version of \002AKILL LIST\002, and\n"
                         "will show who added an AKILL, the date it was added, and when\n"
                         "it expires, as well as the user@host/ip mask and reason.\n"
                         " \n"
                         "\002AKILL CLEAR\002 clears all entries of the AKILL list."));
        return true;
    }
};

class OSAKill : public Module {
    CommandOSAKill commandosakill;

  public:
    OSAKill(const Anope::string &modname,
            const Anope::string &creator) : Module(modname, creator, VENDOR),
        commandosakill(this) {

    }
};

MODULE_INIT(OSAKill)
