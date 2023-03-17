/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2019 linuxdaemon <linuxdaemon.irc@gmail.com>
 *   Copyright (C) 2018, 2021-2022 Sadie Powell <sadie@witchery.services>
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

namespace Who {
class EventListener;
class MatchEventListener;
class VisibleEventListener;
class Request;
}

class Who::EventListener : public Events::ModuleEventListener {
  public:
    EventListener(Module* mod)
        : ModuleEventListener(mod, "event/who") {
    }

    /** Called when a result from WHO is about to be queued.
     * @param request Details about the WHO request which caused this response.
     * @param source The user who initiated this WHO request.
     * @param user The user that this line of the WHO request is about.
     * @param memb The channel membership of the user or the first visible membership if not targeted at a channel.
     * @param numeric The numeric which will be sent in response to the request.
     * @return MOD_RES_ALLOW to explicitly allow the response, MOD_RES_DENY to explicitly deny the
     *         response, or MOD_RES_PASSTHRU to let another module handle the event.
     */
    virtual ModResult OnWhoLine(const Request& request, LocalUser* source,
                                User* user, Membership* memb, Numeric::Numeric& numeric) = 0;
};

class Who::MatchEventListener
    : public Events::ModuleEventListener {
  public:
    MatchEventListener(Module* mod)
        : ModuleEventListener(mod, "event/who-match") {
    }

    /** Called when a WHO request needs to check if a user matches it.
     * @param request Details about the WHO request which caused this match attempt.
     * @param source The user who initiated this WHO request.
     * @param user The user to attempt to match the WHO request against.
     * @return MOD_RES_ALLOW to explicitly allow the match, MOD_RES_DENY to explicitly deny the
     *         match, or MOD_RES_PASSTHRU to let another module handle the event.
     */
    virtual ModResult OnWhoMatch(const Request& request, LocalUser* source,
                                 User* user) = 0;
};

class Who::VisibleEventListener
    : public Events::ModuleEventListener {
  public:
    VisibleEventListener(Module* mod)
        : ModuleEventListener(mod, "event/who-visible") {
    }

    /** Called when a WHO request needs to check if a channel is visible.
     * @param request Details about the WHO request which caused this match attempt.
     * @param source The user who initiated this WHO request.
     * @param memb The channel membership of the user to check the visibility of.
     * @return MOD_RES_ALLOW to explicitly allow the match, MOD_RES_DENY to explicitly deny the
     *         match, or MOD_RES_PASSTHRU to let another module handle the event.
     */
    virtual ModResult OnWhoVisible(const Request& request, LocalUser* source,
                                   Membership* memb) = 0;
};

class Who::Request {
  public:
    typedef std::bitset<UCHAR_MAX + 1> CharState;

    /** The flags for matching users to include. */
    CharState flags;

    /** Whether we are matching using a wildcard or a flag. */
    bool fuzzy_match;

    /** The text to match against. */
    std::string matchtext;

    /** If the target was a channel then the matched channel */
    Channel* matchchan;

    /** The WHO/WHOX responses we will send to the source. */
    std::vector<Numeric::Numeric> results;

    /** Whether the source requested a WHOX response. */
    bool whox;

    /** The fields to include in the WHOX response. */
    CharState whox_fields;

    /** A user specified label for the WHOX response. */
    std::string whox_querytype;

    /** Get the index in the response parameters for the different data fields
     *
     * The fields 'r' (realname) and 'd' (hops) will always be missing in a non-WHOX
     * query, because WHOX splits them to 2 fields, where old WHO has them as one.
     *
     * @param flag The field name to look for
     * @param out The index will be stored in this value
     * @return True if the field is available, false otherwise
     */
    virtual bool GetFieldIndex(char flag, size_t& out) const = 0;

  protected:
    Request()
        : fuzzy_match(false)
        , matchchan(NULL)
        , whox(false) {
    }
};
