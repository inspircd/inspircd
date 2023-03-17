/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2019 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2013-2015 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2013 Adam <Adam@anope.org>
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

#include "utils.h"

class TreeServer;

class CmdBuilder {
  protected:
    /** The raw message contents. */
    std::string content;

    /** Tags which have been added to this message. */
    ClientProtocol::TagMap tags;

    /** The size of tags within the contents. */
    size_t tagsize;

    /** Fires the ServerProtocol::MessageEventListener::OnBuildMessage event for a server target. */
    void FireEvent(Server* target, const char* cmd,
                   ClientProtocol::TagMap& taglist);

    /** Fires the ServerProtocol::MessageEventListener::OnBuildMessage event for a user target. */
    void FireEvent(User* target, const char* cmd, ClientProtocol::TagMap& taglist);

    /** Updates the tag string within the buffer. */
    void UpdateTags();

  public:
    CmdBuilder(const char* cmd)
        : content(1, ':')
        , tagsize(0) {
        content.append(ServerInstance->Config->GetSID());
        push(cmd);
        FireEvent(ServerInstance->FakeClient->server, cmd, tags);
    }

    CmdBuilder(TreeServer* src, const char* cmd)
        : content(1, ':')
        , tagsize(0) {
        content.append(src->GetId());
        push(cmd);
        FireEvent(src, cmd, tags);
    }

    CmdBuilder(User* src, const char* cmd)
        : content(1, ':')
        , tagsize(0) {
        content.append(src->uuid);
        push(cmd);
        if (InspIRCd::IsSID(src->uuid)) {
            FireEvent(src->server, cmd, tags);
        } else {
            FireEvent(src, cmd, tags);
        }
    }

    CmdBuilder& push_raw(const std::string& s) {
        content.append(s);
        return *this;
    }

    CmdBuilder& push_raw(const char* s) {
        content.append(s);
        return *this;
    }

    CmdBuilder& push_raw(char c) {
        content.push_back(c);
        return *this;
    }

    template <typename T>
    CmdBuilder& push_raw_int(T i) {
        content.append(ConvToStr(i));
        return *this;
    }

    template <typename InputIterator>
    CmdBuilder& push_raw(InputIterator first, InputIterator last) {
        content.append(first, last);
        return *this;
    }

    CmdBuilder& push(const std::string& s) {
        content.push_back(' ');
        content.append(s);
        return *this;
    }

    CmdBuilder& push(const char* s) {
        content.push_back(' ');
        content.append(s);
        return *this;
    }

    CmdBuilder& push(char c) {
        content.push_back(' ');
        content.push_back(c);
        return *this;
    }

    template <typename T>
    CmdBuilder& push_int(T i) {
        content.push_back(' ');
        content.append(ConvToStr(i));
        return *this;
    }

    CmdBuilder& push_last(const std::string& s) {
        content.push_back(' ');
        content.push_back(':');
        content.append(s);
        return *this;
    }

    CmdBuilder& push_tags(ClientProtocol::TagMap newtags) {
        // It has to be this way around so new tags get priority.
        newtags.insert(tags.begin(), tags.end());
        std::swap(tags, newtags);
        UpdateTags();
        return *this;
    }

    template<typename T>
    CmdBuilder& insert(const T& cont) {
        for (typename T::const_iterator i = cont.begin(); i != cont.end(); ++i) {
            push(*i);
        }
        return *this;
    }

    const std::string& str() const {
        return content;
    }
    operator const std::string&() const {
        return str();
    }

    void Broadcast() const {
        Utils->DoOneToMany(*this);
    }

    void Forward(TreeServer* omit) const {
        Utils->DoOneToAllButSender(*this, omit);
    }

    void Unicast(User* target) const {
        Utils->DoOneToOne(*this, target->server);
    }
};
