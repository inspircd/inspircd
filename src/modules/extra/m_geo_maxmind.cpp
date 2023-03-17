/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2019-2022 Sadie Powell <sadie@witchery.services>
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

/// $CompilerFlags: require_version("libmaxminddb" "0" "1.2.1") warning("The version of libmaxminddb you are using may cause a segmentation fault if given a corrupt database file!")
/// $CompilerFlags: find_compiler_flags("libmaxminddb" "")

/// $LinkerFlags: find_linker_flags("libmaxminddb" "-lmaxminddb")

/// $PackageInfo: require_system("arch") libmaxminddb pkgconf
/// $PackageInfo: require_system("darwin") libmaxminddb pkg-config
/// $PackageInfo: require_system("debian" "9.0") libmaxminddb-dev pkg-config
/// $PackageInfo: require_system("ubuntu" "16.04") libmaxminddb-dev pkg-config

#ifdef _WIN32
# pragma comment(lib, "maxminddb.lib")
#endif

#include "inspircd.h"
#include "modules/geolocation.h"
#include <maxminddb.h>

class GeolocationExtItem : public ExtensionItem {
  public:
    GeolocationExtItem(Module* parent)
        : ExtensionItem("geolocation", ExtensionItem::EXT_USER, parent) {
    }

    std::string ToHuman(const Extensible* container,
                        void* item) const CXX11_OVERRIDE {
        Geolocation::Location* location = static_cast<Geolocation::Location*>(item);
        return location->GetName() + " [" + location->GetCode() + "]";
    }

    void free(Extensible* container, void* item) CXX11_OVERRIDE {
        Geolocation::Location* old = static_cast<Geolocation::Location*>(item);
        if (old) {
            old->refcount_dec();
        }
    }

    Geolocation::Location* get(const Extensible* item) const {
        return static_cast<Geolocation::Location*>(get_raw(item));
    }

    void set(Extensible* item, Geolocation::Location* value) {
        value->refcount_inc();
        free(item, set_raw(item, value));
    }

    void unset(Extensible* container) {
        free(container, unset_raw(container));
    }
};

typedef insp::flat_map<std::string, Geolocation::Location*> LocationMap;

class GeolocationAPIImpl : public Geolocation::APIBase {
  public:
    GeolocationExtItem ext;
    LocationMap locations;
    MMDB_s mmdb;

    GeolocationAPIImpl(Module* parent)
        : Geolocation::APIBase(parent)
        , ext(parent) {
    }

    Geolocation::Location* GetLocation(User* user) CXX11_OVERRIDE {
        // If we have the location cached then use that instead.
        Geolocation::Location* location = ext.get(user);
        if (location) {
            return location;
        }

        // Attempt to locate this user.
        location = GetLocation(user->client_sa);
        if (!location) {
            return NULL;
        }

        // We found the user. Cache their location for future use.
        ext.set(user, location);
        return location;
    }

    Geolocation::Location* GetLocation(irc::sockets::sockaddrs& sa) CXX11_OVERRIDE {
        // Skip trying to look up a UNIX socket.
        if (sa.family() != AF_INET && sa.family() != AF_INET6) {
            return NULL;
        }

        // Attempt to look up the socket address.
        int result;
        MMDB_lookup_result_s lookup = MMDB_lookup_sockaddr(&mmdb, &sa.sa, &result);
        if (result != MMDB_SUCCESS || !lookup.found_entry) {
            return NULL;
        }

        // Attempt to retrieve the country code.
        MMDB_entry_data_s country_code;
        result = MMDB_get_value(&lookup.entry, &country_code, "country", "iso_code", NULL);
        if (result != MMDB_SUCCESS || !country_code.has_data || country_code.type != MMDB_DATA_TYPE_UTF8_STRING || country_code.data_size != 2) {
            return NULL;
        }

        // If the country has been seen before then use our cached Location object.
        const std::string code(country_code.utf8_string, country_code.data_size);
        LocationMap::iterator liter = locations.find(code);
        if (liter != locations.end()) {
            return liter->second;
        }

        // Attempt to retrieve the country name.
        MMDB_entry_data_s country_name;
        result = MMDB_get_value(&lookup.entry, &country_name, "country", "names", "en", NULL);
        if (result != MMDB_SUCCESS || !country_name.has_data || country_name.type != MMDB_DATA_TYPE_UTF8_STRING) {
            return NULL;
        }

        // Create a Location object and cache it.
        const std::string cname(country_name.utf8_string, country_name.data_size);
        Geolocation::Location* location = new Geolocation::Location(code, cname);
        locations[code] = location;
        return location;
    }
};

class ModuleGeoMaxMind : public Module {
  private:
    GeolocationAPIImpl geoapi;

  public:
    ModuleGeoMaxMind()
        : geoapi(this) {
        memset(&geoapi.mmdb, 0, sizeof(geoapi.mmdb));
    }

    ~ModuleGeoMaxMind() {
        MMDB_close(&geoapi.mmdb);
    }

    Version GetVersion() CXX11_OVERRIDE {
        return Version("Allows the server to perform geolocation lookups on both IP addresses and users.", VF_VENDOR);
    }

    void ReadConfig(ConfigStatus& status) CXX11_OVERRIDE {
        ConfigTag* tag = ServerInstance->Config->ConfValue("maxmind");
        const std::string file = ServerInstance->Config->Paths.PrependConfig(tag->getString("file", "GeoLite2-Country.mmdb", 1));

        // Try to read the new database.
        MMDB_s mmdb;
        int result = MMDB_open(file.c_str(), MMDB_MODE_MMAP, &mmdb);
        if (result != MMDB_SUCCESS)
            throw ModuleException(InspIRCd::Format("Unable to load the MaxMind database (%s): %s",
                                                   file.c_str(), MMDB_strerror(result)));

        // Swap the new database with the old database.
        std::swap(mmdb, geoapi.mmdb);

        // Free the old database.
        MMDB_close(&mmdb);
    }

    void OnGarbageCollect() CXX11_OVERRIDE {
        for (LocationMap::iterator iter = geoapi.locations.begin(); iter != geoapi.locations.end(); ) {
            Geolocation::Location* location = iter->second;
            if (location->GetUseCount()) {
                ServerInstance->Logs->Log(MODNAME, LOG_DEBUG,
                                          "Preserving geolocation data for %s (%s) with use count %u... ",
                                          location->GetName().c_str(), location->GetCode().c_str(),
                                          location->GetUseCount());
                iter++;
            } else {
                ServerInstance->Logs->Log(MODNAME, LOG_DEBUG,
                                          "Deleting unused geolocation data for %s (%s)",
                                          location->GetName().c_str(), location->GetCode().c_str());
                delete location;
                iter = geoapi.locations.erase(iter);
            }
        }
    }

    void OnSetUserIP(LocalUser* user) CXX11_OVERRIDE {
        // Unset the extension so that the location of this user is looked
        // up again next time it is requested.
        geoapi.ext.unset(user);
    }
};

MODULE_INIT(ModuleGeoMaxMind)
