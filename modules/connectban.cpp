/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2021 Dominic Hamon
 *   Copyright (C) 2019 Matt Schatz <genius3000@g3k.solutions>
 *   Copyright (C) 2014 Googolplexed <googol@googolplexed.net>
 *   Copyright (C) 2013, 2019-2023 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012-2014 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012, 2019 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2008 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2008 Craig Edwards <brain@inspircd.org>
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
#include "modules/server.h"
#include "modules/webirc.h"
#include "timeutils.h"
#include "xline.h"

class ModuleConnectBan final
	: public Module
	, public ServerProtocol::LinkEventListener
	, public WebIRC::EventListener
{
private:
	typedef std::map<irc::sockets::cidr_mask, unsigned int> ConnectMap;

	ConnectMap connects;
	unsigned long threshold;
	unsigned long banduration;
	unsigned int ipv4_cidr;
	unsigned int ipv6_cidr;
	unsigned long bootwait;
	unsigned long splitwait;
	time_t ignoreuntil = 0;
	std::string banmessage;

	unsigned char GetRange(LocalUser* user) const
	{
		sa_family_t family = user->client_sa.family();
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
		ServerInstance->Logs.Debug(MODNAME, "BUG: ModuleConnectBan::GetRange(): socket type {} is unknown!", family);
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
		: Module(VF_VENDOR, "Z-lines IP addresses which make excessive connections to the server.")
		, ServerProtocol::LinkEventListener(this)
		, WebIRC::EventListener(this)
	{
	}

	void Prioritize() override
	{
		Module* corexline = ServerInstance->Modules.Find("core_xline");
		ServerInstance->Modules.SetPriority(this, I_OnChangeRemoteAddress, PRIORITY_AFTER, corexline);
	}

	void ReadConfig(ConfigStatus& status) override
	{
		const auto& tag = ServerInstance->Config->ConfValue("connectban");

		ipv4_cidr = tag->getNum<unsigned int>("ipv4cidr", ServerInstance->Config->IPv4Range, 1, 32);
		ipv6_cidr = tag->getNum<unsigned int>("ipv6cidr", ServerInstance->Config->IPv6Range, 1, 128);
		threshold = tag->getNum<unsigned long>("threshold", 10, 1);
		bootwait = tag->getDuration("bootwait", 60*2);
		splitwait = tag->getDuration("splitwait", 60*2);
		banduration = tag->getDuration("banduration", 6*60*60, 1);
		banmessage = tag->getString("banmessage", "Your IP range has been attempting to connect too many times in too short a duration. Wait a while, and you will be able to connect.");

		if (status.initial)
			ignoreuntil = ServerInstance->Time() + bootwait;
	}

	void OnWebIRCAuth(LocalUser* user, const WebIRC::FlagMap* flags) override
	{
		if (IsExempt(user))
			return;

		// HACK: Lower the connection attempts for the gateway IP address. The user
		// will be rechecked for connect spamming shortly after when their IP address
		// is changed and OnChangeRemoteAddress is called.
		irc::sockets::cidr_mask mask(user->client_sa, GetRange(user));
		ConnectMap::iterator iter = connects.find(mask);
		if (iter != connects.end() && iter->second)
			iter->second--;
	}

	void OnServerSplit(const Server* server, bool error) override
	{
		if (splitwait)
			ignoreuntil = std::max<time_t>(ignoreuntil, ServerInstance->Time() + splitwait);
	}

	void OnChangeRemoteAddress(LocalUser* u) override
	{
		if (IsExempt(u) || ignoreuntil > ServerInstance->Time())
			return;

		irc::sockets::cidr_mask mask(u->client_sa, GetRange(u));
		ConnectMap::iterator i = connects.find(mask);

		if (i != connects.end())
		{
			i->second++;

			if (i->second >= threshold)
			{
				// If an IPv6 address begins with a colon then expand it
				// slightly to avoid breaking the server protocol.
				std::string maskstr = mask.str();
				if (maskstr[0] == ':')
					maskstr.insert(maskstr.begin(), 1, '0');

				// Create Z-line for set duration.
				auto* zl = new ZLine(ServerInstance->Time(), banduration, MODNAME "@" + ServerInstance->Config->ServerName, banmessage, maskstr);
				if (!ServerInstance->XLines->AddLine(zl, nullptr))
				{
					delete zl;
					return;
				}

				ServerInstance->SNO.WriteToSnoMask('x', "{} added a timed Z-line on {}, expires in {} (on {}): {}",
					zl->source, maskstr, Duration::ToString(zl->duration),
					Time::ToString(zl->expiry), zl->reason);
				ServerInstance->SNO.WriteGlobalSno('a', "Connect flooding from IP range {} ({})", maskstr, threshold);
				connects.erase(i);
				ServerInstance->XLines->ApplyLines();
			}
		}
		else
		{
			connects[mask] = 1;
		}
	}

	void OnGarbageCollect() override
	{
		ServerInstance->Logs.Debug(MODNAME, "Clearing map.");
		connects.clear();
	}
};

MODULE_INIT(ModuleConnectBan)
