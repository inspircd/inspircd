/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2008 InspIRCd Development Team
 * See: http://www.inspircd.org/wiki/index.php/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"
#include "commands/cmd_modules.h"

extern "C" DllExport Command* init_command(InspIRCd* Instance)
{
	return new CommandModules(Instance);
}

/** Handle /MODULES
 */
CmdResult CommandModules::Handle (const std::vector<std::string>&, User *user)
{
	std::vector<std::string> module_names = ServerInstance->Modules->GetAllModuleNames(0);

  	for (unsigned int i = 0; i < module_names.size(); i++)
	{
		Module* m = ServerInstance->Modules->Find(module_names[i]);
		Version V = m->GetVersion();
		char modulename[MAXBUF];
		char flagstate[MAXBUF];
		*flagstate = 0;
		if (V.Flags & VF_STATIC)
			strlcat(flagstate,", static",MAXBUF);
		if (V.Flags & VF_VENDOR)
			strlcat(flagstate,", vendor",MAXBUF);
		if (V.Flags & VF_COMMON)
			strlcat(flagstate,", common",MAXBUF);
		if (V.Flags & VF_SERVICEPROVIDER)
			strlcat(flagstate,", service provider",MAXBUF);
		if (!flagstate[0])
			strcpy(flagstate,"  <no flags>");
		strlcpy(modulename,module_names[i].c_str(),256);
		if (IS_OPER(user))
		{
			user->WriteNumeric(702, "%s :0x%08lx %d.%d.%d.%d %s (%s)",user->nick.c_str(),(unsigned long)m,V.Major,V.Minor,V.Revision,V.Build,ServerConfig::CleanFilename(modulename),flagstate+2);
		}
		else
		{
			user->WriteNumeric(702, "%s :%s",user->nick.c_str(),ServerConfig::CleanFilename(modulename));
		}
	}
	user->WriteNumeric(703, "%s :End of MODULES list",user->nick.c_str());

	return CMD_SUCCESS;
}
