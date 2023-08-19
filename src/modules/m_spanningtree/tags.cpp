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

ServerTags::ServerTags(Module* Creator)
	: ClientProtocol::MessageTagProvider(Creator)
{
}

ModResult ServerTags::OnProcessTag(User* user, const std::string& tagname, std::string& tagvalue)
{
	if (tagname[0] != '~' || tagname.length() < 2)
		return MOD_RES_PASSTHRU;

	// Only allow tags from remote users.
	return IS_LOCAL(user) ? MOD_RES_DENY : MOD_RES_ALLOW;
}

bool ServerTags::ShouldSendTag(LocalUser* user, const ClientProtocol::MessageTagData& tagdata)
{
	// Server tags should never be sent to users.
	return false;
}
