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

/* $ModDesc: Allows global loading of a module. */

#include "inspircd.h"

/** Handle /GLOADMODULE
 */
class CommandGloadmodule : public Command
{
 public:
	CommandGloadmodule (InspIRCd* Instance) : Command(Instance,"GLOADMODULE", 'o', 1)
	{
		this->source = "m_globalload.so";
		syntax = "<modulename> [servermask]";
		TRANSLATE3(TR_TEXT, TR_TEXT, TR_END);
	}

	CmdResult Handle (const char** parameters, int pcnt, User *user)
	{
		std::string servername = pcnt > 1 ? parameters[1] : "*";

		if (ServerInstance->MatchText(ServerInstance->Config->ServerName, servername))
		{
			if (ServerInstance->Modules->Load(parameters[0]))
			{
				ServerInstance->WriteOpers("*** NEW MODULE '%s' GLOBALLY LOADED BY '%s'",parameters[0],user->nick);
				user->WriteServ("975 %s %s :Module successfully loaded.",user->nick, parameters[0]);
			}
			else
			{
				user->WriteServ("974 %s %s :%s",user->nick, parameters[0],ServerInstance->Modules->LastError().c_str());
			}
		}
		else
			ServerInstance->WriteOpers("*** MODULE '%s' GLOBAL LOAD BY '%s' (not loaded here)",parameters[0],user->nick);

		return CMD_SUCCESS;
	}
};

/** Handle /GUNLOADMODULE
 */
class CommandGunloadmodule : public Command
{
 public:
	CommandGunloadmodule (InspIRCd* Instance) : Command(Instance,"GUNLOADMODULE", 'o', 1)
	{
		this->source = "m_globalload.so";
		syntax = "<modulename> [servermask]";
	}

	CmdResult Handle (const char** parameters, int pcnt, User *user)
	{
		std::string servername = pcnt > 1 ? parameters[1] : "*";

		if (ServerInstance->MatchText(ServerInstance->Config->ServerName, servername))
		{
			if (ServerInstance->Modules->Unload(parameters[0]))
			{
				ServerInstance->WriteOpers("*** MODULE '%s' GLOBALLY UNLOADED BY '%s'",parameters[0],user->nick);
				user->WriteServ("973 %s %s :Module successfully unloaded.",user->nick, parameters[0]);
			}
			else
			{
				user->WriteServ("972 %s %s :%s",user->nick, parameters[0],ServerInstance->Modules->LastError().c_str());
			}
		}
		else
			ServerInstance->WriteOpers("*** MODULE '%s' GLOBAL UNLOAD BY '%s' (not unloaded here)",parameters[0],user->nick);

		return CMD_SUCCESS;
	}
};

/** Handle /GRELOADMODULE
 */
class CommandGreloadmodule : public Command
{
 public:
	CommandGreloadmodule (InspIRCd* Instance) : Command(Instance, "GRELOADMODULE", 'o', 1)
	{
		this->source = "m_globalload.so";
		syntax = "<modulename> [servermask]";
	}

	CmdResult Handle(const char** parameters, int pcnt, User *user)
	{
		std::string servername = pcnt > 1 ? parameters[1] : "*";

		if (ServerInstance->MatchText(ServerInstance->Config->ServerName, servername))
		{
			if (!ServerInstance->Modules->Unload(parameters[0]))
			{
				user->WriteServ("972 %s %s :%s",user->nick, parameters[0],ServerInstance->Modules->LastError().c_str());
			}
			if (!ServerInstance->Modules->Load(parameters[0]))
			{
				user->WriteServ("974 %s %s :%s",user->nick, parameters[0],ServerInstance->Modules->LastError().c_str());
			}
			ServerInstance->WriteOpers("*** MODULE '%s' GLOBALLY RELOADED BY '%s'",parameters[0],user->nick);
			user->WriteServ("975 %s %s :Module successfully loaded.",user->nick, parameters[0]);
		}
		else
			ServerInstance->WriteOpers("*** MODULE '%s' GLOBAL RELOAD BY '%s' (not reloaded here)",parameters[0],user->nick);

		return CMD_SUCCESS;
	}
};

class ModuleGlobalLoad : public Module
{
	CommandGloadmodule *mycommand;
	CommandGunloadmodule *mycommand2;
	CommandGreloadmodule *mycommand3;
	
 public:
	ModuleGlobalLoad(InspIRCd* Me) : Module(Me)
	{
		
		mycommand = new CommandGloadmodule(ServerInstance);
		mycommand2 = new CommandGunloadmodule(ServerInstance);
		mycommand3 = new CommandGreloadmodule(ServerInstance);
		ServerInstance->AddCommand(mycommand);
		ServerInstance->AddCommand(mycommand2);
		ServerInstance->AddCommand(mycommand3);

	}
	
	virtual ~ModuleGlobalLoad()
	{
	}
	
	virtual Version GetVersion()
	{
		return Version(1, 1, 0, 0, VF_COMMON | VF_VENDOR, API_VERSION);
	}
};

MODULE_INIT(ModuleGlobalLoad)

