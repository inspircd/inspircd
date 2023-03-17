/* Chanstats core functions
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
#include "modules/sql.h"

class MySQLInterface : public SQL::Interface {
  public:
    MySQLInterface(Module *o) : SQL::Interface(o) { }

    void OnResult(const SQL::Result &r) anope_override {
    }

    void OnError(const SQL::Result &r) anope_override {
        if (!r.GetQuery().query.empty()) {
            Log(LOG_DEBUG) << "Chanstats: Error executing query " << r.finished_query <<
                           ": " << r.GetError();
        } else {
            Log(LOG_DEBUG) << "Chanstats: Error executing query: " << r.GetError();
        }
    }
};

class CommandCSTop : public Command {
  public:
    CommandCSTop(Module *creator) : Command (creator, "chanserv/top", 0, 2) {
        this->SetDesc(_("Displays the top 3 users of a channel"));
        this->SetSyntax(_("\037channel\037"));
    }

    void Execute(CommandSource &source, const std::vector<Anope::string> &params);
};

class CommandCSTop10 : public Command {
  public:
    CommandCSTop10(Module *creator) : Command (creator, "chanserv/top10", 0, 2) {
        this->SetDesc(_("Displays the top 10 users of a channel"));
        this->SetSyntax(_("\037channel\037"));
    }

    void Execute(CommandSource &source, const std::vector<Anope::string> &params);
};

class CommandCSGTop : public Command {
  public:
    CommandCSGTop(Module *creator) : Command (creator, "chanserv/gtop", 0, 1) {
        this->SetDesc(_("Displays the top 3 users of the network"));
    }

    void Execute(CommandSource &source, const std::vector<Anope::string> &params);
};

class CommandCSGTop10 : public Command {
  public:
    CommandCSGTop10(Module *creator) : Command (creator, "chanserv/gtop10", 0, 1) {
        this->SetDesc(_("Displays the top 10 users of the network"));
    }

    void Execute(CommandSource &source, const std::vector<Anope::string> &params);
};


class CSTop;
static CSTop *me;
class CSTop : public Module {
    CommandCSTop commandcstop;
    CommandCSGTop commandcsgtop;
    CommandCSTop10 commandcstop10;
    CommandCSGTop10 commandcsgtop10;
    ServiceReference<SQL::Provider> sql;
    MySQLInterface sqlinterface;
    Anope::string prefix;

  public:
    CSTop(const Anope::string &modname,
          const Anope::string &creator) : Module(modname, creator, VENDOR),
        commandcstop(this), commandcsgtop(this), commandcstop10(this),
        commandcsgtop10(this), sql("", ""),
        sqlinterface(this) {
        me = this;

    }

    void OnReload(Configuration::Conf *conf) anope_override {
        prefix = conf->GetModule("m_chanstats")->Get<const Anope::string>("prefix", "anope_");
        this->sql = ServiceReference<SQL::Provider>("SQL::Provider", conf->GetModule("m_chanstats")->Get<const Anope::string>("engine"));
    }

    SQL::Result RunQuery(const SQL::Query &query) {
        if (!this->sql) {
            throw SQL::Exception("Unable to locate SQL reference, is m_mysql loaded and configured correctly?");
        }

        SQL::Result res = sql->RunQuery(query);
        if (!res.GetError().empty()) {
            throw SQL::Exception(res.GetError());
        }
        return res;
    }

    void DoTop(CommandSource &source, const std::vector<Anope::string> &params,
               bool is_global, int limit = 1) {

        Anope::string channel;
        if (!params.empty()) {
            channel = params[0];
        } else if (source.c && source.c->ci) {
            channel = source.c->ci->name;
        }

        if (!is_global && channel.empty()) {
            is_global = true;
        }

        try {
            SQL::Query query;
            query = "SELECT nick, letters, words, line, actions,"
                    "smileys_happy+smileys_sad+smileys_other as smileys "
                    "FROM `" + prefix + "chanstats` "
                    "WHERE `nick` != '' AND `chan` = @channel@ AND `type` = 'total' "
                    "ORDER BY `letters` DESC LIMIT @limit@;";
            query.SetValue("limit", limit, false);

            if (is_global) {
                query.SetValue("channel", "");
            } else {
                query.SetValue("channel", channel.c_str());
            }

            SQL::Result res = this->RunQuery(query);

            if (res.Rows() > 0) {
                source.Reply(_("Top %i of %s"), limit,
                             (is_global ? "Network" : channel.c_str()));
                for (int i = 0; i < res.Rows(); ++i) {
                    source.Reply(
                        _("%2lu \002%-16s\002 letters: %s, words: %s, lines: %s, smileys: %s, actions: %s"),
                        i+1, res.Get(i, "nick").c_str(), res.Get(i, "letters").c_str(),
                        res.Get(i, "words").c_str(), res.Get(i, "line").c_str(),
                        res.Get(i, "smileys").c_str(), res.Get(i, "actions").c_str());
                }
            } else {
                source.Reply(_("No stats for %s."), is_global ? "Network" : channel.c_str());
            }
        } catch (const SQL::Exception &ex) {
            Log(LOG_DEBUG) << ex.GetReason();
        }
    }
};

void CommandCSTop::Execute(CommandSource &source,
                           const std::vector<Anope::string> &params) {
    me->DoTop(source, params, false, 3);
}

void CommandCSTop10::Execute(CommandSource &source,
                             const std::vector<Anope::string> &params) {
    me->DoTop(source, params, false, 10);
}

void CommandCSGTop::Execute(CommandSource &source,
                            const std::vector<Anope::string> &params) {
    me->DoTop(source, params, true, 3);
}

void CommandCSGTop10::Execute(CommandSource &source,
                              const std::vector<Anope::string> &params) {
    me->DoTop(source, params, true, 10);
}

MODULE_INIT(CSTop)
