/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2008 Robin Burchell <robin+git@viroteck.net>
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

/* $ModDesc: Throttles the connections of IP ranges who try to connect flood. */

class ModuleConnectBan : public Module
{
 private:
	clonemap connects;
	unsigned int threshold;
	unsigned int banduration;
	unsigned int ipv4_cidr;
	unsigned int ipv6_cidr;
 public:
	void init()
	{
		Implementation eventlist[] = { I_OnUserConnect, I_OnGarbageCollect };
		ServerInstance->Modules->Attach(eventlist, this, sizeof(eventlist)/sizeof(Implementation));
	}

	virtual ~ModuleConnectBan()
	{
	}

	virtual Version GetVersion()
	{
		return Version("Throttles the connections of IP ranges who try to connect flood.", VF_VENDOR);
	}

	void ReadConfig(ConfigReadStatus&)
	{
		std::string duration;

		ipv4_cidr = ServerInstance->Config->GetTag("connectban")->getInt("ipv4cidr");
		if (ipv4_cidr == 0)
			ipv4_cidr = 32;

		ipv6_cidr = ServerInstance->Config->GetTag("connectban")->getInt("ipv6cidr");
		if (ipv6_cidr == 0)
			ipv6_cidr = 128;

		threshold = ServerInstance->Config->GetTag("connectban")->getInt("threshold");

		if (threshold == 0)
			threshold = 10;

		duration = ServerInstance->Config->GetTag("connectban")->getString("duration");

		if (duration.empty())
			duration = "10m";

		banduration = ServerInstance->Duration(duration);
	}

	virtual void OnUserConnect(LocalUser *u)
	{
		int range = 32;
		clonemap::iterator i;

		switch (u->client_sa.sa.sa_family)
		{
			case AF_INET6:
				range = ipv6_cidr;
			break;
			case AF_INET:
				range = ipv4_cidr;
			break;
		}

		irc::sockets::cidr_mask mask(u->client_sa, range);
		i = connects.find(mask);

		if (i != connects.end())
		{
			i->second++;

			if (i->second >= threshold)
			{
				// Create zline for set duration.
				ZLine* zl = new ZLine(ServerInstance->Time(), banduration, ServerInstance->Config->ServerName, "Your IP range has been attempting to connect too many times in too short a duration. Wait a while, and you will be able to connect.", mask.str());
				if (ServerInstance->XLines->AddLine(zl,NULL))
					ServerInstance->XLines->ApplyLines();
				else
					delete zl;

				ServerInstance->SNO->WriteGlobalSno('x',"Module m_connectban added Z:line on *@%s to expire on %s: Connect flooding", 
					mask.str().c_str(), ServerInstance->TimeString(zl->expiry).c_str());
				ServerInstance->SNO->WriteGlobalSno('a', "Connect flooding from IP range %s (%d)", mask.str().c_str(), threshold);
				connects.erase(i);
			}
		}
		else
		{
			connects[mask] = 1;
		}
	}

	virtual void OnGarbageCollect()
	{
		ServerInstance->Logs->Log("m_connectban",DEBUG, "Clearing map.");
		connects.clear();
	}
};

MODULE_INIT(ModuleConnectBan)
