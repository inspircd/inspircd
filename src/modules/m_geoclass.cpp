/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2019-2021, 2023 Sadie Powell <sadie@witchery.services>
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
#include "utility/string.h"

class ModuleGeoClass final
	: public Module
	, public Stats::EventListener
{
private:
	Geolocation::API geoapi;

public:
	ModuleGeoClass()
		: Module(VF_VENDOR, "Allows the server administrator to assign users to connect classes by the country they are connecting from.")
		, Stats::EventListener(this)
		, geoapi(this)
	{
	}

	ModResult OnPreChangeConnectClass(LocalUser* user, const std::shared_ptr<ConnectClass>& klass, std::optional<Numeric::Numeric>& errnum) override
	{
		const std::string country = klass->config->getString("country");
		if (country.empty())
			return MOD_RES_PASSTHRU;

		// If we can't find the location of this user then we can't assign
		// them to a location-specific connect class.
		Geolocation::Location* location = geoapi ? geoapi->GetLocation(user) : nullptr;
		const std::string code = location ? location->GetCode() : "XX";

		irc::spacesepstream codes(country);
		for (std::string token; codes.GetToken(token); )
		{
			if (token.length() != 2)
			{
				ServerInstance->Logs.Debug("CONNECTCLASS", "The {} connect class contains an invalid country code: {}",
					klass->GetName(), token);
				continue;
			}

			// If the user matches this country code then they can use this
			// connect class.
			if (insp::equalsci(token, code))
				return MOD_RES_PASSTHRU;
		}

		// A list of country codes were specified but the user didn't match
		// any of them.
		ServerInstance->Logs.Debug("CONNECTCLASS", "The {} connect class is not suitable as the origin country ({}) is not any of {}.",
			klass->GetName(), code, country);
		return MOD_RES_DENY;
	}

	ModResult OnStats(Stats::Context& stats) override
	{
		if (stats.GetSymbol() != 'G')
			return MOD_RES_PASSTHRU;

		// Counter for the number of users in each country.
		std::map<Geolocation::Location*, size_t> counts;

		// Counter for the number of users in an unknown country.
		size_t unknown = 0;

		for (auto* user : ServerInstance->Users.GetLocalUsers())
		{
			Geolocation::Location* location = geoapi ? geoapi->GetLocation(user) : nullptr;
			if (location)
				counts[location]++;
			else
				unknown++;
		}

		for (const auto& [location, count] : counts)
		{
			stats.AddGenericRow(fmt::format("{} ({}): {}", location->GetName(),
				location->GetCode(), count));
		}

		if (unknown)
			stats.AddGenericRow(fmt::format("Unknown Country: {}", unknown));

		return MOD_RES_DENY;
	}
};

MODULE_INIT(ModuleGeoClass)
