/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2008 Thomas Stagner <aquanight@inspircd.org>
 *   Copyright (C) 2007 Robin Burchell <robin+git@viroteck.net>
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
#include "core_oper.h"

CommandOper::CommandOper(Module* parent)
	: SplitCommand(parent, "OPER", 2, 2)
{
	syntax = "<username> <password>";
}

CmdResult CommandOper::HandleLocal(const std::vector<std::string>& parameters, LocalUser *user)
{
	bool match_login = false;
	bool match_pass = false;
	bool match_hosts = false;

	const std::string userHost = user->ident + "@" + user->host;
	const std::string userIP = user->ident + "@" + user->GetIPString();

	ServerConfig::OperIndex::const_iterator i = ServerInstance->Config->oper_blocks.find(parameters[0]);
	if (i != ServerInstance->Config->oper_blocks.end())
	{
		OperInfo* ifo = i->second;
		ConfigTag* tag = ifo->oper_block;
		match_login = true;
		match_pass = ServerInstance->PassCompare(user, tag->getString("password"), parameters[1], tag->getString("hash"));
		match_hosts = InspIRCd::MatchMask(tag->getString("host"), userHost, userIP);

		if (match_pass && match_hosts)
		{
			/* found this oper's opertype */
			user->Oper(ifo);
			return CMD_SUCCESS;
		}
	}

	std::string fields;
	if (!match_login)
		fields.append("login ");
	if (!match_pass)
		fields.append("password ");
	if (!match_hosts)
		fields.append("hosts");

	// tell them they suck, and lag them up to help prevent brute-force attacks
	user->WriteNumeric(ERR_NOOPERHOST, ":Invalid oper credentials");
	user->CommandFloodPenalty += 10000;

	ServerInstance->SNO->WriteGlobalSno('o', "WARNING! Failed oper attempt by %s using login '%s': The following fields do not match: %s", user->GetFullRealHost().c_str(), parameters[0].c_str(), fields.c_str());
	ServerInstance->Logs->Log("OPER", LOG_DEFAULT, "OPER: Failed oper attempt by %s using login '%s': The following fields did not match: %s", user->GetFullRealHost().c_str(), parameters[0].c_str(), fields.c_str());
	return CMD_FAILURE;
}
