/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2017 B00mX0r <b00mx0r@aureus.pw>
 *   Copyright (C) 2013-2014, 2016-2020, 2022 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2013 Adam <Adam@anope.org>
 *   Copyright (C) 2012-2016, 2018 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012, 2019 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2006-2009 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2006-2008 Craig Edwards <brain@inspircd.org>
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
#include "listmode.h"

namespace {
ChanModeReference ban(NULL, "ban");
}

Channel::Channel(const std::string &cname, time_t ts)
    : name(cname), age(ts), topicset(0) {
    if (!ServerInstance->chanlist.insert(std::make_pair(cname, this)).second) {
        throw CoreException("Cannot create duplicate channel " + cname);
    }
}

void Channel::SetMode(ModeHandler* mh, bool on) {
    if (mh && mh->GetId() != ModeParser::MODEID_MAX) {
        modes[mh->GetId()] = on;
    }
}

void Channel::SetTopic(User* u, const std::string& ntopic, time_t topicts,
                       const std::string* setter) {
    // Send a TOPIC message to the channel only if the new topic text differs
    if (this->topic != ntopic) {
        this->topic = ntopic;
        ClientProtocol::Messages::Topic topicmsg(u, this, this->topic);
        Write(ServerInstance->GetRFCEvents().topic, topicmsg);
    }

    // Always update setter and set time
    if (!setter) {
        setter = ServerInstance->Config->FullHostInTopic ? &u->GetFullHost() :
                 &u->nick;
    }
    this->setby.assign(*setter, 0, ServerInstance->Config->Limits.GetMaxMask());
    this->topicset = topicts;

    FOREACH_MOD(OnPostTopicChange, (u, this, this->topic));
}

Membership* Channel::AddUser(User* user) {
    std::pair<MemberMap::iterator, bool> ret = userlist.insert(std::make_pair(user,
            insp::aligned_storage<Membership>()));
    if (!ret.second) {
        return NULL;
    }

    Membership* memb = new(ret.first->second) Membership(user, this);
    return memb;
}

void Channel::DelUser(User* user) {
    MemberMap::iterator it = userlist.find(user);
    if (it != userlist.end()) {
        DelUser(it);
    }
}

void Channel::CheckDestroy() {
    if (!userlist.empty()) {
        return;
    }

    ModResult res;
    FIRST_MOD_RESULT(OnChannelPreDelete, res, (this));
    if (res == MOD_RES_DENY) {
        return;
    }

    // If the channel isn't in chanlist then it is already in the cull list, don't add it again
    chan_hash::iterator iter = ServerInstance->chanlist.find(this->name);
    if ((iter == ServerInstance->chanlist.end()) || (iter->second != this)) {
        return;
    }

    FOREACH_MOD(OnChannelDelete, (this));
    ServerInstance->chanlist.erase(iter);
    ServerInstance->GlobalCulls.AddItem(this);
}

void Channel::DelUser(const MemberMap::iterator& membiter) {
    Membership* memb = membiter->second;
    memb->cull();
    memb->~Membership();
    userlist.erase(membiter);

    // If this channel became empty then it should be removed
    CheckDestroy();
}

Membership* Channel::GetUser(User* user) {
    MemberMap::iterator i = userlist.find(user);
    if (i == userlist.end()) {
        return NULL;
    }
    return i->second;
}

void Channel::SetDefaultModes() {
    ServerInstance->Logs->Log("CHANNELS", LOG_DEBUG, "SetDefaultModes %s",
                              ServerInstance->Config->DefaultModes.c_str());
    irc::spacesepstream list(ServerInstance->Config->DefaultModes);
    std::string modeseq;
    std::string parameter;

    list.GetToken(modeseq);

    for (std::string::iterator n = modeseq.begin(); n != modeseq.end(); ++n) {
        ModeHandler* mode = ServerInstance->Modes->FindMode(*n, MODETYPE_CHANNEL);
        if (mode) {
            if (mode->IsPrefixMode()) {
                continue;
            }

            if (mode->NeedsParam(true)) {
                list.GetToken(parameter);
                // If the parameter begins with a ':' then it's invalid
                if (parameter.c_str()[0] == ':') {
                    continue;
                }
            } else {
                parameter.clear();
            }

            if ((mode->NeedsParam(true)) && (parameter.empty())) {
                continue;
            }

            mode->OnModeChange(ServerInstance->FakeClient, ServerInstance->FakeClient, this,
                               parameter, true);
        }
    }
}

/*
 * add a channel to a user, creating the record for it if needed and linking
 * it to the user record
 */
Channel* Channel::JoinUser(LocalUser* user, std::string cname, bool override,
                           const std::string& key) {
    if (user->registered != REG_ALL) {
        ServerInstance->Logs->Log("CHANNELS", LOG_DEBUG,
                                  "Attempted to join unregistered user " + user->uuid + " to channel " + cname);
        return NULL;
    }

    /*
     * We don't restrict the number of channels that remote users or users that are override-joining may be in.
     * We restrict local users to <connect:maxchans> channels.
     * We restrict local operators to <oper:maxchans> channels.
     * This is a lot more logical than how it was formerly. -- w00t
     */
    if (!override) {
        unsigned int maxchans = user->GetClass()->maxchans;
        if (!maxchans) {
            maxchans = ServerInstance->Config->MaxChans;
        }
        if (user->IsOper()) {
            unsigned int opermaxchans = ConvToNum<unsigned int>
                                        (user->oper->getConfig("maxchans"));
            // If not set, use 2.0's <channels:opers>, if that's not set either, use limit from CC
            if (!opermaxchans && user->HasPrivPermission("channels/high-join-limit")) {
                opermaxchans = ServerInstance->Config->OperMaxChans;
            }
            if (opermaxchans > maxchans) {
                maxchans = opermaxchans;
            }
        }
        if (user->chans.size() >= maxchans) {
            user->WriteNumeric(ERR_TOOMANYCHANNELS, cname, "You are on too many channels");
            return NULL;
        }
    }

    // Crop channel name if it's too long
    if (cname.length() > ServerInstance->Config->Limits.ChanMax) {
        cname.resize(ServerInstance->Config->Limits.ChanMax);
    }

    Channel* chan = ServerInstance->FindChan(cname);
    bool created_by_local = (chan ==
                             NULL); // Flag that will be passed to modules in the OnUserJoin() hook later
    std::string privs; // Prefix mode(letter)s to give to the joining user

    if (!chan) {
        privs = ServerInstance->Config->DefaultModes.substr(0,
                ServerInstance->Config->DefaultModes.find(' '));

        if (override == false) {
            // Ask the modules whether they're ok with the join, pass NULL as Channel* as the channel is yet to be created
            ModResult MOD_RESULT;
            FIRST_MOD_RESULT(OnUserPreJoin, MOD_RESULT, (user, NULL, cname, privs, key));
            if (MOD_RESULT == MOD_RES_DENY) {
                return NULL;    // A module wasn't happy with the join, abort
            }
        }

        chan = new Channel(cname, ServerInstance->Time());
        // Set the default modes on the channel (<options:defaultmodes>)
        chan->SetDefaultModes();
    } else {
        /* Already on the channel */
        if (chan->HasUser(user)) {
            return NULL;
        }

        if (override == false) {
            ModResult MOD_RESULT;
            FIRST_MOD_RESULT(OnUserPreJoin, MOD_RESULT, (user, chan, cname, privs, key));

            // A module explicitly denied the join and (hopefully) generated a message
            // describing the situation, so we may stop here without sending anything
            if (MOD_RESULT == MOD_RES_DENY) {
                return NULL;
            }

            // If no module returned MOD_RES_DENY or MOD_RES_ALLOW (which is the case
            // most of the time) then proceed to check channel bans.
            //
            // If a module explicitly allowed the join (by returning MOD_RES_ALLOW),
            // then this entire section is skipped
            if (MOD_RESULT == MOD_RES_PASSTHRU) {
                if (chan->IsBanned(user)) {
                    user->WriteNumeric(ERR_BANNEDFROMCHAN, chan->name,
                                       "Cannot join channel (you're banned)");
                    return NULL;
                }
            }
        }
    }

    // We figured that this join is allowed and also created the
    // channel if it didn't exist before, now do the actual join
    chan->ForceJoin(user, &privs, false, created_by_local);
    return chan;
}

Membership* Channel::ForceJoin(User* user, const std::string* privs,
                               bool bursting, bool created_by_local) {
    if (IS_SERVER(user)) {
        ServerInstance->Logs->Log("CHANNELS", LOG_DEBUG,
                                  "Attempted to join server user " + user->uuid + " to channel " + this->name);
        return NULL;
    }

    Membership* memb = this->AddUser(user);
    if (!memb) {
        return NULL;    // Already on the channel
    }

    user->chans.push_front(memb);

    if (privs) {
        // If the user was granted prefix modes (in the OnUserPreJoin hook, or they're a
        // remote user and their own server set the modes), then set them internally now
        for (std::string::const_iterator i = privs->begin(); i != privs->end(); ++i) {
            PrefixMode* mh = ServerInstance->Modes->FindPrefixMode(*i);
            if (mh) {
                std::string nick = user->nick;
                // Set the mode on the user
                mh->OnModeChange(ServerInstance->FakeClient, NULL, this, nick, true);
            }
        }
    }

    // Tell modules about this join, they have the chance now to populate except_list with users we won't send the JOIN (and possibly MODE) to
    CUList except_list;
    FOREACH_MOD(OnUserJoin, (memb, bursting, created_by_local, except_list));

    ClientProtocol::Events::Join joinevent(memb);
    this->Write(joinevent, 0, except_list);

    FOREACH_MOD(OnPostJoin, (memb));
    return memb;
}

bool Channel::IsBanned(User* user) {
    ModResult result;
    FIRST_MOD_RESULT(OnCheckChannelBan, result, (user, this));

    if (result != MOD_RES_PASSTHRU) {
        return (result == MOD_RES_DENY);
    }

    ListModeBase* banlm = static_cast<ListModeBase*>(*ban);
    if (!banlm) {
        return false;
    }

    const ListModeBase::ModeList* bans = banlm->GetList(this);
    if (bans) {
        for (ListModeBase::ModeList::const_iterator it = bans->begin();
                it != bans->end(); it++) {
            if (CheckBan(user, it->mask)) {
                return true;
            }
        }
    }
    return false;
}

bool Channel::CheckBan(User* user, const std::string& mask) {
    ModResult result;
    FIRST_MOD_RESULT(OnCheckBan, result, (user, this, mask));
    if (result != MOD_RES_PASSTHRU) {
        return (result == MOD_RES_DENY);
    }

    // extbans were handled above, if this is one it obviously didn't match
    if ((mask.length() <= 2) || (mask[1] == ':')) {
        return false;
    }

    std::string::size_type at = mask.find('@');
    if (at == std::string::npos) {
        return false;
    }

    const std::string nickIdent = user->nick + "!" + user->ident;
    std::string prefix(mask, 0, at);
    if (InspIRCd::Match(nickIdent, prefix, NULL)) {
        std::string suffix(mask, at + 1);
        if (InspIRCd::Match(user->GetRealHost(), suffix, NULL) ||
                InspIRCd::Match(user->GetDisplayedHost(), suffix, NULL) ||
                InspIRCd::MatchCIDR(user->GetIPString(), suffix, NULL)) {
            return true;
        }
    }
    return false;
}

ModResult Channel::GetExtBanStatus(User *user, char type) {
    ModResult rv;
    FIRST_MOD_RESULT(OnExtBanCheck, rv, (user, this, type));
    if (rv != MOD_RES_PASSTHRU) {
        return rv;
    }

    ListModeBase* banlm = static_cast<ListModeBase*>(*ban);
    if (!banlm) {
        return MOD_RES_PASSTHRU;
    }

    const ListModeBase::ModeList* bans = banlm->GetList(this);
    if (bans) {
        for (ListModeBase::ModeList::const_iterator it = bans->begin();
                it != bans->end(); ++it) {
            if (it->mask.length() <= 2 || it->mask[0] != type || it->mask[1] != ':') {
                continue;
            }

            if (CheckBan(user, it->mask.substr(2))) {
                return MOD_RES_DENY;
            }
        }
    }
    return MOD_RES_PASSTHRU;
}

/* Channel::PartUser
 * Remove a channel from a users record, remove the reference to the Membership object
 * from the channel and destroy it.
 */
bool Channel::PartUser(User* user, std::string& reason) {
    MemberMap::iterator membiter = userlist.find(user);

    if (membiter == userlist.end()) {
        return false;
    }

    Membership* memb = membiter->second;
    CUList except_list;
    FOREACH_MOD(OnUserPart, (memb, reason, except_list));

    ClientProtocol::Messages::Part partmsg(memb, reason);
    Write(ServerInstance->GetRFCEvents().part, partmsg, 0, except_list);

    // Remove this channel from the user's chanlist
    user->chans.erase(memb);
    // Remove the Membership from this channel's userlist and destroy it
    this->DelUser(membiter);

    return true;
}

void Channel::KickUser(User* src, const MemberMap::iterator& victimiter,
                       const std::string& reason) {
    Membership* memb = victimiter->second;
    CUList except_list;
    FOREACH_MOD(OnUserKick, (src, memb, reason, except_list));

    ClientProtocol::Messages::Kick kickmsg(src, memb, reason);
    Write(ServerInstance->GetRFCEvents().kick, kickmsg, 0, except_list);

    memb->user->chans.erase(memb);
    this->DelUser(victimiter);
}

void Channel::Write(ClientProtocol::Event& protoev, char status,
                    const CUList& except_list) {
    unsigned int minrank = 0;
    if (status) {
        PrefixMode* mh = ServerInstance->Modes->FindPrefix(status);
        if (mh) {
            minrank = mh->GetPrefixRank();
        }
    }
    for (MemberMap::iterator i = userlist.begin(); i != userlist.end(); i++) {
        LocalUser* user = IS_LOCAL(i->first);
        if ((user) && (!except_list.count(user))) {
            /* User doesn't have the status we're after */
            if (minrank && i->second->getRank() < minrank) {
                continue;
            }

            user->Send(protoev);
        }
    }
}

const char* Channel::ChanModes(bool showsecret) {
    static std::string scratch;
    std::string sparam;

    scratch.clear();

    /* This was still iterating up to 190, Channel::modes is only 64 elements -- Om */
    for(int n = 0; n < 64; n++) {
        ModeHandler* mh = ServerInstance->Modes->FindMode(n + 65, MODETYPE_CHANNEL);
        if (mh && IsModeSet(mh)) {
            scratch.push_back(n + 65);

            ParamModeBase* pm = mh->IsParameterMode();
            if (!pm) {
                continue;
            }

            if (pm->IsParameterSecret() && !showsecret) {
                sparam += " <" + pm->name + ">";
            } else {
                sparam += ' ';
                pm->GetParameter(this, sparam);
            }
        }
    }

    scratch += sparam;
    return scratch.c_str();
}

void Channel::WriteNotice(const std::string& text, char status) {
    ClientProtocol::Messages::Privmsg privmsg(
        ClientProtocol::Messages::Privmsg::nocopy, ServerInstance->FakeClient, this,
        text, MSG_NOTICE, status);
    Write(ServerInstance->GetRFCEvents().privmsg, privmsg, status);
}

void Channel::WriteRemoteNotice(const std::string& text, char status) {
    WriteNotice(text, status);
    ServerInstance->PI->SendMessage(this, status, text, MSG_NOTICE);
}

/* returns the status character for a given user on a channel, e.g. @ for op,
 * % for halfop etc. If the user has several modes set, the highest mode
 * the user has must be returned.
 */
char Membership::GetPrefixChar() const {
    char pf = 0;
    unsigned int bestrank = 0;

    for (std::string::const_iterator i = modes.begin(); i != modes.end(); ++i) {
        PrefixMode* mh = ServerInstance->Modes->FindPrefixMode(*i);
        if (mh && mh->GetPrefixRank() > bestrank && mh->GetPrefix()) {
            bestrank = mh->GetPrefixRank();
            pf = mh->GetPrefix();
        }
    }
    return pf;
}

unsigned int Membership::getRank() {
    char mchar = modes.c_str()[0];
    unsigned int rv = 0;
    if (mchar) {
        PrefixMode* mh = ServerInstance->Modes->FindPrefixMode(mchar);
        if (mh) {
            rv = mh->GetPrefixRank();
        }
    }
    return rv;
}

std::string Membership::GetAllPrefixChars() const {
    std::string ret;
    for (std::string::const_iterator i = modes.begin(); i != modes.end(); ++i) {
        PrefixMode* mh = ServerInstance->Modes->FindPrefixMode(*i);
        if (mh && mh->GetPrefix()) {
            ret.push_back(mh->GetPrefix());
        }
    }

    return ret;
}

unsigned int Channel::GetPrefixValue(User* user) {
    MemberMap::iterator m = userlist.find(user);
    if (m == userlist.end()) {
        return 0;
    }
    return m->second->getRank();
}

bool Membership::SetPrefix(PrefixMode* delta_mh, bool adding) {
    char prefix = delta_mh->GetModeChar();
    for (unsigned int i = 0; i < modes.length(); i++) {
        char mchar = modes[i];
        PrefixMode* mh = ServerInstance->Modes->FindPrefixMode(mchar);
        if (mh && mh->GetPrefixRank() <= delta_mh->GetPrefixRank()) {
            modes = modes.substr(0,i) +
                    (adding ? std::string(1, prefix) : "") +
                    modes.substr(mchar == prefix ? i+1 : i);
            return adding != (mchar == prefix);
        }
    }
    if (adding) {
        modes.push_back(prefix);
    }
    return adding;
}


void Membership::WriteNotice(const std::string& text) const {
    LocalUser* const localuser = IS_LOCAL(user);
    if (!localuser) {
        return;
    }

    ClientProtocol::Messages::Privmsg privmsg(
        ClientProtocol::Messages::Privmsg::nocopy, ServerInstance->FakeClient,
        this->chan, text, MSG_NOTICE);
    localuser->Send(ServerInstance->GetRFCEvents().privmsg, privmsg);
}
