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
/// $PackageInfo: require_system("debian") libgeoip-dev pkg-config
/// $PackageInfo: require_system("ubuntu") libgeoip-dev pkg-config

#include "inspircd.h"
#include "xline.h"
#include "modules/stats.h"
#include "modules/whois.h"

// Fix warnings about the use of commas at end of enumerator lists on C++03.
#if defined __clang__
# pragma clang diagnostic ignored "-Wc++11-extensions"
#elif defined __GNUC__
# if (__GNUC__ > 4) || ((__GNUC__ == 4) && (__GNUC_MINOR__ >= 8))
#  pragma GCC diagnostic ignored "-Wpedantic"
# else
#  pragma GCC diagnostic ignored "-pedantic"
# endif
#endif

#include <GeoIP.h>

#ifdef _WIN32
# pragma comment(lib, "GeoIP.lib")
#endif

enum
{
	// InspIRCd-specific.
	RPL_WHOISCOUNTRY = 344
};

class ModuleGeoIP : public Module, public Stats::EventListener, public Whois::EventListener
{
	StringExtItem ext;
	bool extban;
	GeoIP* ipv4db;
	GeoIP* ipv6db;

	std::string* SetExt(User* user)
	{
		const char* code = NULL;
		switch (user->client_sa.family())
		{
			case AF_INET:
				code = GeoIP_country_code_by_addr(ipv4db, user->GetIPString().c_str());
				break;

			case AF_INET6:
				code = GeoIP_country_code_by_addr_v6(ipv6db, user->GetIPString().c_str());
				break;
		}

		ext.set(user, code ? code : "UNK");
		return ext.get(user);
	}

 public:
	ModuleGeoIP()
		: Stats::EventListener(this)
		, Whois::EventListener(this)
		, ext("geoip_cc", ExtensionItem::EXT_USER, this)
		, extban(true)
		, ipv4db(NULL)
		, ipv6db(NULL)
	{
	}

	void init() CXX11_OVERRIDE
	{
		ipv4db = GeoIP_open_type(GEOIP_COUNTRY_EDITION, GEOIP_STANDARD);
		if (!ipv4db)
			throw ModuleException("Unable to load the IPv4 GeoIP database. Are you missing GeoIP.dat?");

		ipv6db = GeoIP_open_type(GEOIP_COUNTRY_EDITION_V6, GEOIP_STANDARD);
		if (!ipv6db)
			throw ModuleException("Unable to load the IPv6 GeoIP database. Are you missing GeoIPv6.dat?");

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
		if (ipv4db)
			GeoIP_delete(ipv4db);

		if (ipv6db)
			GeoIP_delete(ipv6db);
	}

	void ReadConfig(ConfigStatus&) CXX11_OVERRIDE
	{
		ConfigTag* tag = ServerInstance->Config->ConfValue("geoip");
		extban = tag->getBool("extban");
	}

	Version GetVersion() CXX11_OVERRIDE
	{
		return Version("Provides a way to assign users to connect classes by country using GeoIP lookup", VF_OPTCOMMON|VF_VENDOR);
	}

	void On005Numeric(std::map<std::string, std::string>& tokens) CXX11_OVERRIDE
	{
		if (extban)
			tokens["EXTBAN"].push_back('G');
	}

	ModResult OnCheckBan(User* user, Channel*, const std::string& mask) CXX11_OVERRIDE
	{
		if (extban && (mask.length() > 2) && (mask[0] == 'G') && (mask[1] == ':'))
		{
			std::string* cc = ext.get(user);
			if (!cc)
				cc = SetExt(user);

			if (InspIRCd::Match(*cc, mask.substr(2)))
				return MOD_RES_DENY;
		}
		return MOD_RES_PASSTHRU;
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

	void OnSetUserIP(LocalUser* user) CXX11_OVERRIDE
	{
		// If user has sent NICK/USER, re-set the ExtItem as this is likely CGI:IRC changing the IP
		if (user->registered == REG_NICKUSER)
			SetExt(user);
	}

	void OnWhois(Whois::Context& whois) CXX11_OVERRIDE
	{
		// If the extban is disabled we don't expose users location.
		if (!extban)
			return;

		std::string* cc = ext.get(whois.GetTarget());
		if (!cc)
			cc = SetExt(whois.GetTarget());

		whois.SendLine(RPL_WHOISCOUNTRY, *cc, "is located in this country");
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
