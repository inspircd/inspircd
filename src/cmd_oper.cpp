/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd is copyright (C) 2002-2006 ChatSpike-Dev.
 *                       E-mail:
 *                <brain@chatspike.net>
 *                <Craig@chatspike.net>
 *
 * Written by Craig Edwards, Craig McLure, and others.
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "configreader.h"
#include "typedefs.h"
#include "users.h"
#include "modules.h"
#include "wildcard.h"
#include "commands/cmd_oper.h"
#include "commands/cmd_whois.h"

bool OneOfMatches(const char* host, const char* ip, const char* hostlist)
{
	std::stringstream hl(hostlist);
	std::string xhost;
	while (hl >> xhost)
	{
		if (match(host,xhost.c_str()) || match(ip,xhost.c_str(),true))
		{
			return true;
		}
	}
	return false;
}



extern "C" command_t* init_command(InspIRCd* Instance)
{
	return new cmd_oper(Instance);
}

CmdResult cmd_oper::Handle (const char** parameters, int pcnt, userrec *user)
{
	char LoginName[MAXBUF];
	char Password[MAXBUF];
	char OperType[MAXBUF];
	char TypeName[MAXBUF];
	char HostName[MAXBUF];
	char TheHost[MAXBUF];
	char TheIP[MAXBUF];
	int j;
	bool found = false;
	bool type_invalid = false;

	bool match_login = false;
	bool match_pass = false;
	bool match_hosts = false;

	snprintf(TheHost,MAXBUF,"%s@%s",user->ident,user->host);
	snprintf(TheIP, MAXBUF,"%s@%s",user->ident,user->GetIPString());

	for (int i = 0; i < ServerInstance->Config->ConfValueEnum(ServerInstance->Config->config_data, "oper"); i++)
	{
		ServerInstance->Config->ConfValue(ServerInstance->Config->config_data, "oper", "name", i, LoginName, MAXBUF);
		ServerInstance->Config->ConfValue(ServerInstance->Config->config_data, "oper", "password", i, Password, MAXBUF);
		ServerInstance->Config->ConfValue(ServerInstance->Config->config_data, "oper", "type", i, OperType, MAXBUF);
		ServerInstance->Config->ConfValue(ServerInstance->Config->config_data, "oper", "host", i, HostName, MAXBUF);

		match_login = !strcmp(LoginName,parameters[0]);
		match_pass = !ServerInstance->OperPassCompare(Password,parameters[1]);
		match_hosts = OneOfMatches(TheHost,TheIP,HostName);

		if (match_login && match_pass && match_hosts)
		{
			type_invalid = true;
			for (j =0; j < ServerInstance->Config->ConfValueEnum(ServerInstance->Config->config_data, "type"); j++)
			{
				ServerInstance->Config->ConfValue(ServerInstance->Config->config_data, "type","name", j, TypeName, MAXBUF);

				if (!strcmp(TypeName,OperType))
				{
					/* found this oper's opertype */
					ServerInstance->Config->ConfValue(ServerInstance->Config->config_data, "type","host", j, HostName, MAXBUF);
					if (*HostName)
						user->ChangeDisplayedHost(HostName);
					if (!ServerInstance->IsNick(TypeName))
					{
						user->WriteServ("491 %s :Invalid oper type (oper types must follow the same syntax as nicknames)",user->nick);
						ServerInstance->SNO->WriteToSnoMask('o',"CONFIGURATION ERROR! Oper type '%s' contains invalid characters",OperType);
						ServerInstance->Log(DEFAULT,"OPER: Failed oper attempt by %s!%s@%s: credentials valid, but oper type erroneous.",user->nick,user->ident,user->host);
						return CMD_FAILURE;
					}
					found = true;
					type_invalid = false;
					break;
				}
			}
		}
		if (match_login || found)
			break;
	}
	if (found)
	{
		/* correct oper credentials */
		ServerInstance->SNO->WriteToSnoMask('o',"%s (%s@%s) is now an IRC operator of type %s (using oper '%s')",user->nick,user->ident,user->host,Spacify(OperType),parameters[0]);
		user->WriteServ("381 %s :You are now an IRC operator of type %s",user->nick,Spacify(OperType));
		if (!user->modes[UM_OPERATOR])
			user->Oper(OperType);
	}
	else
	{
		if (!type_invalid)
		{
			std::string fields = "";
			if (!match_login)
				fields.append("login ");
			if (!match_pass)
				fields.append("password ");
			if (!match_hosts)
				fields.append("hosts");
			user->WriteServ("491 %s :Invalid oper credentials",user->nick);
			ServerInstance->SNO->WriteToSnoMask('o',"WARNING! Failed oper attempt by %s!%s@%s using login '%s': The following fields do not match: %s",user->nick,user->ident,user->host, parameters[0], fields.c_str());
			ServerInstance->Log(DEFAULT,"OPER: Failed oper attempt by %s!%s@%s using login '%s': The following fields did not match: %s",user->nick,user->ident,user->host,parameters[0],fields.c_str());
			return CMD_FAILURE;
		}
		else
		{
			user->WriteServ("491 %s :Your oper block does not have a valid opertype associated with it",user->nick);
			ServerInstance->SNO->WriteToSnoMask('o',"CONFIGURATION ERROR! Oper block '%s': missing OperType %s",parameters[0],OperType);
			ServerInstance->Log(DEFAULT,"OPER: Failed oper attempt by %s!%s@%s using login '%s': credentials valid, but oper type nonexistent.",user->nick,user->ident,user->host,parameters[0]);
			return CMD_FAILURE;
		}
	}
	return CMD_SUCCESS;
}

