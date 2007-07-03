/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2007 InspIRCd Development Team
 * See: http://www.inspircd.org/wiki/index.php/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"
#include "users.h"
#include "modules.h"

/* $ModDesc: Provides support for the CHGNAME command */

/** Handle /CHGNAME
 */
class cmd_chgname : public command_t
{
 public:
	cmd_chgname (InspIRCd* Instance) : command_t(Instance,"CHGNAME", 'o', 2)
	{
		this->source = "m_chgname.so";
		syntax = "<nick> <newname>";
	}
	
	CmdResult Handle(const char** parameters, int pcnt, userrec *user)
	{
		userrec* dest = ServerInstance->FindNick(parameters[0]);

		if (!dest)
		{
			user->WriteServ("401 %s %s :No such nick/channel", user->nick, parameters[0]);
			return CMD_FAILURE;
		}
		
		if (!*parameters[1])
		{
			user->WriteServ("NOTICE %s :*** GECOS must be specified", user->nick);
			return CMD_FAILURE;
		}
		
		if (strlen(parameters[1]) > MAXGECOS)
		{
			user->WriteServ("NOTICE %s :*** GECOS too long", user->nick);
			return CMD_FAILURE;
		}
		
		if (IS_LOCAL(dest))
		{
			dest->ChangeName(parameters[1]);
			ServerInstance->WriteOpers("%s used CHGNAME to change %s's real name to '%s'", user->nick, dest->nick, dest->fullname);
			return CMD_LOCALONLY; /* name change routed by FNAME in spanningtree now */
		}

		/* route it! */
		return CMD_SUCCESS;
	}
};


class ModuleChgName : public Module
{
	cmd_chgname* mycommand;
	
	
public:
	ModuleChgName(InspIRCd* Me) : Module(Me)
	{
		mycommand = new cmd_chgname(ServerInstance);
		ServerInstance->AddCommand(mycommand);
	}
	
	virtual ~ModuleChgName()
	{
	}
	
	virtual Version GetVersion()
	{
		return Version(1,1,0,0,VF_VENDOR,API_VERSION);
	}
	
};

MODULE_INIT(ModuleChgName)
