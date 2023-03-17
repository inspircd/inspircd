/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2018, 2020 Sadie Powell <sadie@witchery.services>
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
#include "modules/cap.h"

namespace CTCTags {
class CapReference;
class EventListener;
class TagMessage;
class TagMessageDetails;
}

class CTCTags::CapReference : public Cap::Reference {
  public:
    CapReference(Module* mod)
        : Cap::Reference(mod, "message-tags") {
    }
};

class CTCTags::TagMessage : public ClientProtocol::Message {
  private:
    void PushTarget(const char* target, char status) {
        if (status) {
            std::string rawtarget(1, status);
            rawtarget.append(target);
            PushParam(rawtarget);
        } else {
            PushParam(target);
        }
    }

  public:
    TagMessage(User* source, const Channel* targetchan,
               const ClientProtocol::TagMap& Tags, char status = 0)
        : ClientProtocol::Message("TAGMSG", source) {
        PushTarget(targetchan->name.c_str(), status);
        AddTags(Tags);
    }

    TagMessage(User* source, const User* targetuser,
               const ClientProtocol::TagMap& Tags)
        : ClientProtocol::Message("TAGMSG", source) {
        if (targetuser->registered & REG_NICK) {
            PushParamRef(targetuser->nick);
        } else {
            PushParam("*");
        }
        AddTags(Tags);
    }

    TagMessage(User* source, const char* targetstr,
               const ClientProtocol::TagMap& Tags, char status = 0)
        : ClientProtocol::Message("TAGMSG", source) {
        PushTarget(targetstr, status);
        AddTags(Tags);
    }

    TagMessage(const char* source, const char* targetstr,
               const ClientProtocol::TagMap& Tags, char status = 0)
        : ClientProtocol::Message("TAGMSG", source) {
        PushTarget(targetstr, status);
        AddTags(Tags);
    }
};

class CTCTags::TagMessageDetails {
  public:
    /** Whether to echo the tags at all. */
    bool echo;

    /* Whether to send the original tags back to clients with echo-message support. */
    bool echo_original;

    /** Whether to update the source user's idle time. */
    bool update_idle;

    /** The users who are exempted from receiving this message. */
    CUList exemptions;

    /** IRCv3 message tags sent to the server by the user. */
    const ClientProtocol::TagMap tags_in;

    /** IRCv3 message tags sent out to users who get this message. */
    ClientProtocol::TagMap tags_out;

    TagMessageDetails(const ClientProtocol::TagMap& tags)
        : echo(true)
        , echo_original(false)
        , update_idle(true)
        , tags_in(tags) {
    }
};

class CTCTags::EventListener
    : public Events::ModuleEventListener {
  protected:
    EventListener(Module* mod, unsigned int eventprio = DefaultPriority)
        : ModuleEventListener(mod, "event/tagmsg", eventprio) {
    }

  public:
    /** Called before a user sends a tag message to a channel, a user, or a server glob mask.
     * @param user The user sending the message.
     * @param target The target of the message. This can either be a channel, a user, or a server
     *               glob mask.
     * @param details Details about the message such as the message tags or whether to echo. See the
     *                TagMessageDetails class for more information.
     * @return MOD_RES_ALLOW to explicitly allow the message, MOD_RES_DENY to explicitly deny the
     *         message, or MOD_RES_PASSTHRU to let another module handle the event.
     */
    virtual ModResult OnUserPreTagMessage(User* user, const MessageTarget& target,
                                          TagMessageDetails& details) {
        return MOD_RES_PASSTHRU;
    }

    /** Called immediately after a user sends a tag message to a channel, a user, or a server glob mask.
     * @param user The user sending the message.
     * @param target The target of the message. This can either be a channel, a user, or a server
     *               glob mask.
     * @param details Details about the message such as the message tags or whether to echo. See the
     *                TagMessageDetails class for more information.
     */
    virtual void OnUserPostTagMessage(User* user, const MessageTarget& target,
                                      const TagMessageDetails& details) { }

    /** Called immediately before a user sends a tag message to a channel, a user, or a server glob mask.
     * @param user The user sending the message.
     * @param target The target of the message. This can either be a channel, a user, or a server
     *               glob mask.
     * @param details Details about the message such as the message tags or whether to echo. See the
     *                TagMessageDetails class for more information.
     */
    virtual void OnUserTagMessage(User* user, const MessageTarget& target,
                                  const TagMessageDetails& details) { }

    /** Called when a tag message sent by a user to a channel, a user, or a server glob mask is blocked.
     * @param user The user sending the message.
     * @param target The target of the message. This can either be a channel, a user, or a server
     *               glob mask.
     * @param details Details about the message such as the message tags or whether to echo. See the
     *                TagMessageDetails class for more information.
     */
    virtual void OnUserTagMessageBlocked(User* user, const MessageTarget& target,
                                         const TagMessageDetails& details) { }
};
