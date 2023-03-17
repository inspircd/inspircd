/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2010 Daniel De Graaf <danieldg@inspircd.org>
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

/// $ModConfig: <opmoderated modechar="U">
/// $ModDepends: core 3
/// $ModDesc: Implements channel mode +U and extban 'u' - moderator mute


#include "inspircd.h"
#include "modules/exemption.h"

class ModuleOpModerated : public Module {
    SimpleChannelModeHandler opmod;
    CheckExemption::EventProvider exemptionprov;

  public:
    ModuleOpModerated()
        : opmod(this, "opmoderated",
                ServerInstance->Config->ConfValue("opmoderated")->getString("modechar", "U", 1,
                        1)[0])
        , exemptionprov(this) {
    }

    void Prioritize() CXX11_OVERRIDE {
        // since we steal the message, we should be last (let everyone else eat it first)
        ServerInstance->Modules->SetPriority(this, I_OnUserPreMessage, PRIORITY_LAST);
    }

    ModResult OnUserPreMessage(User *user, const MessageTarget& target,
                               MessageDetails& details) CXX11_OVERRIDE {
        if (target.type != MessageTarget::TYPE_CHANNEL || target.status) {
            return MOD_RES_PASSTHRU;
        }

        if (IS_LOCAL(user) && user->HasPrivPermission("channels/ignore-opmoderated")) {
            return MOD_RES_PASSTHRU;
        }

        Channel* const chan = target.Get<Channel>();
        if (CheckExemption::Call(exemptionprov, user, chan, "opmoderated") == MOD_RES_ALLOW) {
            return MOD_RES_PASSTHRU;
        }

        if (!chan->GetExtBanStatus(user, 'u').check(!chan->IsModeSet(&opmod)) && chan->GetPrefixValue(user) < VOICE_VALUE) {
            // Add any unprivileged users to the exemption list.
            const Channel::MemberMap& users = chan->GetUsers();
            for (Channel::MemberMap::const_iterator i = users.begin(); i != users.end();
                    ++i) {
                if (i->second->getRank() < OP_VALUE) {
                    details.exemptions.insert(i->first);
                }
            }
        }

        return MOD_RES_PASSTHRU;
    }

    void On005Numeric(std::map<std::string, std::string>& tokens) CXX11_OVERRIDE {
        tokens["EXTBAN"].push_back('u');
    }

    Version GetVersion() CXX11_OVERRIDE {
        return Version("Implements opmoderated channel mode +U (non-voiced messages sent to ops) and extban 'u'", VF_OPTCOMMON);
    }
};

MODULE_INIT(ModuleOpModerated)
