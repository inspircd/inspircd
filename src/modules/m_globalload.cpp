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

/* $ModDesc: Allows global loading of a module. */

#include "inspircd.h"

/** Handle /GLOADMODULE
 */
class CommandGloadmodule : public Command
{
 public:
	CommandGloadmodule(Module* Creator) : Command(Creator,"GLOADMODULE", 1)
	{
		flags_needed = 'o';
		syntax = "<modulename> [servermask]";
		TRANSLATE3(TR_TEXT, TR_TEXT, TR_END);
	}

	CmdResult Handle (const std::vector<std::string> &parameters, User *user)
	{
		std::string servername = parameters.size() > 1 ? parameters[1] : "*";

		if (InspIRCd::Match(ServerInstance->Config->ServerName.c_str(), servername))
		{
			if (ServerInstance->Modules->Load(parameters[0].c_str()))
			{
				ServerInstance->SNO->WriteToSnoMask('a', "NEW MODULE '%s' GLOBALLY LOADED BY '%s'",parameters[0].c_str(), user->nick.c_str());
				user->WriteNumeric(975, "%s %s :Module successfully loaded.",user->nick.c_str(), parameters[0].c_str());
			}
			else
			{
				user->WriteNumeric(974, "%s %s :%s",user->nick.c_str(), parameters[0].c_str(), ServerInstance->Modules->LastError().c_str());
			}
		}
		else
			ServerInstance->SNO->WriteToSnoMask('a', "MODULE '%s' GLOBAL LOAD BY '%s' (not loaded here)",parameters[0].c_str(), user->nick.c_str());

		return CMD_SUCCESS;
	}

	RouteDescriptor GetRouting(User* user, const std::vector<std::string>& parameters)
	{
		return ROUTE_BROADCAST;
	}
};

/** Handle /GUNLOADMODULE
 */
class CommandGunloadmodule : public Command
{
 public:
	CommandGunloadmodule(Module* Creator) : Command(Creator,"GUNLOADMODULE", 1)
	{
		flags_needed = 'o';
		syntax = "<modulename> [servermask]";
	}

	CmdResult Handle (const std::vector<std::string> &parameters, User *user)
	{
		std::string servername = parameters.size() > 1 ? parameters[1] : "*";

		if (InspIRCd::Match(ServerInstance->Config->ServerName.c_str(), servername))
		{
			Module* m = ServerInstance->Modules->Find(parameters[0]);
			if (m && ServerInstance->Modules->Unload(m))
			{
				ServerInstance->SNO->WriteToSnoMask('a', "MODULE '%s' GLOBALLY UNLOADED BY '%s'",parameters[0].c_str(), user->nick.c_str());
				user->SendText(":%s 973 %s %s :Module successfully unloaded.",
					ServerInstance->Config->ServerName.c_str(), user->nick.c_str(), parameters[0].c_str());
			}
			else
			{
				user->WriteNumeric(972, "%s %s :%s",user->nick.c_str(), parameters[0].c_str(), ServerInstance->Modules->LastError().c_str());
			}
		}
		else
			ServerInstance->SNO->WriteToSnoMask('a', "MODULE '%s' GLOBAL UNLOAD BY '%s' (not unloaded here)",parameters[0].c_str(), user->nick.c_str());

		return CMD_SUCCESS;
	}

	RouteDescriptor GetRouting(User* user, const std::vector<std::string>& parameters)
	{
		return ROUTE_BROADCAST;
	}
};

class GReloadModuleWorker : public HandlerBase1<void, bool>
{
 public:
	const std::string nick;
	const std::string name;
	const std::string uid;
	GReloadModuleWorker(const std::string& usernick, const std::string& uuid, const std::string& modn)
		: nick(usernick), name(modn), uid(uuid) {}
	void Call(bool result)
	{
		ServerInstance->SNO->WriteToSnoMask('a', "MODULE '%s' GLOBALLY RELOADED BY '%s'%s", name.c_str(), nick.c_str(), result ? "" : " (failed here)");
		User* user = ServerInstance->FindNick(uid);
		if (user)
			user->WriteNumeric(975, "%s %s :Module %ssuccessfully reloaded.",
				user->nick.c_str(), name.c_str(), result ? "" : "un");
		ServerInstance->GlobalCulls.AddItem(this);
	}
};

/** Handle /GRELOADMODULE
 */
class CommandGreloadmodule : public Command
{
 public:
	CommandGreloadmodule(Module* Creator) : Command(Creator, "GRELOADMODULE", 1)
	{
		flags_needed = 'o'; syntax = "<modulename> [servermask]";
	}

	CmdResult Handle(const std::vector<std::string> &parameters, User *user)
	{
		std::string servername = parameters.size() > 1 ? parameters[1] : "*";

		if (InspIRCd::Match(ServerInstance->Config->ServerName.c_str(), servername))
		{
			Module* m = ServerInstance->Modules->Find(parameters[0]);
			if (m)
				ServerInstance->Modules->Reload(m, new GReloadModuleWorker(user->nick, user->uuid, parameters[0]));
			else
			{
				user->WriteNumeric(975, "%s %s :Could not find module by that name", user->nick.c_str(), parameters[0].c_str());
				return CMD_FAILURE;
			}
		}
		else
			ServerInstance->SNO->WriteToSnoMask('a', "MODULE '%s' GLOBAL RELOAD BY '%s' (not reloaded here)",parameters[0].c_str(), user->nick.c_str());

		return CMD_SUCCESS;
	}

	RouteDescriptor GetRouting(User* user, const std::vector<std::string>& parameters)
	{
		return ROUTE_BROADCAST;
	}
};

class ModuleGlobalLoad : public Module
{
	CommandGloadmodule cmd1;
	CommandGunloadmodule cmd2;
	CommandGreloadmodule cmd3;

 public:
	ModuleGlobalLoad()
		: cmd1(this), cmd2(this), cmd3(this)
	{
		ServerInstance->AddCommand(&cmd1);
		ServerInstance->AddCommand(&cmd2);
		ServerInstance->AddCommand(&cmd3);
	}

	~ModuleGlobalLoad()
	{
	}

	Version GetVersion()
	{
		return Version("Allows global loading of a module.", VF_COMMON | VF_VENDOR);
	}
};

MODULE_INIT(ModuleGlobalLoad)

