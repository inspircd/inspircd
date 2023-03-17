/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2017, 2019 Sadie Powell <sadie@witchery.services>
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

#include "event.h"

namespace CheckExemption {
class EventListener;
class EventProvider;

/** Helper function for calling the CheckExemption::EventListener::OnCheckExemption event.
 * @param prov The CheckExemption::EventProvider which is calling the event.
 * @param user The user to check exemption for.
 * @param chan The channel to check exemption on.
 * @param restriction The restriction to check for.
 * @return Either MOD_RES_ALLOW if the exemption was confirmed, MOD_RES_DENY if the exemption was
 *         denied or MOD_RES_PASSTHRU if no module handled the event.
 */
inline ModResult Call(const CheckExemption::EventProvider& prov, User* user,
                      Channel* chan, const std::string& restriction);
}

class CheckExemption::EventListener
    : public Events::ModuleEventListener {
  protected:
    EventListener(Module* mod, unsigned int eventprio = DefaultPriority)
        : ModuleEventListener(mod, "event/exemption", eventprio) {
    }

  public:
    /** Called when checking if a user is exempt from something.
     * @param user The user to check exemption for.
     * @param chan The channel to check exemption on.
     * @param restriction The restriction to check for.
     * @return Either MOD_RES_ALLOW to confirm an exemption, MOD_RES_DENY to deny an exemption,
     *         or MOD_RES_PASSTHRU to let another module handle the event.
     */
    virtual ModResult OnCheckExemption(User* user, Channel* chan,
                                       const std::string& restriction) = 0;
};

class CheckExemption::EventProvider
    : public Events::ModuleEventProvider {
  public:
    EventProvider(Module* mod)
        : ModuleEventProvider(mod, "event/exemption") {
    }
};

inline ModResult CheckExemption::Call(const CheckExemption::EventProvider& prov,
                                      User* user, Channel* chan, const std::string& restriction) {
    ModResult result;
    FIRST_MOD_RESULT_CUSTOM(prov, CheckExemption::EventListener, OnCheckExemption,
                            result, (user, chan, restriction));
    return result;
}
