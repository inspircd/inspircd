/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2018-2020, 2022-2023 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2017 B00mX0r <b00mx0r@aureus.pw>
 *   Copyright (C) 2014 Attila Molnar <attilamolnar@hush.com>
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

#include "treeserver.h"
#include "commands.h"

CmdResult CommandSInfo::HandleServer(TreeServer* server, CommandBase::Params& params)
{
	const std::string& key = params.front();
	const std::string& value = params.back();

	if (irc::equals(key, "customversion"))
	{
		server->customversion = value;
	}
	else if (irc::equals(key,  "desc"))
	{
		// Only sent when the description of a server changes because of a rehash; not sent on burst
		ServerInstance->Logs.Debug(MODNAME, "Server description of {} changed: {}",
			server->GetName(), value);
		server->SetDesc(value);
	}
	else if (irc::equals(key,  "rawbranch"))
	{
		server->rawbranch = value;
	}
	else if (irc::equals(key,  "rawversion"))
	{
		server->rawversion = value;
	}

	// BEGIN DEPRECATED KEYS
	else if (irc::equals(key,  "fullversion"))
	{
		// InspIRCd-4.0.0-a10. sadie.testnet.inspircd.org :[597] Test
		// version             server                       uid  custom-version
		irc::tokenstream versionstream(value);

		versionstream.GetMiddle(server->rawversion);
		if (server->rawversion.back() == '.')
			server->rawversion.pop_back();

		for (std::string token; versionstream.GetTrailing(token); )
			server->customversion = token;

		const std::string sidprefix = INSP_FORMAT("[{}] ", server->GetId());
		if (!server->customversion.compare(0, sidprefix.size(), sidprefix))
			server->customversion.erase(0, sidprefix.size());

		ServerInstance->Logs.Debug(MODNAME, "Extracted entries from fullversion key: rawversion={} customversion={}",
			server->rawversion, server->customversion);
	}
	else if (irc::equals(key,  "version"))
	{
		// InspIRCd-4. testnet.inspircd.org :Test
		irc::tokenstream versionstream(value);

		versionstream.GetMiddle(server->rawbranch);
		if (server->rawbranch.back() == '.')
			server->rawbranch.pop_back();

		for (std::string token; versionstream.GetTrailing(token); )
			server->customversion = token;

		ServerInstance->Logs.Debug(MODNAME, "Extracted entries from version key: rawbranch={} customversion={}",
			server->rawbranch, server->customversion);
	}
	// END DEPRECATED KEYS

	return CmdResult::SUCCESS;
}

CommandSInfo::Builder::Builder(TreeServer* server, const char* key, const std::string& val)
	: CmdBuilder(server, "SINFO")
{
	push(key).push_last(val);
}
