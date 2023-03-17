/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2022 delthas
 *   Copyright (C) 2017-2019, 2022 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2015, 2018 Attila Molnar <attilamolnar@hush.com>
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
#include "modules/cap.h"
#include "modules/ircv3.h"
#include "modules/monitor.h"

class ModuleIRCv3ChgHost : public Module {
    Cap::Capability cap;
    ClientProtocol::EventProvider protoevprov;
    Monitor::API monitorapi;

    void DoChgHost(User* user, const std::string& ident, const std::string& host) {
        if (!(user->registered & REG_NICKUSER)) {
            return;
        }

        ClientProtocol::Message msg("CHGHOST", user);
        msg.PushParamRef(ident);
        msg.PushParamRef(host);
        ClientProtocol::Event protoev(protoevprov, msg);
        IRCv3::WriteNeighborsWithCap res(user, protoev, cap, true);
        Monitor::WriteWatchersWithCap(monitorapi, user, protoev, cap,
                                      res.GetAlreadySentId());
    }

  public:
    ModuleIRCv3ChgHost()
        : cap(this, "chghost")
        , protoevprov(this, "CHGHOST")
        , monitorapi(this) {
    }

    void OnChangeIdent(User* user, const std::string& newident) CXX11_OVERRIDE {
        DoChgHost(user, newident, user->GetDisplayedHost());
    }

    void OnChangeHost(User* user, const std::string& newhost) CXX11_OVERRIDE {
        DoChgHost(user, user->ident, newhost);
    }

    Version GetVersion() CXX11_OVERRIDE {
        return Version("Provides the IRCv3 chghost client capability.", VF_VENDOR);
    }
};

MODULE_INIT(ModuleIRCv3ChgHost)
