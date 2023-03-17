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
#include "modules/os_session.h"

namespace {
/* The default session limit */
unsigned session_limit;
/* How many times to kill before adding an AKILL */
unsigned max_session_kill;
/* How long session akills should last */
time_t session_autokill_expiry;
/* Reason to use for session kills */
Anope::string sle_reason;
/* Optional second reason */
Anope::string sle_detailsloc;

/* Max limit that can be used for exceptions */
unsigned max_exception_limit;
/* How long before exceptions expire by default */
time_t exception_expiry;

/* Number of bits to use when comparing session IPs */
unsigned ipv4_cidr;
unsigned ipv6_cidr;
}

class MySessionService : public SessionService {
    SessionMap Sessions;
    Serialize::Checker<ExceptionVector> Exceptions;
  public:
    MySessionService(Module *m) : SessionService(m), Exceptions("Exception") { }

    Exception *CreateException() anope_override {
        return new Exception();
    }

    void AddException(Exception *e) anope_override {
        this->Exceptions->push_back(e);
    }

    void DelException(Exception *e) anope_override {
        ExceptionVector::iterator it = std::find(this->Exceptions->begin(), this->Exceptions->end(), e);
        if (it != this->Exceptions->end()) {
            this->Exceptions->erase(it);
        }
    }

    Exception *FindException(User *u) anope_override {
        for (std::vector<Exception *>::const_iterator it = this->Exceptions->begin(), it_end = this->Exceptions->end(); it != it_end; ++it) {
            Exception *e = *it;
            if (Anope::Match(u->host, e->mask) || Anope::Match(u->ip.addr(), e->mask)) {
                return e;
            }

            if (cidr(e->mask).match(u->ip)) {
                return e;
            }
        }
        return NULL;
    }

    Exception *FindException(const Anope::string &host) anope_override {
        for (std::vector<Exception *>::const_iterator it = this->Exceptions->begin(), it_end = this->Exceptions->end(); it != it_end; ++it) {
            Exception *e = *it;
            if (Anope::Match(host, e->mask)) {
                return e;
            }

            if (cidr(e->mask).match(sockaddrs(host))) {
                return e;
            }
        }

        return NULL;
    }

    ExceptionVector &GetExceptions() anope_override {
        return this->Exceptions;
    }

    void DelSession(Session *s) {
        this->Sessions.erase(s->addr);
    }

    Session *FindSession(const Anope::string &ip) anope_override {
        cidr c(ip, ip.find(':') != Anope::string::npos ? ipv6_cidr : ipv4_cidr);
        if (!c.valid()) {
            return NULL;
        }
        SessionMap::iterator it = this->Sessions.find(c);
        if (it != this->Sessions.end()) {
            return it->second;
        }
        return NULL;
    }

    SessionMap::iterator FindSessionIterator(const sockaddrs &ip) {
        cidr c(ip, ip.ipv6() ? ipv6_cidr : ipv4_cidr);
        if (!c.valid()) {
            return this->Sessions.end();
        }
        return this->Sessions.find(c);
    }

    Session* &FindOrCreateSession(const cidr &ip) {
        return this->Sessions[ip];
    }

    SessionMap &GetSessions() anope_override {
        return this->Sessions;
    }
};

class ExceptionDelCallback : public NumberList {
  protected:
    CommandSource &source;
    unsigned deleted;
    Command *cmd;
  public:
    ExceptionDelCallback(CommandSource &_source, const Anope::string &numlist,
                         Command *c) : NumberList(numlist, true), source(_source), deleted(0), cmd(c) {
    }

    ~ExceptionDelCallback() {
        if (!deleted) {
            source.Reply(_("No matching entries on session-limit exception list."));
        } else if (deleted == 1) {
            source.Reply(_("Deleted 1 entry from session-limit exception list."));
        } else {
            source.Reply(_("Deleted %d entries from session-limit exception list."),
                         deleted);
        }
    }

    virtual void HandleNumber(unsigned number) anope_override {
        if (!number || number > session_service->GetExceptions().size()) {
            return;
        }

        Log(LOG_ADMIN, source, cmd) << "to remove the session limit exception for " << session_service->GetExceptions()[number - 1]->mask;

        ++deleted;
        DoDel(source, number - 1);
    }

    static void DoDel(CommandSource &source, unsigned index) {
        Exception *e = session_service->GetExceptions()[index];
        FOREACH_MOD(OnExceptionDel, (source, e));

        session_service->DelException(e);
        delete e;
    }
};

class CommandOSSession : public Command {
  private:
    void DoList(CommandSource &source, const std::vector<Anope::string> &params) {
        Anope::string param = params[1];

        unsigned mincount = 0;
        try {
            mincount = convertTo<unsigned>(param);
        } catch (const ConvertException &) { }

        if (mincount <= 1) {
            source.Reply(
                _("Invalid threshold value. It must be a valid integer greater than 1."));
        } else {
            ListFormatter list(source.GetAccount());
            list.AddColumn(_("Session")).AddColumn(_("Host"));

            for (SessionService::SessionMap::iterator it =
                        session_service->GetSessions().begin(),
                    it_end = session_service->GetSessions().end(); it != it_end; ++it) {
                Session *session = it->second;

                if (session->count >= mincount) {
                    ListFormatter::ListEntry entry;
                    entry["Session"] = stringify(session->count);
                    entry["Host"] = session->addr.mask();
                    list.AddEntry(entry);
                }
            }

            source.Reply(_("Hosts with at least \002%d\002 sessions:"), mincount);

            std::vector<Anope::string> replies;
            list.Process(replies);


            for (unsigned i = 0; i < replies.size(); ++i) {
                source.Reply(replies[i]);
            }
        }

        return;
    }

    void DoView(CommandSource &source, const std::vector<Anope::string> &params) {
        Anope::string param = params[1];
        Session *session = session_service->FindSession(param);

        Exception *exception = session_service->FindException(param);
        Anope::string entry = "no entry";
        unsigned limit = session_limit;
        if (exception) {
            if (!exception->limit) {
                limit = 0;
            } else if (exception->limit > limit) {
                limit = exception->limit;
            }
            entry = exception->mask;
        }

        if (!session) {
            source.Reply(
                _("\002%s\002 not found on session list, but has a limit of \002%d\002 because it matches entry: \002%s\002."),
                param.c_str(), limit, entry.c_str());
        } else {
            source.Reply(
                _("The host \002%s\002 currently has \002%d\002 sessions with a limit of \002%d\002 because it matches entry: \002%s\002."),
                session->addr.mask().c_str(), session->count, limit, entry.c_str());
        }
    }
  public:
    CommandOSSession(Module *creator) : Command(creator, "operserv/session", 2, 2) {
        this->SetDesc(_("View the list of host sessions"));
        this->SetSyntax(_("LIST \037threshold\037"));
        this->SetSyntax(_("VIEW \037host\037"));
    }

    void Execute(CommandSource &source,
                 const std::vector<Anope::string> &params) anope_override {
        const Anope::string &cmd = params[0];

        Log(LOG_ADMIN, source, this) << cmd << " " << params[1];

        if (!session_limit) {
            source.Reply(_("Session limiting is disabled."));
        } else if (cmd.equals_ci("LIST")) {
            return this->DoList(source, params);
        } else if (cmd.equals_ci("VIEW")) {
            return this->DoView(source, params);
        } else {
            this->OnSyntaxError(source, "");
        }
    }

    bool OnHelp(CommandSource &source,
                const Anope::string &subcommand) anope_override {
        this->SendSyntax(source);
        source.Reply(" ");
        source.Reply(_("Allows Services Operators to view the session list.\n"
                       " \n"
                       "\002SESSION LIST\002 lists hosts with at least \037threshold\037 sessions.\n"
                       "The threshold must be a number greater than 1. This is to\n"
                       "prevent accidental listing of the large number of single\n"
                       "session hosts.\n"
                       " \n"
                       "\002SESSION VIEW\002 displays detailed information about a specific\n"
                       "host - including the current session count and session limit.\n"
                       "The \037host\037 value may not include wildcards.\n"
                       " \n"
                       "See the \002EXCEPTION\002 help for more information about session\n"
                       "limiting and how to set session limits specific to certain\n"
                       "hosts and groups thereof."));
        return true;
    }
};

class CommandOSException : public Command {
  private:
    void DoAdd(CommandSource &source, const std::vector<Anope::string> &params) {
        Anope::string mask, expiry, limitstr;
        unsigned last_param = 3;

        mask = params.size() > 1 ? params[1] : "";
        if (!mask.empty() && mask[0] == '+') {
            expiry = mask;
            mask = params.size() > 2 ? params[2] : "";
            last_param = 4;
        }

        limitstr = params.size() > last_param - 1 ? params[last_param - 1] : "";

        if (params.size() <= last_param) {
            this->OnSyntaxError(source, "ADD");
            return;
        }

        Anope::string reason = params[last_param];
        if (last_param == 3 && params.size() > 4) {
            reason += " " + params[4];
        }
        if (reason.empty()) {
            this->OnSyntaxError(source, "ADD");
            return;
        }

        time_t expires = !expiry.empty() ? Anope::DoTime(expiry) : exception_expiry;
        if (expires < 0) {
            source.Reply(BAD_EXPIRY_TIME);
            return;
        } else if (expires > 0) {
            expires += Anope::CurTime;
        }

        unsigned limit = -1;
        try {
            limit = convertTo<unsigned>(limitstr);
        } catch (const ConvertException &) { }

        if (limit > max_exception_limit) {
            source.Reply(
                _("Invalid session limit. It must be a valid integer greater than or equal to zero and less than \002%d\002."),
                max_exception_limit);
            return;
        } else {
            if (mask.find('!') != Anope::string::npos
                    || mask.find('@') != Anope::string::npos) {
                source.Reply(
                    _("Invalid hostmask. Only real hostmasks are valid, as exceptions are not matched against nicks or usernames."));
                return;
            }

            for (std::vector<Exception *>::iterator it =
                        session_service->GetExceptions().begin(),
                    it_end = session_service->GetExceptions().end(); it != it_end; ++it) {
                Exception *e = *it;
                if (e->mask.equals_ci(mask)) {
                    if (e->limit != limit) {
                        e->limit = limit;
                        source.Reply(_("Exception for \002%s\002 has been updated to %d."),
                                     mask.c_str(), e->limit);
                    } else {
                        source.Reply(_("\002%s\002 already exists on the EXCEPTION list."),
                                     mask.c_str());
                    }
                    return;
                }
            }

            Exception *exception = new Exception();
            exception->mask = mask;
            exception->limit = limit;
            exception->reason = reason;
            exception->time = Anope::CurTime;
            exception->who = source.GetNick();
            exception->expires = expires;

            EventReturn MOD_RESULT;
            FOREACH_RESULT(OnExceptionAdd, MOD_RESULT, (exception));
            if (MOD_RESULT == EVENT_STOP) {
                delete exception;
            } else {
                Log(LOG_ADMIN, source, this) << "to set the session limit for " << mask <<
                                             " to " << limit;
                session_service->AddException(exception);
                source.Reply(_("Session limit for \002%s\002 set to \002%d\002."), mask.c_str(),
                             limit);
                if (Anope::ReadOnly) {
                    source.Reply(READ_ONLY_MODE);
                }
            }
        }

        return;
    }

    void DoDel(CommandSource &source, const std::vector<Anope::string> &params) {
        const Anope::string &mask = params.size() > 1 ? params[1] : "";

        if (mask.empty()) {
            this->OnSyntaxError(source, "DEL");
            return;
        }

        if (isdigit(mask[0])
                && mask.find_first_not_of("1234567890,-") == Anope::string::npos) {
            ExceptionDelCallback list(source, mask, this);
            list.Process();
        } else {
            unsigned i = 0, end = session_service->GetExceptions().size();
            for (; i < end; ++i)
                if (mask.equals_ci(session_service->GetExceptions()[i]->mask)) {
                    Log(LOG_ADMIN, source, this) << "to remove the session limit exception for " <<
                                                 mask;
                    ExceptionDelCallback::DoDel(source, i);
                    source.Reply(_("\002%s\002 deleted from session-limit exception list."),
                                 mask.c_str());
                    break;
                }
            if (i == end) {
                source.Reply(_("\002%s\002 not found on session-limit exception list."),
                             mask.c_str());
            }
        }

        if (Anope::ReadOnly) {
            source.Reply(READ_ONLY_MODE);
        }

        return;
    }

    void ProcessList(CommandSource &source,
                     const std::vector<Anope::string> &params, ListFormatter &list) {
        const Anope::string &mask = params.size() > 1 ? params[1] : "";

        if (session_service->GetExceptions().empty()) {
            source.Reply(_("The session exception list is empty."));
            return;
        }

        if (!mask.empty()
                && mask.find_first_not_of("1234567890,-") == Anope::string::npos) {
            class ExceptionListCallback : public NumberList {
                CommandSource &source;
                ListFormatter &list;
              public:
                ExceptionListCallback(CommandSource &_source, ListFormatter &_list,
                                      const Anope::string &numlist) : NumberList(numlist, false), source(_source),
                    list(_list) {
                }

                void HandleNumber(unsigned Number) anope_override {
                    if (!Number || Number > session_service->GetExceptions().size()) {
                        return;
                    }

                    Exception *e = session_service->GetExceptions()[Number - 1];

                    ListFormatter::ListEntry entry;
                    entry["Number"] = stringify(Number);
                    entry["Mask"] = e->mask;
                    entry["By"] = e->who;
                    entry["Created"] = Anope::strftime(e->time, NULL, true);
                    entry["Expires"] = Anope::Expires(e->expires, source.GetAccount());
                    entry["Limit"] = stringify(e->limit);
                    entry["Reason"] = e->reason;
                    this->list.AddEntry(entry);
                }
            }
            nl_list(source, list, mask);
            nl_list.Process();
        } else {
            for (unsigned i = 0, end = session_service->GetExceptions().size(); i < end;
                    ++i) {
                Exception *e = session_service->GetExceptions()[i];
                if (mask.empty() || Anope::Match(e->mask, mask)) {
                    ListFormatter::ListEntry entry;
                    entry["Number"] = stringify(i + 1);
                    entry["Mask"] = e->mask;
                    entry["By"] = e->who;
                    entry["Created"] = Anope::strftime(e->time, NULL, true);
                    entry["Expires"] = Anope::Expires(e->expires, source.GetAccount());
                    entry["Limit"] = stringify(e->limit);
                    entry["Reason"] = e->reason;
                    list.AddEntry(entry);
                }
            }
        }

        if (list.IsEmpty()) {
            source.Reply(_("No matching entries on session-limit exception list."));
        } else {
            source.Reply(_("Current Session Limit Exception list:"));

            std::vector<Anope::string> replies;
            list.Process(replies);

            for (unsigned i = 0; i < replies.size(); ++i) {
                source.Reply(replies[i]);
            }
        }
    }

    void DoList(CommandSource &source, const std::vector<Anope::string> &params) {
        ListFormatter list(source.GetAccount());
        list.AddColumn(_("Number")).AddColumn(_("Limit")).AddColumn(_("Mask"));

        this->ProcessList(source, params, list);
    }

    void DoView(CommandSource &source, const std::vector<Anope::string> &params) {
        ListFormatter list(source.GetAccount());
        list.AddColumn(_("Number")).AddColumn(_("Mask")).AddColumn(_("By")).AddColumn(
            _("Created")).AddColumn(_("Expires")).AddColumn(_("Limit")).AddColumn(
                _("Reason"));

        this->ProcessList(source, params, list);
    }

  public:
    CommandOSException(Module *creator) : Command(creator, "operserv/exception", 1,
                5) {
        this->SetDesc(_("Modify the session-limit exception list"));
        this->SetSyntax(
            _("ADD [\037+expiry\037] \037mask\037 \037limit\037 \037reason\037"));
        this->SetSyntax(_("DEL {\037mask\037 | \037entry-num\037 | \037list\037}"));
        this->SetSyntax(_("LIST [\037mask\037 | \037list\037]"));
        this->SetSyntax(_("VIEW [\037mask\037 | \037list\037]"));
    }

    void Execute(CommandSource &source,
                 const std::vector<Anope::string> &params) anope_override {
        const Anope::string &cmd = params[0];

        if (!session_limit) {
            source.Reply(_("Session limiting is disabled."));
        } else if (cmd.equals_ci("ADD")) {
            return this->DoAdd(source, params);
        } else if (cmd.equals_ci("DEL")) {
            return this->DoDel(source, params);
        } else if (cmd.equals_ci("LIST")) {
            return this->DoList(source, params);
        } else if (cmd.equals_ci("VIEW")) {
            return this->DoView(source, params);
        } else {
            this->OnSyntaxError(source, "");
        }
    }

    bool OnHelp(CommandSource &source,
                const Anope::string &subcommand) anope_override {
        this->SendSyntax(source);
        source.Reply(" ");
        source.Reply(_("Allows Services Operators to manipulate the list of hosts that\n"
                       "have specific session limits - allowing certain machines,\n"
                       "such as shell servers, to carry more than the default number\n"
                       "of clients at a time. Once a host reaches its session limit,\n"
                       "all clients attempting to connect from that host will be\n"
                       "killed. Before the user is killed, they are notified, of a\n"
                       "source of help regarding session limiting. The content of\n"
                       "this notice is a config setting."));
        source.Reply(" ");
        source.Reply(_("\002EXCEPTION ADD\002 adds the given host mask to the exception list.\n"
                       "Note that \002nick!user@host\002 and \002user@host\002 masks are invalid!\n"
                       "Only real host masks, such as \002box.host.dom\002 and \002*.host.dom\002,\n"
                       "are allowed because sessions limiting does not take nick or\n"
                       "user names into account. \037limit\037 must be a number greater than\n"
                       "or equal to zero. This determines how many sessions this host\n"
                       "may carry at a time. A value of zero means the host has an\n"
                       "unlimited session limit. See the \002AKILL\002 help for details about\n"
                       "the format of the optional \037expiry\037 parameter.\n"
                       " \n"
                       "\002EXCEPTION DEL\002 removes the given mask from the exception list.\n"
                       " \n"
                       "\002EXCEPTION LIST\002 and \002EXCEPTION VIEW\002 show all current\n"
                       "sessions if the optional mask is given, the list is limited\n"
                       "to those sessions matching the mask. The difference is that\n"
                       "\002EXCEPTION VIEW\002 is more verbose, displaying the name of the\n"
                       "person who added the exception, its session limit, reason,\n"
                       "host mask and the expiry date and time.\n"
                       " \n"
                       "Note that a connecting client will \"use\" the first exception\n"
                       "their host matches."));
        return true;
    }
};

class OSSession : public Module {
    Serialize::Type exception_type;
    MySessionService ss;
    CommandOSSession commandossession;
    CommandOSException commandosexception;
    ServiceReference<XLineManager> akills;

  public:
    OSSession(const Anope::string &modname,
              const Anope::string &creator) : Module(modname, creator, VENDOR),
        exception_type("Exception", Exception::Unserialize), ss(this),
        commandossession(this), commandosexception(this), akills("XLineManager",
                "xlinemanager/sgline") {
        this->SetPermanent(true);
    }

    void Prioritize() anope_override {
        ModuleManager::SetPriority(this, PRIORITY_FIRST);
    }

    void OnReload(Configuration::Conf *conf) anope_override {
        Configuration::Block *block = Config->GetModule(this);

        session_limit = block->Get<int>("defaultsessionlimit");
        max_session_kill = block->Get<int>("maxsessionkill");
        session_autokill_expiry = block->Get<time_t>("sessionautokillexpiry");
        sle_reason = block->Get<const Anope::string>("sessionlimitexceeded");
        sle_detailsloc = block->Get<const Anope::string>("sessionlimitdetailsloc");

        max_exception_limit = block->Get<int>("maxsessionlimit");
        exception_expiry = block->Get<time_t>("exceptionexpiry");

        ipv4_cidr = block->Get<unsigned>("session_ipv4_cidr", "32");
        ipv6_cidr = block->Get<unsigned>("session_ipv6_cidr", "128");

        if (ipv4_cidr > 32 || ipv6_cidr > 128) {
            throw ConfigException(this->name + ": session CIDR value out of range");
        }
    }

    void OnUserConnect(User *u, bool &exempt) anope_override {
        if (u->Quitting() || !session_limit || exempt || !u->server || u->server->IsULined()) {
            return;
        }

        cidr u_ip(u->ip, u->ip.ipv6() ? ipv6_cidr : ipv4_cidr);
        if (!u_ip.valid()) {
            return;
        }

        Session* &session = this->ss.FindOrCreateSession(u_ip);

        if (session) {
            bool kill = false;
            if (session->count >= session_limit) {
                kill = true;
                Exception *exception = this->ss.FindException(u);
                if (exception) {
                    kill = false;
                    if (exception->limit && session->count >= exception->limit) {
                        kill = true;
                    }
                }
            }

            /* Previously on IRCds that send a QUIT (InspIRCD) when a user is killed, the session for a host was
             * decremented in do_quit, which caused problems and fixed here
             *
             * Now, we create the user struture before calling this to fix some user tracking issues,
             * so we must increment this here no matter what because it will either be
             * decremented when the user is killed or quits - Adam
             */
            ++session->count;

            if (kill && !exempt) {
                BotInfo *OperServ = Config->GetClient("OperServ");
                if (OperServ) {
                    if (!sle_reason.empty()) {
                        Anope::string message = sle_reason.replace_all_cs("%IP%", u->ip.addr());
                        u->SendMessage(OperServ, message);
                    }
                    if (!sle_detailsloc.empty()) {
                        u->SendMessage(OperServ, sle_detailsloc);
                    }
                }

                ++session->hits;

                const Anope::string &akillmask = "*@" + session->addr.mask();
                if (max_session_kill && session->hits >= max_session_kill && akills
                        && !akills->HasEntry(akillmask)) {
                    XLine *x = new XLine(akillmask, OperServ ? OperServ->nick : "",
                                         Anope::CurTime + session_autokill_expiry, "Session limit exceeded",
                                         XLineManager::GenerateUID());
                    akills->AddXLine(x);
                    akills->Send(NULL, x);
                    Log(OperServ, "akill/session") << "Added a temporary AKILL for \002" <<
                                                   akillmask << "\002 due to excessive connections";
                } else {
                    u->Kill(OperServ, "Session limit exceeded");
                }
            }
        } else {
            session = new Session(u->ip, u->ip.ipv6() ? ipv6_cidr : ipv4_cidr);
        }
    }

    void OnUserQuit(User *u, const Anope::string &msg) anope_override {
        if (!session_limit || !u->server || u->server->IsULined()) {
            return;
        }

        SessionService::SessionMap &sessions = this->ss.GetSessions();
        SessionService::SessionMap::iterator sit = this->ss.FindSessionIterator(u->ip);

        if (sit == sessions.end()) {
            return;
        }

        Session *session = sit->second;

        if (session->count > 1) {
            --session->count;
            return;
        }

        delete session;
        sessions.erase(sit);
    }

    void OnExpireTick() anope_override {
        if (Anope::NoExpire) {
            return;
        }
        for (unsigned i = this->ss.GetExceptions().size(); i > 0; --i) {
            Exception *e = this->ss.GetExceptions()[i - 1];

            if (!e->expires || e->expires > Anope::CurTime) {
                continue;
            }
            BotInfo *OperServ = Config->GetClient("OperServ");
            Log(OperServ, "expire/exception") << "Session exception for " << e->mask <<
                                              " has expired.";
            this->ss.DelException(e);
            delete e;
        }
    }
};

MODULE_INIT(OSSession)
