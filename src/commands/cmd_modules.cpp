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

/** Handle /MODULES. These command handlers can be reloaded by the core,
 * and handle basic RFC1459 commands. Commands within modules work
 * the same way, however, they can be fully unloaded, where these
 * may not.
 */
class CommandModules : public Command
{
 public:
	/** Constructor for modules.
	 */
	CommandModules ( Module* parent) : Command(parent,"MODULES",0,0) { syntax = "[server]"; }
	/** Handle command.
	 * @param parameters The parameters to the comamnd
	 * @param pcnt The number of parameters passed to teh command
	 * @param user The user issuing the command
	 * @return A value from CmdResult to indicate command success or failure.
	 */
	CmdResult Handle(const std::vector<std::string>& parameters, User *user);
	RouteDescriptor GetRouting(User* user, const std::vector<std::string>& parameters)
	{
		if (parameters.size() >= 1)
			return ROUTE_UNICAST(parameters[0]);
		return ROUTE_LOCALONLY;
	}
};

/** Handle /MODULES
 */
CmdResult CommandModules::Handle (const std::vector<std::string>&, User *user)
{
	std::vector<std::string> module_names = ServerInstance->Modules->GetAllModuleNames(0);

  	for (unsigned int i = 0; i < module_names.size(); i++)
	{
		Module* m = ServerInstance->Modules->Find(module_names[i]);
		Version V = m->GetVersion();

		if (user->HasPrivPermission("servers/auspex"))
		{
			std::string flags("SvcC");
			int pos = 0;
			for (int mult = 1; mult <= VF_OPTCOMMON; mult *= 2, ++pos)
				if (!(V.Flags & mult))
					flags[pos] = '-';

#ifdef PURE_STATIC
			user->SendText(":%s 702 %s :%p %s %s :%s", ServerInstance->Config->ServerName.c_str(),
				user->nick.c_str(), (void*)m, module_names[i].c_str(), flags.c_str(), V.description.c_str());
#else
			std::string srcrev = m->ModuleDLLManager->GetVersion();
			user->SendText(":%s 702 %s :%p %s %s :%s - %s", ServerInstance->Config->ServerName.c_str(),
				user->nick.c_str(), (void*)m, module_names[i].c_str(), flags.c_str(), V.description.c_str(), srcrev.c_str());
#endif
		}
		else
		{
			user->SendText(":%s 702 %s :%s %s", ServerInstance->Config->ServerName.c_str(),
				user->nick.c_str(), module_names[i].c_str(), V.description.c_str());
		}
	}
	user->SendText(":%s 703 %s :End of MODULES list", ServerInstance->Config->ServerName.c_str(), user->nick.c_str());

	return CMD_SUCCESS;
}

COMMAND_INIT(CommandModules)
