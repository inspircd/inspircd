/* Channel-handling routines.
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
#include "channels.h"
#include "regchannel.h"
#include "logger.h"
#include "modules.h"
#include "users.h"
#include "bots.h"
#include "servers.h"
#include "protocol.h"
#include "users.h"
#include "config.h"
#include "access.h"
#include "sockets.h"
#include "language.h"
#include "uplink.h"

channel_map ChannelList;
std::vector<Channel *> Channel::deleting;

Channel::Channel(const Anope::string &nname, time_t ts) {
    if (nname.empty()) {
        throw CoreException("A channel without a name ?");
    }

    this->name = nname;

    this->creation_time = ts;
    this->syncing = this->botchannel = false;
    this->server_modetime = this->chanserv_modetime = 0;
    this->server_modecount = this->chanserv_modecount = this->bouncy_modes =
                                 this->topic_ts = this->topic_time = 0;

    this->ci = ChannelInfo::Find(this->name);
    if (this->ci) {
        this->ci->c = this;
    }

    if (Me && Me->IsSynced()) {
        Log(NULL, this, "create");
    }

    FOREACH_MOD(OnChannelCreate, (this));
}

Channel::~Channel() {
    UnsetExtensibles();

    FOREACH_MOD(OnChannelDelete, (this));

    ModeManager::StackerDel(this);

    if (Me && Me->IsSynced()) {
        Log(NULL, this, "destroy");
    }

    if (this->ci) {
        this->ci->c = NULL;
    }

    ChannelList.erase(this->name);
}

void Channel::Reset() {
    this->modes.clear();

    for (ChanUserList::const_iterator it = this->users.begin(),
            it_end = this->users.end(); it != it_end; ++it) {
        ChanUserContainer *uc = it->second;

        ChannelStatus f = uc->status;
        uc->status.Clear();

        /* reset modes for my clients */
        if (uc->user->server == Me) {
            for (size_t i = 0; i < f.Modes().length(); ++i) {
                this->SetMode(NULL, ModeManager::FindChannelModeByChar(f.Modes()[i]),
                              uc->user->GetUID(), false);
            }
            /* Modes might not exist yet, so be sure the status is really reset */
            uc->status = f;
        }
    }

    for (ChanUserList::const_iterator it = this->users.begin(),
            it_end = this->users.end(); it != it_end; ++it) {
        this->SetCorrectModes(it->second->user, true);
    }

    // If the channel is syncing now, do not force a sync due to Reset(), as we are probably iterating over users in Message::SJoin
    // A sync will come soon
    if (!syncing) {
        this->Sync();
    }
}

void Channel::Sync() {
    syncing = false;
    FOREACH_MOD(OnChannelSync, (this));
    CheckModes();
}

void Channel::CheckModes() {
    if (this->bouncy_modes || this->syncing) {
        return;
    }

    /* Check for mode bouncing */
    if (this->chanserv_modetime == Anope::CurTime
            && this->server_modetime == Anope::CurTime && this->server_modecount >= 3
            && this->chanserv_modecount >= 3) {
        Log() << "Warning: unable to set modes on channel " << this->name <<
              ". Are your servers' U:lines configured correctly?";
        this->bouncy_modes = 1;
        return;
    }

    Reference<Channel> ref = this;
    FOREACH_MOD(OnCheckModes, (ref));
}

bool Channel::CheckDelete() {
    /* Channel is syncing from a netburst, don't destroy it as more users are probably wanting to join immediately
     * We also don't part the bot here either, if necessary we will part it after the sync
     */
    if (this->syncing) {
        return false;
    }

    /* Permanent channels never get deleted */
    if (this->HasMode("PERM")) {
        return false;
    }

    EventReturn MOD_RESULT;
    FOREACH_RESULT(OnCheckDelete, MOD_RESULT, (this));

    return MOD_RESULT != EVENT_STOP && this->users.empty();
}

ChanUserContainer* Channel::JoinUser(User *user, const ChannelStatus *status) {
    if (user->server && user->server->IsSynced()) {
        Log(user, this, "join");
    }

    ChanUserContainer *cuc = new ChanUserContainer(user, this);
    user->chans[this] = cuc;
    this->users[user] = cuc;
    if (status) {
        cuc->status = *status;
    }

    return cuc;
}

void Channel::DeleteUser(User *user) {
    if (user->server && user->server->IsSynced() && !user->Quitting()) {
        Log(user, this, "leave");
    }

    FOREACH_MOD(OnLeaveChannel, (user, this));

    ChanUserContainer *cu = user->FindChannel(this);
    if (!this->users.erase(user)) {
        Log(LOG_DEBUG) << "Channel::DeleteUser() tried to delete nonexistent user " <<
                       user->nick << " from channel " << this->name;
    }

    if (!user->chans.erase(this)) {
        Log(LOG_DEBUG) << "Channel::DeleteUser() tried to delete nonexistent channel "
                       << this->name << " from " << user->nick << "'s channel list";
    }
    delete cu;

    QueueForDeletion();
}

ChanUserContainer *Channel::FindUser(User *u) const {
    ChanUserList::const_iterator it = this->users.find(u);
    if (it != this->users.end()) {
        return it->second;
    }
    return NULL;
}

bool Channel::HasUserStatus(User *u, ChannelModeStatus *cms) {
    /* Usually its more efficient to search the users channels than the channels users */
    ChanUserContainer *cc = u->FindChannel(this);
    if (cc) {
        if (cms) {
            return cc->status.HasMode(cms->mchar);
        } else {
            return cc->status.Empty();
        }
    }

    return false;
}

bool Channel::HasUserStatus(User *u, const Anope::string &mname) {
    return HasUserStatus(u, anope_dynamic_static_cast<ChannelModeStatus *>
                         (ModeManager::FindChannelModeByName(mname)));
}

size_t Channel::HasMode(const Anope::string &mname,
                        const Anope::string &param) {
    if (param.empty()) {
        return modes.count(mname);
    }
    std::vector<Anope::string> v = this->GetModeList(mname);
    for (unsigned int i = 0; i < v.size(); ++i)
        if (v[i].equals_ci(param)) {
            return 1;
        }
    return 0;
}

Anope::string Channel::GetModes(bool complete, bool plus) {
    Anope::string res, params;

    for (std::multimap<Anope::string, Anope::string>::const_iterator it =
                this->modes.begin(), it_end = this->modes.end(); it != it_end; ++it) {
        ChannelMode *cm = ModeManager::FindChannelModeByName(it->first);
        if (!cm || cm->type == MODE_LIST) {
            continue;
        }

        res += cm->mchar;

        if (complete && !it->second.empty()) {
            ChannelModeParam *cmp = NULL;
            if (cm->type == MODE_PARAM) {
                cmp = anope_dynamic_static_cast<ChannelModeParam *>(cm);
            }

            if (plus || !cmp || !cmp->minus_no_arg) {
                params += " " + it->second;
            }
        }
    }

    return res + params;
}

const Channel::ModeList &Channel::GetModes() const {
    return this->modes;
}

template<typename F, typename S>
struct second {
    S operator()(const std::pair<F, S> &p) {
        return p.second;
    }
};

std::vector<Anope::string> Channel::GetModeList(const Anope::string &mname) {
    std::vector<Anope::string> r;
    std::transform(modes.lower_bound(mname), modes.upper_bound(mname),
                   std::back_inserter(r), second<Anope::string, Anope::string>());
    return r;
}

void Channel::SetModeInternal(MessageSource &setter, ChannelMode *ocm,
                              const Anope::string &oparam, bool enforce_mlock) {
    if (!ocm) {
        return;
    }

    Anope::string param = oparam;
    ChannelMode *cm = ocm->Unwrap(param);

    EventReturn MOD_RESULT;

    /* Setting v/h/o/a/q etc */
    if (cm->type == MODE_STATUS) {
        if (param.empty()) {
            Log() << "Channel::SetModeInternal() mode " << cm->mchar <<
                  " with no parameter for channel " << this->name;
            return;
        }

        User *u = User::Find(param);

        if (!u) {
            Log(LOG_DEBUG) << "MODE " << this->name << " +" << cm->mchar <<
                           " for nonexistent user " << param;
            return;
        }

        Log(LOG_DEBUG) << "Setting +" << cm->mchar << " on " << this->name << " for " <<
                       u->nick;

        /* Set the status on the user */
        ChanUserContainer *cc = u->FindChannel(this);
        if (cc) {
            cc->status.AddMode(cm->mchar);
        }

        FOREACH_RESULT(OnChannelModeSet, MOD_RESULT, (this, setter, cm, param));

        /* Enforce secureops, etc */
        if (enforce_mlock && MOD_RESULT != EVENT_STOP) {
            this->SetCorrectModes(u, false);
        }
        return;
    }

    if (cm->type != MODE_LIST) {
        this->modes.erase(cm->name);
    } else if (this->HasMode(cm->name, param)) {
        return;
    }

    this->modes.insert(std::make_pair(cm->name, param));

    if (param.empty() && cm->type != MODE_REGULAR) {
        Log() << "Channel::SetModeInternal() mode " << cm->mchar << " for " <<
              this->name << " with no paramater, but is a param mode";
        return;
    }

    if (cm->type == MODE_LIST) {
        ChannelModeList *cml = anope_dynamic_static_cast<ChannelModeList *>(cm);
        cml->OnAdd(this, param);
    }

    FOREACH_RESULT(OnChannelModeSet, MOD_RESULT, (this, setter, cm, param));

    /* Check if we should enforce mlock */
    if (!enforce_mlock || MOD_RESULT == EVENT_STOP) {
        return;
    }

    this->CheckModes();
}

void Channel::RemoveModeInternal(MessageSource &setter, ChannelMode *ocm,
                                 const Anope::string &oparam, bool enforce_mlock) {
    if (!ocm) {
        return;
    }

    Anope::string param = oparam;
    ChannelMode *cm = ocm->Unwrap(param);

    EventReturn MOD_RESULT;

    /* Setting v/h/o/a/q etc */
    if (cm->type == MODE_STATUS) {
        if (param.empty()) {
            Log() << "Channel::RemoveModeInternal() mode " << cm->mchar <<
                  " with no parameter for channel " << this->name;
            return;
        }

        BotInfo *bi = BotInfo::Find(param);
        User *u = bi ? bi : User::Find(param);

        if (!u) {
            Log(LOG_DEBUG) << "Channel::RemoveModeInternal() MODE " << this->name << "-" <<
                           cm->mchar << " for nonexistent user " << param;
            return;
        }

        Log(LOG_DEBUG) << "Setting -" << cm->mchar << " on " << this->name << " for " <<
                       u->nick;

        /* Remove the status on the user */
        ChanUserContainer *cc = u->FindChannel(this);
        if (cc) {
            cc->status.DelMode(cm->mchar);
        }

        FOREACH_RESULT(OnChannelModeUnset, MOD_RESULT, (this, setter, cm, param));

        if (enforce_mlock && MOD_RESULT != EVENT_STOP) {
            this->SetCorrectModes(u, false);
        }

        return;
    }

    if (cm->type == MODE_LIST) {
        for (Channel::ModeList::iterator it = modes.lower_bound(cm->name),
                it_end = modes.upper_bound(cm->name); it != it_end; ++it)
            if (param.equals_ci(it->second)) {
                this->modes.erase(it);
                break;
            }
    } else {
        this->modes.erase(cm->name);
    }

    if (cm->type == MODE_LIST) {
        ChannelModeList *cml = anope_dynamic_static_cast<ChannelModeList *>(cm);
        cml->OnDel(this, param);
    }

    FOREACH_RESULT(OnChannelModeUnset, MOD_RESULT, (this, setter, cm, param));

    if (cm->name == "PERM") {
        if (this->CheckDelete()) {
            delete this;
            return;
        }
    }

    /* Check for mlock */
    if (!enforce_mlock || MOD_RESULT == EVENT_STOP) {
        return;
    }

    this->CheckModes();
}

void Channel::SetMode(BotInfo *bi, ChannelMode *cm, const Anope::string &param,
                      bool enforce_mlock) {
    Anope::string wparam = param;
    if (!cm) {
        return;
    }
    /* Don't set modes already set */
    if (cm->type == MODE_REGULAR && HasMode(cm->name)) {
        return;
    } else if (cm->type == MODE_PARAM) {
        ChannelModeParam *cmp = anope_dynamic_static_cast<ChannelModeParam *>(cm);
        if (!cmp->IsValid(wparam)) {
            return;
        }

        Anope::string cparam;
        if (GetParam(cm->name, cparam) && cparam.equals_cs(wparam)) {
            return;
        }
    } else if (cm->type == MODE_STATUS) {
        User *u = User::Find(param);
        if (!u || HasUserStatus(u,
                                anope_dynamic_static_cast<ChannelModeStatus *>(cm))) {
            return;
        }
    } else if (cm->type == MODE_LIST) {
        ChannelModeList *cml = anope_dynamic_static_cast<ChannelModeList *>(cm);

        if (!cml->IsValid(wparam)) {
            return;
        }

        if (this->HasMode(cm->name, wparam)) {
            return;
        }
    }

    if (Me->IsSynced()) {
        if (this->chanserv_modetime != Anope::CurTime) {
            this->chanserv_modecount = 0;
            this->chanserv_modetime = Anope::CurTime;
        }

        this->chanserv_modecount++;
    }

    ChannelMode *wcm = cm->Wrap(wparam);

    ModeManager::StackerAdd(bi, this, wcm, true, wparam);
    MessageSource ms(bi);
    SetModeInternal(ms, wcm, wparam, enforce_mlock);
}

void Channel::SetMode(BotInfo *bi, const Anope::string &mname,
                      const Anope::string &param, bool enforce_mlock) {
    SetMode(bi, ModeManager::FindChannelModeByName(mname), param, enforce_mlock);
}

void Channel::RemoveMode(BotInfo *bi, ChannelMode *cm,
                         const Anope::string &wparam, bool enforce_mlock) {
    if (!cm) {
        return;
    }

    /* Don't unset modes that arent set */
    if ((cm->type == MODE_REGULAR || cm->type == MODE_PARAM)
            && !HasMode(cm->name)) {
        return;
    }

    /* Unwrap to be sure we have the internal representation */
    Anope::string param = wparam;
    cm = cm->Unwrap(param);

    /* Don't unset status that aren't set */
    if (cm->type == MODE_STATUS) {
        User *u = User::Find(param);
        if (!u || !HasUserStatus(u,
                                 anope_dynamic_static_cast<ChannelModeStatus *>(cm))) {
            return;
        }
    } else if (cm->type == MODE_LIST) {
        if (!this->HasMode(cm->name, param)) {
            return;
        }
    }

    /* Get the param to send, if we need it */
    if (cm->type == MODE_PARAM) {
        param.clear();
        ChannelModeParam *cmp = anope_dynamic_static_cast<ChannelModeParam *>(cm);
        if (!cmp->minus_no_arg) {
            this->GetParam(cmp->name, param);
        }
    }

    if (Me->IsSynced()) {
        if (this->chanserv_modetime != Anope::CurTime) {
            this->chanserv_modecount = 0;
            this->chanserv_modetime = Anope::CurTime;
        }

        this->chanserv_modecount++;
    }

    /* Wrap to get ircd representation */
    ChannelMode *wcm = cm->Wrap(param);

    ModeManager::StackerAdd(bi, this, wcm, false, param);
    MessageSource ms(bi);
    RemoveModeInternal(ms, wcm, param, enforce_mlock);
}

void Channel::RemoveMode(BotInfo *bi, const Anope::string &mname,
                         const Anope::string &param, bool enforce_mlock) {
    RemoveMode(bi, ModeManager::FindChannelModeByName(mname), param, enforce_mlock);
}

bool Channel::GetParam(const Anope::string &mname,
                       Anope::string &target) const {
    std::multimap<Anope::string, Anope::string>::const_iterator it =
        this->modes.find(mname);

    target.clear();

    if (it != this->modes.end()) {
        target = it->second;
        return true;
    }

    return false;
}

void Channel::SetModes(BotInfo *bi, bool enforce_mlock, const char *cmodes,
                       ...) {
    char buf[BUFSIZE] = "";
    va_list args;
    Anope::string modebuf, sbuf;
    int add = -1;
    va_start(args, cmodes);
    vsnprintf(buf, BUFSIZE - 1, cmodes, args);
    va_end(args);

    Reference<Channel> this_reference(this);

    spacesepstream sep(buf);
    sep.GetToken(modebuf);
    for (unsigned i = 0, end = modebuf.length(); this_reference && i < end; ++i) {
        ChannelMode *cm;

        switch (modebuf[i]) {
        case '+':
            add = 1;
            continue;
        case '-':
            add = 0;
            continue;
        default:
            if (add == -1) {
                continue;
            }
            cm = ModeManager::FindChannelModeByChar(modebuf[i]);
            if (!cm) {
                continue;
            }
        }

        if (add) {
            if (cm->type != MODE_REGULAR && sep.GetToken(sbuf)) {
                if (cm->type == MODE_STATUS) {
                    User *targ = User::Find(sbuf);
                    if (targ != NULL) {
                        sbuf = targ->GetUID();
                    }
                }
                this->SetMode(bi, cm, sbuf, enforce_mlock);
            } else {
                this->SetMode(bi, cm, "", enforce_mlock);
            }
        } else if (!add) {
            if (cm->type != MODE_REGULAR && sep.GetToken(sbuf)) {
                if (cm->type == MODE_STATUS) {
                    User *targ = User::Find(sbuf);
                    if (targ != NULL) {
                        sbuf = targ->GetUID();
                    }
                }
                this->RemoveMode(bi, cm, sbuf, enforce_mlock);
            } else {
                this->RemoveMode(bi, cm, "", enforce_mlock);
            }
        }
    }
}

void Channel::SetModesInternal(MessageSource &source, const Anope::string &mode,
                               time_t ts, bool enforce_mlock) {
    if (!ts)
        ;
    else if (ts > this->creation_time) {
        Log(LOG_DEBUG) << "Dropping mode " << mode << " on " << this->name << ", " << ts
                       << " > " << this->creation_time;
        return;
    } else if (ts < this->creation_time) {
        Log(LOG_DEBUG) << "Changing TS of " << this->name << " from " <<
                       this->creation_time << " to " << ts;
        this->creation_time = ts;
        this->Reset();
    }

    User *setter = source.GetUser();
    /* Removing channel modes *may* delete this channel */
    Reference<Channel> this_reference(this);

    spacesepstream sep_modes(mode);
    Anope::string m;

    sep_modes.GetToken(m);

    Anope::string modestring;
    Anope::string paramstring;
    int add = -1;
    bool changed = false;
    for (unsigned int i = 0, end = m.length(); i < end && this_reference; ++i) {
        ChannelMode *cm;

        switch (m[i]) {
        case '+':
            modestring += '+';
            add = 1;
            continue;
        case '-':
            modestring += '-';
            add = 0;
            continue;
        default:
            if (add == -1) {
                continue;
            }
            cm = ModeManager::FindChannelModeByChar(m[i]);
            if (!cm) {
                Log(LOG_DEBUG) << "Channel::SetModeInternal: Unknown mode char " << m[i];
                continue;
            }
            modestring += cm->mchar;
        }

        if (cm->type == MODE_REGULAR) {
            /* something changed if we are adding a mode we don't have, or removing one we have */
            changed |= !!add != this->HasMode(cm->name);
            if (add) {
                this->SetModeInternal(source, cm, "", false);
            } else {
                this->RemoveModeInternal(source, cm, "", false);
            }
            continue;
        } else if (cm->type == MODE_PARAM) {
            ChannelModeParam *cmp = anope_dynamic_static_cast<ChannelModeParam *>(cm);

            if (!add && cmp->minus_no_arg) {
                this->RemoveModeInternal(source, cm, "", false);
                continue;
            }
        }
        Anope::string token;
        if (sep_modes.GetToken(token)) {
            User *u = NULL;
            if (cm->type == MODE_STATUS && (u = User::Find(token))) {
                paramstring += " " + u->nick;
            } else {
                paramstring += " " + token;
            }

            changed |= !!add != this->HasMode(cm->name, token);
            /* CheckModes below doesn't check secureops (+ the module event) */
            if (add) {
                this->SetModeInternal(source, cm, token, enforce_mlock);
            } else {
                this->RemoveModeInternal(source, cm, token, enforce_mlock);
            }
        } else {
            Log() << "warning: Channel::SetModesInternal() received more modes requiring params than params, modes: "
                  << mode;
        }
    }

    if (!this_reference) {
        return;
    }

    if (changed && source.GetServer() && source.GetServer()->IsSynced()) {
        if (Anope::CurTime != this->server_modetime) {
            this->server_modecount = 0;
            this->server_modetime = Anope::CurTime;
        }

        ++this->server_modecount;
    }

    if (setter) {
        Log(setter, this, "mode") << modestring << paramstring;
    } else {
        Log(LOG_DEBUG) << source.GetName() << " is setting " << this->name << " to " <<
                       modestring << paramstring;
    }

    if (enforce_mlock) {
        this->CheckModes();
    }
}

bool Channel::MatchesList(User *u, const Anope::string &mode) {
    if (!this->HasMode(mode)) {
        return false;
    }

    std::vector<Anope::string> v = this->GetModeList(mode);
    for (unsigned i = 0; i < v.size(); ++i) {
        Entry e(mode, v[i]);
        if (e.Matches(u)) {
            return true;
        }
    }

    return false;
}

void Channel::KickInternal(const MessageSource &source,
                           const Anope::string &nick, const Anope::string &reason) {
    User *sender = source.GetUser();
    User *target = User::Find(nick);
    if (!target) {
        Log(LOG_DEBUG) << "Channel::KickInternal got a nonexistent user " << nick <<
                       " on " << this->name << ": " << reason;
        return;
    }

    if (sender) {
        Log(sender, this, "kick") << "kicked " << target->nick << " (" << reason <<
                                  ")";
    } else {
        Log(target, this, "kick") << "was kicked by " << source.GetName() << " (" <<
                                  reason << ")";
    }

    Anope::string chname = this->name;

    ChanUserContainer *cu = target->FindChannel(this);
    if (cu == NULL) {
        Log(LOG_DEBUG) << "Channel::KickInternal got kick for user " << target->nick <<
                       " from " << source.GetSource() << " who isn't on channel " << this->name;
        return;
    }

    ChannelStatus status = cu->status;

    FOREACH_MOD(OnPreUserKicked, (source, cu, reason));
    this->DeleteUser(target);
    FOREACH_MOD(OnUserKicked, (source, target, this->name, status, reason));
}

bool Channel::Kick(BotInfo *bi, User *u, const char *reason, ...) {
    va_list args;
    char buf[BUFSIZE] = "";
    va_start(args, reason);
    vsnprintf(buf, BUFSIZE - 1, reason, args);
    va_end(args);

    /* Do not kick protected clients or Ulines */
    if (u->IsProtected()) {
        return false;
    }

    if (bi == NULL) {
        bi = this->ci->WhoSends();
    }

    EventReturn MOD_RESULT;
    FOREACH_RESULT(OnBotKick, MOD_RESULT, (bi, this, u, buf));
    if (MOD_RESULT == EVENT_STOP) {
        return false;
    }
    IRCD->SendKick(bi, this, u, "%s", buf);
    this->KickInternal(bi, u->nick, buf);
    return true;
}

void Channel::ChangeTopicInternal(User *u, const Anope::string &user,
                                  const Anope::string &newtopic, time_t ts) {
    this->topic = newtopic;
    this->topic_setter = u ? u->nick : user;
    this->topic_ts = ts;
    this->topic_time = Anope::CurTime;

    Log(LOG_DEBUG) << "Topic of " << this->name << " changed by " <<
                   this->topic_setter << " to " << newtopic;

    FOREACH_MOD(OnTopicUpdated, (u, this, user, this->topic));
}

void Channel::ChangeTopic(const Anope::string &user,
                          const Anope::string &newtopic, time_t ts) {
    this->topic = newtopic;
    this->topic_setter = user;
    this->topic_ts = ts;

    IRCD->SendTopic(this->ci->WhoSends(), this);

    /* Now that the topic is set update the time set. This is *after* we set it so the protocol modules are able to tell the old last set time */
    this->topic_time = Anope::CurTime;

    FOREACH_MOD(OnTopicUpdated, (NULL, this, user, this->topic));
}

void Channel::SetCorrectModes(User *user, bool give_modes) {
    if (user == NULL) {
        return;
    }

    if (!this->ci) {
        return;
    }

    Log(LOG_DEBUG) << "Setting correct user modes for " << user->nick << " on " <<
                   this->name << " (" << (give_modes ? "" : "not ") << "giving modes)";

    AccessGroup u_access = ci->AccessFor(user);

    /* Initially only take modes if the channel is being created by a non netmerge */
    bool take_modes = this->syncing && user->server->IsSynced();

    FOREACH_MOD(OnSetCorrectModes, (user, this, u_access, give_modes, take_modes));

    /* Never take modes from ulines */
    if (user->server->IsULined()) {
        take_modes = false;
    }

    /* whether or not we are giving modes */
    bool giving = give_modes;
    /* whether or not we have given a mode */
    bool given = false;
    for (unsigned i = 0; i < ModeManager::GetStatusChannelModesByRank().size();
            ++i) {
        ChannelModeStatus *cm = ModeManager::GetStatusChannelModesByRank()[i];
        bool has_priv = u_access.HasPriv("AUTO" + cm->name);

        if (give_modes && has_priv) {
            /* Always give op. If we have already given one mode, don't give more until it has a symbol */
            if (cm->name == "OP" || !given || (giving && cm->symbol)) {
                this->SetMode(NULL, cm, user->GetUID(), false);
                /* Now if this contains a symbol don't give any more modes, to prevent setting +qaohv etc on users */
                giving = !cm->symbol;
                given = true;
            }
        }
        /* modes that have no privileges assigned shouldn't be removed (like operprefix, ojoin) */
        else if (take_modes && !has_priv
                 && ci->GetLevel(cm->name + "ME") != ACCESS_INVALID
                 && !u_access.HasPriv(cm->name + "ME")) {
            /* Only remove modes if they are > voice */
            if (cm->name == "VOICE") {
                take_modes = false;
            } else {
                this->RemoveMode(NULL, cm, user->GetUID(), false);
            }
        }
    }
}

bool Channel::Unban(User *u, const Anope::string &mode, bool full) {
    if (!this->HasMode(mode)) {
        return false;
    }

    bool ret = false;

    std::vector<Anope::string> v = this->GetModeList(mode);
    for (unsigned int i = 0; i < v.size(); ++i) {
        Entry ban(mode, v[i]);
        if (ban.Matches(u, full)) {
            this->RemoveMode(NULL, mode, ban.GetMask());
            ret = true;
        }
    }

    return ret;
}

bool Channel::CheckKick(User *user) {
    if (user->super_admin) {
        return false;
    }

    /* We don't enforce services restrictions on clients on ulined services
     * as this will likely lead to kick/rejoin floods. ~ Viper */
    if (user->IsProtected()) {
        return false;
    }

    Anope::string mask, reason;

    EventReturn MOD_RESULT;
    FOREACH_RESULT(OnCheckKick, MOD_RESULT, (user, this, mask, reason));
    if (MOD_RESULT != EVENT_STOP) {
        return false;
    }

    if (mask.empty()) {
        mask = this->ci->GetIdealBan(user);
    }
    if (reason.empty()) {
        reason = Language::Translate(user->Account(), CHAN_NOT_ALLOWED_TO_JOIN);
    }

    Log(LOG_DEBUG) << "Autokicking " << user->nick << " (" << mask << ") from " <<
                   this->name;

    this->SetMode(NULL, "BAN", mask);
    this->Kick(NULL, user, "%s", reason.c_str());

    return true;
}

Channel* Channel::Find(const Anope::string &name) {
    channel_map::const_iterator it = ChannelList.find(name);

    if (it != ChannelList.end()) {
        return it->second;
    }
    return NULL;
}

Channel *Channel::FindOrCreate(const Anope::string &name, bool &created,
                               time_t ts) {
    Channel* &chan = ChannelList[name];
    created = chan == NULL;
    if (!chan) {
        chan = new Channel(name, ts);
    }
    return chan;
}

void Channel::QueueForDeletion() {
    if (std::find(deleting.begin(), deleting.end(), this) == deleting.end()) {
        deleting.push_back(this);
    }
}

void Channel::DeleteChannels() {
    for (unsigned int i = 0; i < deleting.size(); ++i) {
        Channel *c = deleting[i];

        if (c->CheckDelete()) {
            delete c;
        }
    }
    deleting.clear();
}
