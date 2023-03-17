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

class SXLineDelCallback : public NumberList {
    XLineManager *xlm;
    Command *command;
    CommandSource &source;
    unsigned deleted;
  public:
    SXLineDelCallback(XLineManager *x, Command *c, CommandSource &_source,
                      const Anope::string &numlist) : NumberList(numlist, true), xlm(x), command(c),
        source(_source), deleted(0) {
    }

    ~SXLineDelCallback() {
        if (!deleted) {
            source.Reply(_("No matching entries on the %s list."), source.command.c_str());
        } else if (deleted == 1) {
            source.Reply(_("Deleted 1 entry from the %s list."), source.command.c_str());
        } else {
            source.Reply(_("Deleted %d entries from the %s list."), deleted,
                         source.command.c_str());
        }
    }

    void HandleNumber(unsigned number) anope_override {
        if (!number) {
            return;
        }

        XLine *x = this->xlm->GetEntry(number - 1);

        if (!x) {
            return;
        }

        Log(LOG_ADMIN, source, command) << "to remove " << x->mask << " from the list";

        ++deleted;
        DoDel(this->xlm, source, x);
    }

    static void DoDel(XLineManager *xlm, CommandSource &source, XLine *x) {
        xlm->DelXLine(x);
    }
};

class CommandOSSXLineBase : public Command {
  private:
    virtual XLineManager* xlm() = 0;

    virtual void OnAdd(CommandSource &source,
                       const std::vector<Anope::string> &params) = 0;

    void OnDel(CommandSource &source, const std::vector<Anope::string> &params) {

        if (!this->xlm() || this->xlm()->GetList().empty()) {
            source.Reply(_("%s list is empty."), source.command.c_str());
            return;
        }

        const Anope::string &mask = params.size() > 1 ? params[1] : "";

        if (mask.empty()) {
            this->OnSyntaxError(source, "DEL");
            return;
        }

        if (isdigit(mask[0])
                && mask.find_first_not_of("1234567890,-") == Anope::string::npos) {
            SXLineDelCallback list(this->xlm(), this, source, mask);
            list.Process();
        } else {
            XLine *x = this->xlm()->HasEntry(mask);

            if (!x) {
                source.Reply(_("\002%s\002 not found on the %s list."), mask.c_str(),
                             source.command.c_str());
                return;
            }

            FOREACH_MOD(OnDelXLine, (source, x, this->xlm()));

            SXLineDelCallback::DoDel(this->xlm(), source, x);
            source.Reply(_("\002%s\002 deleted from the %s list."), mask.c_str(),
                         source.command.c_str());
            Log(LOG_ADMIN, source, this) << "to remove " << mask << " from the list";
        }

        if (Anope::ReadOnly) {
            source.Reply(READ_ONLY_MODE);
        }

        return;
    }

    void ProcessList(CommandSource &source,
                     const std::vector<Anope::string> &params, ListFormatter &list) {
        if (!this->xlm() || this->xlm()->GetList().empty()) {
            source.Reply(_("%s list is empty."), source.command.c_str());
            return;
        }

        const Anope::string &mask = params.size() > 1 ? params[1] : "";

        if (!mask.empty() && isdigit(mask[0])
                && mask.find_first_not_of("1234567890,-") == Anope::string::npos) {
            class SXLineListCallback : public NumberList {
                XLineManager *xlm;
                CommandSource &source;
                ListFormatter &list;
              public:
                SXLineListCallback(XLineManager *x, CommandSource &_source,
                                   ListFormatter &_list, const Anope::string &numlist) : NumberList(numlist,
                                               false), xlm(x), source(_source), list(_list) {
                }

                void HandleNumber(unsigned number) anope_override {
                    if (!number) {
                        return;
                    }

                    const XLine *x = this->xlm->GetEntry(number - 1);

                    if (!x) {
                        return;
                    }

                    ListFormatter::ListEntry entry;
                    entry["Number"] = stringify(number);
                    entry["Mask"] = x->mask;
                    entry["By"] = x->by;
                    entry["Created"] = Anope::strftime(x->created, NULL, true);
                    entry["Expires"] = Anope::Expires(x->expires, source.nc);
                    entry["ID"] = x->id;
                    entry["Reason"] = x->reason;
                    list.AddEntry(entry);
                }
            }
            sl_list(this->xlm(), source, list, mask);
            sl_list.Process();
        } else {
            for (unsigned i = 0, end = this->xlm()->GetCount(); i < end; ++i) {
                const XLine *x = this->xlm()->GetEntry(i);

                if (mask.empty() || mask.equals_ci(x->mask) || mask == x->id
                        || Anope::Match(x->mask, mask, false, true)) {
                    ListFormatter::ListEntry entry;
                    entry["Number"] = stringify(i + 1);
                    entry["Mask"] = x->mask;
                    entry["By"] = x->by;
                    entry["Created"] = Anope::strftime(x->created, NULL, true);
                    entry["Expires"] = Anope::Expires(x->expires, source.nc);
                    entry["ID"] = x->id;
                    entry["Reason"] = x->reason;
                    list.AddEntry(entry);
                }
            }
        }

        if (list.IsEmpty()) {
            source.Reply(_("No matching entries on the %s list."), source.command.c_str());
        } else {
            source.Reply(_("Current %s list:"), source.command.c_str());

            std::vector<Anope::string> replies;
            list.Process(replies);

            for (unsigned i = 0; i < replies.size(); ++i) {
                source.Reply(replies[i]);
            }
        }
    }

    void OnList(CommandSource &source, const std::vector<Anope::string> &params) {
        ListFormatter list(source.GetAccount());
        list.AddColumn(_("Number")).AddColumn(_("Mask")).AddColumn(_("Reason"));

        this->ProcessList(source, params, list);
    }

    void OnView(CommandSource &source, const std::vector<Anope::string> &params) {
        ListFormatter list(source.GetAccount());
        list.AddColumn(_("Number")).AddColumn(_("Mask")).AddColumn(_("By")).AddColumn(
            _("Created")).AddColumn(_("Expires"));
        if (Config->GetModule("operserv")->Get<bool>("akillids")) {
            list.AddColumn(_("ID"));
        }
        list.AddColumn(_("Reason"));

        this->ProcessList(source, params, list);
    }

    void OnClear(CommandSource &source) {
        FOREACH_MOD(OnDelXLine, (source, NULL, this->xlm()));

        for (unsigned i = this->xlm()->GetCount(); i > 0; --i) {
            XLine *x = this->xlm()->GetEntry(i - 1);
            this->xlm()->DelXLine(x);
        }

        Log(LOG_ADMIN, source, this) << "to CLEAR the list";
        source.Reply(_("The %s list has been cleared."), source.command.c_str());
        if (Anope::ReadOnly) {
            source.Reply(READ_ONLY_MODE);
        }

        return;
    }
  public:
    CommandOSSXLineBase(Module *creator,
                        const Anope::string &cmd) : Command(creator, cmd, 1, 4) {
    }

    const Anope::string GetDesc(CommandSource &source) const anope_override {
        return Anope::printf(Language::Translate(source.GetAccount(),
                             _("Manipulate the %s list")), source.command.upper().c_str());
    }

    void Execute(CommandSource &source,
                 const std::vector<Anope::string> &params) anope_override {
        const Anope::string &cmd = params[0];

        if (cmd.equals_ci("ADD")) {
            return this->OnAdd(source, params);
        } else if (cmd.equals_ci("DEL")) {
            return this->OnDel(source, params);
        } else if (cmd.equals_ci("LIST")) {
            return this->OnList(source, params);
        } else if (cmd.equals_ci("VIEW")) {
            return this->OnView(source, params);
        } else if (cmd.equals_ci("CLEAR")) {
            return this->OnClear(source);
        } else {
            this->OnSyntaxError(source, "");
        }

        return;
    }

    virtual bool OnHelp(CommandSource &source,
                        const Anope::string &subcommand) anope_override = 0;
};

class CommandOSSNLine : public CommandOSSXLineBase {
    XLineManager *xlm() anope_override {
        return this->snlines;
    }

    void OnAdd(CommandSource &source,
               const std::vector<Anope::string> &params) anope_override {
        if (!this->xlm()) {
            return;
        }

        unsigned last_param = 2;
        Anope::string param, expiry;

        param = params.size() > 1 ? params[1] : "";
        if (!param.empty() && param[0] == '+') {
            expiry = param;
            param = params.size() > 2 ? params[2] : "";
            last_param = 3;
        }

        time_t expires = !expiry.empty() ? Anope::DoTime(expiry) : Config->GetModule("operserv")->Get<time_t>("snlineexpiry", "30d");
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

        if (param.empty()) {
            this->OnSyntaxError(source, "ADD");
            return;
        }

        Anope::string rest = param;
        if (params.size() > last_param) {
            rest += " " + params[last_param];
        }

        if (rest.find(':') == Anope::string::npos) {
            this->OnSyntaxError(source, "ADD");
            return;
        }

        sepstream sep(rest, ':');
        Anope::string mask;
        sep.GetToken(mask);
        Anope::string reason = sep.GetRemaining();

        if (mask.empty() || reason.empty()) {
            this->OnSyntaxError(source, "ADD");
            return;
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

        /* Clean up the last character of the mask if it is a space
         * See bug #761
         */
        unsigned masklen = mask.length();
        if (mask[masklen - 1] == ' ') {
            mask.erase(masklen - 1);
        }

        if (Config->GetModule("operserv")->Get<bool>("addakiller", "yes") && !source.GetNick().empty()) {
            reason = "[" + source.GetNick() + "] " + reason;
        }

        if (mask.find_first_not_of("/.*?") == Anope::string::npos) {
            source.Reply(USERHOST_MASK_TOO_WIDE, mask.c_str());
            return;
        }

        XLine *x = new XLine(mask, source.GetNick(), expires, reason);
        if (Config->GetModule("operserv")->Get<bool>("akillids")) {
            x->id = XLineManager::GenerateUID();
        }

        unsigned int affected = 0;
        for (user_map::const_iterator it = UserListByNick.begin(); it != UserListByNick.end(); ++it)
            if (this->xlm()->Check(it->second, x)) {
                ++affected;
            }
        float percent = static_cast<float>(affected) / static_cast<float>(UserListByNick.size()) * 100.0;

        if (percent > 95) {
            source.Reply(USERHOST_MASK_TOO_WIDE, mask.c_str());
            Log(LOG_ADMIN, source, this) << "tried to " << source.command << " " << percent
                                         << "% of the network (" << affected << " users)";
            delete x;
            return;
        }

        if (!this->xlm()->CanAdd(source, mask, expires, reason)) {
            return;
        }

        EventReturn MOD_RESULT;
        FOREACH_RESULT(OnAddXLine, MOD_RESULT, (source, x, this->xlm()));
        if (MOD_RESULT == EVENT_STOP) {
            delete x;
            return;
        }

        this->xlm()->AddXLine(x);

        if (Config->GetModule("operserv")->Get<bool>("killonsnline", "yes")) {
            Anope::string rreason = "G-Lined: " + reason;

            for (user_map::const_iterator it = UserListByNick.begin();
                    it != UserListByNick.end(); ++it) {
                User *user = it->second;

                if (!user->HasMode("OPER") && user->server != Me
                        && this->xlm()->Check(user, x)) {
                    user->Kill(Me, rreason);
                }
            }

            this->xlm()->Send(NULL, x);
        }

        source.Reply(_("\002%s\002 added to the %s list."), mask.c_str(), source.command.c_str());
        Log(LOG_ADMIN, source, this) << "on " << mask << " (" << reason << "), expires in " << (expires ? Anope::Duration(expires - Anope::CurTime) : "never") << " [affects " << affected << " user(s) (" << percent << "%)]";
        if (Anope::ReadOnly) {
            source.Reply(READ_ONLY_MODE);
        }
    }

    ServiceReference<XLineManager> snlines;
  public:
    CommandOSSNLine(Module *creator) : CommandOSSXLineBase(creator,
                "operserv/snline"), snlines("XLineManager", "xlinemanager/snline") {
        this->SetSyntax(_("ADD [+\037expiry\037] \037mask\037:\037reason\037"));
        this->SetSyntax(
            _("DEL {\037mask\037 | \037entry-num\037 | \037list\037 | \037id\037}"));
        this->SetSyntax(_("LIST [\037mask\037 | \037list\037 | \037id\037]"));
        this->SetSyntax(_("VIEW [\037mask\037 | \037list\037 | \037id\037]"));
        this->SetSyntax("CLEAR");
    }

    bool OnHelp(CommandSource &source,
                const Anope::string &subcommand) anope_override {
        this->SendSyntax(source);
        source.Reply(" ");
        source.Reply(_("Allows Services Operators to manipulate the SNLINE list.  If\n"
                       "a user with a realname matching an SNLINE mask attempts to\n"
                       "connect, Services will not allow it to pursue his IRC\n"
                       "session."));
        source.Reply(_(" \n"
                       "\002SNLINE ADD\002 adds the given realname mask to the SNLINE\n"
                       "list for the given reason (which \002must\002 be given).\n"
                       "\037expiry\037 is specified as an integer followed by one of \037d\037\n"
                       "(days), \037h\037 (hours), or \037m\037 (minutes). Combinations (such as\n"
                       "\0371h30m\037) are not permitted. If a unit specifier is not\n"
                       "included, the default is days (so \037+30\037 by itself means 30\n"
                       "days). To add an SNLINE which does not expire, use \037+0\037.  If the\n"
                       "realname mask to be added starts with a \037+\037, an expiry time must\n"
                       "be given, even if it is the same as the default.  The\n"
                       "current SNLINE default expiry time can be found with the\n"
                       "\002STATS AKILL\002 command.\n"
                       " \n"
                       "\002Note\002: because the realname mask may contain spaces, the\n"
                       "separator between it and the reason is a colon."));
        const Anope::string &regexengine = Config->GetBlock("options")->Get<const Anope::string>("regexengine");
        if (!regexengine.empty()) {
            source.Reply(" ");
            source.Reply(_("Regex matches are also supported using the %s engine.\n"
                           "Enclose your mask in // if this is desired."), regexengine.c_str());
        }
        source.Reply(_(" \n"
                       "The \002SNLINE DEL\002 command removes the given mask from the\n"
                       "SNLINE list if it is present.  If a list of entry numbers is\n"
                       "given, those entries are deleted.  (See the example for LIST\n"
                       "below.)\n"
                       " \n"
                       "The \002SNLINE LIST\002 command displays the SNLINE list.\n"
                       "If a wildcard mask is given, only those entries matching the\n"
                       "mask are displayed.  If a list of entry numbers is given,\n"
                       "only those entries are shown; for example:\n"
                       "   \002SNLINE LIST 2-5,7-9\002\n"
                       "      Lists SNLINE entries numbered 2 through 5 and 7\n"
                       "      through 9.\n"
                       " \n"
                       "\002SNLINE VIEW\002 is a more verbose version of \002SNLINE LIST\002, and\n"
                       "will show who added an SNLINE, the date it was added, and when\n"
                       "it expires, as well as the realname mask and reason.\n"
                       " \n"
                       "\002SNLINE CLEAR\002 clears all entries of the SNLINE list."));
        return true;
    }
};

class CommandOSSQLine : public CommandOSSXLineBase {
    XLineManager *xlm() anope_override {
        return this->sqlines;
    }

    void OnAdd(CommandSource &source,
               const std::vector<Anope::string> &params) anope_override {
        if (!this->xlm()) {
            return;
        }

        unsigned last_param = 2;
        Anope::string expiry, mask;

        mask = params.size() > 1 ? params[1] : "";
        if (!mask.empty() && mask[0] == '+') {
            expiry = mask;
            mask = params.size() > 2 ? params[2] : "";
            last_param = 3;
        }

        time_t expires = !expiry.empty() ? Anope::DoTime(expiry) : Config->GetModule("operserv")->Get<time_t>("sqlineexpiry", "30d");
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

        if (params.size() <= last_param) {
            this->OnSyntaxError(source, "ADD");
            return;
        }

        Anope::string reason = params[last_param];
        if (last_param == 2 && params.size() > 3) {
            reason += " " + params[3];
        }

        if (mask.empty() || reason.empty()) {
            this->OnSyntaxError(source, "ADD");
            return;
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

        if (Config->GetModule("operserv")->Get<bool>("addakiller", "yes") && !source.GetNick().empty()) {
            reason = "[" + source.GetNick() + "] " + reason;
        }

        if (mask.find_first_not_of("./?*") == Anope::string::npos) {
            source.Reply(USERHOST_MASK_TOO_WIDE, mask.c_str());
            return;
        }

        XLine *x = new XLine(mask, source.GetNick(), expires, reason);
        if (Config->GetModule("operserv")->Get<bool>("akillids")) {
            x->id = XLineManager::GenerateUID();
        }

        unsigned int affected = 0;
        for (user_map::const_iterator it = UserListByNick.begin(); it != UserListByNick.end(); ++it)
            if (this->xlm()->Check(it->second, x)) {
                ++affected;
            }
        float percent = static_cast<float>(affected) / static_cast<float>(UserListByNick.size()) * 100.0;

        if (percent > 95) {
            source.Reply(USERHOST_MASK_TOO_WIDE, mask.c_str());
            Log(LOG_ADMIN, source, this) << "tried to SQLine " << percent <<
                                         "% of the network (" << affected << " users)";
            delete x;
            return;
        }

        if (!this->sqlines->CanAdd(source, mask, expires, reason)) {
            return;
        }

        EventReturn MOD_RESULT;
        FOREACH_RESULT(OnAddXLine, MOD_RESULT, (source, x, this->xlm()));
        if (MOD_RESULT == EVENT_STOP) {
            delete x;
            return;
        }

        this->xlm()->AddXLine(x);

        if (Config->GetModule("operserv")->Get<bool>("killonsqline", "yes")) {
            Anope::string rreason = "Q-Lined: " + reason;

            if (mask[0] == '#') {
                for (channel_map::const_iterator cit = ChannelList.begin(),
                        cit_end = ChannelList.end(); cit != cit_end; ++cit) {
                    Channel *c = cit->second;

                    if (!Anope::Match(c->name, mask, false, true)) {
                        continue;
                    }

                    std::vector<User *> users;
                    for (Channel::ChanUserList::iterator it = c->users.begin(),
                            it_end = c->users.end(); it != it_end; ++it) {
                        ChanUserContainer *uc = it->second;
                        User *user = uc->user;

                        if (!user->HasMode("OPER") && user->server != Me) {
                            users.push_back(user);
                        }
                    }

                    for (unsigned i = 0; i < users.size(); ++i) {
                        c->Kick(NULL, users[i], "%s", reason.c_str());
                    }
                }
            } else {
                for (user_map::const_iterator it = UserListByNick.begin();
                        it != UserListByNick.end(); ++it) {
                    User *user = it->second;

                    if (!user->HasMode("OPER") && user->server != Me
                            && this->xlm()->Check(user, x)) {
                        user->Kill(Me, rreason);
                    }
                }
            }

            this->xlm()->Send(NULL, x);
        }

        source.Reply(_("\002%s\002 added to the %s list."), mask.c_str(), source.command.c_str());
        Log(LOG_ADMIN, source, this) << "on " << mask << " (" << reason << "), expires in " << (expires ? Anope::Duration(expires - Anope::CurTime) : "never") << " [affects " << affected << " user(s) (" << percent << "%)]";
        if (Anope::ReadOnly) {
            source.Reply(READ_ONLY_MODE);
        }
    }

    ServiceReference<XLineManager> sqlines;
  public:
    CommandOSSQLine(Module *creator) : CommandOSSXLineBase(creator,
                "operserv/sqline"), sqlines("XLineManager", "xlinemanager/sqline") {
        this->SetSyntax(_("ADD [+\037expiry\037] \037mask\037 \037reason\037"));
        this->SetSyntax(
            _("DEL {\037mask\037 | \037entry-num\037 | \037list\037 | \037id\037}"));
        this->SetSyntax(_("LIST [\037mask\037 | \037list\037 | \037id\037]"));
        this->SetSyntax(_("VIEW [\037mask\037 | \037list\037 | \037id\037]"));
        this->SetSyntax("CLEAR");
    }

    bool OnHelp(CommandSource &source,
                const Anope::string &subcommand) anope_override {
        this->SendSyntax(source);
        source.Reply(" ");
        source.Reply(_("Allows Services Operators to manipulate the SQLINE list.  If\n"
                       "a user with a nick matching an SQLINE mask attempts to\n"
                       "connect, Services will not allow it to pursue his IRC\n"
                       "session.\n"
                       "If the first character of the mask is #, services will\n"
                       "prevent the use of matching channels. If the mask is a\n"
                       "regular expression, the expression will be matched against\n"
                       "channels too."));
        source.Reply(_(" \n"
                       "\002SQLINE ADD\002 adds the given (nick's) mask to the SQLINE\n"
                       "list for the given reason (which \002must\002 be given).\n"
                       "\037expiry\037 is specified as an integer followed by one of \037d\037\n"
                       "(days), \037h\037 (hours), or \037m\037 (minutes). Combinations (such as\n"
                       "\0371h30m\037) are not permitted. If a unit specifier is not\n"
                       "included, the default is days (so \037+30\037 by itself means 30\n"
                       "days). To add an SQLINE which does not expire, use \037+0\037.\n"
                       "If the mask to be added starts with a \037+\037, an expiry time\n"
                       "must be given, even if it is the same as the default. The\n"
                       "current SQLINE default expiry time can be found with the\n"
                       "\002STATS AKILL\002 command."));
        const Anope::string &regexengine = Config->GetBlock("options")->Get<const Anope::string>("regexengine");
        if (!regexengine.empty()) {
            source.Reply(" ");
            source.Reply(_("Regex matches are also supported using the %s engine.\n"
                           "Enclose your mask in // if this is desired."), regexengine.c_str());
        }
        source.Reply(_(" \n"
                       "The \002SQLINE DEL\002 command removes the given mask from the\n"
                       "SQLINE list if it is present. If a list of entry numbers is\n"
                       "given, those entries are deleted. (See the example for LIST\n"
                       "below.)\n"
                       " \n"
                       "The \002SQLINE LIST\002 command displays the SQLINE list.\n"
                       "If a wildcard mask is given, only those entries matching the\n"
                       "mask are displayed. If a list of entry numbers is given,\n"
                       "only those entries are shown; for example:\n"
                       "   \002SQLINE LIST 2-5,7-9\002\n"
                       "      Lists SQLINE entries numbered 2 through 5 and 7\n"
                       "      through 9.\n"
                       " \n"
                       "\002SQLINE VIEW\002 is a more verbose version of \002SQLINE LIST\002, and\n"
                       "will show who added an SQLINE, the date it was added, and when\n"
                       "it expires, as well as the mask and reason.\n"
                       " \n"
                       "\002SQLINE CLEAR\002 clears all entries of the SQLINE list."));
        return true;
    }
};

class OSSXLine : public Module {
    CommandOSSNLine commandossnline;
    CommandOSSQLine commandossqline;

  public:
    OSSXLine(const Anope::string &modname,
             const Anope::string &creator) : Module(modname, creator, VENDOR),
        commandossnline(this), commandossqline(this) {
    }
};

MODULE_INIT(OSSXLine)
