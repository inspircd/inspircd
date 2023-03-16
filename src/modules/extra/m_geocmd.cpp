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

/// $ModAuthor: Sadie Powell
/// $ModAuthorMail: sadie@witchery.services
/// $ModDesc: Provides the /GEOLOCATE command which performs Geolocation lookups on arbitrary IP addresses.
/// $ModDepends: core 3


#include "inspircd.h"
#include "modules/geolocation.h"

class CommandGeolocate
	: public SplitCommand
{
 private:
	Geolocation::API geoapi;

 public:
	CommandGeolocate(Module* Creator)
		: SplitCommand(Creator, "GEOLOCATE", 1)
		, geoapi(Creator)
	{
		allow_empty_last_param = false;
		flags_needed = 'o';
		syntax = "<ipaddr> [<ipaddr>]+";
	}

	CmdResult HandleLocal(LocalUser* user, const Params& parameters) CXX11_OVERRIDE
	{
		irc::sockets::sockaddrs sa;
		for (Command::Params::const_iterator iter = parameters.begin(); iter != parameters.end(); ++iter)
		{
			// Try to parse the address.
			const std::string& address = *iter;
			if (!irc::sockets::aptosa(address, 0, sa))
			{
				user->WriteNotice("*** GEOLOCATE: " + address + " is not a valid IP address!");
				continue;
			}

			// Try to geolocate the IP address.
			Geolocation::Location* location = geoapi ? geoapi->GetLocation(sa) : NULL;
			if (!location)
			{
				user->WriteNotice("*** GEOLOCATE: " + sa.addr() + " could not be geolocated!");
				continue;
			}

			user->WriteNotice(InspIRCd::Format("*** GEOLOCATE: %s is located in %s (%s).", sa.addr().c_str(),
				location->GetName().c_str(), location->GetCode().c_str()));
		}
		return CMD_SUCCESS;
	}
};

class ModuleGeoCommand
	: public Module
{
 private:
	CommandGeolocate cmd;

 public:
	ModuleGeoCommand()
		: cmd(this)
	{
	}

	Version GetVersion() CXX11_OVERRIDE
	{
		return Version("Provides the /GEOLOCATE command which performs Geolocation lookups on arbitrary IP addresses");
	}
};

MODULE_INIT(ModuleGeoCommand)
