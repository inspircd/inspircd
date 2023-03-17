/* Routines to maintain a list of connected servers
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
#include "xline.h"
#include "servers.h"
#include "bots.h"
#include "regchannel.h"
#include "protocol.h"
#include "config.h"
#include "channels.h"

/* Anope */
Server *Me = NULL;

Anope::map<Server *> Servers::ByName;
Anope::map<Server *> Servers::ByID;

std::set<Anope::string> Servers::Capab;

Server::Server(Server *up, const Anope::string &sname, unsigned shops,
               const Anope::string &desc, const Anope::string &ssid, bool jupe) : name(sname),
    hops(shops), description(desc), sid(ssid), uplink(up), users(0) {
    syncing = true;
    juped = jupe;
    quitting = false;

    Servers::ByName[sname] = this;
    if (!ssid.empty()) {
        Servers::ByID[ssid] = this;
    }

    Log(this, "connect") << "has connected to the network (uplinked to " <<
                         (this->uplink ? this->uplink->GetName() : "no uplink") << ")";

    /* Add this server to our uplinks leaf list */
    if (this->uplink) {
        this->uplink->AddLink(this);

        /* Check to be sure this isn't a juped server */
        if (Me == this->uplink && !juped) {
            /* Now do mode related stuff as we know what modes exist .. */
            for (botinfo_map::iterator it = BotListByNick->begin(),
                    it_end = BotListByNick->end(); it != it_end; ++it) {
                BotInfo *bi = it->second;
                Anope::string modes = !bi->botmodes.empty() ? ("+" + bi->botmodes) :
                                      IRCD->DefaultPseudoclientModes;

                bi->SetModesInternal(bi, modes.c_str());
                for (unsigned i = 0; i < bi->botchannels.size(); ++i) {
                    size_t h = bi->botchannels[i].find('#');
                    if (h == Anope::string::npos) {
                        continue;
                    }
                    Anope::string chname = bi->botchannels[i].substr(h);
                    Channel *c = Channel::Find(chname);
                    if (c && c->FindUser(bi)) {
                        Anope::string want_modes = bi->botchannels[i].substr(0, h);
                        for (unsigned j = 0; j < want_modes.length(); ++j) {
                            ChannelMode *cm = ModeManager::FindChannelModeByChar(want_modes[j]);
                            if (cm == NULL) {
                                cm = ModeManager::FindChannelModeByChar(ModeManager::GetStatusChar(
                                        want_modes[j]));
                            }
                            if (cm && cm->type == MODE_STATUS) {
                                MessageSource ms = bi;
                                c->SetModeInternal(ms, cm, bi->nick);
                            }
                        }
                    }
                }
            }

            IRCD->SendBOB();

            for (unsigned i = 0; i < Me->GetLinks().size(); ++i) {
                Server *s = Me->GetLinks()[i];

                if (s->juped) {
                    IRCD->SendServer(s);
                }
            }

            /* We make the bots go online */
            for (user_map::const_iterator it = UserListByNick.begin();
                    it != UserListByNick.end(); ++it) {
                User *u = it->second;

                BotInfo *bi = BotInfo::Find(u->GetUID());
                if (bi) {
                    XLine x(bi->nick, "Reserved for services");
                    IRCD->SendSQLine(NULL, &x);
                }

                IRCD->SendClientIntroduction(u);
                if (bi) {
                    bi->introduced = true;
                }
            }

            for (channel_map::const_iterator it = ChannelList.begin(),
                    it_end = ChannelList.end(); it != it_end; ++it) {
                Channel *c = it->second;

                if (c->users.empty()) {
                    IRCD->SendChannel(c);
                } else
                    for (Channel::ChanUserList::const_iterator cit = c->users.begin(),
                            cit_end = c->users.end(); cit != cit_end; ++cit) {
                        IRCD->SendJoin(cit->second->user, c, &cit->second->status);
                    }

                for (Channel::ModeList::const_iterator it2 = c->GetModes().begin();
                        it2 != c->GetModes().end(); ++it2) {
                    ChannelMode *cm = ModeManager::FindChannelModeByName(it2->first);
                    if (!cm || cm->type != MODE_LIST) {
                        continue;
                    }
                    ModeManager::StackerAdd(c->ci->WhoSends(), c, cm, true, it2->second);
                }

                if (!c->topic.empty() && !c->topic_setter.empty()) {
                    IRCD->SendTopic(c->ci->WhoSends(), c);
                }

                c->syncing = true;
            }
        }
    }

    FOREACH_MOD(OnNewServer, (this));
}

Server::~Server() {
    Log(this, "quit") << "quit from " << (this->uplink ? this->uplink->GetName() :
                                          "no uplink") << " for " << this->quit_reason;

    for (user_map::const_iterator it = UserListByNick.begin();
            it != UserListByNick.end(); ++it) {
        User *u = it->second;

        if (u->server == this) {
            u->Quit(this->quit_reason);
            u->server = NULL;
        }
    }

    Log(LOG_DEBUG) << "Finished removing all users for " << this->GetName();

    if (this->uplink) {
        this->uplink->DelLink(this);
    }

    for (unsigned i = this->links.size(); i > 0; --i) {
        this->links[i - 1]->Delete(this->quit_reason);
    }

    Servers::ByName.erase(this->name);
    if (!this->sid.empty()) {
        Servers::ByID.erase(this->sid);
    }
}

void Server::Delete(const Anope::string &reason) {
    this->quit_reason = reason;
    this->quitting = true;
    FOREACH_MOD(OnServerQuit, (this));
    delete this;
}

const Anope::string &Server::GetName() const {
    return this->name;
}

unsigned Server::GetHops() const {
    return this->hops;
}

void Server::SetDescription(const Anope::string &desc) {
    this->description = desc;
}

const Anope::string &Server::GetDescription() const {
    return this->description;
}

void Server::SetSID(const Anope::string &nsid) {
    if (!this->sid.empty()) {
        throw CoreException("Server already has an id?");
    }
    this->sid = nsid;
    Servers::ByID[nsid] = this;
}

const Anope::string &Server::GetSID() const {
    if (!this->sid.empty() && IRCD->RequiresID) {
        return this->sid;
    } else {
        return this->name;
    }
}

const Anope::string &Server::GetQuitReason() const {
    return this->quit_reason;
}

const std::vector<Server *> &Server::GetLinks() const {
    return this->links;
}

Server *Server::GetUplink() {
    return this->uplink;
}

void Server::AddLink(Server *s) {
    this->links.push_back(s);

    Log(this, "connect") << "introduced " << s->GetName();
}

void Server::DelLink(Server *s) {
    if (this->links.empty()) {
        throw CoreException("Server::DelLink called on " + this->GetName() + " for " +
                            s->GetName() + " but we have no links?");
    }

    for (unsigned i = 0, j = this->links.size(); i < j; ++i) {
        if (this->links[i] == s) {
            this->links.erase(this->links.begin() + i);
            break;
        }
    }

    Log(this, "quit") << "quit " << s->GetName();
}

void Server::Sync(bool sync_links) {
    if (this->IsSynced()) {
        return;
    }

    syncing = false;

    Log(this, "sync") << "is done syncing";

    FOREACH_MOD(OnServerSync, (this));

    if (sync_links && !this->links.empty()) {
        for (unsigned i = 0, j = this->links.size(); i < j; ++i) {
            this->links[i]->Sync(true);
        }
    }

    bool me = this->GetUplink() && this->GetUplink() == Me;

    if (me) {
        FOREACH_MOD(OnPreUplinkSync, (this));
    }

    for (channel_map::const_iterator it = ChannelList.begin(),
            it_end = ChannelList.end(); it != it_end;) {
        Channel *c = it->second;
        ++it;

        if (c->syncing) {
            c->Sync();
        }
    }

    if (me) {
        IRCD->SendEOB();
        Me->Sync(false);

        FOREACH_MOD(OnUplinkSync, (this));

        if (!Anope::NoFork) {
            Log(LOG_TERMINAL) << "Successfully linked, launching into background...";
            Anope::Fork();
        }
    }
}

bool Server::IsSynced() const {
    return !syncing;
}

void Server::Unsync() {
    syncing = true;
}

bool Server::IsULined() const {
    if (this == Me) {
        return true;
    }

    for (unsigned i = 0; i < Config->Ulines.size(); ++i)
        if (Config->Ulines[i].equals_ci(this->GetName())) {
            return true;
        }
    return false;
}

bool Server::IsJuped() const {
    return juped;
}

bool Server::IsQuitting() const {
    return quitting;
}

void Server::Notice(BotInfo *source, const Anope::string &message) {
    if (Config->UsePrivmsg && Config->DefPrivmsg) {
        IRCD->SendGlobalPrivmsg(source, this, message);
    } else {
        IRCD->SendGlobalNotice(source, this, message);
    }
}

Server *Server::Find(const Anope::string &name, bool name_only) {
    Anope::map<Server *>::iterator it;

    if (!name_only) {
        it = Servers::ByID.find(name);
        if (it != Servers::ByID.end()) {
            return it->second;
        }
    }

    it = Servers::ByName.find(name);
    if (it != Servers::ByName.end()) {
        return it->second;
    }

    return NULL;
}

Server* Servers::GetUplink() {
    for (unsigned i = 0; Me && i < Me->GetLinks().size(); ++i)
        if (!Me->GetLinks()[i]->IsJuped()) {
            return Me->GetLinks()[i];
        }
    return NULL;
}
