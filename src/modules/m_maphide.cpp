/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2009 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"

/* $ModDesc: Hide /MAP and /LINKS in the same form as ircu (mostly useless) */

class ModuleMapHide : public Module
{
	std::string url;
 public:
	ModuleMapHide(InspIRCd* Me)
		: Module(Me)
	{
		// Create a new command
		ServerInstance->Modules->Attach(I_OnPreCommand, this);
		ServerInstance->Modules->Attach(I_OnRehash, this);
		OnRehash(NULL, "");
	}

	void OnRehash(User* user, const std::string &parameter)
	{
		ConfigReader MyConf(ServerInstance);
		url = MyConf.ReadValue("security", "maphide", 0);
	}

	int OnPreCommand(std::string &command, std::vector<std::string> &parameters, User *user, bool validated, const std::string &original_line)
	{
		if (!IS_OPER(user) && !url.empty() && (command == "MAP" || command == "LINKS"))
		{
			user->WriteServ("NOTICE %s :/%s has been disabled; visit %s", user->nick.c_str(), command.c_str(), url.c_str());
			return 1;
		}
		else
			return 0;
	}

	virtual ~ModuleMapHide()
	{
	}

	virtual Version GetVersion()
	{
		return Version("$Id$", VF_VENDOR, API_VERSION);
	}
};

MODULE_INIT(ModuleMapHide)

