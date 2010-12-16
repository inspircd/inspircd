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

/* $ModDesc: Provides masking of user hostnames via traditional /VHOST command */

/** Handle /VHOST
 */
class CommandVhost : public Command
{
 public:
	CommandVhost(Module* Creator) : Command(Creator,"VHOST", 2)
	{
		syntax = "<username> <password>";
	}

	CmdResult Handle (const std::vector<std::string> &parameters, User *user)
	{
		ConfigTagList tags = ServerInstance->Config->GetTags("vhost");
		for(ConfigIter i = tags.first; i != tags.second; ++i)
		{
			ConfigTag* tag = i->second;
			std::string mask = tag->getString("host");
			std::string username = tag->getString("user");
			std::string pass = tag->getString("pass");
			std::string hash = tag->getString("hash");

			if (parameters[0] == username && !ServerInstance->PassCompare(user, pass, parameters[1], hash))
			{
				if (!mask.empty())
				{
					user->WriteServ("NOTICE %s :Setting your VHost: %s", user->nick.c_str(), mask.c_str());
					user->ChangeDisplayedHost(mask.c_str());
					return CMD_SUCCESS;
				}
			}
		}

		user->WriteServ("NOTICE %s :Invalid username or password.", user->nick.c_str());
		return CMD_FAILURE;
	}
};

class ModuleVHost : public Module
{
 private:
	CommandVhost cmd;

 public:
	ModuleVHost() : cmd(this) {}

	void init()
	{
		ServerInstance->AddCommand(&cmd);
	}

	Version GetVersion()
	{
		return Version("Provides masking of user hostnames via traditional /VHOST command",VF_VENDOR);
	}

};

MODULE_INIT(ModuleVHost)

