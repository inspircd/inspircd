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

using namespace std;

#include "inspircd_config.h"
#include "inspircd.h"
#include "inspircd_io.h"
#include <time.h>
#include <string>
#ifdef GCC3
#include <ext/hash_map>
#else
#include <hash_map>
#endif
#include <map>
#include <sstream>
#include <vector>
#include <deque>
#include "users.h"
#include "ctables.h"
#include "globals.h"
#include "modules.h"
#include "dynamic.h"
#include "wildcard.h"
#include "message.h"
#include "commands.h"
#include "mode.h"
#include "xline.h"
#include "inspstring.h"
#include "dnsqueue.h"
#include "helperfuncs.h"
#include "hashcomp.h"
#include "socketengine.h"
#include "typedefs.h"
#include "command_parse.h"
#include "cmd_oper.h"

extern ServerConfig* Config;
extern InspIRCd* ServerInstance;
extern int MODCOUNT;
extern std::vector<Module*> modules;
extern std::vector<ircd_module*> factory;
extern time_t TIME;
extern user_hash clientlist;
extern chan_hash chanlist;
extern whowas_hash whowas;
extern std::vector<userrec*> all_opers;
extern std::vector<userrec*> local_users;
extern userrec* fd_ref_table[MAX_DESCRIPTORS];

bool OneOfMatches(const char* host, const char* hostlist)
{
	std::stringstream hl(hostlist);
	std::string xhost;
	while (hl >> xhost)
	{
		log(DEBUG,"Oper: Matching host %s",xhost.c_str());
		if (match(host,xhost.c_str()))
		{
			return true;
		}
	}
	return false;
}

void cmd_oper::Handle (char **parameters, int pcnt, userrec *user)
{
	char LoginName[MAXBUF];
	char Password[MAXBUF];
	char OperType[MAXBUF];
	char TypeName[MAXBUF];
	char HostName[MAXBUF];
	char TheHost[MAXBUF];
	int j;
	bool found = false;
	bool fail2 = false;

	snprintf(TheHost,MAXBUF,"%s@%s",user->ident,user->host);

	for (int i = 0; i < Config->ConfValueEnum("oper",&Config->config_f); i++)
	{
		Config->ConfValue("oper","name",i,LoginName,&Config->config_f);
		Config->ConfValue("oper","password",i,Password,&Config->config_f);
		Config->ConfValue("oper","type",i,OperType,&Config->config_f);
		Config->ConfValue("oper","host",i,HostName,&Config->config_f);
		if ((!strcmp(LoginName,parameters[0])) && (!operstrcmp(Password,parameters[1])) && (OneOfMatches(TheHost,HostName)))
		{
			fail2 = true;
			for (j =0; j < Config->ConfValueEnum("type",&Config->config_f); j++)
			{
				Config->ConfValue("type","name",j,TypeName,&Config->config_f);

				if (!strcmp(TypeName,OperType))
				{
					/* found this oper's opertype */
					Config->ConfValue("type","host",j,HostName,&Config->config_f);
					if (*HostName)
						ChangeDisplayedHost(user,HostName);
					if (!isnick(TypeName))
					{
						WriteServ(user->fd,"491 %s :Invalid oper type (oper types must follow the same syntax as nicknames)",user->nick);
						WriteOpers("*** CONFIGURATION ERROR! Oper type invalid for OperType '%s'",OperType);
						log(DEFAULT,"OPER: Failed oper attempt by %s!%s@%s: credentials valid, but oper type erroneous.",user->nick,user->ident,user->host);
						return;
					}
					strlcpy(user->oper,TypeName,NICKMAX-1);
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
                WriteOpers("*** %s (%s@%s) is now an IRC operator of type %s",user->nick,user->ident,user->host,OperType);
                WriteServ(user->fd,"381 %s :You are now an IRC operator of type %s",user->nick,OperType);
		if (!strchr(user->modes,'o'))
		{
			strcat(user->modes,"o");
			WriteServ(user->fd,"MODE %s :+o",user->nick);
			FOREACH_MOD(I_OnOper,OnOper(user,OperType));
			log(DEFAULT,"OPER: %s!%s@%s opered as type: %s",user->nick,user->ident,user->host,OperType);
			AddOper(user);
			FOREACH_MOD(I_OnPostOper,OnPostOper(user,OperType));
		}
	}
	else
	{
		if (!fail2)
		{
			WriteServ(user->fd,"491 %s :Invalid oper credentials",user->nick);
			WriteOpers("*** WARNING! Failed oper attempt by %s!%s@%s!",user->nick,user->ident,user->host);
			log(DEFAULT,"OPER: Failed oper attempt by %s!%s@%s: user, host or password did not match.",user->nick,user->ident,user->host);
		}
		else
		{
			WriteServ(user->fd,"491 %s :Your oper block does not have a valid opertype associated with it",user->nick);
			WriteOpers("*** CONFIGURATION ERROR! Oper block mismatch for OperType %s",OperType);
                        log(DEFAULT,"OPER: Failed oper attempt by %s!%s@%s: credentials valid, but oper type nonexistent.",user->nick,user->ident,user->host);
		}
	}
	return;
}


