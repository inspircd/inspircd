/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2011 Jackmcbarn <jackmcbarn@jackmcbarn.no-ip.org>
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
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
#include "hashcomp.h"

/** Handle /OPER. These command handlers can be reloaded by the core,
 * and handle basic RFC1459 commands. Commands within modules work
 * the same way, however, they can be fully unloaded, where these
 * may not.
 */
class CommandOper : public SplitCommand
{
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

CmdResult CommandOper::HandleLocal(const std::vector<std::string>& parameters, LocalUser *user)
{
	char TheHost[MAXBUF];
	char TheIP[MAXBUF];

	snprintf(TheHost,MAXBUF,"%s@%s",user->ident.c_str(),user->host.c_str());
	snprintf(TheIP, MAXBUF,"%s@%s",user->ident.c_str(),user->GetIPString());

	OperPermissionData perm(user, parameters[0]);
	FOR_EACH_MOD(OnPermissionCheck, (perm));

	if (perm.result == MOD_RES_DENY)
		goto fail;

	if (!perm.oper)
	{
		perm.reason = "oper block not found";
		goto fail;
	}

	if (perm.result != MOD_RES_ALLOW)
	{
		ConfigTag* tag = perm.oper->config_blocks[0];
		if (!OneOfMatches(TheHost,TheIP,tag->getString("host")))
		{
			perm.reason = "host does not match";
			goto fail;
		}

		if (ServerInstance->PassCompare(user, tag->getString("password"), parameters[1], tag->getString("hash")))
		{
			perm.reason = "password does not match";
			goto fail;
		}
	}

	user->Oper(perm.oper);
	return CMD_SUCCESS;

fail:
	char broadcast[MAXBUF];

	// tell them they suck, and lag them up to help prevent brute-force attacks
	user->WriteNumeric(491, "%s :Invalid oper credentials",user->nick.c_str());
	user->CommandFloodPenalty += 10000;

	snprintf(broadcast, MAXBUF, "WARNING! Failed oper attempt by %s!%s@%s using login '%s': %s",
		user->nick.c_str(), user->ident.c_str(), user->host.c_str(), parameters[0].c_str(), perm.reason.c_str());
	ServerInstance->SNO->WriteGlobalSno('o',std::string(broadcast));

	ServerInstance->Logs->Log("OPER",DEFAULT,"OPER: Failed oper attempt by %s!%s@%s using login '%s': %s",
		user->nick.c_str(), user->ident.c_str(), user->host.c_str(), parameters[0].c_str(), perm.reason.c_str());
	return CMD_FAILURE;
}

COMMAND_INIT(CommandOper)
