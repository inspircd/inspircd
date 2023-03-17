/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2013-2014 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2013, 2017 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2008 Thomas Stagner <aquanight@inspircd.org>
 *   Copyright (C) 2008 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2006 Craig Edwards <brain@inspircd.org>
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

class SnomaskManager;
class Snomask {
    /** Description of this snomask, e.g.: OPER, ANNOUNCEMENT, XLINE
     */
    std::string Description;

    /** Information about the last sent message,
     * used for sending "last message repeated X times" messages
     */
    std::string LastMessage;
    char LastLetter;
    unsigned int Count;

    /** Log and send a message to all opers who have the given snomask set
     * @param letter The target users of this message
     * @param desc The description of this snomask, will be prepended to the message
     * @param msg The message to send
     */
    static void Send(char letter, const std::string& desc, const std::string& msg);

  public:
    /** Create a new Snomask
     */
    Snomask();

    /** Sends a message to all opers with this snomask.
     * @param message The message to send
     * @param letter The snomask character to send the message to.
     */
    void SendMessage(const std::string& message, char letter);

    /** Sends out the (last message repeated N times) message
     */
    void Flush();

    /** Returns the description of this snomask
     * @param letter The letter of this snomask. If uppercase, the description of the remote
     * variant of this snomask will be returned (i.e.: "REMOTE" will be prepended to the description).
     * @return The description of this snomask
     */
    std::string GetDescription(char letter) const;

    friend class SnomaskManager;
};

/** Snomask manager handles routing of SNOMASK (usermode +s) messages to opers.
 * Modules and the core can enable and disable snomask characters. If they do,
 * then sending snomasks using these characters becomes possible.
 */
class CoreExport SnomaskManager : public fakederef<SnomaskManager> {
    Snomask masks[26];

  public:
    /** Create a new SnomaskManager
     */
    SnomaskManager();

    /** Enable a snomask.
     * @param letter The snomask letter to enable. Once enabled,
     * server notices may be routed to users with this letter in
     * their list, and users may add this letter to their list.
     * @param description The descriptive text sent along with any
     * server notices, at the start of the notice, e.g. "GLOBOPS".
     */
    void EnableSnomask(char letter, const std::string &description);

    /** Write to all users with a given snomask (local server only)
     * @param letter The snomask letter to write to
     * @param text The text to send to the users
     */
    void WriteToSnoMask(char letter, const std::string &text);

    /** Write to all users with a given snomask (local server only)
     * @param letter The snomask letter to write to
     * @param text A format string containing text to send
     * @param ... Format arguments
     */
    void WriteToSnoMask(char letter, const char* text, ...) CUSTOM_PRINTF(3, 4);

    /** Write to all users with a given snomask (sent globally)
     * @param letter The snomask letter to write to
     * @param text The text to send to the users
     */
    void WriteGlobalSno(char letter, const std::string &text);

    /** Write to all users with a given snomask (sent globally)
     * @param letter The snomask letter to write to
     * @param text A format string containing text to send
     * @param ... Format arguments
     */
    void WriteGlobalSno(char letter, const char* text, ...) CUSTOM_PRINTF(3, 4);

    /** Called once per 5 seconds from the mainloop, this flushes any cached
     * snotices. The way the caching works is as follows:
     * Calls to WriteToSnoMask write to a cache, if the call is the same as it was
     * for the previous call, then a count is incremented. If it is different,
     * the previous message it just sent normally via NOTICE (with count if > 1)
     * and the new message is cached. This acts as a sender in case the number of notices
     * is not particularly significant, in order to keep notices going out.
     */
    void FlushSnotices();

    /** Check whether a given character is an enabled (initialized) snomask.
     * Valid snomask chars are lower- or uppercase letters and have a description.
     * Snomasks are initialized with EnableSnomask().
     * @param ch The character to check
     * @return True if the given char is allowed to be set via +s.
     */
    bool IsSnomaskUsable(char ch) const;
};
