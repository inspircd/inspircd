/*
 *
 * (C) 2012-2023 Anope Team
 * Contact us at team@anope.org
 *
 * Please read COPYING and README for further details.
 */

#include "module.h"
#include "modules/sql.h"

static Module *me;

class SQLAuthenticationResult : public SQL::Interface {
    Reference<User> user;
    IdentifyRequest *req;

  public:
    SQLAuthenticationResult(User *u, IdentifyRequest *r) : SQL::Interface(me),
        user(u), req(r) {
        req->Hold(me);
    }

    ~SQLAuthenticationResult() {
        req->Release(me);
    }

    void OnResult(const SQL::Result &r) anope_override {
        if (r.Rows() == 0) {
            Log(LOG_DEBUG) << "m_sql_authentication: Unsuccessful authentication for " <<
                           req->GetAccount();
            delete this;
            return;
        }

        Log(LOG_DEBUG) << "m_sql_authentication: Successful authentication for " << req->GetAccount();

        Anope::string email;
        try {
            email = r.Get(0, "email");
        } catch (const SQL::Exception &) { }

        NickAlias *na = NickAlias::Find(req->GetAccount());
        BotInfo *NickServ = Config->GetClient("NickServ");
        if (na == NULL) {
            na = new NickAlias(req->GetAccount(), new NickCore(req->GetAccount()));
            FOREACH_MOD(OnNickRegister, (user, na, ""));
            if (user && NickServ) {
                user->SendMessage(NickServ,
                                  _("Your account \002%s\002 has been successfully created."), na->nick.c_str());
            }
        }

        if (!email.empty() && email != na->nc->email) {
            na->nc->email = email;
            if (user && NickServ) {
                user->SendMessage(NickServ, _("Your email has been updated to \002%s\002."),
                                  email.c_str());
            }
        }

        req->Success(me);
        delete this;
    }

    void OnError(const SQL::Result &r) anope_override {
        Log(this->owner) << "m_sql_authentication: Error executing query " << r.GetQuery().query << ": " << r.GetError();
        delete this;
    }
};

class ModuleSQLAuthentication : public Module {
    Anope::string engine;
    Anope::string query;
    Anope::string disable_reason, disable_email_reason;

    ServiceReference<SQL::Provider> SQL;

  public:
    ModuleSQLAuthentication(const Anope::string &modname,
                            const Anope::string &creator) : Module(modname, creator, EXTRA | VENDOR) {
        me = this;

    }

    void OnReload(Configuration::Conf *conf) anope_override {
        Configuration::Block *config = conf->GetModule(this);
        this->engine = config->Get<const Anope::string>("engine");
        this->query =  config->Get<const Anope::string>("query");
        this->disable_reason = config->Get<const Anope::string>("disable_reason");
        this->disable_email_reason = config->Get<Anope::string>("disable_email_reason");

        this->SQL = ServiceReference<SQL::Provider>("SQL::Provider", this->engine);
    }

    EventReturn OnPreCommand(CommandSource &source, Command *command,
                             std::vector<Anope::string> &params) anope_override {
        if (!this->disable_reason.empty() && (command->name == "nickserv/register" || command->name == "nickserv/group")) {
            source.Reply(this->disable_reason);
            return EVENT_STOP;
        }

        if (!this->disable_email_reason.empty() && command->name == "nickserv/set/email") {
            source.Reply(this->disable_email_reason);
            return EVENT_STOP;
        }

        return EVENT_CONTINUE;
    }

    void OnCheckAuthentication(User *u, IdentifyRequest *req) anope_override {
        if (!this->SQL) {
            Log(this) << "Unable to find SQL engine";
            return;
        }

        SQL::Query q(this->query);
        q.SetValue("a", req->GetAccount());
        q.SetValue("p", req->GetPassword());
        if (u) {
            q.SetValue("n", u->nick);
            q.SetValue("i", u->ip.addr());
        } else {
            q.SetValue("n", "");
            q.SetValue("i", "");
        }


        this->SQL->Run(new SQLAuthenticationResult(u, req), q);

        Log(LOG_DEBUG) << "m_sql_authentication: Checking authentication for " << req->GetAccount();
    }
};

MODULE_INIT(ModuleSQLAuthentication)
