/* Common message handlers
 *
 * (C) 2003-2023 Anope Team
 * Contact us at team@anope.org
 *
 * Please read COPYING and README for further details.
 *
 * Based on the original code of Epona by Lara.
 * Based on the original code of Services by Andy Church.
 */

#include "services.h"
#include "modules.h"
#include "users.h"
#include "protocol.h"
#include "config.h"
#include "uplink.h"
#include "opertype.h"
#include "messages.h"
#include "servers.h"
#include "channels.h"

using namespace Message;

void Away::Run(MessageSource &source,
               const std::vector<Anope::string> &params) {
    const Anope::string &msg = !params.empty() ? params[0] : "";

    FOREACH_MOD(OnUserAway, (source.GetUser(), msg));
    if (!msg.empty()) {
        Log(source.GetUser(), "away") << "is now away: " << msg;
    } else {
        Log(source.GetUser(), "away") << "is no longer away";
    }
}

void Capab::Run(MessageSource &source,
                const std::vector<Anope::string> &params) {
    if (params.size() == 1) {
        spacesepstream sep(params[0]);
        Anope::string token;
        while (sep.GetToken(token)) {
            Servers::Capab.insert(token);
        }
    } else
        for (unsigned i = 0; i < params.size(); ++i) {
            Servers::Capab.insert(params[i]);
        }
}

void Error::Run(MessageSource &source,
                const std::vector<Anope::string> &params) {
    Log(LOG_TERMINAL) << "ERROR: " << params[0];
    Anope::QuitReason = "Received ERROR from uplink: " + params[0];
    Anope::Quitting = true;
}

void Invite::Run(MessageSource &source,
                 const std::vector<Anope::string> &params) {
    User *targ = User::Find(params[0]);
    Channel *c = Channel::Find(params[1]);

    if (!targ || targ->server != Me || !c || c->FindUser(targ)) {
        return;
    }

    FOREACH_MOD(OnInvite, (source.GetUser(), c, targ));
}

void Join::Run(MessageSource &source,
               const std::vector<Anope::string> &params) {
    User *user = source.GetUser();
    const Anope::string &channels = params[0];

    Anope::string channel;
    commasepstream sep(channels);

    while (sep.GetToken(channel)) {
        /* Special case for /join 0 */
        if (channel == "0") {
            for (User::ChanUserList::iterator it = user->chans.begin(),
                    it_end = user->chans.end(); it != it_end; ) {
                ChanUserContainer *cc = it->second;
                Channel *c = cc->chan;
                ++it;

                FOREACH_MOD(OnPrePartChannel, (user, c));
                cc->chan->DeleteUser(user);
                FOREACH_MOD(OnPartChannel, (user, c, c->name, ""));
            }
            continue;
        }

        std::list<SJoinUser> users;
        users.push_back(std::make_pair(ChannelStatus(), user));

        Channel *chan = Channel::Find(channel);
        SJoin(source, channel, chan ? chan->creation_time : Anope::CurTime, "", users);
    }
}

void Join::SJoin(MessageSource &source, const Anope::string &chan, time_t ts,
                 const Anope::string &modes, const std::list<SJoinUser> &users) {
    bool created;
    Channel *c = Channel::FindOrCreate(chan, created, ts ? ts : Anope::CurTime);
    bool keep_their_modes = true;

    if (created) {
        c->syncing = true;
    }
    /* Some IRCds do not include a TS */
    else if (!ts)
        ;
    /* Our creation time is newer than what the server gave us, so reset the channel to the older time */
    else if (c->creation_time > ts) {
        c->creation_time = ts;
        c->Reset();
    }
    /* Their TS is newer, don't accept any modes from them */
    else if (ts > c->creation_time) {
        keep_their_modes = false;
    }

    /* Update the modes for the channel */
    if (keep_their_modes && !modes.empty())
        /* If we are syncing, mlock is checked later in Channel::Sync. It is important to not check it here
         * so that Channel::SetCorrectModes can correctly detect the presence of channel mode +r.
         */
    {
        c->SetModesInternal(source, modes, ts, !c->syncing);
    }

    for (std::list<SJoinUser>::const_iterator it = users.begin(),
            it_end = users.end(); it != it_end; ++it) {
        const ChannelStatus &status = it->first;
        User *u = it->second;
        keep_their_modes = ts <=
                           c->creation_time; // OnJoinChannel can call modules which can modify this channel's ts

        if (c->FindUser(u)) {
            continue;
        }

        /* Add the user to the channel */
        c->JoinUser(u, keep_their_modes ? &status : NULL);

        /* Check if the user is allowed to join */
        if (c->CheckKick(u)) {
            continue;
        }

        /* Set whatever modes the user should have, and remove any that
         * they aren't allowed to have (secureops etc).
         */
        c->SetCorrectModes(u, true);

        FOREACH_MOD(OnJoinChannel, (u, c));
    }

    /* Channel is done syncing */
    if (c->syncing) {
        /* Sync the channel (mode lock, topic, etc) */
        /* the channel is synced when the netmerge is complete */
        Server *src = source.GetServer() ? source.GetServer() : Me;
        if (src && src->IsSynced()) {
            c->Sync();

            if (c->CheckDelete()) {
                c->QueueForDeletion();
            }
        }
    }
}

void Kick::Run(MessageSource &source,
               const std::vector<Anope::string> &params) {
    const Anope::string &channel = params[0];
    const Anope::string &users = params[1];
    const Anope::string &reason = params.size() > 2 ? params[2] : "";

    Channel *c = Channel::Find(channel);
    if (!c) {
        return;
    }

    Anope::string user;
    commasepstream sep(users);

    while (sep.GetToken(user)) {
        c->KickInternal(source, user, reason);
    }
}

void Kill::Run(MessageSource &source,
               const std::vector<Anope::string> &params) {
    User *u = User::Find(params[0]);
    BotInfo *bi;

    if (!u) {
        return;
    }

    /* Recover if someone kills us. */
    if (u->server == Me && (bi = dynamic_cast<BotInfo *>(u))) {
        static time_t last_time = 0;

        if (last_time == Anope::CurTime) {
            Anope::QuitReason = "Kill loop detected. Are Services U:Lined?";
            Anope::Quitting = true;
            return;
        }
        last_time = Anope::CurTime;

        bi->OnKill();
    } else {
        u->KillInternal(source, params[1]);
    }
}

void Message::Mode::Run(MessageSource &source,
                        const std::vector<Anope::string> &params) {
    Anope::string buf;
    for (unsigned i = 1; i < params.size(); ++i) {
        buf += " " + params[i];
    }

    if (IRCD->IsChannelValid(params[0])) {
        Channel *c = Channel::Find(params[0]);

        if (c) {
            c->SetModesInternal(source, buf.substr(1), 0);
        }
    } else {
        User *u = User::Find(params[0]);

        if (u) {
            u->SetModesInternal(source, "%s", buf.substr(1).c_str());
        }
    }
}

/* XXX We should cache the file somewhere not open/read/close it on every request */
void MOTD::Run(MessageSource &source,
               const std::vector<Anope::string> &params) {
    Server *s = Server::Find(params[0]);
    if (s != Me) {
        return;
    }

    FILE *f = fopen(
                  Config->GetBlock("serverinfo")->Get<const Anope::string>("motd").c_str(), "r");
    if (f) {
        IRCD->SendNumeric(375, source.GetSource(), ":- %s Message of the Day",
                          s->GetName().c_str());
        char buf[BUFSIZE];
        while (fgets(buf, sizeof(buf), f)) {
            buf[strlen(buf) - 1] = 0;
            IRCD->SendNumeric(372, source.GetSource(), ":- %s", buf);
        }
        fclose(f);
        IRCD->SendNumeric(376, source.GetSource(), ":End of /MOTD command.");
    } else {
        IRCD->SendNumeric(422, source.GetSource(),
                          ":- MOTD file not found!  Please contact your IRC administrator.");
    }
}

void Notice::Run(MessageSource &source,
                 const std::vector<Anope::string> &params) {
    Anope::string message = params[1];

    User *u = source.GetUser();

    /* ignore channel notices */
    if (!IRCD->IsChannelValid(params[0])) {
        BotInfo *bi = BotInfo::Find(params[0]);
        if (!bi) {
            return;
        }
        FOREACH_MOD(OnBotNotice, (u, bi, message));
    }
}

void Part::Run(MessageSource &source,
               const std::vector<Anope::string> &params) {
    User *u = source.GetUser();
    const Anope::string &reason = params.size() > 1 ? params[1] : "";

    Anope::string channel;
    commasepstream sep(params[0]);

    while (sep.GetToken(channel)) {
        Channel *c = Channel::Find(channel);

        if (!c || !u->FindChannel(c)) {
            continue;
        }

        Log(u, c, "part") << "Reason: " << (!reason.empty() ? reason : "No reason");
        FOREACH_MOD(OnPrePartChannel, (u, c));
        c->DeleteUser(u);
        FOREACH_MOD(OnPartChannel, (u, c, c->name, !reason.empty() ? reason : ""));
    }
}

void Ping::Run(MessageSource &source,
               const std::vector<Anope::string> &params) {
    IRCD->SendPong(params.size() > 1 ? params[1] : Me->GetSID(), params[0]);
}

void Privmsg::Run(MessageSource &source,
                  const std::vector<Anope::string> &params) {
    const Anope::string &receiver = params[0];
    Anope::string message = params[1];

    User *u = source.GetUser();

    if (IRCD->IsChannelValid(receiver)) {
        Channel *c = Channel::Find(receiver);
        if (c) {
            FOREACH_MOD(OnPrivmsg, (u, c, message));
        }
    } else {
        /* If a server is specified (nick@server format), make sure it matches
         * us, and strip it off. */
        Anope::string botname = receiver;
        size_t s = receiver.find('@');
        bool nick_only = false;
        if (s != Anope::string::npos) {
            Anope::string servername(receiver.begin() + s + 1, receiver.end());
            botname = botname.substr(0, s);
            nick_only = true;
            if (!servername.equals_ci(Me->GetName())) {
                return;
            }
        } else if (!IRCD->RequiresID && Config->UseStrictPrivmsg) {
            BotInfo *bi = BotInfo::Find(receiver);
            if (!bi) {
                return;
            }
            Log(LOG_DEBUG) << "Ignored PRIVMSG without @ from " << u->nick;
            u->SendMessage(bi,
                           _("\"/msg %s\" is no longer supported.  Use \"/msg %s@%s\" or \"/%s\" instead."),
                           bi->nick.c_str(), bi->nick.c_str(), Me->GetName().c_str(), bi->nick.c_str());
            return;
        }

        BotInfo *bi = BotInfo::Find(botname, nick_only);

        if (bi) {
            if (message[0] == '\1' && message[message.length() - 1] == '\1') {
                if (message.substr(0, 6).equals_ci("\1PING ")) {
                    Anope::string buf = message;
                    buf.erase(buf.begin());
                    buf.erase(buf.end() - 1);
                    IRCD->SendCTCP(bi, u->nick, "%s", buf.c_str());
                } else if (message.substr(0, 9).equals_ci("\1VERSION\1")) {
                    Module *enc = ModuleManager::FindFirstOf(ENCRYPTION);
                    IRCD->SendCTCP(bi, u->nick, "VERSION Anope-%s %s :%s - (%s) -- %s",
                                   Anope::Version().c_str(), Me->GetName().c_str(),
                                   IRCD->GetProtocolName().c_str(), enc ? enc->name.c_str() : "(none)",
                                   Anope::VersionBuildString().c_str());
                }
                return;
            }

            EventReturn MOD_RESULT;
            FOREACH_RESULT(OnBotPrivmsg, MOD_RESULT, (u, bi, message));
            if (MOD_RESULT == EVENT_STOP) {
                return;
            }

            bi->OnMessage(u, message);
        }
    }

    return;
}

void Quit::Run(MessageSource &source,
               const std::vector<Anope::string> &params) {
    const Anope::string &reason = params[0];
    User *user = source.GetUser();

    Log(user, "quit") << "quit (Reason: " << (!reason.empty() ? reason :
                      "no reason") << ")";

    user->Quit(reason);
}

void SQuit::Run(MessageSource &source,
                const std::vector<Anope::string> &params) {
    Server *s = Server::Find(params[0]);

    if (!s) {
        Log(LOG_DEBUG) << "SQUIT for nonexistent server " << params[0];
        return;
    }

    if (s == Me) {
        if (Me->GetLinks().empty()) {
            return;
        }

        s = Me->GetLinks().front();
    }

    s->Delete(s->GetName() + " " + s->GetUplink()->GetName());
}

void Stats::Run(MessageSource &source,
                const std::vector<Anope::string> &params) {
    User *u = source.GetUser();

    switch (params[0][0]) {
    case 'l':
        if (u->HasMode("OPER")) {
            IRCD->SendNumeric(211, source.GetSource(),
                              "Server SendBuf SentBytes SentMsgs RecvBuf RecvBytes RecvMsgs ConnTime");
            IRCD->SendNumeric(211, source.GetSource(), "%s %d %d %d %d %d %d %ld",
                              Config->Uplinks[Anope::CurrentUplink].host.c_str(),
                              UplinkSock->WriteBufferLen(), TotalWritten, -1, UplinkSock->ReadBufferLen(),
                              TotalRead, -1, static_cast<long>(Anope::CurTime - Anope::StartTime));
        }

        IRCD->SendNumeric(219, source.GetSource(), "%c :End of /STATS report.",
                          params[0][0]);
        break;
    case 'o':
    case 'O':
        /* Check whether the user is an operator */
        if (!u->HasMode("OPER")
                && Config->GetBlock("options")->Get<bool>("hidestatso")) {
            IRCD->SendNumeric(219, source.GetSource(), "%c :End of /STATS report.",
                              params[0][0]);
        } else {
            for (unsigned i = 0; i < Oper::opers.size(); ++i) {
                Oper *o = Oper::opers[i];

                const NickAlias *na = NickAlias::Find(o->name);
                if (na) {
                    IRCD->SendNumeric(243, source.GetSource(), "O * * %s %s 0", o->name.c_str(),
                                      o->ot->GetName().replace_all_cs(" ", "_").c_str());
                }
            }

            IRCD->SendNumeric(219, source.GetSource(), "%c :End of /STATS report.",
                              params[0][0]);
        }

        break;
    case 'u': {
        long uptime = static_cast<long>(Anope::CurTime - Anope::StartTime);
        IRCD->SendNumeric(242, source.GetSource(),
                          ":Services up %d day%s, %02d:%02d:%02d", uptime / 86400,
                          uptime / 86400 == 1 ? "" : "s", (uptime / 3600) % 24, (uptime / 60) % 60,
                          uptime % 60);
        IRCD->SendNumeric(250, source.GetSource(),
                          ":Current users: %d (%d ops); maximum %d", UserListByNick.size(), OperCount,
                          MaxUserCount);
        IRCD->SendNumeric(219, source.GetSource(), "%c :End of /STATS report.",
                          params[0][0]);
        break;
        } /* case 'u' */

    default:
        IRCD->SendNumeric(219, source.GetSource(), "%c :End of /STATS report.",
                          params[0][0]);
    }

    return;
}

void Time::Run(MessageSource &source,
               const std::vector<Anope::string> &params) {
    time_t t;
    time(&t);
    struct tm *tm = localtime(&t);
    char buf[64];
    strftime(buf, sizeof(buf), "%a %b %d %H:%M:%S %Y %Z", tm);
    IRCD->SendNumeric(391, source.GetSource(), "%s :%s", Me->GetName().c_str(),
                      buf);
    return;
}

void Topic::Run(MessageSource &source,
                const std::vector<Anope::string> &params) {
    Channel *c = Channel::Find(params[0]);
    if (c) {
        c->ChangeTopicInternal(source.GetUser(), source.GetSource(), params[1],
                               Anope::CurTime);
    }

    return;
}

void Version::Run(MessageSource &source,
                  const std::vector<Anope::string> &params) {
    Module *enc = ModuleManager::FindFirstOf(ENCRYPTION);
    IRCD->SendNumeric(351, source.GetSource(), "Anope-%s %s :%s -(%s) -- %s",
                      Anope::Version().c_str(), Me->GetName().c_str(),
                      IRCD->GetProtocolName().c_str(), enc ? enc->name.c_str() : "(none)",
                      Anope::VersionBuildString().c_str());
}

void Whois::Run(MessageSource &source,
                const std::vector<Anope::string> &params) {
    User *u = User::Find(params[0]);

    if (u && u->server == Me) {
        const BotInfo *bi = BotInfo::Find(u->GetUID());
        IRCD->SendNumeric(311, source.GetSource(), "%s %s %s * :%s", u->nick.c_str(),
                          u->GetIdent().c_str(), u->host.c_str(), u->realname.c_str());
        if (bi) {
            IRCD->SendNumeric(307, source.GetSource(), "%s :is a registered nick",
                              bi->nick.c_str());
        }
        IRCD->SendNumeric(312, source.GetSource(), "%s %s :%s", u->nick.c_str(),
                          Me->GetName().c_str(),
                          Config->GetBlock("serverinfo")->Get<const Anope::string>("description").c_str());
        if (bi) {
            IRCD->SendNumeric(317, source.GetSource(),
                              "%s %ld %ld :seconds idle, signon time", bi->nick.c_str(),
                              static_cast<long>(Anope::CurTime - bi->lastmsg),
                              static_cast<long>(bi->signon));
        }
        IRCD->SendNumeric(313, source.GetSource(), "%s :is a Network Service",
                          u->nick.c_str());
        IRCD->SendNumeric(318, source.GetSource(), "%s :End of /WHOIS list.",
                          u->nick.c_str());
    } else {
        IRCD->SendNumeric(401, source.GetSource(), "%s :No such user.",
                          params[0].c_str());
    }
}
