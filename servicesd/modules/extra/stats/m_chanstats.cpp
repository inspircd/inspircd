/*
 *
 * (C) 2012-2023 Anope Team
 * Contact us at team@anope.org
 *
 * Please read COPYING and README for further details.
 */

#include "module.h"
#include "modules/sql.h"

class CommandCSSetChanstats : public Command {
  public:
    CommandCSSetChanstats(Module *creator) : Command(creator,
                "chanserv/set/chanstats", 2, 2) {
        this->SetDesc(_("Turn chanstats statistics on or off"));
        this->SetSyntax(_("\037channel\037 {ON | OFF}"));
    }

    void Execute(CommandSource &source,
                 const std::vector<Anope::string> &params) anope_override {
        ChannelInfo *ci = ChannelInfo::Find(params[0]);
        if (!ci) {
            source.Reply(CHAN_X_NOT_REGISTERED, params[0].c_str());
            return;
        }

        EventReturn MOD_RESULT;
        FOREACH_RESULT(OnSetChannelOption, MOD_RESULT, (source, this, ci, params[1]));
        if (MOD_RESULT == EVENT_STOP) {
            return;
        }

        if (MOD_RESULT != EVENT_ALLOW && !source.AccessFor(ci).HasPriv("SET") && source.permission.empty() && !source.HasPriv("chanserv/administration")) {
            source.Reply(ACCESS_DENIED);
            return;
        }

        if (params[1].equals_ci("ON")) {
            ci->Extend<bool>("CS_STATS");
            source.Reply(_("Chanstats statistics are now enabled for this channel."));
            Log(source.AccessFor(ci).HasPriv("SET") ? LOG_COMMAND : LOG_OVERRIDE, source,
                this, ci) << "to enable chanstats";
        } else if (params[1].equals_ci("OFF")) {
            Log(source.AccessFor(ci).HasPriv("SET") ? LOG_COMMAND : LOG_OVERRIDE, source,
                this, ci) << "to disable chanstats";
            ci->Shrink<bool>("CS_STATS");
            source.Reply(_("Chanstats statistics are now disabled for this channel."));
        } else {
            this->OnSyntaxError(source, "");
        }
    }

    bool OnHelp(CommandSource &source, const Anope::string &) anope_override {
        this->SendSyntax(source);
        source.Reply(" ");
        source.Reply(_("Turns chanstats statistics ON or OFF."));
        return true;
    }
};

class CommandNSSetChanstats : public Command {
  public:
    CommandNSSetChanstats(Module *creator,
                          const Anope::string &sname = "nickserv/set/chanstats",
                          size_t min = 1 ) : Command(creator, sname, min, min + 1) {
        this->SetDesc(_("Turn chanstats statistics on or off"));
        this->SetSyntax("{ON | OFF}");
    }
    void Run(CommandSource &source, const Anope::string &user,
             const Anope::string &param, bool saset = false) {
        NickAlias *na = NickAlias::Find(user);
        if (!na) {
            source.Reply(NICK_X_NOT_REGISTERED, user.c_str());
            return;
        }

        EventReturn MOD_RESULT;
        FOREACH_RESULT(OnSetNickOption, MOD_RESULT, (source, this, na->nc, param));
        if (MOD_RESULT == EVENT_STOP) {
            return;
        }

        if (param.equals_ci("ON")) {
            Log(na->nc == source.GetAccount() ? LOG_COMMAND : LOG_ADMIN, source,
                this) << "to enable chanstats for " << na->nc->display;
            na->nc->Extend<bool>("NS_STATS");
            if (saset) {
                source.Reply(_("Chanstats statistics are now enabled for %s"),
                             na->nc->display.c_str());
            } else {
                source.Reply(_("Chanstats statistics are now enabled for your nick."));
            }
        } else if (param.equals_ci("OFF")) {
            Log(na->nc == source.GetAccount() ? LOG_COMMAND : LOG_ADMIN, source,
                this) << "to disable chanstats for " << na->nc->display;
            na->nc->Shrink<bool>("NS_STATS");
            if (saset) {
                source.Reply(_("Chanstats statistics are now disabled for %s"),
                             na->nc->display.c_str());
            } else {
                source.Reply(_("Chanstats statistics are now disabled for your nick."));
            }
        } else {
            this->OnSyntaxError(source, "CHANSTATS");
        }
    }

    void Execute(CommandSource &source,
                 const std::vector<Anope::string> &params) anope_override {
        this->Run(source, source.nc->display, params[0]);
    }

    bool OnHelp(CommandSource &source, const Anope::string &) anope_override {
        this->SendSyntax(source);
        source.Reply(" ");
        source.Reply(_("Turns chanstats statistics ON or OFF."));
        return true;
    }
};

class CommandNSSASetChanstats : public CommandNSSetChanstats {
  public:
    CommandNSSASetChanstats(Module *creator) : CommandNSSetChanstats(creator,
                "nickserv/saset/chanstats", 2) {
        this->ClearSyntax();
        this->SetSyntax(_("\037nickname\037 {ON | OFF}"));
    }

    void Execute(CommandSource &source,
                 const std::vector<Anope::string> &params) anope_override {
        this->Run(source, params[0], params[1], true);
    }

    bool OnHelp(CommandSource &source, const Anope::string &) anope_override {
        this->SendSyntax(source);
        source.Reply(" ");
        source.Reply(_("Turns chanstats channel statistics ON or OFF for this user."));
        return true;
    }
};

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

class MChanstats : public Module {
    SerializableExtensibleItem<bool> cs_stats, ns_stats;

    CommandCSSetChanstats commandcssetchanstats;

    CommandNSSetChanstats commandnssetchanstats;
    CommandNSSASetChanstats commandnssasetchanstats;

    ServiceReference<SQL::Provider> sql;
    MySQLInterface sqlinterface;
    SQL::Query query;
    Anope::string SmileysHappy, SmileysSad, SmileysOther, prefix;
    std::vector<Anope::string> TableList, ProcedureList, EventList;
    bool NSDefChanstats, CSDefChanstats;

    void RunQuery(const SQL::Query &q) {
        if (sql) {
            sql->Run(&sqlinterface, q);
        }
    }

    size_t CountWords(const Anope::string &msg) {
        size_t words = 0;
        for (size_t pos = 0; pos != Anope::string::npos; pos = msg.find(" ", pos+1)) {
            words++;
        }
        return words;
    }
    size_t CountSmileys(const Anope::string &msg, const Anope::string &smileylist) {
        size_t smileys = 0;
        spacesepstream sep(smileylist);
        Anope::string buf;

        while (sep.GetToken(buf) && !buf.empty()) {
            for (size_t pos = msg.find(buf, 0); pos != Anope::string::npos;
                    pos = msg.find(buf, pos+1)) {
                smileys++;
            }
        }
        return smileys;
    }

    const Anope::string GetDisplay(User *u) {
        if (u && u->Account() && ns_stats.HasExt(u->Account())) {
            return u->Account()->display;
        } else {
            return "";
        }
    }

    void GetTables() {
        TableList.clear();
        ProcedureList.clear();
        EventList.clear();
        if (!sql) {
            return;
        }

        SQL::Result r = this->sql->RunQuery(this->sql->GetTables(prefix));
        for (int i = 0; i < r.Rows(); ++i) {
            const std::map<Anope::string, Anope::string> &map = r.Row(i);
            for (std::map<Anope::string, Anope::string>::const_iterator it = map.begin();
                    it != map.end(); ++it) {
                TableList.push_back(it->second);
            }
        }
        query = "SHOW PROCEDURE STATUS WHERE `Db` = Database();";
        r = this->sql->RunQuery(query);
        for (int i = 0; i < r.Rows(); ++i) {
            ProcedureList.push_back(r.Get(i, "Name"));
        }
        query = "SHOW EVENTS WHERE `Db` = Database();";
        r = this->sql->RunQuery(query);
        for (int i = 0; i < r.Rows(); ++i) {
            EventList.push_back(r.Get(i, "Name"));
        }
    }

    bool HasTable(const Anope::string &table) {
        for (std::vector<Anope::string>::const_iterator it = TableList.begin();
                it != TableList.end(); ++it)
            if (*it == table) {
                return true;
            }
        return false;
    }

    bool HasProcedure(const Anope::string &table) {
        for (std::vector<Anope::string>::const_iterator it = ProcedureList.begin();
                it != ProcedureList.end(); ++it)
            if (*it == table) {
                return true;
            }
        return false;
    }

    bool HasEvent(const Anope::string &table) {
        for (std::vector<Anope::string>::const_iterator it = EventList.begin();
                it != EventList.end(); ++it)
            if (*it == table) {
                return true;
            }
        return false;
    }


    void CheckTables() {
        this->GetTables();
        if (!this->HasTable(prefix +"chanstats")) {
            query = "CREATE TABLE `" + prefix + "chanstats` ("
                    "`id` int(11) NOT NULL AUTO_INCREMENT,"
                    "`chan` varchar(64) NOT NULL DEFAULT '',"
                    "`nick` varchar(64) NOT NULL DEFAULT '',"
                    "`type` ENUM('total', 'monthly', 'weekly', 'daily') NOT NULL,"
                    "`letters` int(10) unsigned NOT NULL DEFAULT '0',"
                    "`words` int(10) unsigned NOT NULL DEFAULT '0',"
                    "`line` int(10) unsigned NOT NULL DEFAULT '0',"
                    "`actions` int(10) unsigned NOT NULL DEFAULT '0',"
                    "`smileys_happy` int(10) unsigned NOT NULL DEFAULT '0',"
                    "`smileys_sad` int(10) unsigned NOT NULL DEFAULT '0',"
                    "`smileys_other` int(10) unsigned NOT NULL DEFAULT '0',"
                    "`kicks` int(10) unsigned NOT NULL DEFAULT '0',"
                    "`kicked` int(10) unsigned NOT NULL DEFAULT '0',"
                    "`modes` int(10) unsigned NOT NULL DEFAULT '0',"
                    "`topics` int(10) unsigned NOT NULL DEFAULT '0',"
                    "`time0` int(10) unsigned NOT NULL default '0',"
                    "`time1` int(10) unsigned NOT NULL default '0',"
                    "`time2` int(10) unsigned NOT NULL default '0',"
                    "`time3` int(10) unsigned NOT NULL default '0',"
                    "`time4` int(10) unsigned NOT NULL default '0',"
                    "`time5` int(10) unsigned NOT NULL default '0',"
                    "`time6` int(10) unsigned NOT NULL default '0',"
                    "`time7` int(10) unsigned NOT NULL default '0',"
                    "`time8` int(10) unsigned NOT NULL default '0',"
                    "`time9` int(10) unsigned NOT NULL default '0',"
                    "`time10` int(10) unsigned NOT NULL default '0',"
                    "`time11` int(10) unsigned NOT NULL default '0',"
                    "`time12` int(10) unsigned NOT NULL default '0',"
                    "`time13` int(10) unsigned NOT NULL default '0',"
                    "`time14` int(10) unsigned NOT NULL default '0',"
                    "`time15` int(10) unsigned NOT NULL default '0',"
                    "`time16` int(10) unsigned NOT NULL default '0',"
                    "`time17` int(10) unsigned NOT NULL default '0',"
                    "`time18` int(10) unsigned NOT NULL default '0',"
                    "`time19` int(10) unsigned NOT NULL default '0',"
                    "`time20` int(10) unsigned NOT NULL default '0',"
                    "`time21` int(10) unsigned NOT NULL default '0',"
                    "`time22` int(10) unsigned NOT NULL default '0',"
                    "`time23` int(10) unsigned NOT NULL default '0',"
                    "PRIMARY KEY (`id`),"
                    "UNIQUE KEY `chan` (`chan`,`nick`,`type`),"
                    "KEY `nick` (`nick`),"
                    "KEY `chan_` (`chan`),"
                    "KEY `type` (`type`)"
                    ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;";
            this->RunQuery(query);
        }
        /* There is no CREATE OR REPLACE PROCEDURE in MySQL */
        if (this->HasProcedure(prefix + "chanstats_proc_update")) {
            query = "DROP PROCEDURE " + prefix + "chanstats_proc_update";
            this->RunQuery(query);
        }
        query = "CREATE PROCEDURE `" + prefix + "chanstats_proc_update`"
                "(chan_ VARCHAR(255), nick_ VARCHAR(255), line_ INT(10), letters_ INT(10),"
                "words_ INT(10), actions_ INT(10), sm_h_ INT(10), sm_s_ INT(10), sm_o_ INT(10),"
                "kicks_ INT(10), kicked_ INT(10), modes_ INT(10), topics_ INT(10))"
                "BEGIN "
                "DECLARE time_ VARCHAR(20);"
                "SET time_ = CONCAT('time', hour(now()));"
                "INSERT IGNORE INTO `" + prefix + "chanstats` (`nick`,`chan`, `type`) VALUES "
                "('', chan_, 'total'), ('', chan_, 'monthly'),"
                "('', chan_, 'weekly'), ('', chan_, 'daily');"
                "IF nick_ != '' THEN "
                "INSERT IGNORE INTO `" + prefix + "chanstats` (`nick`,`chan`, `type`) VALUES "
                "(nick_, chan_, 'total'), (nick_, chan_, 'monthly'),"
                "(nick_, chan_, 'weekly'),(nick_, chan_, 'daily'),"
                "(nick_, '', 'total'), (nick_, '', 'monthly'),"
                "(nick_, '', 'weekly'), (nick_, '', 'daily');"
                "END IF;"
                "SET @update_query = CONCAT('UPDATE `" + prefix +
                "chanstats` SET line=line+', line_, ',"
                "letters=letters+', letters_, ' , words=words+', words_, ', actions=actions+', actions_, ', "
                "smileys_happy=smileys_happy+', sm_h_, ', smileys_sad=smileys_sad+', sm_s_, ', "
                "smileys_other=smileys_other+', sm_o_, ', kicks=kicks+', kicks_, ', kicked=kicked+', kicked_, ', "
                "modes=modes+', modes_, ', topics=topics+', topics_, ', ', time_ , '=', time_, '+', line_ ,' "
                "WHERE (nick='''' OR nick=''', nick_, ''') AND (chan='''' OR chan=''', chan_, ''')');"
                "PREPARE update_query FROM @update_query;"
                "EXECUTE update_query;"
                "DEALLOCATE PREPARE update_query;"
                "END";
        this->RunQuery(query);

        if (this->HasProcedure(prefix + "chanstats_proc_chgdisplay")) {
            query = "DROP PROCEDURE " + prefix + "chanstats_proc_chgdisplay;";
            this->RunQuery(query);
        }
        query = "CREATE PROCEDURE `" + prefix + "chanstats_proc_chgdisplay`"
                "(old_nick varchar(255), new_nick varchar(255))"
                "BEGIN "
                "DECLARE res_count int(10) unsigned;"
                "SELECT COUNT(nick) INTO res_count FROM `" + prefix +
                "chanstats` WHERE nick = new_nick;"
                "IF res_count = 0 THEN "
                "UPDATE `" + prefix +
                "chanstats` SET `nick` = new_nick WHERE `nick` = old_nick;"
                "ELSE "
                "my_cursor: BEGIN "
                "DECLARE no_more_rows BOOLEAN DEFAULT FALSE;"
                "DECLARE chan_ VARCHAR(255);"
                "DECLARE type_ ENUM('total', 'monthly', 'weekly', 'daily');"
                "DECLARE letters_, words_, line_, actions_, smileys_happy_,"
                "smileys_sad_, smileys_other_, kicks_, kicked_, modes_, topics_,"
                "time0_, time1_, time2_, time3_, time4_, time5_, time6_, time7_, time8_, time9_,"
                "time10_, time11_, time12_, time13_, time14_, time15_, time16_, time17_, time18_,"
                "time19_, time20_, time21_, time22_, time23_ INT(10) unsigned;"
                "DECLARE stats_cursor CURSOR FOR "
                "SELECT chan, type, letters, words, line, actions, smileys_happy,"
                "smileys_sad, smileys_other, kicks, kicked, modes, topics, time0, time1,"
                "time2, time3, time4, time5, time6, time7, time8, time9, time10, time11,"
                "time12, time13, time14, time15, time16, time17, time18, time19, time20,"
                "time21, time22, time23 "
                "FROM `" + prefix + "chanstats` "
                "WHERE `nick` = old_nick;"
                "DECLARE CONTINUE HANDLER FOR NOT FOUND "
                "SET no_more_rows = TRUE;"
                "OPEN stats_cursor;"
                "the_loop: LOOP "
                "FETCH stats_cursor "
                "INTO chan_, type_, letters_, words_, line_, actions_, smileys_happy_,"
                "smileys_sad_, smileys_other_, kicks_, kicked_, modes_, topics_,"
                "time0_, time1_, time2_, time3_, time4_, time5_, time6_, time7_, time8_,"
                "time9_, time10_, time11_, time12_, time13_, time14_, time15_, time16_,"
                "time17_, time18_, time19_, time20_, time21_, time22_, time23_;"
                "IF no_more_rows THEN "
                "CLOSE stats_cursor;"
                "LEAVE the_loop;"
                "END IF;"
                "INSERT INTO `" + prefix + "chanstats` "
                "(chan, nick, type, letters, words, line, actions, smileys_happy, "
                "smileys_sad, smileys_other, kicks, kicked, modes, topics, time0, time1, "
                "time2, time3, time4, time5, time6, time7, time8, time9, time10, time11,"
                "time12, time13, time14, time15, time16, time17, time18, time19, time20,"
                "time21, time22, time23)"
                "VALUES (chan_, new_nick, type_, letters_, words_, line_, actions_, smileys_happy_,"
                "smileys_sad_, smileys_other_, kicks_, kicked_, modes_, topics_,"
                "time0_, time1_, time2_, time3_, time4_, time5_, time6_, time7_, time8_, "
                "time9_, time10_, time11_, time12_, time13_, time14_, time15_, time16_, "
                "time17_, time18_, time19_, time20_, time21_, time22_, time23_)"
                "ON DUPLICATE KEY UPDATE letters=letters+VALUES(letters), words=words+VALUES(words),"
                "line=line+VALUES(line), actions=actions+VALUES(actions),"
                "smileys_happy=smileys_happy+VALUES(smileys_happy),"
                "smileys_sad=smileys_sad+VALUES(smileys_sad),"
                "smileys_other=smileys_other+VALUES(smileys_other),"
                "kicks=kicks+VALUES(kicks), kicked=kicked+VALUES(kicked),"
                "modes=modes+VALUES(modes), topics=topics+VALUES(topics),"
                "time1=time1+VALUES(time1), time2=time2+VALUES(time2), time3=time3+VALUES(time3),"
                "time4=time4+VALUES(time4), time5=time5+VALUES(time5), time6=time6+VALUES(time6),"
                "time7=time7+VALUES(time7), time8=time8+VALUES(time8), time9=time9+VALUES(time9),"
                "time10=time10+VALUES(time10), time11=time11+VALUES(time11), time12=time12+VALUES(time12),"
                "time13=time13+VALUES(time13), time14=time14+VALUES(time14), time15=time15+VALUES(time15),"
                "time16=time16+VALUES(time16), time17=time17+VALUES(time17), time18=time18+VALUES(time18),"
                "time19=time19+VALUES(time19), time20=time20+VALUES(time20), time21=time21+VALUES(time21),"
                "time22=time22+VALUES(time22), time23=time23+VALUES(time23);"
                "END LOOP;"
                "DELETE FROM `" + prefix + "chanstats` WHERE `nick` = old_nick;"
                "END my_cursor;"
                "END IF;"
                "END;";
        this->RunQuery(query);

        /* don't prepend any database prefix to events so we can always delete/change old events */
        if (this->HasEvent("chanstats_event_cleanup_daily")) {
            query = "DROP EVENT chanstats_event_cleanup_daily";
            this->RunQuery(query);
        }
        query = "CREATE EVENT `chanstats_event_cleanup_daily` "
                "ON SCHEDULE EVERY 1 DAY STARTS CURRENT_DATE "
                "DO UPDATE `" + prefix +
                "chanstats` SET letters=0, words=0, line=0, actions=0, smileys_happy=0,"
                "smileys_sad=0, smileys_other=0, kicks=0, modes=0, topics=0, time0=0, time1=0, time2=0,"
                "time3=0, time4=0, time5=0, time6=0, time7=0, time8=0, time9=0, time10=0, time11=0,"
                "time12=0, time13=0, time14=0, time15=0, time16=0, time17=0, time18=0, time19=0,"
                "time20=0, time21=0, time22=0, time23=0 "
                "WHERE type='daily';";
        this->RunQuery(query);

        if (this->HasEvent("chanstats_event_cleanup_weekly")) {
            query = "DROP EVENT `chanstats_event_cleanup_weekly`";
            this->RunQuery(query);
        }
        query = "CREATE EVENT `chanstats_event_cleanup_weekly` "
                "ON SCHEDULE EVERY 1 WEEK STARTS ADDDATE(CURDATE(), INTERVAL 1-DAYOFWEEK(CURDATE()) DAY) "
                "DO UPDATE `" + prefix +
                "chanstats` SET letters=0, words=0, line=0, actions=0, smileys_happy=0,"
                "smileys_sad=0, smileys_other=0, kicks=0, modes=0, topics=0, time0=0, time1=0, time2=0,"
                "time3=0, time4=0, time5=0, time6=0, time7=0, time8=0, time9=0, time10=0, time11=0,"
                "time12=0, time13=0, time14=0, time15=0, time16=0, time17=0, time18=0, time19=0,"
                "time20=0, time21=0, time22=0, time23=0 "
                "WHERE type='weekly';";
        this->RunQuery(query);

        if (this->HasEvent("chanstats_event_cleanup_monthly")) {
            query = "DROP EVENT `chanstats_event_cleanup_monthly`;";
            this->RunQuery(query);
        }
        query = "CREATE EVENT `chanstats_event_cleanup_monthly` "
                "ON SCHEDULE EVERY 1 MONTH STARTS LAST_DAY(CURRENT_TIMESTAMP) + INTERVAL 1 DAY "
                "DO BEGIN "
                "UPDATE `" + prefix +
                "chanstats` SET letters=0, words=0, line=0, actions=0, smileys_happy=0,"
                "smileys_sad=0, smileys_other=0, kicks=0, modes=0, topics=0, time0=0, time1=0, time2=0,"
                "time3=0, time4=0, time5=0, time6=0, time7=0, time8=0, time9=0, time10=0, time11=0,"
                "time12=0, time13=0, time14=0, time15=0, time16=0, time17=0, time18=0, time19=0, "
                "time20=0, time21=0, time22=0, time23=0 "
                "WHERE type='monthly';"
                "OPTIMIZE TABLE `" + prefix + "chanstats`;"
                "END;";
        this->RunQuery(query);
    }


  public:
    MChanstats(const Anope::string &modname, const Anope::string &creator) :
        Module(modname, creator, EXTRA | VENDOR),
        cs_stats(this, "CS_STATS"), ns_stats(this, "NS_STATS"),
        commandcssetchanstats(this), commandnssetchanstats(this),
        commandnssasetchanstats(this),
        sqlinterface(this) {
    }

    void OnReload(Configuration::Conf *conf) anope_override {
        Configuration::Block *block = conf->GetModule(this);
        prefix = block->Get<const Anope::string>("prefix", "anope_");
        SmileysHappy = block->Get<const Anope::string>("SmileysHappy");
        SmileysSad = block->Get<const Anope::string>("SmileysSad");
        SmileysOther = block->Get<const Anope::string>("SmileysOther");
        NSDefChanstats = block->Get<bool>("ns_def_chanstats");
        CSDefChanstats = block->Get<bool>("cs_def_chanstats");
        Anope::string engine = block->Get<const Anope::string>("engine");
        this->sql = ServiceReference<SQL::Provider>("SQL::Provider", engine);
        if (sql) {
            this->CheckTables();
        } else {
            Log(this) << "no database connection to " << engine;
        }
    }

    void OnChanInfo(CommandSource &source, ChannelInfo *ci, InfoFormatter &info,
                    bool show_all) anope_override {
        if (!show_all) {
            return;
        }
        if (cs_stats.HasExt(ci)) {
            info.AddOption(_("Chanstats"));
        }
    }

    void OnNickInfo(CommandSource &source, NickAlias *na, InfoFormatter &info,
                    bool show_hidden) anope_override {
        if (!show_hidden) {
            return;
        }
        if (ns_stats.HasExt(na->nc)) {
            info.AddOption(_("Chanstats"));
        }
    }

    void OnTopicUpdated(User *source, Channel *c, const Anope::string &user,
                        const Anope::string &topic) anope_override {
        if (!source || !source->Account() || !c->ci || !cs_stats.HasExt(c->ci)) {
            return;
        }
        query = "CALL " + prefix + "chanstats_proc_update(@channel@, @nick@, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1);";
        query.SetValue("channel", c->name);
        query.SetValue("nick", GetDisplay(source));
        this->RunQuery(query);
    }

    EventReturn OnChannelModeSet(Channel *c, MessageSource &setter,
                                 ChannelMode *mode, const Anope::string &param) anope_override {
        this->OnModeChange(c, setter.GetUser());
        return EVENT_CONTINUE;
    }

    EventReturn OnChannelModeUnset(Channel *c, MessageSource &setter, ChannelMode *,
                                   const Anope::string &param) anope_override {
        this->OnModeChange(c, setter.GetUser());
        return EVENT_CONTINUE;
    }

  private:
    void OnModeChange(Channel *c, User *u) {
        if (!u || !u->Account() || !c->ci || !cs_stats.HasExt(c->ci)) {
            return;
        }

        query = "CALL " + prefix +
                "chanstats_proc_update(@channel@, @nick@, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0);";
        query.SetValue("channel", c->name);
        query.SetValue("nick", GetDisplay(u));
        this->RunQuery(query);
    }

  public:
    void OnPreUserKicked(const MessageSource &source, ChanUserContainer *cu,
                         const Anope::string &kickmsg) anope_override {
        if (!cu->chan->ci || !cs_stats.HasExt(cu->chan->ci)) {
            return;
        }

        query = "CALL " + prefix + "chanstats_proc_update(@channel@, @nick@, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0);";
        query.SetValue("channel", cu->chan->name);
        query.SetValue("nick", GetDisplay(cu->user));
        this->RunQuery(query);

        query = "CALL " + prefix + "chanstats_proc_update(@channel@, @nick@, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0);";
        query.SetValue("channel", cu->chan->name);
        query.SetValue("nick", GetDisplay(source.GetUser()));
        this->RunQuery(query);
    }

    void OnPrivmsg(User *u, Channel *c, Anope::string &msg) anope_override {
        if (!c->ci || !cs_stats.HasExt(c->ci)) {
            return;
        }

        size_t letters = msg.length();
        size_t words = this->CountWords(msg);

        size_t action = 0;
        if (msg.find("\01ACTION")!=Anope::string::npos) {
            action = 1;
            letters = letters - 7;
            words--;
        }

        // count smileys
        size_t smileys_happy = CountSmileys(msg, SmileysHappy);
        size_t smileys_sad = CountSmileys(msg, SmileysSad);
        size_t smileys_other = CountSmileys(msg, SmileysOther);

        // do not count smileys as words
        size_t smileys = smileys_happy + smileys_sad + smileys_other;
        if (smileys > words) {
            words = 0;
        } else {
            words = words - smileys;
        }

        query = "CALL " + prefix + "chanstats_proc_update(@channel@, @nick@, 1, @letters@, @words@, @action@, "
        "@smileys_happy@, @smileys_sad@, @smileys_other@, '0', '0', '0', '0');";
        query.SetValue("channel", c->name);
        query.SetValue("nick", GetDisplay(u));
        query.SetValue("letters", letters);
        query.SetValue("words", words);
        query.SetValue("action", action);
        query.SetValue("smileys_happy", smileys_happy);
        query.SetValue("smileys_sad", smileys_sad);
        query.SetValue("smileys_other", smileys_other);
        this->RunQuery(query);
    }

    void OnDelCore(NickCore *nc) anope_override {
        query = "DELETE FROM `" + prefix + "chanstats` WHERE `nick` = @nick@;";
        query.SetValue("nick", nc->display);
        this->RunQuery(query);
    }

    void OnChangeCoreDisplay(NickCore *nc,
                             const Anope::string &newdisplay) anope_override {
        query = "CALL " + prefix + "chanstats_proc_chgdisplay(@old_display@, @new_display@);";
        query.SetValue("old_display", nc->display);
        query.SetValue("new_display", newdisplay);
        this->RunQuery(query);
    }

    void OnDelChan(ChannelInfo *ci) anope_override {
        query = "DELETE FROM `" + prefix + "chanstats` WHERE `chan` = @channel@;";
        query.SetValue("channel", ci->name);
        this->RunQuery(query);
    }

    void OnChanRegistered(ChannelInfo *ci) {
        if (CSDefChanstats) {
            ci->Extend<bool>("CS_STATS");
        }
    }

    void OnNickRegister(User *user, NickAlias *na, const Anope::string &) {
        if (NSDefChanstats) {
            na->nc->Extend<bool>("NS_STATS");
        }
    }
};

MODULE_INIT(MChanstats)
