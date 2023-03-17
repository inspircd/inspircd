/*
 *
 * (C) 2013-2023 Anope Team
 * Contact us at team@anope.org
 *
 * Please read COPYING and README for further details.
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
            Log(LOG_DEBUG) << "m_irc2sql: Error executing query " << r.finished_query <<
                           ": " << r.GetError();
        } else {
            Log(LOG_DEBUG) << "m_irc2sql: Error executing query: " << r.GetError();
        }
    }
};

class IRC2SQL : public Module {
    ServiceReference<SQL::Provider> sql;
    MySQLInterface sqlinterface;
    SQL::Query query;
    std::vector<Anope::string> TableList, ProcedureList, EventList;
    Anope::string prefix, GeoIPDB;
    bool quitting, introduced_myself, ctcpuser, ctcpeob, firstrun;
    BotInfo *StatServ;
    PrimitiveExtensibleItem<bool> versionreply;

    void RunQuery(const SQL::Query &q);
    void GetTables();

    bool HasTable(const Anope::string &table);
    bool HasProcedure(const Anope::string &table);
    bool HasEvent(const Anope::string &table);

    void CheckTables();

  public:
    IRC2SQL(const Anope::string &modname, const Anope::string &creator) :
        Module(modname, creator, EXTRA | VENDOR), sql("", ""), sqlinterface(this),
        versionreply(this, "CTCPVERSION") {
        firstrun = true;
        quitting = false;
        introduced_myself = false;
    }

    void OnShutdown() anope_override;
    void OnReload(Configuration::Conf *config) anope_override;
    void OnNewServer(Server *server) anope_override;
    void OnServerQuit(Server *server) anope_override;
    void OnUserConnect(User *u, bool &exempt) anope_override;
    void OnUserQuit(User *u, const Anope::string &msg) anope_override;
    void OnUserNickChange(User *u, const Anope::string &oldnick) anope_override;
    void OnUserAway(User *u, const Anope::string &message) anope_override;
    void OnFingerprint(User *u) anope_override;
    void OnUserModeSet(const MessageSource &setter, User *u,
                       const Anope::string &mname) anope_override;
    void OnUserModeUnset(const MessageSource &setter, User *u,
                         const Anope::string &mname) anope_override;
    void OnUserLogin(User *u) anope_override;
    void OnNickLogout(User *u) anope_override;
    void OnSetDisplayedHost(User *u) anope_override;

    void OnChannelCreate(Channel *c) anope_override;
    void OnChannelDelete(Channel *c) anope_override;
    void OnLeaveChannel(User *u, Channel *c) anope_override;
    void OnJoinChannel(User *u, Channel *c) anope_override;
    EventReturn OnChannelModeSet(Channel *c, MessageSource &setter,
                                 ChannelMode *mode, const Anope::string &param) anope_override;
    EventReturn OnChannelModeUnset(Channel *c, MessageSource &setter,
                                   ChannelMode *mode, const Anope::string &param) anope_override;

    void OnTopicUpdated(User *source, Channel *c, const Anope::string &user,
                        const Anope::string &topic) anope_override;

    void OnBotNotice(User *u, BotInfo *bi, Anope::string &message) anope_override;
};
