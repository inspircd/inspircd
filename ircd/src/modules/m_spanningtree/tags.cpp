/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2019-2020 Sadie Powell <sadie@witchery.services>
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


#include "main.h"

ServiceTag::ServiceTag(Module* mod)
    : ClientProtocol::MessageTagProvider(mod)
    , ctctagcap(mod) {
}

void ServiceTag::OnPopulateTags(ClientProtocol::Message& msg) {
    User* const user = msg.GetSourceUser();
    if (user && user->server->IsULine()) {
        msg.AddTag("inspircd.org/service", this, "");
    }
}

bool ServiceTag::ShouldSendTag(LocalUser* user,
                               const ClientProtocol::MessageTagData& tagdata) {
    return ctctagcap.get(user);
}
