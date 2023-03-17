/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2013 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2012 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2008 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2007 John Brooks <special@inspircd.org>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
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

class ModuleChanCreate : public Module {
  public:
    void init() CXX11_OVERRIDE {
        ServerInstance->SNO->EnableSnomask('j', "CHANCREATE");
    }

    Version GetVersion() CXX11_OVERRIDE {
        return Version("Sends a notice to snomasks j (local) and J (remote) when a channel is created.", VF_VENDOR);
    }

    void OnUserJoin(Membership* memb, bool sync, bool created,
                    CUList& except) CXX11_OVERRIDE {
        if ((created) && (IS_LOCAL(memb->user))) {
            ServerInstance->SNO->WriteGlobalSno('j', "Channel %s created by %s",
                                                memb->chan->name.c_str(), memb->user->GetFullRealHost().c_str());
        }
    }
};

MODULE_INIT(ModuleChanCreate)
