/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2019 Sadie Powell <sadie@witchery.services>
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
#include "modules/whois.h"

enum
{
	// InspIRCd-specific.
	RPL_WHOISCOUNTRY = 344
};

class ModuleGeoBan
	: public Module
	, public Whois::EventListener
{
 private:
	Geolocation::API geoapi;

 public:
	ModuleGeoBan()
		: Whois::EventListener(this)
		, geoapi(this)
	{
	}

	Version GetVersion() CXX11_OVERRIDE
	{
		return Version("Provides a way to ban users by country", VF_OPTCOMMON|VF_VENDOR);
	}

	void On005Numeric(std::map<std::string, std::string>& tokens) CXX11_OVERRIDE
	{
		tokens["EXTBAN"].push_back('G');
	}

	ModResult OnCheckBan(User* user, Channel*, const std::string& mask) CXX11_OVERRIDE
	{
		if ((mask.length() > 2) && (mask[0] == 'G') && (mask[1] == ':'))
		{
			Geolocation::Location* location = geoapi ? geoapi->GetLocation(user) : NULL;
			const std::string code = location ? location->GetCode() : "XX";

			// Does this user match against the ban?
			if (InspIRCd::Match(code, mask.substr(2)))
				return MOD_RES_DENY;
		}
		return MOD_RES_PASSTHRU;
	}

	void OnWhois(Whois::Context& whois) CXX11_OVERRIDE
	{
		Geolocation::Location* location = geoapi ? geoapi->GetLocation(whois.GetTarget()) : NULL;
		if (location)
			whois.SendLine(RPL_WHOISCOUNTRY, location->GetCode(), "is connecting from " + location->GetName());
		else
			whois.SendLine(RPL_WHOISCOUNTRY, "*", "is connecting from an unknown country");
	}
};

MODULE_INIT(ModuleGeoBan)
