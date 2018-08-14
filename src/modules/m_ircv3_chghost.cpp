/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2015 Attila Molnar <attilamolnar@hush.com>
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

class ModuleIRCv3ChgHost : public Module
{
	Cap::Capability cap;
	ClientProtocol::EventProvider protoevprov;

	void DoChgHost(User* user, const std::string& ident, const std::string& host)
	{
		ClientProtocol::Message msg("CHGHOST", user);
		msg.PushParamRef(ident);
		msg.PushParamRef(host);
		ClientProtocol::Event protoev(protoevprov, msg);
		IRCv3::WriteNeighborsWithCap(user, protoev, cap);
	}

 public:
	ModuleIRCv3ChgHost()
		: cap(this, "chghost")
		, protoevprov(this, "CHGHOST")
	{
	}

	void OnChangeIdent(User* user, const std::string& newident) CXX11_OVERRIDE
	{
		DoChgHost(user, newident, user->GetDisplayedHost());
	}

	void OnChangeHost(User* user, const std::string& newhost) CXX11_OVERRIDE
	{
		DoChgHost(user, user->ident, newhost);
	}

	Version GetVersion() CXX11_OVERRIDE
	{
		return Version("Provides the chghost IRCv3 extension", VF_VENDOR);
	}
};

MODULE_INIT(ModuleIRCv3ChgHost)
