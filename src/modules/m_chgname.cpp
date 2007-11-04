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

/* $ModDesc: Provides support for the CHGNAME command */

/** Handle /CHGNAME
 */
class CommandChgname : public Command
{
 public:
	CommandChgname (InspIRCd* Instance) : Command(Instance,"CHGNAME", 'o', 2)
	{
		this->source = "m_chgname.so";
		syntax = "<nick> <newname>";
		TRANSLATE3(TR_NICK, TR_TEXT, TR_END);
	}
	
	CmdResult Handle(const char** parameters, int pcnt, User *user)
	{
		User* dest = ServerInstance->FindNick(parameters[0]);

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
	CommandChgname* mycommand;
	
	
public:
	ModuleChgName(InspIRCd* Me) : Module(Me)
	{
		mycommand = new CommandChgname(ServerInstance);
		ServerInstance->AddCommand(mycommand);

	}
	
	virtual ~ModuleChgName()
	{
	}
	
	virtual Version GetVersion()
	{
		return Version(1, 1, 0, 0, VF_COMMON | VF_VENDOR, API_VERSION);
	}
	
};

MODULE_INIT(ModuleChgName)
