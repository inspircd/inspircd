/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2019, 2021 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2016 Attila Molnar <attilamolnar@hush.com>
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

namespace SpanningTree
{
	class RemoteUser;
}

class SpanningTree::RemoteUser final
	: public ::RemoteUser
{
public:
	RemoteUser(const std::string& uid, Server* srv);
	void WriteRemoteNotice(const std::string& text) override;
	void WriteRemoteNumeric(const Numeric::Numeric& numeric) override;
};
