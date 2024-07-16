/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2021-2023 Sadie Powell <sadie@witchery.services>
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
#include "utility/string.h"

class CoreModClients final
	: public Module
{
private:

public:
	CoreModClients()
		: Module(VF_CORE | VF_VENDOR, "Accepts connections to the server.")
	{
	}

	ModResult OnAcceptConnection(int nfd, ListenSocket* from, const irc::sockets::sockaddrs& client, const irc::sockets::sockaddrs& server) override
	{
		if (!insp::equalsci(from->bind_tag->getString("type", "clients", 1), "clients"))
			return MOD_RES_PASSTHRU;

		ServerInstance->Users.AddUser(nfd, from, client, server);
		return MOD_RES_ALLOW;
	}
};

MODULE_INIT(CoreModClients)
