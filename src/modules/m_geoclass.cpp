/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2019-2020 Sadie Powell <sadie@witchery.services>
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
#include "modules/geolocation.h"
#include "modules/stats.h"

enum {
    // InspIRCd-specific.
    RPL_STATSCOUNTRY = 801
};

class ModuleGeoClass
    : public Module
    , public Stats::EventListener {
  private:
    Geolocation::API geoapi;

  public:
    ModuleGeoClass()
        : Stats::EventListener(this)
        , geoapi(this) {
    }

    Version GetVersion() CXX11_OVERRIDE {
        return Version("Allows the server administrator to assign users to connect classes by the country they are connecting from.", VF_VENDOR);
    }

    ModResult OnSetConnectClass(LocalUser* user,
                                ConnectClass* myclass) CXX11_OVERRIDE {
        const std::string country = myclass->config->getString("country");
        if (country.empty()) {
            return MOD_RES_PASSTHRU;
        }

        // If we can't find the location of this user then we can't assign
        // them to a location-specific connect class.
        Geolocation::Location* location = geoapi ? geoapi->GetLocation(user) : NULL;
        const std::string code = location ? location->GetCode() : "XX";

        irc::spacesepstream codes(country);
        for (std::string token; codes.GetToken(token); ) {
            // If the user matches this country code then they can use this
            // connect class.
            if (stdalgo::string::equalsci(token, code)) {
                return MOD_RES_PASSTHRU;
            }
        }

        // A list of country codes were specified but the user didn't match
        // any of them.
        ServerInstance->Logs->Log("CONNECTCLASS", LOG_DEBUG, "The %s connect class is not suitable as the origin country (%s) is not any of %s",
                                  myclass->GetName().c_str(), code.c_str(), country.c_str());
        return MOD_RES_DENY;
    }

    ModResult OnStats(Stats::Context& stats) CXX11_OVERRIDE {
        if (stats.GetSymbol() != 'G') {
            return MOD_RES_PASSTHRU;
        }

        // Counter for the number of users in each country.
        typedef std::map<Geolocation::Location*, size_t> CountryCounts;
        CountryCounts counts;

        // Counter for the number of users in an unknown country.
        size_t unknown = 0;

        const UserManager::LocalList& list = ServerInstance->Users.GetLocalUsers();
        for (UserManager::LocalList::const_iterator iter = list.begin(); iter != list.end(); ++iter) {
            Geolocation::Location* location = geoapi ? geoapi->GetLocation(*iter) : NULL;
            if (location) {
                counts[location]++;
            } else {
                unknown++;
            }
        }

        for (CountryCounts::const_iterator iter = counts.begin(); iter != counts.end(); ++iter) {
            Geolocation::Location* location = iter->first;
            stats.AddRow(RPL_STATSCOUNTRY, iter->second, location->GetCode(),
                         location->GetName());
        }

        if (unknown) {
            stats.AddRow(RPL_STATSCOUNTRY, unknown, "*", "Unknown Country");
        }

        return MOD_RES_DENY;
    }
};

MODULE_INIT(ModuleGeoClass)
