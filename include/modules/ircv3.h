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


#pragma once

namespace IRCv3
{
	class WriteNeighborsWithCap;
}

class IRCv3::WriteNeighborsWithCap : public User::ForEachNeighborHandler
{
	const Cap::Capability& cap;
	const std::string& msg;

	void Execute(LocalUser* user) CXX11_OVERRIDE
	{
		if (cap.get(user))
			user->Write(msg);
	}

 public:
	WriteNeighborsWithCap(User* user, const std::string& message, const Cap::Capability& capability)
		: cap(capability)
		, msg(message)
	{
		user->ForEachNeighbor(*this, false);
	}
};
