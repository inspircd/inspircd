/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2020 Matt Schatz <genius3000@g3k.solutions>
 *   Copyright (C) 2019, 2021 Sadie Powell <sadie@witchery.services>
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
#include "modules/extban.h"
#include "modules/geolocation.h"
#include "modules/whois.h"

class CountryExtBan
	: public ExtBan::MatchingBase
{
 private:
	Geolocation::API& geoapi;

 public:
	CountryExtBan(Module* Creator, Geolocation::API& api)
		: ExtBan::MatchingBase(Creator, "country", 'G')
		, geoapi(api)
	{
	}

	bool IsMatch(User* user, Channel* channel, const std::string& text) override
	{
		Geolocation::Location* location = geoapi ? geoapi->GetLocation(user) : NULL;
		const std::string code = location ? location->GetCode() : "XX";

		// Does this user match against the ban?
		return InspIRCd::Match(code, text);
	}
};

class ModuleGeoBan
	: public Module
	, public Whois::EventListener
{
 private:
	Geolocation::API geoapi;
	CountryExtBan extban;

 public:
	ModuleGeoBan()
		: Module(VF_VENDOR | VF_OPTCOMMON, "Adds extended ban G: which matches against two letter country codes.")
		, Whois::EventListener(this)
		, geoapi(this)
		, extban(this, geoapi)
	{
	}

	void OnWhois(Whois::Context& whois) override
	{
		if (whois.GetTarget()->server->IsService())
			return;

		Geolocation::Location* location = geoapi ? geoapi->GetLocation(whois.GetTarget()) : NULL;
		if (location)
			whois.SendLine(RPL_WHOISCOUNTRY, location->GetCode(), "is connecting from " + location->GetName());
		else
			whois.SendLine(RPL_WHOISCOUNTRY, "*", "is connecting from an unknown country");
	}
};

MODULE_INIT(ModuleGeoBan)
