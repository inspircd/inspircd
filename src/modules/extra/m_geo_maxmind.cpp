/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2019-2023 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2019 Matt Schatz <genius3000@g3k.solutions>
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

/// $CompilerFlags: find_compiler_flags("libmaxminddb")
/// $LinkerFlags: find_linker_flags("libmaxminddb")

/// $PackageInfo: require_system("arch") libmaxminddb pkgconf
/// $PackageInfo: require_system("darwin") libmaxminddb pkg-config
/// $PackageInfo: require_system("debian" "9.0") libmaxminddb-dev pkg-config
/// $PackageInfo: require_system("ubuntu" "16.04") libmaxminddb-dev pkg-config

#ifdef _WIN32
# pragma comment(lib, "maxminddb.lib")
#endif

#include <maxminddb.h>

#include "inspircd.h"
#include "extension.h"
#include "modules/geolocation.h"

class GeolocationExtItem final
	: public ExtensionItem
{
public:
	GeolocationExtItem(Module* parent)
		: ExtensionItem(parent, "geolocation", ExtensionType::USER)
	{
	}

	std::string ToHuman(const Extensible* container, void* item) const noexcept override
	{
		Geolocation::Location* location = static_cast<Geolocation::Location*>(item);
		return location->GetName() + " [" + location->GetCode() + "]";
	}

	void Delete(Extensible* container, void* item) override
	{
		Geolocation::Location* old = static_cast<Geolocation::Location*>(item);
		if (old)
			old->refcount_dec();
	}

	Geolocation::Location* Get(const User* user) const
	{
		return static_cast<Geolocation::Location*>(GetRaw(user));
	}

	void Set(User* user, Geolocation::Location* value)
	{
		value->refcount_inc();
		Delete(user, SetRaw(user, value));
	}

	void Unset(User* user)
	{
		Delete(user, UnsetRaw(user));
	}
};

typedef insp::flat_map<std::string, Geolocation::Location*> LocationMap;

class GeolocationAPIImpl final
	: public Geolocation::APIBase
{
public:
	GeolocationExtItem ext;
	LocationMap locations;
	MMDB_s mmdb;

	GeolocationAPIImpl(Module* parent)
		: Geolocation::APIBase(parent)
		, ext(parent)
	{
	}

	Geolocation::Location* GetLocation(User* user) override
	{
		// If we have the location cached then use that instead.
		Geolocation::Location* location = ext.Get(user);
		if (location)
			return location;

		// Attempt to locate this user.
		location = GetLocation(user->client_sa);
		if (!location)
			return nullptr;

		// We found the user. Cache their location for future use.
		ext.Set(user, location);
		return location;
	}

	Geolocation::Location* GetLocation(irc::sockets::sockaddrs& sa) override
	{
		// Skip trying to look up a UNIX socket.
		if (!sa.is_ip())
			return nullptr;

		// Attempt to look up the socket address.
		int result;
		MMDB_lookup_result_s lookup = MMDB_lookup_sockaddr(&mmdb, &sa.sa, &result);
		if (result != MMDB_SUCCESS || !lookup.found_entry)
			return nullptr;

		// Attempt to retrieve the country code.
		MMDB_entry_data_s country_code;
		result = MMDB_get_value(&lookup.entry, &country_code, "country", "iso_code", nullptr);
		if (result != MMDB_SUCCESS || !country_code.has_data || country_code.type != MMDB_DATA_TYPE_UTF8_STRING || country_code.data_size != 2)
			return nullptr;

		// If the country has been seen before then use our cached Location object.
		const std::string code(country_code.utf8_string, country_code.data_size);
		LocationMap::iterator liter = locations.find(code);
		if (liter != locations.end())
			return liter->second;

		// Attempt to retrieve the country name.
		MMDB_entry_data_s country_name;
		result = MMDB_get_value(&lookup.entry, &country_name, "country", "names", "en", nullptr);
		if (result != MMDB_SUCCESS || !country_name.has_data || country_name.type != MMDB_DATA_TYPE_UTF8_STRING)
			return nullptr;

		// Create a Location object and cache it.
		const std::string cname(country_name.utf8_string, country_name.data_size);
		auto* location = new Geolocation::Location(code, cname);
		locations[code] = location;
		return location;
	}
};

class ModuleGeoMaxMind final
	: public Module
{
private:
	GeolocationAPIImpl geoapi;

public:
	ModuleGeoMaxMind()
		: Module(VF_VENDOR, "Allows the server to perform geolocation lookups on both IP addresses and users.")
		, geoapi(this)
	{
		memset(&geoapi.mmdb, 0, sizeof(geoapi.mmdb));
	}

	~ModuleGeoMaxMind() override
	{
		MMDB_close(&geoapi.mmdb);
	}

	void init() override
	{
		ServerInstance->Logs.Normal(MODNAME, "{} is running against libmaxminddb version {}",
			MODNAME, MMDB_lib_version());

	}

	void ReadConfig(ConfigStatus& status) override
	{
		const auto& tag = ServerInstance->Config->ConfValue("maxmind");
		const std::string file = ServerInstance->Config->Paths.PrependConfig(tag->getString("file", "GeoLite2-Country.mmdb", 1));

		// Try to read the new database.
		MMDB_s mmdb;
		int result = MMDB_open(file.c_str(), MMDB_MODE_MMAP, &mmdb);
		if (result != MMDB_SUCCESS)
			throw ModuleException(this, INSP_FORMAT("Unable to load the MaxMind database ({}): {}",
				file, MMDB_strerror(result)));

		// Swap the new database with the old database.
		std::swap(mmdb, geoapi.mmdb);

		// Free the old database.
		MMDB_close(&mmdb);
	}

	void OnGarbageCollect() override
	{
		for (LocationMap::iterator iter = geoapi.locations.begin(); iter != geoapi.locations.end(); )
		{
			Geolocation::Location* location = iter->second;
			if (location->GetUseCount())
			{
				ServerInstance->Logs.Debug(MODNAME, "Preserving geolocation data for {} ({}) with use count {}... ",
					location->GetName(), location->GetCode(), location->GetUseCount());
				iter++;
			}
			else
			{
				ServerInstance->Logs.Debug(MODNAME, "Deleting unused geolocation data for {} ({})",
					location->GetName(), location->GetCode());
				delete location;
				iter = geoapi.locations.erase(iter);
			}
		}
	}

	void OnChangeRemoteAddress(LocalUser* user) override
	{
		// Unset the extension so that the location of this user is looked
		// up again next time it is requested.
		geoapi.ext.Unset(user);
	}
};

MODULE_INIT(ModuleGeoMaxMind)
