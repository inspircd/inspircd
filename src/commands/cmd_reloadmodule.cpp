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

class CommandReloadmodule : public Command
{
 public:
	/** Constructor for reloadmodule.
	 */
	CommandReloadmodule ( Module* parent) : Command( parent, "RELOADMODULE",1) { flags_needed = 'o'; syntax = "<modulename>"; }
	/** Handle command.
	 * @param parameters The parameters to the comamnd
	 * @param pcnt The number of parameters passed to teh command
	 * @param user The user issuing the command
	 * @return A value from CmdResult to indicate command success or failure.
	 */
	CmdResult Handle(const std::vector<std::string>& parameters, User *user);
};

class ReloadModuleWorker : public HandlerBase1<void, bool>
{
 public:
	const std::string name;
	const std::string uid;
	ReloadModuleWorker(const std::string& uuid, const std::string& modn)
		: name(modn), uid(uuid) {}
	void Call(bool result)
	{
		ServerInstance->SNO->WriteGlobalSno('a', "RELOAD MODULE: %s %ssuccessfully reloaded",
			name.c_str(), result ? "" : "un");
		User* user = ServerInstance->FindNick(uid);
		if (user)
			user->WriteNumeric(975, "%s %s :Module %ssuccessfully reloaded.",
				user->nick.c_str(), name.c_str(), result ? "" : "un");
		ServerInstance->GlobalCulls.AddItem(this);
	}
};

CmdResult CommandReloadmodule::Handle (const std::vector<std::string>& parameters, User *user)
{
	if (parameters[0] == "cmd_reloadmodule.so")
	{
		user->WriteNumeric(975, "%s %s :You cannot reload cmd_reloadmodule.so (unload and load it)",
			user->nick.c_str(), parameters[0].c_str());
		return CMD_FAILURE;
	}

	Module* m = ServerInstance->Modules->Find(parameters[0]);
	if (m)
	{
		ServerInstance->Modules->Reload(m, new ReloadModuleWorker(user->uuid, parameters[0]));
		return CMD_SUCCESS;
	}
	else
	{
		user->WriteNumeric(975, "%s %s :Could not find module by that name", user->nick.c_str(), parameters[0].c_str());
		return CMD_FAILURE;
	}
}

COMMAND_INIT(CommandReloadmodule)
