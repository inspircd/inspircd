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

/* $ModDesc: Provides support for the CHGIDENT command */

/** Handle /CHGIDENT
 */
class CommandChgident : public Command
{
 public:
	CommandChgident (InspIRCd* Instance) : Command(Instance,"CHGIDENT", 'o', 2)
	{
		this->source = "m_chgident.so";
		syntax = "<nick> <newident>";
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
			user->WriteServ("NOTICE %s :*** CHGIDENT: Ident must be specified", user->nick);
			return CMD_FAILURE;
		}
		
		if (strlen(parameters[1]) > IDENTMAX - 1)
		{
			user->WriteServ("NOTICE %s :*** CHGIDENT: Ident is too long", user->nick);
			return CMD_FAILURE;
		}
		
		if (!ServerInstance->IsIdent(parameters[1]))
		{
			user->WriteServ("NOTICE %s :*** CHGIDENT: Invalid characters in ident", user->nick);
			return CMD_FAILURE;
		}

		dest->ChangeIdent(parameters[1]);

		if (!ServerInstance->ULine(user->server))
			ServerInstance->WriteOpers("%s used CHGIDENT to change %s's ident to '%s'", user->nick, dest->nick, dest->ident);

		/* route it! */
		return CMD_SUCCESS;
	}
};


class ModuleChgIdent : public Module
{
	CommandChgident* mycommand;
	
	
public:
	ModuleChgIdent(InspIRCd* Me) : Module(Me)
	{
		mycommand = new CommandChgident(ServerInstance);
		ServerInstance->AddCommand(mycommand);

	}
	
	virtual ~ModuleChgIdent()
	{
	}
	
	virtual Version GetVersion()
	{
		return Version(1, 1, 0, 0, VF_COMMON | VF_VENDOR, API_VERSION);
	}
	
};

MODULE_INIT(ModuleChgIdent)

