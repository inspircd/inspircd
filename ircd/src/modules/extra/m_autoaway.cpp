/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2020 Sadie Powell <sadie@witchery.services>
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

/// $ModAuthor: Sadie Powell
/// $ModAuthorMail: sadie@witchery.services
/// $ModConfig: <autoaway checkperiod="5m" idleperiod="24h" message="Idle">
/// $ModDepends: core 3
/// $ModDesc: Automatically marks idle users as away.


#include "inspircd.h"
#include "modules/away.h"

enum {
    // From RFC 1459.
    RPL_UNAWAY = 305,
    RPL_NOWAWAY = 306
};

class ModuleAutoAway
    : public Module
    , public Timer
    , Away::EventListener {
  private:
    LocalIntExt autoaway;
    Away::EventProvider awayevprov;
    unsigned long idleperiod;
    std::string message;
    bool setting;

  public:
    ModuleAutoAway()
        : Timer(0, true)
        , Away::EventListener(this)
        , autoaway("autoaway", ExtensionItem::EXT_CHANNEL, this)
        , awayevprov(this)
        , setting(false) {
    }

    void ReadConfig(ConfigStatus&) CXX11_OVERRIDE {
        ConfigTag* tag = ServerInstance->Config->ConfValue("autoaway");
        SetInterval(tag->getDuration("checkperiod", 5*60));
        idleperiod = tag->getDuration("idleperiod", 24*60*60);
        message = tag->getString("message", "Idle");
    }

    bool Tick(time_t) CXX11_OVERRIDE {
        ServerInstance->Logs->Log(MODNAME, LOG_DEBUG, "Checking for idle users ...");
        setting = true;
        time_t idlethreshold = ServerInstance->Time() - idleperiod;
        const UserManager::LocalList& users = ServerInstance->Users.GetLocalUsers();
        for (UserManager::LocalList::const_iterator iter = users.begin(); iter != users.end(); ++iter) {
            LocalUser* user = *iter;

            // Skip users who are already away or who are not idle.
            if (user->IsAway() || user->idle_lastmsg > idlethreshold) {
                continue;
            }

            autoaway.set(user, 1);
            user->awaytime = ServerInstance->Time();
            user->awaymsg.assign(message, 0, ServerInstance->Config->Limits.MaxAway);
            user->WriteNumeric(RPL_NOWAWAY,
                               "You have been automatically marked as being away");
            FOREACH_MOD_CUSTOM(awayevprov, Away::EventListener, OnUserAway, (user));
        }
        setting = false;
        return true;
    }

    void OnUserAway(User* user) CXX11_OVERRIDE {
        // If the user is changing their away status then unmark them.
        if (IS_LOCAL(user) && !setting) {
            autoaway.set(user, 0);
        }
    }

    void OnUserBack(User* user) CXX11_OVERRIDE {
        // If the user is unsetting their away status then unmark them.
        if (IS_LOCAL(user)) {
            autoaway.set(user, 0);
        }
    }

    void OnUserPostMessage(User* user, const MessageTarget& target,
                           const MessageDetails& details) CXX11_OVERRIDE {
        if (!IS_LOCAL(user) || !autoaway.get(user)) {
            return;
        }

        autoaway.set(user, 0);
        user->awaytime = 0;
        user->awaymsg.clear();
        user->WriteNumeric(RPL_UNAWAY, "You are no longer automatically marked as being away");
        FOREACH_MOD_CUSTOM(awayevprov, Away::EventListener, OnUserBack, (user));
    }

    Version GetVersion() CXX11_OVERRIDE {
        return Version("Automatically marks idle users as away.");
    }
};

MODULE_INIT(ModuleAutoAway)
