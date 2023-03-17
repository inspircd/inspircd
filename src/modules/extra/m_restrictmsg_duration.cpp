/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2018-2020 Matt Schatz <genius3000@g3k.solutions>
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

/// $ModAuthor: genius3000
/// $ModAuthorMail: genius3000@g3k.solutions
/// $ModConfig: <restrictmsg_duration duration="1m" target="both" notify="no" exemptoper="yes" exemptuline="yes" exemptregistered="yes">
/// $ModDepends: core 3
/// $ModDesc: Restrict messages until a user has been connected for a specified duration.

/* Config descriptions:
 * duration:         time string for how long after sign on to restrict messages. Default: 1m
 * target:           which targets to block: user, chan, or both. Default: both
 * notify:           whether to let the user know their message was blocked. Default: no
 * exemptoper:       whether to exempt messages to opers. Default: yes
 * exemptuline:      whether to exempt messages to U-Lined clients (services). Default: yes
 * exemptregistered: whether to exempt messages from registered (and identified) users. Default: yes
 * Connect Class exemption (add to any connect class block you wish to exempt from this):
 * exemptrestrictmsg="yes"
 */


#include "inspircd.h"
#include "modules/account.h"

class ModuleRestrictMsgDuration : public Module {
    bool blockuser;
    bool blockchan;
    bool exemptoper;
    bool exemptuline;
    bool exemptregistered;
    bool notify;
    time_t duration;

  public:
    void Prioritize() CXX11_OVERRIDE {
        // Go last to let filter, +R, bans, etc. act first
        ServerInstance->Modules->SetPriority(this, PRIORITY_LAST);
    }

    void ReadConfig(ConfigStatus&) CXX11_OVERRIDE {
        ConfigTag* tag = ServerInstance->Config->ConfValue("restrictmsg_duration");

        const std::string target = tag->getString("target", "both");
        if (!stdalgo::string::equalsci(target, "user") && !stdalgo::string::equalsci(target, "chan") && !stdalgo::string::equalsci(target, "both")) {
            throw ModuleException("Invalid \"target\" of '" + target +
                                  "' in <restrictmsg_duration>. Default of both will be used.");
        }

        blockuser = (!stdalgo::string::equalsci(target, "chan"));
        blockchan = (!stdalgo::string::equalsci(target, "user"));
        exemptoper = tag->getBool("exemptoper", true);
        exemptuline = tag->getBool("exemptuline", true);
        exemptregistered = tag->getBool("exemptregistered", true);
        notify = tag->getBool("notify");
        duration = tag->getDuration("duration", 60);
    }

    ModResult OnUserPreMessage(User* user, const MessageTarget& target,
                               MessageDetails& details) CXX11_OVERRIDE {
        // Only check messages targeting a user or channel
        if (target.type != MessageTarget::TYPE_USER && target.type != MessageTarget::TYPE_CHANNEL) {
            return MOD_RES_PASSTHRU;
        }

        // Only check against non-oper local users
        LocalUser* src = IS_LOCAL(user);
        if (!src || src->IsOper()) {
            return MOD_RES_PASSTHRU;
        }

        // Check their connected duration
        if (src->signon + duration <= ServerInstance->Time()) {
            return MOD_RES_PASSTHRU;
        }

        // Check for connect class exemption
        if (src->MyClass->config->getBool("exemptrestrictmsg")) {
            return MOD_RES_PASSTHRU;
        }

        // Source is registered (and identified) exemption
        if (exemptregistered) {
            const AccountExtItem* accountext = GetAccountExtItem();
            const std::string* account = accountext ? accountext->get(src) : NULL;
            if (account) {
                return MOD_RES_PASSTHRU;
            }
        }

        if (blockuser && target.type == MessageTarget::TYPE_USER) {
            User* const dst = target.Get<User>();

            // Target is Oper exemption
            if (exemptoper && dst->IsOper()) {
                return MOD_RES_PASSTHRU;
            }
            // Target is on a U-Lined server exemption
            if (exemptuline && dst->server->IsULine()) {
                return MOD_RES_PASSTHRU;
            }

            if (notify) {
                src->WriteNumeric(ERR_CANTSENDTOUSER, target.Get<User>()->nick,
                                  InspIRCd::Format("You cannot send messages within the first %lu seconds of connecting.",
                                                   duration));
            }

            return MOD_RES_DENY;
        } else if (blockchan && target.type == MessageTarget::TYPE_CHANNEL) {
            if (notify)
                src->WriteNumeric(ERR_CANNOTSENDTOCHAN, target.Get<Channel>()->name,
                                  InspIRCd::Format("You cannot send messages within the first %lu seconds of connecting.",
                                                   duration));

            return MOD_RES_DENY;
        }

        return MOD_RES_PASSTHRU;
    }

    Version GetVersion() CXX11_OVERRIDE {
        return Version("Restrict messages until a user has been connected for a specified duration.");
    }
};

MODULE_INIT(ModuleRestrictMsgDuration)
