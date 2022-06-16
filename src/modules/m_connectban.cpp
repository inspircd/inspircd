/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2019 Matt Schatz <genius3000@g3k.solutions>
 *   Copyright (C) 2014 Googolplexed <googol@googolplexed.net>
 *   Copyright (C) 2013, 2017-2021 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012-2014 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012, 2019 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2008, 2010 Craig Edwards <brain@inspircd.org>
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
#include "modules/webirc.h"

class ModuleConnectBan
	: public Module
	, public WebIRC::EventListener
{
	typedef std::map<irc::sockets::cidr_mask, unsigned int> ConnectMap;
	ConnectMap connects;
	unsigned int threshold;
	unsigned int banduration;
	unsigned int ipv4_cidr;
	unsigned int ipv6_cidr;
	std::string banmessage;

	unsigned char GetRange(LocalUser* user)
	{
		int family = user->client_sa.family();
		switch (family)
		{
			case AF_INET:
				return ipv4_cidr;

			case AF_INET6:
				return ipv6_cidr;

			case AF_UNIX:
				// Ranges for UNIX sockets are ignored entirely.
				return 0;
		}

		// If we have reached this point then we have encountered a bug.
		ServerInstance->Logs->Log(MODNAME, LOG_DEBUG, "BUG: ModuleConnectBan::GetRange(): socket type %d is unknown!", family);
		return 0;
	}

	static bool IsExempt(LocalUser* user)
	{
		// E-lined and already banned users shouldn't be hit.
		if (user->exempt || user->quitting)
			return true;

		// Users in an exempt class shouldn't be hit.
		return user->GetClass() && !user->GetClass()->config->getBool("useconnectban", true);
	}

 public:
	ModuleConnectBan()
		: WebIRC::EventListener(this)
	{
	}

	void Prioritize() CXX11_OVERRIDE
	{
		Module* corexline = ServerInstance->Modules->Find("core_xline");
		ServerInstance->Modules->SetPriority(this, I_OnSetUserIP, PRIORITY_AFTER, corexline);
	}

	Version GetVersion() CXX11_OVERRIDE
	{
		return Version("Z-lines IP addresses which make excessive connections to the server.", VF_VENDOR);
	}

	void ReadConfig(ConfigStatus& status) CXX11_OVERRIDE
	{
		ConfigTag* tag = ServerInstance->Config->ConfValue("connectban");

		ipv4_cidr = tag->getUInt("ipv4cidr", 32, 1, 32);
		ipv6_cidr = tag->getUInt("ipv6cidr", 128, 1, 128);
		threshold = tag->getUInt("threshold", 10, 1);
		banduration = tag->getDuration("duration", 10*60, 1);
		banmessage = tag->getString("banmessage", "Your IP range has been attempting to connect too many times in too short a duration. Wait a while, and you will be able to connect.");
	}

	void OnWebIRCAuth(LocalUser* user, const WebIRC::FlagMap* flags) CXX11_OVERRIDE
	{
		if (IsExempt(user))
			return;

		// HACK: Lower the connection attempts for the gateway IP address. The user
		// will be rechecked for connect spamming shortly after when their IP address
		// is changed and OnSetUserIP is called.
		irc::sockets::cidr_mask mask(user->client_sa, GetRange(user));
		ConnectMap::iterator iter = connects.find(mask);
		if (iter != connects.end() && iter->second)
			iter->second--;
	}

	void OnSetUserIP(LocalUser* u) CXX11_OVERRIDE
	{
		if (IsExempt(u))
			return;

		irc::sockets::cidr_mask mask(u->client_sa, GetRange(u));
		ConnectMap::iterator i = connects.find(mask);

		if (i != connects.end())
		{
			i->second++;

			if (i->second >= threshold)
			{
				// Create Z-line for set duration.
				ZLine* zl = new ZLine(ServerInstance->Time(), banduration, ServerInstance->Config->ServerName, banmessage, mask.str());
				if (!ServerInstance->XLines->AddLine(zl, NULL))
				{
					delete zl;
					return;
				}
				ServerInstance->XLines->ApplyLines();
				std::string maskstr = mask.str();
				ServerInstance->SNO->WriteGlobalSno('x', "Z-line added by module m_connectban on %s to expire in %s (on %s): Connect flooding",
					maskstr.c_str(), InspIRCd::DurationString(zl->duration).c_str(), InspIRCd::TimeString(zl->expiry).c_str());
				ServerInstance->SNO->WriteGlobalSno('a', "Connect flooding from IP range %s (%d)", maskstr.c_str(), threshold);
				connects.erase(i);
			}
		}
		else
		{
			connects[mask] = 1;
		}
	}

	void OnGarbageCollect() CXX11_OVERRIDE
	{
		ServerInstance->Logs->Log(MODNAME, LOG_DEBUG, "Clearing map.");
		connects.clear();
	}
};

MODULE_INIT(ModuleConnectBan)
