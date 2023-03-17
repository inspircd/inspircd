/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2013-2014, 2016, 2018 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2013, 2017-2019, 2022 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012, 2019 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2008 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2007-2008 Craig Edwards <brain@inspircd.org>
 *   Copyright (C) 2007, 2009 Dennis Friis <peavey@inspircd.org>
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
#include "modules/exemption.h"
#include "modules/names.h"
#include "modules/who.h"

class AuditoriumMode : public SimpleChannelModeHandler {
  public:
    AuditoriumMode(Module* Creator) : SimpleChannelModeHandler(Creator,
                "auditorium", 'u') {
        ranktoset = ranktounset = OP_VALUE;
    }
};

class ModuleAuditorium;

namespace {

/** Hook handler for join client protocol events.
 * This allows us to block join protocol events completely, including all associated messages (e.g. MODE, away-notify AWAY).
 * This is not the same as OnUserJoin() because that runs only when a real join happens but this runs also when a module
 * such as delayjoin or hostcycle generates a join.
 */
class JoinHook : public ClientProtocol::EventHook {
    ModuleAuditorium* const parentmod;
    bool active;

  public:
    JoinHook(ModuleAuditorium* mod);
    void OnEventInit(const ClientProtocol::Event& ev) CXX11_OVERRIDE;
    ModResult OnPreEventSend(LocalUser* user, const ClientProtocol::Event& ev,
                             ClientProtocol::MessageList& messagelist) CXX11_OVERRIDE;
};

}

class ModuleAuditorium
    : public Module
    , public Names::EventListener
    , public Who::EventListener
    , public Who::VisibleEventListener {
    CheckExemption::EventProvider exemptionprov;
    AuditoriumMode aum;
    bool OpsVisible;
    bool OpsCanSee;
    bool OperCanSee;
    JoinHook joinhook;

  public:
    ModuleAuditorium()
        : Names::EventListener(this)
        , Who::EventListener(this)
        , Who::VisibleEventListener(this)
        , exemptionprov(this)
        , aum(this)
        , joinhook(this) {
    }

    void ReadConfig(ConfigStatus& status) CXX11_OVERRIDE {
        ConfigTag* tag = ServerInstance->Config->ConfValue("auditorium");
        OpsVisible = tag->getBool("opvisible");
        OpsCanSee = tag->getBool("opcansee");
        OperCanSee = tag->getBool("opercansee", true);
    }

    Version GetVersion() CXX11_OVERRIDE {
        return Version("Adds channel mode u (auditorium) which hides unprivileged users in a channel from each other.", VF_VENDOR);
    }

    /* Can they be seen by everyone? */
    bool IsVisible(Membership* memb) {
        if (!memb->chan->IsModeSet(&aum)) {
            return true;
        }

        ModResult res = CheckExemption::Call(exemptionprov, memb->user, memb->chan,
                                             "auditorium-vis");
        return res.check(OpsVisible && memb->getRank() >= OP_VALUE);
    }

    /* Can they see this specific membership? */
    bool CanSee(User* issuer, Membership* memb) {
        // If user is oper and operoverride is on, don't touch the list
        if (OperCanSee && issuer->HasPrivPermission("channels/auspex")) {
            return true;
        }

        // You can always see yourself
        if (issuer == memb->user) {
            return true;
        }

        // Can you see the list by permission?
        ModResult res = CheckExemption::Call(exemptionprov, issuer, memb->chan,
                                             "auditorium-see");
        if (res.check(OpsCanSee && memb->chan->GetPrefixValue(issuer) >= OP_VALUE)) {
            return true;
        }

        return false;
    }

    ModResult OnNamesListItem(LocalUser* issuer, Membership* memb,
                              std::string& prefixes, std::string& nick) CXX11_OVERRIDE {
        if (IsVisible(memb)) {
            return MOD_RES_PASSTHRU;
        }

        if (CanSee(issuer, memb)) {
            return MOD_RES_PASSTHRU;
        }

        // Don't display this user in the NAMES list
        return MOD_RES_DENY;
    }

    /** Build CUList for showing this join/part/kick */
    void BuildExcept(Membership* memb, CUList& excepts) {
        if (IsVisible(memb)) {
            return;
        }

        const Channel::MemberMap& users = memb->chan->GetUsers();
        for (Channel::MemberMap::const_iterator i = users.begin(); i != users.end();
                ++i) {
            if (IS_LOCAL(i->first) && !CanSee(i->first, memb)) {
                excepts.insert(i->first);
            }
        }
    }

    void OnUserPart(Membership* memb, std::string &partmessage,
                    CUList& excepts) CXX11_OVERRIDE {
        BuildExcept(memb, excepts);
    }

    void OnUserKick(User* source, Membership* memb, const std::string &reason,
                    CUList& excepts) CXX11_OVERRIDE {
        BuildExcept(memb, excepts);
    }

    void OnBuildNeighborList(User* source, IncludeChanList& include,
                             std::map<User*, bool>& exception) CXX11_OVERRIDE {
        for (IncludeChanList::iterator i = include.begin(); i != include.end(); ) {
            Membership* memb = *i;
            if (IsVisible(memb)) {
                ++i;
                continue;
            }

            // this channel should not be considered when listing my neighbors
            i = include.erase(i);
            // however, that might hide me from ops that can see me...
            const Channel::MemberMap& users = memb->chan->GetUsers();
            for(Channel::MemberMap::const_iterator j = users.begin(); j != users.end();
                    ++j) {
                if (IS_LOCAL(j->first) && CanSee(j->first, memb)) {
                    exception[j->first] = true;
                }
            }
        }
    }

    ModResult OnWhoLine(const Who::Request& request, LocalUser* source, User* user,
                        Membership* memb, Numeric::Numeric& numeric) CXX11_OVERRIDE {
        if (!memb) {
            return MOD_RES_PASSTHRU;
        }
        if (IsVisible(memb)) {
            return MOD_RES_PASSTHRU;
        }
        if (CanSee(source, memb)) {
            return MOD_RES_PASSTHRU;
        }
        return MOD_RES_DENY;
    }

    ModResult OnWhoVisible(const Who::Request& request, LocalUser* source,
                           Membership* memb) CXX11_OVERRIDE {
        // Never pick a channel as the first visible one if the channel is in auditorium mode.
        return IsVisible(memb) || CanSee(source, memb) ? MOD_RES_PASSTHRU : MOD_RES_DENY;
    }
};

JoinHook::JoinHook(ModuleAuditorium* mod)
    : ClientProtocol::EventHook(mod, "JOIN", 10)
    , parentmod(mod) {
}

void JoinHook::OnEventInit(const ClientProtocol::Event& ev) {
    const ClientProtocol::Events::Join& join =
        static_cast<const ClientProtocol::Events::Join&>(ev);
    active = !parentmod->IsVisible(join.GetMember());
}

ModResult JoinHook::OnPreEventSend(LocalUser* user,
                                   const ClientProtocol::Event& ev, ClientProtocol::MessageList& messagelist) {
    if (!active) {
        return MOD_RES_PASSTHRU;
    }

    const ClientProtocol::Events::Join& join =
        static_cast<const ClientProtocol::Events::Join&>(ev);
    return ((parentmod->CanSee(user,
                               join.GetMember())) ? MOD_RES_PASSTHRU : MOD_RES_DENY);
}

MODULE_INIT(ModuleAuditorium)
