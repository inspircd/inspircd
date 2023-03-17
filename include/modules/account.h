/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2015 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2013, 2020 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2009 Uli Schlachter <psychon@inspircd.org>
 *   Copyright (C) 2008 Craig Edwards <brain@inspircd.org>
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


#pragma once

#include <map>
#include <string>

#include "event.h"

typedef StringExtItem AccountExtItem;

inline AccountExtItem* GetAccountExtItem() {
    return static_cast<AccountExtItem*>
           (ServerInstance->Extensions.GetItem("accountname"));
}

inline AccountExtItem* GetAccountIdExtItem() {
    return static_cast<AccountExtItem*>
           (ServerInstance->Extensions.GetItem("accountid"));
}

class AccountEventListener : public Events::ModuleEventListener {
  public:
    AccountEventListener(Module* mod)
        : ModuleEventListener(mod, "event/account") {
    }

    /** Called when a user logs in or logs out
     * @param user User logging in or out
     * @param newaccount New account name of the user or empty string if the user
     * logged out
     */
    virtual void OnAccountChange(User* user, const std::string& newaccount) = 0;
};
