/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2018-2019 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2015 Attila Molnar <attilamolnar@hush.com>
 *
 * This file is part of InspIRCd.  InspIRCd is free software: you can
 * redistribute it and/or modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation, version 2.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */


#include "inspircd.h"

#include "invite.h"

class InviteExpireTimer : public Timer {
    Invite::Invite* const inv;

    bool Tick(time_t currtime) CXX11_OVERRIDE;

  public:
    InviteExpireTimer(Invite::Invite* invite, time_t timeout);
};

static Invite::APIImpl* apiimpl;

void RemoveInvite(Invite::Invite* inv, bool remove_user, bool remove_chan) {
    apiimpl->Destruct(inv, remove_user, remove_chan);
}

void UnserializeInvite(LocalUser* user, const std::string& str) {
    apiimpl->Unserialize(user, str);
}

Invite::APIBase::APIBase(Module* parent)
    : DataProvider(parent, "core_channel_invite") {
}

Invite::APIImpl::APIImpl(Module* parent)
    : APIBase(parent)
    , userext(parent, "invite_user")
    , chanext(parent, "invite_chan") {
    apiimpl = this;
}

void Invite::APIImpl::Destruct(Invite* inv, bool remove_user,
                               bool remove_chan) {
    Store<LocalUser>* ustore = userext.get(inv->user);
    if (ustore) {
        ustore->invites.erase(inv);
        if ((remove_user) && (ustore->invites.empty())) {
            userext.unset(inv->user);
        }
    }

    Store<Channel>* cstore = chanext.get(inv->chan);
    if (cstore) {
        cstore->invites.erase(inv);
        if ((remove_chan) && (cstore->invites.empty())) {
            chanext.unset(inv->chan);
        }
    }

    delete inv;
}

bool Invite::APIImpl::Remove(LocalUser* user, Channel* chan) {
    Invite* inv = Find(user, chan);
    if (inv) {
        Destruct(inv);
        return true;
    }
    return false;
}

void Invite::APIImpl::Create(LocalUser* user, Channel* chan, time_t timeout) {
    if ((timeout != 0) && (ServerInstance->Time() >= timeout))
        // Expired, don't bother
    {
        return;
    }

    ServerInstance->Logs->Log(MODNAME, LOG_DEBUG,
                              "Invite::APIImpl::Create(): user=%s chan=%s timeout=%lu", user->uuid.c_str(),
                              chan->name.c_str(), (unsigned long)timeout);

    Invite* inv = Find(user, chan);
    if (inv) {
        // We only ever extend invites, so nothing to do if the existing one is not timed
        if (!inv->IsTimed()) {
            return;
        }

        ServerInstance->Logs->Log(MODNAME, LOG_DEBUG,
                                  "Invite::APIImpl::Create(): changing expiration in %p", (void*) inv);
        if (timeout == 0) {
            // Convert timed invite to non-expiring
            delete inv->expiretimer;
            inv->expiretimer = NULL;
        } else if (inv->expiretimer->GetTrigger() >= ServerInstance->Time() + timeout) {
            // New expiration time is further than the current, extend the expiration
            inv->expiretimer->SetInterval(timeout - ServerInstance->Time());
        }
    } else {
        inv = new Invite(user, chan);
        if (timeout) {
            inv->expiretimer = new InviteExpireTimer(inv, timeout - ServerInstance->Time());
            ServerInstance->Timers.AddTimer(inv->expiretimer);
        }

        userext.get(user, true)->invites.push_front(inv);
        chanext.get(chan, true)->invites.push_front(inv);
        ServerInstance->Logs->Log(MODNAME, LOG_DEBUG,
                                  "Invite::APIImpl::Create(): created new Invite %p", (void*) inv);
    }
}

Invite::Invite* Invite::APIImpl::Find(LocalUser* user, Channel* chan) {
    const List* list = APIImpl::GetList(user);
    if (!list) {
        return NULL;
    }

    for (List::iterator i = list->begin(); i != list->end(); ++i) {
        Invite* inv = *i;
        if (inv->chan == chan) {
            return inv;
        }
    }

    return NULL;
}

const Invite::List* Invite::APIImpl::GetList(LocalUser* user) {
    Store<LocalUser>* list = userext.get(user);
    if (list) {
        return &list->invites;
    }
    return NULL;
}

void Invite::APIImpl::Unserialize(LocalUser* user, const std::string& value) {
    irc::spacesepstream ss(value);
    for (std::string channame, exptime; (ss.GetToken(channame)
                                         && ss.GetToken(exptime)); ) {
        Channel* chan = ServerInstance->FindChan(channame);
        if (chan) {
            Create(user, chan, ConvToNum<time_t>(exptime));
        }
    }
}

Invite::Invite::Invite(LocalUser* u, Channel* c)
    : user(u)
    , chan(c)
    , expiretimer(NULL) {
}

Invite::Invite::~Invite() {
    delete expiretimer;
    ServerInstance->Logs->Log(MODNAME, LOG_DEBUG, "Invite::~ %p", (void*) this);
}

void Invite::Invite::Serialize(bool human, bool show_chans, std::string& out) {
    if (show_chans) {
        out.append(this->chan->name);
    } else {
        out.append(human ? user->nick : user->uuid);
    }
    out.push_back(' ');

    if (expiretimer) {
        out.append(ConvToStr(expiretimer->GetTrigger()));
    } else {
        out.push_back('0');
    }
    out.push_back(' ');
}

InviteExpireTimer::InviteExpireTimer(Invite::Invite* invite, time_t timeout)
    : Timer(timeout)
    , inv(invite) {
}

bool InviteExpireTimer::Tick(time_t currtime) {
    ServerInstance->Logs->Log(MODNAME, LOG_DEBUG,
                              "InviteExpireTimer::Tick(): expired %p", (void*) inv);
    apiimpl->Destruct(inv);
    return false;
}
