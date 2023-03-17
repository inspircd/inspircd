/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2013-2016, 2018 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012-2013, 2017-2020 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2007-2008 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2007, 2009 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2003-2006, 2008 Craig Edwards <brain@inspircd.org>
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

#include "membership.h"
#include "mode.h"
#include "parammode.h"

/** Holds an entry for a ban list, exemption list, or invite list.
 * This class contains a single element in a channel list, such as a banlist.
 */

/** Holds all relevant information for a channel.
 * This class represents a channel, and contains its name, modes, topic, topic set time,
 * etc, and an instance of the BanList type.
 */
class CoreExport Channel : public Extensible {
  public:
    /** A map of Memberships on a channel keyed by User pointers
     */
    typedef std::map<User*, insp::aligned_storage<Membership> > MemberMap;

  private:
    /** Set default modes for the channel on creation
     */
    void SetDefaultModes();

    /** Modes for the channel.
     * It is a bitset where each item in it represents if a mode is set.
     * To see if a mode is set, inspect modes[mh->modeid]
     */
    std::bitset<ModeParser::MODEID_MAX> modes;

    /** Remove the given membership from the channel's internal map of
     * memberships and destroy the Membership object.
     * This function does not remove the channel from User::chanlist.
     * Since the parameter is an iterator to the target, the complexity
     * of this function is constant.
     * @param membiter The MemberMap iterator to remove, must be valid
     */
    void DelUser(const MemberMap::iterator& membiter);

  public:
    /** Creates a channel record and initialises it with default values
     * @param name The name of the channel
     * @param ts The creation time of the channel
     * @throw CoreException if this channel name is in use
     */
    Channel(const std::string &name, time_t ts);

    /** Checks whether the channel should be destroyed, and if yes, begins
     * the teardown procedure.
     *
     * If there are users on the channel or a module vetoes the deletion
     * (OnPreChannelDelete hook) then nothing else happens.
     * Otherwise, first the OnChannelDelete event is fired, then the channel is
     * removed from the channel list. All pending invites are destroyed and
     * finally the channel is added to the cull list.
     */
    void CheckDestroy();

    /** The channel's name.
     */
    std::string name;

    /** Time that the object was instantiated (used for TS calculation etc)
    */
    time_t age;

    /** User list.
     */
    MemberMap userlist;

    /** Channel topic.
     * If this is an empty string, no channel topic is set.
     */
    std::string topic;

    /** Time topic was set.
     * If no topic was ever set, this will be equal to Channel::created
     */
    time_t topicset;

    /** The last user to set the topic.
     * If this member is an empty string, no topic was ever set.
     */
    std::string setby; /* 128 */

    /** Sets or unsets a custom mode in the channels info
     * @param mode The mode character to set or unset
     * @param value True if you want to set the mode or false if you want to remove it
     */
    void SetMode(ModeHandler* mode, bool value);

    /** Returns true if a mode is set on a channel
     * @param mode The mode character you wish to query
     * @return True if the custom mode is set, false if otherwise
     */
    bool IsModeSet(ModeHandler* mode) {
        return ((mode->GetId() != ModeParser::MODEID_MAX) && (modes[mode->GetId()]));
    }
    bool IsModeSet(ModeHandler& mode) {
        return IsModeSet(&mode);
    }
    bool IsModeSet(ChanModeReference& mode);

    /** Returns the parameter for a custom mode on a channel.
     * @param mode The mode character you wish to query
     *
     * For example if "+L #foo" is set, and you pass this method
     * 'L', it will return '\#foo'. If the mode is not set on the
     * channel, or the mode has no parameters associated with it,
     * it will return an empty string.
     *
     * @return The parameter for this mode is returned, or an empty string
     */
    std::string GetModeParameter(ModeHandler* mode);
    std::string GetModeParameter(ChanModeReference& mode);
    std::string GetModeParameter(ParamModeBase* pm);

    /** Sets the channel topic.
     * @param user The user setting the topic.
     * @param topic The topic to set it to.
     * @param topicts Timestamp of the new topic.
     * @param setter Setter string, may be used when the original setter is no longer online.
     * If omitted or NULL, the setter string is obtained from the user.
     */
    void SetTopic(User* user, const std::string& topic, time_t topicts,
                  const std::string* setter = NULL);

    /** Obtain the channel "user counter"
     * This returns the number of users on this channel
     *
     * @return The number of users on this channel
     */
    size_t GetUserCounter() const {
        return userlist.size();
    }

    /** Add a user pointer to the internal reference list
     * @param user The user to add
     *
     * The data inserted into the reference list is a table as it is
     * an arbitrary pointer compared to other users by its memory address,
     * as this is a very fast 32 or 64 bit integer comparison.
     */
    Membership* AddUser(User* user);

    /** Delete a user pointer to the internal reference list
     * @param user The user to delete
     */
    void DelUser(User* user);

    /** Obtain the internal reference list
     * The internal reference list contains a list of User*.
     * These are used for rapid comparison to determine
     * channel membership for PRIVMSG, NOTICE, QUIT, PART etc.
     * The resulting pointer to the vector should be considered
     * readonly and only modified via AddUser and DelUser.
     *
     * @return This function returns pointer to a map of User pointers (CUList*).
     */
    const MemberMap& GetUsers() const {
        return userlist;
    }

    /** Returns true if the user given is on the given channel.
     * @param user The user to look for
     * @return True if the user is on this channel
     */
    bool HasUser(User* user);

    Membership* GetUser(User* user);

    /** Make src kick user from this channel with the given reason.
     * @param src The source of the kick
     * @param victimiter Iterator to the user being kicked, must be valid
     * @param reason The reason for the kick
     */
    void KickUser(User* src, const MemberMap::iterator& victimiter,
                  const std::string& reason);

    /** Make src kick user from this channel with the given reason.
     * @param src The source of the kick
     * @param user The user being kicked
     * @param reason The reason for the kick
     */
    void KickUser(User* src, User* user, const std::string& reason) {
        MemberMap::iterator it = userlist.find(user);
        if (it != userlist.end()) {
            KickUser(src, it, reason);
        }
    }

    /** Part a user from this channel with the given reason.
     * If the reason field is NULL, no reason will be sent.
     * @param user The user who is parting (must be on this channel)
     * @param reason The part reason
     * @return True if the user was on the channel and left, false if they weren't and nothing happened
     */
    bool PartUser(User* user, std::string& reason);

    /** Join a local user to a channel, with or without permission checks. May be a channel that doesn't exist yet.
     * @param user The user to join to the channel.
     * @param channame The channel name to join to. Does not have to exist.
     * @param key The key of the channel, if given
     * @param override If true, override all join restrictions such as +bkil
     * @return A pointer to the Channel the user was joined to. A new Channel may have
     * been created if the channel did not exist before the user was joined to it.
     * If the user could not be joined to a channel, the return value is NULL.
     */
    static Channel* JoinUser(LocalUser* user, std::string channame,
                             bool override = false, const std::string& key = "");

    /** Join a user to an existing channel, without doing any permission checks
     * @param user The user to join to the channel
     * @param privs Privileges (prefix mode letters) to give to this user, may be NULL
     * @param bursting True if this join is the result of a netburst (passed to modules in the OnUserJoin hook)
     * @param created_by_local True if this channel was just created by a local user (passed to modules in the OnUserJoin hook)
     * @return A newly created Membership object, or NULL if the user was already inside the channel or if the user is a server user
     */
    Membership* ForceJoin(User* user, const std::string* privs = NULL,
                          bool bursting = false, bool created_by_local = false);

    /** Write to all users on a channel except some users
     * @param protoev Event to send, may contain any number of messages.
     * @param status The status of the users to write to, e.g. '@' or '%'. Use a value of 0 to write to everyone
     * @param except_list List of users not to send to
     */
    void Write(ClientProtocol::Event& protoev, char status = 0,
               const CUList& except_list = CUList());

    /** Write to all users on a channel except some users.
     * @param protoevprov Protocol event provider for the message.
     * @param msg Message to send.
     * @param status The status of the users to write to, e.g. '@' or '%'. Use a value of 0 to write to everyone
     * @param except_list List of users not to send to
     */
    void Write(ClientProtocol::EventProvider& protoevprov,
               ClientProtocol::Message& msg, char status = 0,
               const CUList& except_list = CUList());

    /** Return the channel's modes with parameters.
     * @param showsecret If this is set to true, the value of secret parameters
     * are shown, otherwise they are replaced with '&lt;name&gt;'.
     * @return The channel mode string
     */
    const char* ChanModes(bool showsecret);

    /** Get the value of a users prefix on this channel.
     * @param user The user to look up
     * @return The module or core-defined value of the users prefix.
     * The values for op, halfop and voice status are constants in
     * mode.h, and are OP_VALUE, HALFOP_VALUE, and VOICE_VALUE respectively.
     * If the value you are given does not match one of these three, it is
     * a module-defined mode, and it should be compared in proportion to
     * these three constants. For example a value greater than OP_VALUE
     * is a prefix of greater 'worth' than ops, and a value less than
     * VOICE_VALUE is of lesser 'worth' than a voice.
     */
    unsigned int GetPrefixValue(User* user);

    /** Check if a user is banned on this channel
     * @param user A user to check against the banlist
     * @returns True if the user given is banned
     */
    bool IsBanned(User* user);

    /** Check a single ban for match
     */
    bool CheckBan(User* user, const std::string& banmask);

    /** Get the status of an "action" type extban
     */
    ModResult GetExtBanStatus(User *u, char type);

    /** Write a NOTICE to all local users on the channel
     * @param text Text to send
     * @param status The minimum status rank to send this message to.
     */
    void WriteNotice(const std::string& text, char status = 0);
    void WriteRemoteNotice(const std::string& text, char status = 0);
};

inline bool Channel::HasUser(User* user) {
    return (userlist.find(user) != userlist.end());
}

inline std::string Channel::GetModeParameter(ChanModeReference& mode) {
    if (!mode) {
        return "";
    }
    return GetModeParameter(*mode);
}

inline std::string Channel::GetModeParameter(ModeHandler* mh) {
    std::string out;
    ParamModeBase* pm = mh->IsParameterMode();
    if (pm && this->IsModeSet(pm)) {
        pm->GetParameter(this, out);
    }
    return out;
}

inline std::string Channel::GetModeParameter(ParamModeBase* pm) {
    std::string out;
    if (this->IsModeSet(pm)) {
        pm->GetParameter(this, out);
    }
    return out;
}

inline bool Channel::IsModeSet(ChanModeReference& mode) {
    if (!mode) {
        return false;
    }
    return IsModeSet(*mode);
}
