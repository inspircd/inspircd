/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2018-2019 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2018 Attila Molnar <attilamolnar@hush.com>
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


#include "inspircd.h"
#include "modules/ircv3.h"
#include "modules/ircv3_servertime.h"
#include "modules/server.h"

class ServerTimeTag
    : public IRCv3::ServerTime::Manager
    , public IRCv3::CapTag<ServerTimeTag>
    , public ServerProtocol::MessageEventListener {
    time_t lasttime;
    long lasttimens;
    std::string lasttimestring;

    void RefreshTimeString() {
        const time_t currtime = ServerInstance->Time();
        const long currtimens = ServerInstance->Time_ns();
        if (currtime != lasttime || currtimens != lasttimens) {
            lasttime = currtime;
            lasttimens = currtimens;

            // Cache the string so it's not recreated every time a message is sent.
            lasttimestring = IRCv3::ServerTime::FormatTime(currtime,
                             (currtimens ? currtimens / 1000000 : 0));
        }
    }

  public:
    using ServerProtocol::MessageEventListener::OnBuildMessage;

    ServerTimeTag(Module* mod)
        : IRCv3::ServerTime::Manager(mod)
        , IRCv3::CapTag<ServerTimeTag>(mod, "server-time", "time")
        , ServerProtocol::MessageEventListener(mod)
        , lasttime(0)
        , lasttimens(0) {
        tagprov = this;
    }

    const std::string* GetValue(const ClientProtocol::Message& msg) {
        // Client protocol.
        RefreshTimeString();
        return &lasttimestring;
    }

    void OnBuildMessage(User* source, const char* command,
                        ClientProtocol::TagMap& tags) CXX11_OVERRIDE {
        // Server protocol.
        RefreshTimeString();
        tags.insert(std::make_pair(tagname, ClientProtocol::MessageTagData(this, lasttimestring)));
    }

};

class ModuleIRCv3ServerTime : public Module {
    ServerTimeTag tag;

  public:
    ModuleIRCv3ServerTime()
        : tag(this) {
    }

    Version GetVersion() CXX11_OVERRIDE {
        return Version("Provides the IRCv3 server-time client capability.", VF_VENDOR);
    }
};

MODULE_INIT(ModuleIRCv3ServerTime)
