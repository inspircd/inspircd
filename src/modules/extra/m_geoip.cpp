/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2008 Craig Edwards <craigedwards@brainbox.cc>
 *
 * This file is part of InspIRCd.  InspIRCd is free software: you can
 * redistribute it and/or modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation, version 2.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */


#include "inspircd.h"
#include "xline.h"

#include <GeoIP.h>

/* $ModDesc: Provides a way to restrict users by country using GeoIP lookup */
/* $LinkerFlags: -lGeoIP */

class ModuleGeoIP : public Module
{
	GeoIP * gi;

	bool banunknown;

	std::map<std::string, std::string> GeoBans;


 public:
	ModuleGeoIP(InspIRCd *Me) : Module(Me)
	{
		OnRehash(NULL);
		Implementation eventlist[] = { I_OnRehash, I_OnUserRegister };
		ServerInstance->Modules->Attach(eventlist, this, 2);

		gi = GeoIP_new(GEOIP_STANDARD);
	}

	virtual ~ModuleGeoIP()
	{
	}

	virtual Version GetVersion()
	{
		return Version("$Id$", VF_VENDOR, API_VERSION);
	}

	virtual void OnRehash(User* user)
	{
		GeoBans.clear();

		ConfigReader conf(ServerInstance);

		banunknown = conf.ReadFlag("geoip", "banunknown", 0);

		for (int i = 0; i < conf.Enumerate("geoban"); ++i)
		{
			std::string countrycode = conf.ReadValue("geoban", "country", i);
			std::string reason = conf.ReadValue("geoban", "reason", i);
			GeoBans[countrycode] = reason;
		}
	}

	virtual int OnUserRegister(User* user)
	{
		/* only do lookups on local users */
		if (IS_LOCAL(user))
		{
			const char* c = GeoIP_country_code_by_addr(gi, user->GetIPString());
			if (c)
			{
				std::map<std::string, std::string>::iterator x = GeoBans.find(c);
				if (x != GeoBans.end())
					ServerInstance->Users->QuitUser(user,  x->second);
			}
			else
			{
				if (banunknown)
					ServerInstance->Users->QuitUser(user, "Could not identify your country of origin. Access denied.");
			}
		}
		return 0;
	}
};

MODULE_INIT(ModuleGeoIP)

