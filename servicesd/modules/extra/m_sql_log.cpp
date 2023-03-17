/*
 *
 * (C) 2003-2023 Anope Team
 * Contact us at team@anope.org
 *
 * Please read COPYING and README for further details.
 */

#include "module.h"
#include "modules/sql.h"

class SQLLog : public Module {
    std::set<Anope::string> inited;
    Anope::string table;

  public:
    SQLLog(const Anope::string &modname,
           const Anope::string &creator) : Module(modname, creator, VENDOR | EXTRA) {
    }

    void OnReload(Configuration::Conf *conf) anope_override {
        Configuration::Block *config = conf->GetModule(this);
        this->table = config->Get<const Anope::string>("table", "logs");
    }

    void OnLogMessage(LogInfo *li, const Log *l,
                      const Anope::string &msg) anope_override {
        Anope::string ref_name;
        ServiceReference<SQL::Provider> SQL;

        for (unsigned i = 0; i < li->targets.size(); ++i) {
            const Anope::string &target = li->targets[i];
            size_t sz = target.find("sql_log:");
            if (!sz) {
                ref_name = target.substr(8);
                SQL = ServiceReference<SQL::Provider>("SQL::Provider", ref_name);
                break;
            }
        }

        if (!SQL) {
            return;
        }

        if (!inited.count(ref_name)) {
            inited.insert(ref_name);

            SQL::Query create("CREATE TABLE IF NOT EXISTS `" + table + "` ("
                              "`date` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,"
                              "`type` varchar(64) NOT NULL,"
                              "`user` varchar(64) NOT NULL,"
                              "`acc` varchar(64) NOT NULL,"
                              "`command` varchar(64) NOT NULL,"
                              "`channel` varchar(64) NOT NULL,"
                              "`msg` text NOT NULL"
                              ")");

            SQL->Run(NULL, create);
        }

        SQL::Query insert("INSERT INTO `" + table + "` (`type`,`user`,`acc`,`command`,`channel`,`msg`)"
                          "VALUES (@type@, @user@, @acc@, @command@, @channel@, @msg@)");

        switch (l->type) {
        case LOG_ADMIN:
            insert.SetValue("type", "ADMIN");
            break;
        case LOG_OVERRIDE:
            insert.SetValue("type", "OVERRIDE");
            break;
        case LOG_COMMAND:
            insert.SetValue("type", "COMMAND");
            break;
        case LOG_SERVER:
            insert.SetValue("type", "SERVER");
            break;
        case LOG_CHANNEL:
            insert.SetValue("type", "CHANNEL");
            break;
        case LOG_USER:
            insert.SetValue("type", "USER");
            break;
        case LOG_MODULE:
            insert.SetValue("type", "MODULE");
            break;
        case LOG_NORMAL:
            insert.SetValue("type", "NORMAL");
            break;
        default:
            return;
        }

        insert.SetValue("user", l->u ? l->u->nick : "");
        insert.SetValue("acc", l->nc ? l->nc->display : "");
        insert.SetValue("command", l->c ? l->c->name : "");
        insert.SetValue("channel", l->ci ? l->ci->name : "");
        insert.SetValue("msg", msg);

        SQL->Run(NULL, insert);
    }
};

MODULE_INIT(SQLLog)
