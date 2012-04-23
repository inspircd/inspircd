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

/* $ModDesc: Throttles the connections of any users who try connect flood */

class ModuleConnectBan : public Module
{
 private:
	clonemap connects;
	unsigned int threshold;
	unsigned int banduration;
	unsigned int ipv4_cidr;
	unsigned int ipv6_cidr;
 public:
	ModuleConnectBan(InspIRCd* Me) : Module(Me)
	{
		Implementation eventlist[] = { I_OnUserConnect, I_OnGarbageCollect, I_OnRehash };
		ServerInstance->Modules->Attach(eventlist, this, 3);
		OnRehash(NULL);
	}

	virtual ~ModuleConnectBan()
	{
	}

	virtual Version GetVersion()
	{
		return Version("$Id$", VF_VENDOR,API_VERSION);
	}

	virtual void OnRehash(User* user)
	{
		ConfigReader Conf(ServerInstance);
		std::string duration;

		ipv4_cidr = Conf.ReadInteger("connectban", "ipv4cidr", 0, true);
		if (ipv4_cidr == 0)
			ipv4_cidr = 32;

		ipv6_cidr = Conf.ReadInteger("connectban", "ipv6cidr", 0, true);
		if (ipv6_cidr == 0)
			ipv6_cidr = 128;

		threshold = Conf.ReadInteger("connectban", "threshold", 0, true);

		if (threshold == 0)
			threshold = 10;

		duration = Conf.ReadValue("connectban", "duration", 0, true);

		if (duration.empty())
			duration = "10m";

		banduration = ServerInstance->Duration(duration);
	}

	virtual void OnUserConnect(User *u)
	{
		int range = 32;
		clonemap::iterator i;

		switch (u->GetProtocolFamily())
		{
	#ifdef SUPPORT_IP6LINKS
			case AF_INET6:
			{
				range = ipv6_cidr;
			}
			break;
	#endif
			case AF_INET:
			{
				range = ipv4_cidr;
			}
			break;
		}

		i = connects.find(u->GetCIDRMask(range));

		if (i != connects.end())
		{
			i->second++;

			if (i->second >= threshold)
			{
				// Create zline for set duration.
				ZLine* zl = new ZLine(ServerInstance, ServerInstance->Time(), banduration, ServerInstance->Config->ServerName, "Your IP range has been attempting to connect too many times in too short a duration. Wait a while, and you will be able to connect.", u->GetCIDRMask(range));
				if (ServerInstance->XLines->AddLine(zl,NULL))
					ServerInstance->XLines->ApplyLines();
				else
					delete zl;

				ServerInstance->SNO->WriteToSnoMask('x', "Connect flooding from IP range %s (%d)", u->GetCIDRMask(range), threshold);
				connects.erase(i);
			}
		}
		else
		{
			connects[u->GetCIDRMask(range)] = 1;
		}
	}

	virtual void OnGarbageCollect()
	{
		ServerInstance->Logs->Log("m_connectban",DEBUG, "Clearing map.");
		connects.clear();
	}
};

MODULE_INIT(ModuleConnectBan)
