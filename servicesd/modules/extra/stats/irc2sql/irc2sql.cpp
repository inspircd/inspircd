/*
 *
 * (C) 2013-2023 Anope Team
 * Contact us at team@anope.org
 *
 * Please read COPYING and README for further details.
 */

#include "irc2sql.h"

void IRC2SQL::OnShutdown() {
    // TODO: test if we really have to use blocking query here
    // (sometimes m_mysql get unloaded before the other thread executed all queries)
    if (this->sql) {
        SQL::Result r = this->sql->RunQuery(SQL::Query("CALL " + prefix +
                                            "OnShutdown()"));
    }
    quitting = true;
}

void IRC2SQL::OnReload(Configuration::Conf *conf) {
    Configuration::Block *block = Config->GetModule(this);
    prefix = block->Get<const Anope::string>("prefix", "anope_");
    GeoIPDB = block->Get<const Anope::string>("geoip_database");
    ctcpuser = block->Get<bool>("ctcpuser", "no");
    ctcpeob = block->Get<bool>("ctcpeob", "yes");
    Anope::string engine = block->Get<const Anope::string>("engine");
    this->sql = ServiceReference<SQL::Provider>("SQL::Provider", engine);
    if (sql) {
        this->CheckTables();
    } else {
        Log() << "IRC2SQL: no database connection to " << engine;
    }

    const Anope::string &snick = block->Get<const Anope::string>("client");
    if (snick.empty()) {
        throw ConfigException(Module::name + ": <client> must be defined");
    }
    StatServ = BotInfo::Find(snick, true);
    if (!StatServ) {
        throw ConfigException(Module::name + ": no bot named " + snick);
    }

    if (firstrun) {
        firstrun = false;

        for (Anope::map<Server *>::const_iterator it = Servers::ByName.begin();
                it != Servers::ByName.end(); ++it) {
            this->OnNewServer(it->second);
        }

        for (channel_map::const_iterator it = ChannelList.begin(),
                it_end = ChannelList.end(); it != it_end; ++it) {
            this->OnChannelCreate(it->second);
        }

        for (user_map::const_iterator it = UserListByNick.begin();
                it != UserListByNick.end(); ++it) {
            User *u = it->second;
            bool exempt = false;
            this->OnUserConnect(u, exempt);
            for (User::ChanUserList::const_iterator cit = u->chans.begin(),
                    cit_end = u->chans.end(); cit != cit_end; ++cit) {
                this->OnJoinChannel(u, cit->second->chan);
            }
        }
    }

}

void IRC2SQL::OnNewServer(Server *server) {
    query = "INSERT DELAYED INTO `" + prefix +
            "server` (name, hops, comment, link_time, online, ulined) "
            "VALUES (@name@, @hops@, @comment@, now(), 'Y', @ulined@) "
            "ON DUPLICATE KEY UPDATE name=VALUES(name), hops=VALUES(hops), comment=VALUES(comment), "
            "link_time=VALUES(link_time), online=VALUES(online), ulined=VALUES(ulined)";
    query.SetValue("name", server->GetName());
    query.SetValue("hops", server->GetHops());
    query.SetValue("comment", server->GetDescription());
    query.SetValue("ulined", server->IsULined() ? "Y" : "N");
    this->RunQuery(query);
}

void IRC2SQL::OnServerQuit(Server *server) {
    if (quitting) {
        return;
    }

    query = "CALL " + prefix + "ServerQuit(@name@)";
    query.SetValue("name", server->GetName());
    this->RunQuery(query);
}

void IRC2SQL::OnUserConnect(User *u, bool &exempt) {
    if (!introduced_myself) {
        this->OnNewServer(Me);
        introduced_myself = true;
    }

    query = "CALL " + prefix +
            "UserConnect(@nick@,@host@,@vhost@,@chost@,@realname@,@ip@,@ident@,@vident@,"
            "@account@,@secure@,@fingerprint@,@signon@,@server@,@uuid@,@modes@,@oper@)";
    query.SetValue("nick", u->nick);
    query.SetValue("host", u->host);
    query.SetValue("vhost", u->vhost);
    query.SetValue("chost", u->chost);
    query.SetValue("realname", u->realname);
    query.SetValue("ip", u->ip.addr());
    query.SetValue("ident", u->GetIdent());
    query.SetValue("vident", u->GetVIdent());
    query.SetValue("secure", u->HasMode("SSL") || u->HasExt("ssl") ? "Y" : "N");
    query.SetValue("account", u->Account() ? u->Account()->display : "");
    query.SetValue("fingerprint", u->fingerprint);
    query.SetValue("signon", u->signon);
    query.SetValue("server", u->server->GetName());
    query.SetValue("uuid", u->GetUID());
    query.SetValue("modes", u->GetModes());
    query.SetValue("oper", u->HasMode("OPER") ? "Y" : "N");
    this->RunQuery(query);

    if (ctcpuser && (Me->IsSynced() || ctcpeob) && u->server != Me) {
        IRCD->SendPrivmsg(StatServ, u->GetUID(), "\1VERSION\1");
    }

}

void IRC2SQL::OnUserQuit(User *u, const Anope::string &msg) {
    if (quitting || u->server->IsQuitting()) {
        return;
    }

    query = "CALL " + prefix + "UserQuit(@nick@)";
    query.SetValue("nick", u->nick);
    this->RunQuery(query);
}

void IRC2SQL::OnUserNickChange(User *u, const Anope::string &oldnick) {
    query = "UPDATE `" + prefix + "user` SET nick=@newnick@ WHERE nick=@oldnick@";
    query.SetValue("newnick", u->nick);
    query.SetValue("oldnick", oldnick);
    this->RunQuery(query);
}

void IRC2SQL::OnUserAway(User *u, const Anope::string &message) {
    query = "UPDATE `" + prefix +
            "user` SET away=@away@, awaymsg=@awaymsg@ WHERE nick=@nick@";
    query.SetValue("away", (!message.empty()) ? "Y" : "N");
    query.SetValue("awaymsg", message);
    query.SetValue("nick", u->nick);
    this->RunQuery(query);
}

void IRC2SQL::OnFingerprint(User *u) {
    query = "UPDATE `" + prefix +
            "user` SET secure=@secure@, fingerprint=@fingerprint@ WHERE nick=@nick@";
    query.SetValue("secure", u->HasMode("SSL") || u->HasExt("ssl") ? "Y" : "N");
    query.SetValue("fingerprint", u->fingerprint);
    query.SetValue("nick", u->nick);
    this->RunQuery(query);
}

void IRC2SQL::OnUserModeSet(const MessageSource &setter, User *u,
                            const Anope::string &mname) {
    query = "UPDATE `" + prefix +
            "user` SET modes=@modes@, oper=@oper@ WHERE nick=@nick@";
    query.SetValue("nick", u->nick);
    query.SetValue("modes", u->GetModes());
    query.SetValue("oper", u->HasMode("OPER") ? "Y" : "N");
    this->RunQuery(query);
}

void IRC2SQL::OnUserModeUnset(const MessageSource &setter, User *u,
                              const Anope::string &mname) {
    this->OnUserModeSet(setter, u, mname);
}

void IRC2SQL::OnUserLogin(User *u) {
    query = "UPDATE `" + prefix + "user` SET account=@account@ WHERE nick=@nick@";
    query.SetValue("nick", u->nick);
    query.SetValue("account", u->Account() ? u->Account()->display : "");
    this->RunQuery(query);
}

void IRC2SQL::OnNickLogout(User *u) {
    this->OnUserLogin(u);
}

void IRC2SQL::OnSetDisplayedHost(User *u) {
    query = "UPDATE `" + prefix + "user` "
            "SET vhost=@vhost@ "
            "WHERE nick=@nick@";
    query.SetValue("vhost", u->GetDisplayedHost());
    query.SetValue("nick", u->nick);
    this->RunQuery(query);
}

void IRC2SQL::OnChannelCreate(Channel *c) {
    query = "INSERT INTO `" + prefix +
            "chan` (channel, topic, topicauthor, topictime, modes) "
            "VALUES (@channel@,@topic@,@topicauthor@,@topictime@,@modes@) "
            "ON DUPLICATE KEY UPDATE channel=VALUES(channel), topic=VALUES(topic),"
            "topicauthor=VALUES(topicauthor), topictime=VALUES(topictime), modes=VALUES(modes)";
    query.SetValue("channel", c->name);
    query.SetValue("topic", c->topic);
    query.SetValue("topicauthor", c->topic_setter);
    if (c->topic_ts > 0) {
        query.SetValue("topictime", c->topic_ts);
    } else {
        query.SetValue("topictime", "NULL", false);
    }
    query.SetValue("modes", c->GetModes(true,true));
    this->RunQuery(query);
}

void IRC2SQL::OnChannelDelete(Channel *c) {
    query = "DELETE FROM `" + prefix + "chan` WHERE channel=@channel@";
    query.SetValue("channel",  c->name);
    this->RunQuery(query);
}

void IRC2SQL::OnJoinChannel(User *u, Channel *c) {
    Anope::string modes;
    ChanUserContainer *cu = u->FindChannel(c);
    if (cu) {
        modes = cu->status.Modes();
    }

    query = "CALL " + prefix + "JoinUser(@nick@,@channel@,@modes@)";
    query.SetValue("nick", u->nick);
    query.SetValue("channel", c->name);
    query.SetValue("modes", modes);
    this->RunQuery(query);
}

EventReturn IRC2SQL::OnChannelModeSet(Channel *c, MessageSource &setter,
                                      ChannelMode *mode, const Anope::string &param) {
    if (mode->type == MODE_STATUS) {
        User *u = User::Find(param);
        if (u == NULL) {
            return EVENT_CONTINUE;
        }

        ChanUserContainer *cc = u->FindChannel(c);
        if (cc == NULL) {
            return EVENT_CONTINUE;
        }

        query = "UPDATE `" + prefix + "user` AS u, `" + prefix + "ison` AS i, `" +
                prefix + "chan` AS c"
                " SET i.modes=@modes@"
                " WHERE u.nick=@nick@ AND c.channel=@channel@"
                " AND u.nickid = i.nickid AND c.chanid = i.chanid";
        query.SetValue("nick", u->nick);
        query.SetValue("modes", cc->status.Modes());
        query.SetValue("channel", c->name);
        this->RunQuery(query);
    } else {
        query = "UPDATE `" + prefix + "chan` SET modes=@modes@ WHERE channel=@channel@";
        query.SetValue("channel", c->name);
        query.SetValue("modes", c->GetModes(true,true));
        this->RunQuery(query);
    }
    return EVENT_CONTINUE;
}

EventReturn IRC2SQL::OnChannelModeUnset(Channel *c, MessageSource &setter,
                                        ChannelMode *mode, const Anope::string &param) {
    this->OnChannelModeSet(c, setter, mode, param);
    return EVENT_CONTINUE;
}

void IRC2SQL::OnLeaveChannel(User *u, Channel *c) {
    if (quitting) {
        return;
    }
    /*
     * user is quitting, we already received a OnUserQuit()
     * at this point the user is already removed from SQL and all channels
     */
    if (u->Quitting()) {
        return;
    }
    query = "CALL " + prefix + "PartUser(@nick@,@channel@)";
    query.SetValue("nick", u->nick);
    query.SetValue("channel", c->name);
    this->RunQuery(query);
}

void IRC2SQL::OnTopicUpdated(User *source, Channel *c,
                             const Anope::string &user, const Anope::string &topic) {
    query = "UPDATE `" + prefix + "chan` "
            "SET topic=@topic@, topicauthor=@author@, topictime=FROM_UNIXTIME(@time@) "
            "WHERE channel=@channel@";
    query.SetValue("topic", c->topic);
    query.SetValue("author", c->topic_setter);
    query.SetValue("time", c->topic_ts);
    query.SetValue("channel", c->name);
    this->RunQuery(query);
}

void IRC2SQL::OnBotNotice(User *u, BotInfo *bi, Anope::string &message) {
    Anope::string versionstr;
    if (bi != StatServ) {
        return;
    }
    if (message[0] == '\1' && message[message.length() - 1] == '\1') {
        if (message.substr(0, 9).equals_ci("\1VERSION ")) {
            if (u->HasExt("CTCPVERSION")) {
                return;
            }
            u->Extend<bool>("CTCPVERSION");

            versionstr = Anope::NormalizeBuffer(message.substr(9, message.length() - 10));
            if (versionstr.empty()) {
                return;
            }
            query = "UPDATE `" + prefix + "user` "
                    "SET version=@version@ "
                    "WHERE nick=@nick@";
            query.SetValue("version", versionstr);
            query.SetValue("nick", u->nick);
            this->RunQuery(query);
        }
    }
}

MODULE_INIT(IRC2SQL)
