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

/// $CompilerFlags: find_compiler_flags("geoip" "")
/// $LinkerFlags: find_linker_flags("geoip" "-lGeoIP")

/// $PackageInfo: require_system("centos" "7.0") GeoIP-devel pkgconfig
/// $PackageInfo: require_system("darwin") geoip pkg-config
/// $PackageInfo: require_system("ubuntu") libgeoip-dev pkg-config

#include "inspircd.h"
#include "xline.h"

// Fix warnings about the use of commas at end of enumerator lists on C++03.
#if defined __clang__
# pragma clang diagnostic ignored "-Wc++11-extensions"
#elif defined __GNUC__
# pragma GCC diagnostic ignored "-pedantic"
#endif

#include <GeoIP.h>

#ifdef _WIN32
# pragma comment(lib, "GeoIP.lib")
#endif

class ModuleGeoIP : public Module
{
	LocalStringExt ext;
	GeoIP* gi;

	std::string* SetExt(LocalUser* user)
	{
		const char* c = GeoIP_country_code_by_addr(gi, user->GetIPString().c_str());
		if (!c)
			c = "UNK";

		std::string* cc = new std::string(c);
		ext.set(user, cc);
		return cc;
	}

 public:
	ModuleGeoIP()
		: ext("geoip_cc", ExtensionItem::EXT_USER, this)
		, gi(NULL)
	{
	}

	void init() CXX11_OVERRIDE
	{
		gi = GeoIP_new(GEOIP_STANDARD);
		if (gi == NULL)
				throw ModuleException("Unable to initialize geoip, are you missing GeoIP.dat?");

		const UserManager::LocalList& list = ServerInstance->Users.GetLocalUsers();
		for (UserManager::LocalList::const_iterator i = list.begin(); i != list.end(); ++i)
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

	Version GetVersion() CXX11_OVERRIDE
	{
		return Version("Provides a way to assign users to connect classes by country using GeoIP lookup", VF_VENDOR);
	}

	ModResult OnSetConnectClass(LocalUser* user, ConnectClass* myclass) CXX11_OVERRIDE
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

	ModResult OnStats(Stats::Context& stats) CXX11_OVERRIDE
	{
		if (stats.GetSymbol() != 'G')
			return MOD_RES_PASSTHRU;

		unsigned int unknown = 0;
		std::map<std::string, unsigned int> results;

		const UserManager::LocalList& list = ServerInstance->Users.GetLocalUsers();
		for (UserManager::LocalList::const_iterator i = list.begin(); i != list.end(); ++i)
		{
			std::string* cc = ext.get(*i);
			if (cc)
				results[*cc]++;
			else
				unknown++;
		}

		for (std::map<std::string, unsigned int>::const_iterator i = results.begin(); i != results.end(); ++i)
		{
			stats.AddRow(801, "GeoIPSTATS " + i->first + " " + ConvToStr(i->second));
		}

		if (unknown)
			stats.AddRow(801, "GeoIPSTATS Unknown " + ConvToStr(unknown));

		return MOD_RES_DENY;
	}
};

MODULE_INIT(ModuleGeoIP)
