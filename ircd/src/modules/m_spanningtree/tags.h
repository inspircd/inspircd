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


#pragma once

#include "modules/ctctags.h"

class ServiceTag : public ClientProtocol::MessageTagProvider {
  private:
    CTCTags::CapReference ctctagcap;

  public:
    ServiceTag(Module* mod);
    void OnPopulateTags(ClientProtocol::Message& msg) CXX11_OVERRIDE;
    bool ShouldSendTag(LocalUser* user,
                       const ClientProtocol::MessageTagData& tagdata) CXX11_OVERRIDE;
};
