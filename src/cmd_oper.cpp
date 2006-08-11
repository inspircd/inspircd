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

#include <string>
#include <sstream>
#include <vector>
#include "inspircd_config.h"
#include "configreader.h"
#include "typedefs.h"
#include "users.h"
#include "globals.h"
#include "modules.h"
#include "dynamic.h"
#include "wildcard.h"
#include "commands.h"
#include "mode.h"
#include "xline.h"
#include "inspstring.h"
#include "helperfuncs.h"
#include "hashcomp.h"
#include "socketengine.h"

#include "command_parse.h"
#include "commands/cmd_oper.h"

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

void cmd_oper::Handle (const char** parameters, int pcnt, userrec *user)
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
	bool fail2 = false;

	snprintf(TheHost,MAXBUF,"%s@%s",user->ident,user->host);
	snprintf(TheIP, MAXBUF,"%s@%s",user->ident,user->GetIPString());

	for (int i = 0; i < ServerInstance->Config->ConfValueEnum(ServerInstance->Config->config_data, "oper"); i++)
	{
		ServerInstance->Config->ConfValue(ServerInstance->Config->config_data, "oper", "name", i, LoginName, MAXBUF);
		ServerInstance->Config->ConfValue(ServerInstance->Config->config_data, "oper", "password", i, Password, MAXBUF);
		ServerInstance->Config->ConfValue(ServerInstance->Config->config_data, "oper", "type", i, OperType, MAXBUF);
		ServerInstance->Config->ConfValue(ServerInstance->Config->config_data, "oper", "host", i, HostName, MAXBUF);

		if ((!strcmp(LoginName,parameters[0])) && (!operstrcmp(Password,parameters[1])) && (OneOfMatches(TheHost,TheIP,HostName)))
		{
			fail2 = true;
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
						ServerInstance->WriteOpers("*** CONFIGURATION ERROR! Oper type invalid for OperType '%s'",OperType);
						log(DEFAULT,"OPER: Failed oper attempt by %s!%s@%s: credentials valid, but oper type erroneous.",user->nick,user->ident,user->host);
						return;
					}
					found = true;
					fail2 = false;
					break;
				}
			}
		}
		if (found)
			break;
	}
	if (found)
	{
		/* correct oper credentials */
		ServerInstance->WriteOpers("*** %s (%s@%s) is now an IRC operator of type %s",user->nick,user->ident,user->host,OperType);
		user->WriteServ("381 %s :You are now an IRC operator of type %s",user->nick,OperType);
		if (!user->modes[UM_OPERATOR])
			user->Oper(OperType);
	}
	else
	{
		if (!fail2)
		{
			user->WriteServ("491 %s :Invalid oper credentials",user->nick);
			ServerInstance->WriteOpers("*** WARNING! Failed oper attempt by %s!%s@%s!",user->nick,user->ident,user->host);
			log(DEFAULT,"OPER: Failed oper attempt by %s!%s@%s: user, host or password did not match.",user->nick,user->ident,user->host);
		}
		else
		{
			user->WriteServ("491 %s :Your oper block does not have a valid opertype associated with it",user->nick);
			ServerInstance->WriteOpers("*** CONFIGURATION ERROR! Oper block mismatch for OperType %s",OperType);
			log(DEFAULT,"OPER: Failed oper attempt by %s!%s@%s: credentials valid, but oper type nonexistent.",user->nick,user->ident,user->host);
		}
	}
	return;
}
