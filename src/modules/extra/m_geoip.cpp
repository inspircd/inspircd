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
#include "xline.h"

#include <GeoIP.h>

/* $ModDesc: Provides a way to restrict users by country using GeoIP lookup */
/* $LinkerFlags: -lGeoIP */

class ModuleGeoIP : public Module
{
	LocalStringExt ext;
	GeoIP* gi;

 public:
	ModuleGeoIP() : ext("geoip_cc", this)
	{
		gi = GeoIP_new(GEOIP_STANDARD);
	}

	void init()
	{
		ServerInstance->Modules->AddService(ext);
		Implementation eventlist[] = { I_OnSetConnectClass };
		ServerInstance->Modules->Attach(eventlist, this, sizeof(eventlist)/sizeof(Implementation));
	}

	~ModuleGeoIP()
	{
		GeoIP_delete(gi);
	}

	Version GetVersion()
	{
		return Version("Provides a way to assign users to connect classes by country using GeoIP lookup", VF_VENDOR);
	}

	ModResult OnSetConnectClass(LocalUser* user, ConnectClass* myclass)
	{
		std::string* cc = ext.get(user);
		if (!cc)
		{
			const char* c = GeoIP_country_code_by_addr(gi, user->GetIPString());
			if (!c)
				c = "UNK";
			cc = new std::string(c);
			ext.set(user, cc);
		}
		std::string geoip = myclass->config->getString("geoip");
		if (geoip.empty())
			return MOD_RES_PASSTHRU;
		irc::commasepstream list(geoip);
		std::string country;
		while (list.GetToken(country))
			if (country == *cc)
				return MOD_RES_PASSTHRU;
		return MOD_RES_DENY;
	}
};

MODULE_INIT(ModuleGeoIP)

