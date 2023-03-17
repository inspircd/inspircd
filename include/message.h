/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2018 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2017-2018, 2020 Sadie Powell <sadie@witchery.services>
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

/** Whether message was a PRIVMSG or a NOTICE. */
enum MessageType {
    /** The message is a PRIVMSG. */
    MSG_PRIVMSG,

    /** The message is a NOTICE. */
    MSG_NOTICE
};

class CoreExport MessageDetails {
  public:
    /** Whether to echo the message at all. */
    bool echo;

    /* Whether to send the original message back to clients with echo-message support. */
    bool echo_original;

    /** Whether to update the source user's idle time. */
    bool update_idle;

    /** The users who are exempted from receiving this message. */
    CUList exemptions;

    /* The original message as sent by the user. */
    const std::string original_text;

    /** IRCv3 message tags sent to the server by the user. */
    const ClientProtocol::TagMap tags_in;

    /** IRCv3 message tags sent out to users who get this message. */
    ClientProtocol::TagMap tags_out;

    /** The message which will be sent to clients. */
    std::string text;

    /** The type of message. */
    MessageType type;

    /** Determines whether the specified message is a CTCP. If the specified message
     * is a CTCP then the CTCP name and CTCP body are extracted and stored in the
     * name and body references.
     * @param name The location to store the parsed CTCP name.
     * @param body The location to store the parsed CTCP body.
     */
    virtual bool IsCTCP(std::string& name, std::string& body) const = 0;

    /** Determines whether the specified message is a CTCP. If the specified message
     * is a CTCP then the CTCP name is extracted and stored in the name reference.
     * @param name The location to store the parsed CTCP name.
     */
    virtual bool IsCTCP(std::string& name) const = 0;

    /** Determines whether the specified message is a CTCP. */
    virtual bool IsCTCP() const = 0;

  protected:
    MessageDetails(MessageType mt, const std::string& msg,
                   const ClientProtocol::TagMap& tags)
        : echo(true)
        , echo_original(false)
        , update_idle(true)
        , original_text(msg)
        , tags_in(tags)
        , text(msg)
        , type(mt) {
    }
};

/** Represents the target of a message (NOTICE, PRIVMSG, etc). */
class CoreExport MessageTarget {
  public:
    /** An enumeration of possible message target types. */
    enum TargetType {
        /** The target of the message is a user. */
        TYPE_USER,

        /** The target of the message is a channel. */
        TYPE_CHANNEL,

        /** The target of the message is a server. */
        TYPE_SERVER
    };

  private:
    /** The target of the message. */
    void* dest;

  public:
    /** If type is TYPE_CHANNEL and the user specified a status rank. */
    char status;

    /** The type of the target of the message. If this is TYPE_CHANNEL then dest
     * is a Channel*, TYPE_USER then dest is a User*, and TYPE_SERVER then dest is
     * a std::string* containing a server glob.
     */
    MessageTarget::TargetType type;

    /** Initialises a new channel message target.
     * @param channel The channel which is the target of the message.
     * @param statuschar The lowest status rank that the message is being sent to.
     */
    MessageTarget(Channel* channel, char statuschar)
        : dest(channel)
        , status(statuschar)
        , type(TYPE_CHANNEL) {
    }

    /** Initialises a new user message target.
     * @param user The user which is the target of the message.
     */
    MessageTarget(User* user)
        : dest(user)
        , status(0)
        , type(TYPE_USER) {
    }

    /** Initialises a new server message target.
     * @param server The server glob which is the target of the message.
     */
    MessageTarget(std::string* server)
        : dest(server)
        , status(0)
        , type(TYPE_SERVER) {
    }

    /** Retrieves the target of this message. */
    template<typename T>
    T* Get() const {
        return static_cast<T*>(dest);
    }

    /** Retrieves the name of the target of this message. */
    const std::string& GetName() const {
        switch (type) {
        case TYPE_CHANNEL:
            return Get<Channel>()->name;
        case TYPE_USER:
            return Get<User>()->nick;
        case TYPE_SERVER:
            return *Get<std::string>();
        }

        // We should never reach this point during a normal execution but
        // handle it just in case.
        static const std::string target = "*";
        return target;
    }
};
