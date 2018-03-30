/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
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

#ifdef _WIN32
# pragma comment(lib, "GeoIP.lib")
#endif

/* $ModDesc: Provides a way to restrict users by country using GeoIP lookup */
/* $LinkerFlags: -lGeoIP */

class ModuleGeoIP : public Module
{
	LocalStringExt ext;
	GeoIP* gi;

	std::string* SetExt(LocalUser* user)
	{
		const char* c = GeoIP_country_code_by_addr(gi, user->GetIPString());
		if (!c)
			c = "UNK";

		std::string* cc = new std::string(c);
		ext.set(user, cc);
		return cc;
	}

 public:
	ModuleGeoIP() : ext("geoip_cc", this), gi(NULL)
	{
	}

	void init()
	{
		gi = GeoIP_new(GEOIP_STANDARD);
		if (gi == NULL)
				throw ModuleException("Unable to initialize geoip, are you missing GeoIP.dat?");

		ServerInstance->Modules->AddService(ext);
		Implementation eventlist[] = { I_OnSetConnectClass, I_OnSetUserIP, I_OnStats };
		ServerInstance->Modules->Attach(eventlist, this, sizeof(eventlist)/sizeof(Implementation));

		for (LocalUserList::const_iterator i = ServerInstance->Users->local_users.begin(); i != ServerInstance->Users->local_users.end(); ++i)
		{
			LocalUser* user = *i;
			if ((user->registered == REG_ALL) && (!ext.get(user)))
			{
				SetExt(user);
			}
		}
	}

	~ModuleGeoIP()
	{
		if (gi)
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
			cc = SetExt(user);

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

	void OnSetUserIP(LocalUser* user)
	{
		// If user has sent NICK/USER, re-set the ExtItem as this is likely CGI:IRC changing the IP
		if (user->registered == REG_NICKUSER)
			SetExt(user);
	}

	ModResult OnStats(char symbol, User* user, string_list &out)
	{
		if (symbol != 'G')
			return MOD_RES_PASSTHRU;

		unsigned int unknown = 0;
		std::map<std::string, unsigned int> results;
		for (LocalUserList::const_iterator i = ServerInstance->Users->local_users.begin(); i != ServerInstance->Users->local_users.end(); ++i)
		{
			std::string* cc = ext.get(*i);
			if (cc)
				results[*cc]++;
			else
				unknown++;
		}

		std::string p = ServerInstance->Config->ServerName + " 801 " + user->nick + " :GeoIPSTATS ";
		for (std::map<std::string, unsigned int>::const_iterator i = results.begin(); i != results.end(); ++i)
		{
			out.push_back(p + i->first + " " + ConvToStr(i->second));
		}

		if (unknown)
			out.push_back(p + "Unknown " + ConvToStr(unknown));

		return MOD_RES_DENY;
	}
};

MODULE_INIT(ModuleGeoIP)

