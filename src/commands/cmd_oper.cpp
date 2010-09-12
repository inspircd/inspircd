/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2010 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"
#include "hashcomp.h"

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

	snprintf(TheHost,MAXBUF,"%s@%s",user->ident.c_str(),user->host.c_str());
	snprintf(TheIP, MAXBUF,"%s@%s",user->ident.c_str(),user->GetIPString());

	std::string why;
	OperInfo* ifo;
	ConfigTag* tag;

	OperIndex::iterator i = ServerInstance->Config->oper_blocks.find(parameters[0]);
	if (i == ServerInstance->Config->oper_blocks.end())
	{
		why = "oper block not found";
		goto fail;
	}

	ifo = i->second;
	tag = ifo->config_blocks[0];
	if (!OneOfMatches(TheHost,TheIP,tag->getString("host")))
	{
		why = "host does not match";
		goto fail;
	}

	if (ServerInstance->PassCompare(user, tag->getString("password"), parameters[1], tag->getString("hash")))
	{
		why = "password does not match";
		goto fail;
	}

	user->Oper(ifo);
	return CMD_SUCCESS;

fail:
	char broadcast[MAXBUF];

	// tell them they suck, and lag them up to help prevent brute-force attacks
	user->WriteNumeric(491, "%s :Invalid oper credentials",user->nick.c_str());
	user->CommandFloodPenalty += 10000;

	snprintf(broadcast, MAXBUF, "WARNING! Failed oper attempt by %s!%s@%s using login '%s': %s",
		user->nick.c_str(), user->ident.c_str(), user->host.c_str(), parameters[0].c_str(), why.c_str());
	ServerInstance->SNO->WriteGlobalSno('o',std::string(broadcast));

	ServerInstance->Logs->Log("OPER",DEFAULT,"OPER: Failed oper attempt by %s!%s@%s using login '%s': %s",
		user->nick.c_str(), user->ident.c_str(), user->host.c_str(), parameters[0].c_str(), why.c_str());
	return CMD_FAILURE;
}

COMMAND_INIT(CommandOper)
