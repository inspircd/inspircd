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
#include "wildcard.h"

/* $ModDesc: Sets (and unsets) modes on users when they connect */

class ModuleModesOnConnect : public Module
{
 private:

	ConfigReader *Conf;

 public:
	ModuleModesOnConnect(InspIRCd* Me)
		: Module(Me)
	{
		
		Conf = new ConfigReader(ServerInstance);
		Implementation eventlist[] = { I_OnPostConnect, I_OnRehash };
		ServerInstance->Modules->Attach(eventlist, this, 2);
	}


	virtual void OnRehash(User* user, const std::string &parameter)
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
		return Version(1,2,0,1,VF_VENDOR,API_VERSION);
	}
	
	virtual void OnPostConnect(User* user)
	{
		if (!IS_LOCAL(user))
			return;

		for (int j = 0; j < Conf->Enumerate("connect"); j++)
		{
			std::string hostn = Conf->ReadValue("connect","allow",j);
			/* XXX: Fixme: does not respect port, limit, etc */
			if ((match(user->GetIPString(),hostn,true)) || (match(user->host,hostn)))
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
	}
};

MODULE_INIT(ModuleModesOnConnect)
