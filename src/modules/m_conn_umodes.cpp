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

/* $ModDesc: Sets (and unsets) modes on users when they connect */

class ModuleModesOnConnect : public Module
{
 private:

	ConfigReader *Conf;

 public:
	ModuleModesOnConnect(InspIRCd* Me) : Module(Me)
	{

		Conf = new ConfigReader(ServerInstance);
		Implementation eventlist[] = { I_OnUserConnect, I_OnRehash };
		ServerInstance->Modules->Attach(eventlist, this, 2);
		// for things like +x on connect, important, otherwise we have to resort to config order (bleh) -- w00t
		ServerInstance->Modules->SetPriority(this, PRIORITY_FIRST);
	}


	virtual void OnRehash(User* user)
	{
		delete Conf;
		Conf = new ConfigReader(ServerInstance);
	}

	virtual ~ModuleModesOnConnect()
	{
		delete Conf;
	}

	virtual Version GetVersion()
	{
		return Version("$Id$", VF_VENDOR,API_VERSION);
	}

	virtual void OnUserConnect(User* user)
	{
		if (!IS_LOCAL(user))
			return;

		// Backup and zero out the disabled usermodes, so that we can override them here.
		char save[64];
		memcpy(save, ServerInstance->Config->DisabledUModes,
				sizeof(ServerInstance->Config->DisabledUModes));
		memset(ServerInstance->Config->DisabledUModes, 0, 64);

		for (int j = 0; j < Conf->Enumerate("connect"); j++)
		{
			std::string hostn = Conf->ReadValue("connect","allow",j);
			/* XXX: Fixme: does not respect port, limit, etc */
			if ((InspIRCd::MatchCIDR(user->GetIPString(),hostn, ascii_case_insensitive_map)) || (InspIRCd::Match(user->host,hostn, ascii_case_insensitive_map)))
			{
				std::string ThisModes = Conf->ReadValue("connect","modes",j);
				if (!ThisModes.empty())
				{
					std::string buf;
					std::stringstream ss(ThisModes);

					std::vector<std::string> tokens;

					// split ThisUserModes into modes and mode params
					while (ss >> buf)
						tokens.push_back(buf);

					std::vector<std::string> modes;
					modes.push_back(user->nick);
					modes.push_back(tokens[0]);

					if (tokens.size() > 1)
					{
						// process mode params
						for (unsigned int k = 1; k < tokens.size(); k++)
						{
							modes.push_back(tokens[k]);
						}
					}

					ServerInstance->Parser->CallHandler("MODE", modes, user);
				}
				break;
			}
		}

		memcpy(ServerInstance->Config->DisabledUModes, save, 64);
	}
};

MODULE_INIT(ModuleModesOnConnect)
