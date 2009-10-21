/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2009 InspIRCd Development Team
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
	std::string type;
	bool found = false;
	bool match_login = false;
	bool match_pass = false;
	bool match_hosts = false;

	snprintf(TheHost,MAXBUF,"%s@%s",user->ident.c_str(),user->host.c_str());
	snprintf(TheIP, MAXBUF,"%s@%s",user->ident.c_str(),user->GetIPString());

	for (int i = 0;; i++)
	{
		ConfigTag* tag = ServerInstance->Config->ConfValue("oper", i);
		if (!tag)
			break;
		if (tag->getString("name") != parameters[0])
			continue;
		match_login = true;
		match_pass = !ServerInstance->PassCompare(user, tag->getString("password"), parameters[1], tag->getString("hash"));
		match_hosts = OneOfMatches(TheHost,TheIP,tag->getString("host"));

		if (match_pass && match_hosts)
		{
			type = tag->getString("type");
			ConfigTag* typeTag = ServerInstance->Config->opertypes[type];

			if (typeTag)
			{
				/* found this oper's opertype */
				if (!ServerInstance->IsNick(type.c_str(), ServerInstance->Config->Limits.NickMax))
				{
					user->WriteNumeric(491, "%s :Invalid oper type (oper types must follow the same syntax as nicknames)",user->nick.c_str());
					ServerInstance->SNO->WriteToSnoMask('o',"CONFIGURATION ERROR! Oper type '%s' contains invalid characters",type.c_str());
					ServerInstance->Logs->Log("OPER",DEFAULT,"OPER: Failed oper attempt by %s!%s@%s: credentials valid, but oper type erroneous.", user->nick.c_str(), user->ident.c_str(), user->host.c_str());
					return CMD_FAILURE;
				}
				std::string host = typeTag->getString("host");
				if (!host.empty())
					user->ChangeDisplayedHost(host.c_str());
				std::string opClass = typeTag->getString("class");
				if (!opClass.empty())
				{
					user->SetClass(opClass);
					user->CheckClass();
				}
				found = true;
			}
		}
		break;
	}
	if (found)
	{
		/* correct oper credentials */
		user->Oper(type, parameters[0]);
	}
	else
	{
		char broadcast[MAXBUF];

		if (type.empty())
		{
			std::string fields;
			if (!match_login)
				fields.append("login ");
			if (!match_pass)
				fields.append("password ");
			if (!match_hosts)
				fields.append("hosts");

			// tell them they suck, and lag them up to help prevent brute-force attacks
			user->WriteNumeric(491, "%s :Invalid oper credentials",user->nick.c_str());
			user->Penalty += 10;

			snprintf(broadcast, MAXBUF, "WARNING! Failed oper attempt by %s!%s@%s using login '%s': The following fields do not match: %s", user->nick.c_str(), user->ident.c_str(), user->host.c_str(), parameters[0].c_str(), fields.c_str());
			ServerInstance->SNO->WriteToSnoMask('o',std::string(broadcast));
			ServerInstance->PI->SendSNONotice("o", std::string("OPER: ") + broadcast);

			ServerInstance->Logs->Log("OPER",DEFAULT,"OPER: Failed oper attempt by %s!%s@%s using login '%s': The following fields did not match: %s", user->nick.c_str(), user->ident.c_str(), user->host.c_str(), parameters[0].c_str(), fields.c_str());
			return CMD_FAILURE;
		}
		else
		{
			user->WriteNumeric(491, "%s :Your oper block does not have a valid opertype associated with it",user->nick.c_str());

			snprintf(broadcast, MAXBUF, "CONFIGURATION ERROR! Oper block '%s': missing OperType %s",parameters[0].c_str(),type.c_str());

			ServerInstance->SNO->WriteToSnoMask('o', std::string(broadcast));

			ServerInstance->Logs->Log("OPER",DEFAULT,"OPER: Failed oper attempt by %s!%s@%s using login '%s': credentials valid, but oper type nonexistent.", user->nick.c_str(), user->ident.c_str(), user->host.c_str(), parameters[0].c_str());
			return CMD_FAILURE;
		}
	}
	return CMD_SUCCESS;
}

COMMAND_INIT(CommandOper)
