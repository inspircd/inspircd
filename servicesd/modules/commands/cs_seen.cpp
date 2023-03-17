/* cs_seen: provides a seen command by tracking all users
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

enum TypeInfo {
    NEW, NICK_TO, NICK_FROM, JOIN, PART, QUIT, KICK
};

static bool simple;
struct SeenInfo;
static SeenInfo *FindInfo(const Anope::string &nick);
typedef Anope::hash_map<SeenInfo *> database_map;
database_map database;

struct SeenInfo : Serializable {
    Anope::string nick;
    Anope::string vhost;
    TypeInfo type;
    Anope::string nick2;    // for nickchanges and kicks
    Anope::string channel;  // for join/part/kick
    Anope::string message;  // for part/kick/quit
    time_t last;            // the time when the user was last seen

    SeenInfo() : Serializable("SeenInfo") {
    }

    ~SeenInfo() {
        database_map::iterator iter = database.find(nick);
        if (iter != database.end() && iter->second == this) {
            database.erase(iter);
        }
    }

    void Serialize(Serialize::Data &data) const anope_override {
        data["nick"] << nick;
        data["vhost"] << vhost;
        data["type"] << type;
        data["nick2"] << nick2;
        data["channel"] << channel;
        data["message"] << message;
        data.SetType("last", Serialize::Data::DT_INT);
        data["last"] << last;
    }

    static Serializable* Unserialize(Serializable *obj, Serialize::Data &data) {
        Anope::string snick;

        data["nick"] >> snick;

        SeenInfo *s;
        if (obj) {
            s = anope_dynamic_static_cast<SeenInfo *>(obj);
        } else {
            SeenInfo* &info = database[snick];
            if (!info) {
                info = new SeenInfo();
            }
            s = info;
        }

        s->nick = snick;
        data["vhost"] >> s->vhost;
        unsigned int n;
        data["type"] >> n;
        s->type = static_cast<TypeInfo>(n);
        data["nick2"] >> s->nick2;
        data["channel"] >> s->channel;
        data["message"] >> s->message;
        data["last"] >> s->last;

        if (!obj) {
            database[s->nick] = s;
        }
        return s;
    }
};

static SeenInfo *FindInfo(const Anope::string &nick) {
    database_map::iterator iter = database.find(nick);
    if (iter != database.end()) {
        return iter->second;
    }
    return NULL;
}

static bool ShouldHide(const Anope::string &channel, User *u) {
    Channel *targetchan = Channel::Find(channel);
    const ChannelInfo *targetchan_ci = targetchan ? *targetchan->ci :
                                       ChannelInfo::Find(channel);

    if (targetchan && targetchan->HasMode("SECRET")) {
        return true;
    } else if (targetchan_ci && targetchan_ci->HasExt("CS_PRIVATE")) {
        return true;
    } else if (u && u->HasMode("PRIV")) {
        return true;
    }
    return false;
}

class CommandOSSeen : public Command {
  public:
    CommandOSSeen(Module *creator) : Command(creator, "operserv/seen", 1, 2) {
        this->SetDesc(_("Statistics and maintenance for seen data"));
        this->SetSyntax("STATS");
        this->SetSyntax(_("CLEAR \037time\037"));
    }

    void Execute(CommandSource &source,
                 const std::vector<Anope::string> &params) anope_override {
        if (params[0].equals_ci("STATS")) {
            size_t mem_counter;
            mem_counter = sizeof(database_map);
            for (database_map::iterator it = database.begin(), it_end = database.end();
                    it != it_end; ++it) {
                mem_counter += (5 * sizeof(Anope::string)) + sizeof(TypeInfo) + sizeof(time_t);
                mem_counter += it->first.capacity();
                mem_counter += it->second->vhost.capacity();
                mem_counter += it->second->nick2.capacity();
                mem_counter += it->second->channel.capacity();
                mem_counter += it->second->message.capacity();
            }
            source.Reply(
                _("%lu nicks are stored in the database, using %.2Lf kB of memory."),
                database.size(), static_cast<long double>(mem_counter) / 1024);
        } else if (params[0].equals_ci("CLEAR")) {
            time_t time = 0;
            if ((params.size() < 2) || (0 >= (time = Anope::DoTime(params[1])))) {
                this->OnSyntaxError(source, params[0]);
                return;
            }
            time = Anope::CurTime - time;
            database_map::iterator buf;
            size_t counter = 0;
            for (database_map::iterator it = database.begin(), it_end = database.end();
                    it != it_end;) {
                buf = it;
                ++it;
                if (time < buf->second->last) {
                    Log(LOG_DEBUG) << buf->first << " was last seen " << Anope::strftime(
                                       buf->second->last) << ", deleting entry";
                    delete buf->second;
                    counter++;
                }
            }
            Log(LOG_ADMIN, source, this) << "CLEAR and removed " << counter <<
                                         " nicks that were added after " << Anope::strftime(time, NULL, true);
            source.Reply(_("Database cleared, removed %lu nicks that were added after %s."),
                         counter, Anope::strftime(time, source.nc, true).c_str());
        } else {
            this->SendSyntax(source);
        }
    }

    bool OnHelp(CommandSource &source,
                const Anope::string &subcommand) anope_override {
        this->SendSyntax(source);
        source.Reply(" ");
        source.Reply(_("The \002STATS\002 command prints out statistics about stored nicks and memory usage."));
        source.Reply(_("The \002CLEAR\002 command lets you clean the database by removing all entries from the\n"
                       "database that were added within \037time\037.\n"
                       " \n"
                       "Example:\n"
                       " %s CLEAR 30m\n"
                       " Will remove all entries that were added within the last 30 minutes."), source.command.c_str());
        return true;
    }
};

class CommandSeen : public Command {
    void SimpleSeen(CommandSource &source,
                    const std::vector<Anope::string> &params) {
        if (!source.c || !source.c->ci) {
            if (source.IsOper()) {
                source.Reply("Seen in simple mode is designed as a fantasy command only!");
            }
            return;
        }

        BotInfo *bi = BotInfo::Find(params[0], true);
        if (bi) {
            if (bi == source.c->ci->bi) {
                source.Reply(_("You found me, %s!"), source.GetNick().c_str());
            } else {
                source.Reply(_("%s is a network service."), bi->nick.c_str());
            }
            return;
        }

        NickAlias *na = NickAlias::Find(params[0]);
        if (!na) {
            source.Reply(_("I don't know who %s is."), params[0].c_str());
            return;
        }

        if (source.GetAccount() == na->nc) {
            source.Reply(_("Looking for yourself, eh %s?"), source.GetNick().c_str());
            return;
        }

        User *target = User::Find(params[0], true);

        if (target && source.c->FindUser(target)) {
            source.Reply(_("%s is on the channel right now!"), target->nick.c_str());
            return;
        }

        for (Channel::ChanUserList::const_iterator it = source.c->users.begin(),
                it_end = source.c->users.end(); it != it_end; ++it) {
            ChanUserContainer *uc = it->second;
            User *u = uc->user;

            if (u->Account() == na->nc) {
                source.Reply(_("%s is on the channel right now (as %s)!"), params[0].c_str(),
                             u->nick.c_str());
                return;
            }
        }

        AccessGroup ag = source.c->ci->AccessFor(na->nc);
        time_t last = 0;
        for (unsigned int i = 0; i < ag.paths.size(); ++i) {
            ChanAccess::Path &p = ag.paths[i];

            if (p.empty()) {
                continue;
            }

            ChanAccess *a = p[p.size() - 1];

            if (a->GetAccount() == na->nc && a->last_seen > last) {
                last = a->last_seen;
            }
        }

        if (last > Anope::CurTime || !last) {
            source.Reply(_("I've never seen %s on this channel."), na->nick.c_str());
        } else {
            source.Reply(_("%s was last seen here %s ago."), na->nick.c_str(),
                         Anope::Duration(Anope::CurTime - last, source.GetAccount()).c_str());
        }
    }

  public:
    CommandSeen(Module *creator) : Command(creator, "chanserv/seen", 1, 2) {
        this->SetDesc(_("Tells you about the last time a user was seen"));
        this->SetSyntax(_("\037nick\037"));
        this->AllowUnregistered(true);
    }

    void Execute(CommandSource &source,
                 const std::vector<Anope::string> &params) anope_override {
        const Anope::string &target = params[0];

        if (simple) {
            return this->SimpleSeen(source, params);
        }

        if (target.length() > Config->GetBlock("networkinfo")->Get<unsigned>("nicklen")) {
            source.Reply(_("Nick too long, max length is %u characters."),
                         Config->GetBlock("networkinfo")->Get<unsigned>("nicklen"));
            return;
        }

        if (BotInfo::Find(target, true) != NULL) {
            source.Reply(_("%s is a client on services."), target.c_str());
            return;
        }

        if (target.equals_ci(source.GetNick())) {
            source.Reply(_("You might see yourself in the mirror, %s."),
                         source.GetNick().c_str());
            return;
        }

        SeenInfo *info = FindInfo(target);
        if (!info) {
            source.Reply(_("Sorry, I have not seen %s."), target.c_str());
            return;
        }

        User *u2 = User::Find(target, true);
        Anope::string onlinestatus;
        if (u2) {
            onlinestatus = ".";
        } else {
            onlinestatus = Anope::printf(Language::Translate(source.nc,
                                         _(" but %s mysteriously dematerialized.")), target.c_str());
        }

        Anope::string timebuf = Anope::Duration(Anope::CurTime - info->last, source.nc);
        Anope::string timebuf2 = Anope::strftime(info->last, source.nc, true);

        if (info->type == NEW) {
            source.Reply(_("%s (%s) was last seen connecting %s ago (%s)%s"),
                         target.c_str(), info->vhost.c_str(), timebuf.c_str(), timebuf2.c_str(),
                         onlinestatus.c_str());
        } else if (info->type == NICK_TO) {
            u2 = User::Find(info->nick2, true);
            if (u2) {
                onlinestatus = Anope::printf(Language::Translate(source.nc,
                                             _(". %s is still online.")), u2->nick.c_str());
            } else {
                onlinestatus = Anope::printf(Language::Translate(source.nc,
                                             _(", but %s mysteriously dematerialized.")), info->nick2.c_str());
            }

            source.Reply(_("%s (%s) was last seen changing nick to %s %s ago%s"),
                         target.c_str(), info->vhost.c_str(), info->nick2.c_str(), timebuf.c_str(),
                         onlinestatus.c_str());
        } else if (info->type == NICK_FROM) {
            source.Reply(_("%s (%s) was last seen changing nick from %s to %s %s ago%s"),
                         target.c_str(), info->vhost.c_str(), info->nick2.c_str(), target.c_str(),
                         timebuf.c_str(), onlinestatus.c_str());
        } else if (info->type == JOIN) {
            if (ShouldHide(info->channel, u2))
                source.Reply(_("%s (%s) was last seen joining a secret channel %s ago%s"),
                             target.c_str(), info->vhost.c_str(), timebuf.c_str(), onlinestatus.c_str());
            else
                source.Reply(_("%s (%s) was last seen joining %s %s ago%s"),
                             target.c_str(), info->vhost.c_str(), info->channel.c_str(), timebuf.c_str(),
                             onlinestatus.c_str());
        } else if (info->type == PART) {
            if (ShouldHide(info->channel, u2))
                source.Reply(_("%s (%s) was last seen parting a secret channel %s ago%s"),
                             target.c_str(), info->vhost.c_str(), timebuf.c_str(), onlinestatus.c_str());
            else
                source.Reply(_("%s (%s) was last seen parting %s %s ago%s"),
                             target.c_str(), info->vhost.c_str(), info->channel.c_str(), timebuf.c_str(),
                             onlinestatus.c_str());
        } else if (info->type == QUIT) {
            source.Reply(_("%s (%s) was last seen quitting (%s) %s ago (%s)."),
                         target.c_str(), info->vhost.c_str(), info->message.c_str(), timebuf.c_str(),
                         timebuf2.c_str());
        } else if (info->type == KICK) {
            if (ShouldHide(info->channel, u2))
                source.Reply(_("%s (%s) was kicked from a secret channel %s ago%s"),
                             target.c_str(), info->vhost.c_str(), timebuf.c_str(), onlinestatus.c_str());
            else
                source.Reply(_("%s (%s) was kicked from %s (\"%s\") %s ago%s"),
                             target.c_str(), info->vhost.c_str(), info->channel.c_str(),
                             info->message.c_str(), timebuf.c_str(), onlinestatus.c_str());
        }
    }

    bool OnHelp(CommandSource &source,
                const Anope::string &subcommand) anope_override {
        this->SendSyntax(source);
        source.Reply(" ");
        source.Reply(_("Checks for the last time \037nick\037 was seen joining, leaving,\n"
                       "or changing nick on the network and tells you when and, depending\n"
                       "on channel or user settings, where it was."));
        return true;
    }
};

class CSSeen : public Module {
    Serialize::Type seeninfo_type;
    CommandSeen commandseen;
    CommandOSSeen commandosseen;
  public:
    CSSeen(const Anope::string &modname,
           const Anope::string &creator) : Module(modname, creator, VENDOR),
        seeninfo_type("SeenInfo", SeenInfo::Unserialize), commandseen(this),
        commandosseen(this) {
    }

    void OnReload(Configuration::Conf *conf) anope_override {
        simple = conf->GetModule(this)->Get<bool>("simple");
    }

    void OnExpireTick() anope_override {
        size_t previous_size = database.size();
        time_t purgetime = Config->GetModule(this)->Get<time_t>("purgetime");
        if (!purgetime) {
            purgetime = Anope::DoTime("30d");
        }
        for (database_map::iterator it = database.begin(), it_end = database.end(); it != it_end;) {
            database_map::iterator cur = it;
            ++it;

            if ((Anope::CurTime - cur->second->last) > purgetime) {
                Log(LOG_DEBUG) << cur->first << " was last seen " << Anope::strftime(
                                   cur->second->last) << ", purging entries";
                delete cur->second;
            }
        }
        Log(LOG_DEBUG) << "cs_seen: Purged database, checked " << previous_size << " nicks and removed " << (previous_size - database.size()) << " old entries.";
    }

    void OnUserConnect(User *u, bool &exempt) anope_override {
        if (!u->Quitting()) {
            UpdateUser(u, NEW, u->nick, "", "", "");
        }
    }

    void OnUserNickChange(User *u, const Anope::string &oldnick) anope_override {
        UpdateUser(u, NICK_TO, oldnick, u->nick, "", "");
        UpdateUser(u, NICK_FROM, u->nick, oldnick, "", "");
    }

    void OnUserQuit(User *u, const Anope::string &msg) anope_override {
        UpdateUser(u, QUIT, u->nick, "", "", msg);
    }

    void OnJoinChannel(User *u, Channel *c) anope_override {
        UpdateUser(u, JOIN, u->nick, "", c->name, "");
    }

    void OnPartChannel(User *u, Channel *c, const Anope::string &channel,
                       const Anope::string &msg) anope_override {
        UpdateUser(u, PART, u->nick, "", channel, msg);
    }

    void OnPreUserKicked(const MessageSource &source, ChanUserContainer *cu,
                         const Anope::string &msg) anope_override {
        UpdateUser(cu->user, KICK, cu->user->nick, source.GetSource(), cu->chan->name, msg);
    }

  private:
    void UpdateUser(const User *u, const TypeInfo Type, const Anope::string &nick,
                    const Anope::string &nick2, const Anope::string &channel,
                    const Anope::string &message) {
        if (simple || !u->server->IsSynced()) {
            return;
        }

        SeenInfo* &info = database[nick];
        if (!info) {
            info = new SeenInfo();
        }
        info->nick = nick;
        info->vhost = u->GetVIdent() + "@" + u->GetDisplayedHost();
        info->type = Type;
        info->last = Anope::CurTime;
        info->nick2 = nick2;
        info->channel = channel;
        info->message = message;
    }
};

MODULE_INIT(CSSeen)
