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

class ModuleIRCv3ChgHost final
	: public Module
{
	Cap::Capability cap;
	ClientProtocol::EventProvider protoevprov;
	Monitor::API monitorapi;

	void DoChgHost(User* user, const std::string& username, const std::string& hostname)
	{
		if (!(user->connected & User::CONN_NICKUSER))
			return;

		ClientProtocol::Message msg("CHGHOST", user);
		msg.PushParamRef(username);
		msg.PushParamRef(hostname);
		ClientProtocol::Event protoev(protoevprov, msg);
		IRCv3::WriteNeighborsWithCap res(user, protoev, cap, true);
		Monitor::WriteWatchersWithCap(monitorapi, user, protoev, cap, res.GetAlreadySentId());
	}

public:
	ModuleIRCv3ChgHost()
		: Module(VF_VENDOR, "Provides the IRCv3 chghost client capability.")
		, cap(this, "chghost")
		, protoevprov(this, "CHGHOST")
		, monitorapi(this)
	{
	}

	void OnChangeUser(User* user, const std::string& newuser) override
	{
		DoChgHost(user, newuser, user->GetDisplayedHost());
	}

	void OnChangeHost(User* user, const std::string& newhost) override
	{
		DoChgHost(user, user->GetDisplayedUser(), newhost);
	}
};

MODULE_INIT(ModuleIRCv3ChgHost)
