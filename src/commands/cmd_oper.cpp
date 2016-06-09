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

bool OneOfMatches(const char* host, const char* ip, const char* hostlist);

/** Handle /OPER. These command handlers can be reloaded by the core,
 * and handle basic RFC1459 commands. Commands within modules work
 * the same way, however, they can be fully unloaded, where these
 * may not.
 */
class CommandOper : public SplitCommand
{
 public:
	/** Constructor for oper.
	 */
	CommandOper ( Module* parent) : SplitCommand(parent,"OPER",2,2) { syntax = "<username> <password>"; }
	/** Handle command.
	 * @param parameters The parameters to the comamnd
	 * @param pcnt The number of parameters passed to teh command
	 * @param user The user issuing the command
	 * @return A value from CmdResult to indicate command success or failure.
	 */
	CmdResult HandleLocal(const std::vector<std::string>& parameters, LocalUser *user);
};

bool OneOfMatches(const char* host, const char* ip, const std::string& hostlist)
{
	std::stringstream hl(hostlist);
	std::string xhost;
	while (hl >> xhost)
	{
		if (InspIRCd::Match(host, xhost, ascii_case_insensitive_map) || InspIRCd::MatchCIDR(ip, xhost, ascii_case_insensitive_map))
		{
			return true;
		}
	}
	return false;
}

CmdResult CommandOper::HandleLocal(const std::vector<std::string>& parameters, LocalUser *user)
{
	char TheHost[MAXBUF];
	char TheIP[MAXBUF];
	bool match_login = false;
	bool match_pass = false;
	bool match_hosts = false;

	snprintf(TheHost,MAXBUF,"%s@%s",user->ident.c_str(),user->host.c_str());
	snprintf(TheIP, MAXBUF,"%s@%s",user->ident.c_str(),user->GetIPString());

	OperIndex::iterator i = ServerInstance->Config->oper_blocks.find(parameters[0]);
	if ((i != ServerInstance->Config->oper_blocks.end()) && (i->second->oper_block))
	{
		OperInfo* ifo = i->second;
		ConfigTag* tag = ifo->oper_block;
		match_login = true;
		match_pass = !ServerInstance->PassCompare(user, tag->getString("password"), parameters[1], tag->getString("hash"));
		match_hosts = OneOfMatches(TheHost,TheIP,tag->getString("host"));

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
	user->WriteNumeric(491, "%s :Invalid oper credentials",user->nick.c_str());
	user->CommandFloodPenalty += 10000;

	ServerInstance->SNO->WriteGlobalSno('o', "WARNING! Failed oper attempt by %s using login '%s': The following fields do not match: %s", user->GetFullRealHost().c_str(), parameters[0].c_str(), fields.c_str());
	ServerInstance->Logs->Log("OPER",DEFAULT,"OPER: Failed oper attempt by %s using login '%s': The following fields did not match: %s", user->GetFullRealHost().c_str(), parameters[0].c_str(), fields.c_str());
	return CMD_FAILURE;
}

COMMAND_INIT(CommandOper)
