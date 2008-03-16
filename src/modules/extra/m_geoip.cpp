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
#include "xline.h"

#include <GeoIP.h>

/* $ModDesc: Provides a way to restrict users by country using GeoIP lookup */
/* $LinkerFlags: -lGeoIP */

class ModuleGeoIP : public Module
{
	GeoIP * gi;

 public:
	ModuleGeoIP(InspIRCd *Me) : Module(Me)
	{
		ReadConf();
		Implementation eventlist[] = { I_OnRehash, I_OnUserRegister };
		ServerInstance->Modules->Attach(eventlist, this, 2);

		gi = GeoIP_new(GEOIP_STANDARD);
	}

	virtual ~ModuleGeoIP()
	{
	}

	virtual Version GetVersion()
	{
		return Version(1, 2, 0, 0, VF_VENDOR, API_VERSION);
	}

	virtual void ReadConf()
	{
		ConfigReader *MyConf = new ConfigReader(ServerInstance);
		delete MyConf;
	}

	virtual void OnRehash(User* user, const std::string &parameter)
	{
		ReadConf();
	}

	virtual int OnUserRegister(User* user)
	{
		/* only do lookups on local users */
		if (IS_LOCAL(user))
		{
			const char* c = GeoIP_country_code_by_addr(gi, user->GetIPString());
			if (c)
			{
				std::string country(c);
				ServerInstance->Logs->Log("m_geoip", DEBUG, "*** Country: %s", country.c_str());
			}
			else
			{
				ServerInstance->Logs->Log("m_geoip", DEBUG, "*** No country for %s!", user->GetIPString());
			}
		}

		/* don't do anything with this hot potato */
		return 0;
	}
};

MODULE_INIT(ModuleGeoIP)

