/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2013, 2018-2023 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2013, 2016 Adam <Adam@anope.org>
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


#ifndef _WIN32
# include <arpa/inet.h>
#endif

#include "inspircd.h"
#include "extension.h"
#include "modules/dns.h"

namespace
{
	BoolExtItem* dl;
}

class UserResolver
	: public DNS::Request
{
protected:
	// The socket address that the associated user was connected from at the time of lookup.
	const irc::sockets::sockaddrs sa;

	// The UUID of the user we are looking up the hostname of.
	const std::string uuid;

	UserResolver(DNS::Manager* mgr, Module* me, LocalUser* user, const std::string& to_resolve, DNS::QueryType qt)
		: DNS::Request(mgr, me, to_resolve, qt)
		, sa(user->client_sa)
		, uuid(user->uuid)
	{
	}

	// Handles errors which happen during DNS resolution.
	static void HandleError(LocalUser* user, const std::string& message)
	{
		user->WriteNotice("*** " + message + "; using your IP address (" + user->GetAddress() + ") instead.");

		bool display_is_real = user->GetDisplayedHost() == user->GetRealHost();
		user->ChangeRealHost(user->GetAddress(), display_is_real);

		dl->Unset(user);
	}

	/** Logs the result of a DNS lookup. */
	inline void LogLookup(const DNS::ResourceRecord& rr, bool cached) const
	{
		ServerInstance->Logs.Debug(MODNAME, "DNS {} result for {}: '{}' -> '{}'{}",
			manager->GetTypeStr(question.type), uuid, rr.name,
			rr.rdata, cached ? " (cached)" : "");
	}

public:
	void OnError(const DNS::Query* query) override
	{
		LocalUser* user = ServerInstance->Users.FindUUID<LocalUser>(uuid);
		if (user && user->client_sa == sa)
			HandleError(user, "Could not resolve your hostname: " + this->manager->GetErrorStr(query->error));
	}
};

// Handles checking the retrieved PTR record against its A/AAAA records.
class UserIPResolver final
	: public UserResolver
{
public:
	UserIPResolver(DNS::Manager* mgr, Module* me, LocalUser* user, const std::string& host)
		: UserResolver(mgr, me, user, host, user->client_sa.family() == AF_INET6 ? DNS::QUERY_AAAA : DNS::QUERY_A)
	{
	}

	void OnLookupComplete(const DNS::Query* query) override
	{
		LocalUser* user = ServerInstance->Users.FindUUID<LocalUser>(uuid);
		if (!user || user->client_sa != sa)
			return;

		bool hasrecord = false;
		bool matches = false;
		const DNS::QueryType qt = user->client_sa.family() == AF_INET6 ? DNS::QUERY_AAAA : DNS::QUERY_A;
		for (const auto& rr : query->answers)
		{
			if (rr.type != qt)
				continue;

			// We've seen an A/AAAA record.
			hasrecord = true;

			switch (user->client_sa.family())
			{
				case AF_INET:
				{
					// Does this result match the user's IPv4 address?
					struct in_addr v4addr;
					if (inet_pton(AF_INET, rr.rdata.c_str(), &v4addr) == 1)
						matches = !memcmp(&user->client_sa.in4.sin_addr, &v4addr, sizeof(v4addr));
					break;
				}

				case AF_INET6:
				{
					// Does this result match the user's IPv6 address?
					struct in6_addr v6addr;
					if (inet_pton(AF_INET6, rr.rdata.c_str(), &v6addr) == 1)
						matches = !memcmp(&user->client_sa.in6.sin6_addr, &v6addr, sizeof(v6addr));
					break;
				}
			}

			// If we've found a valid match then stop processing records.
			if (matches)
			{
				LogLookup(rr, query->cached);
				break;
			}
		}

		if (matches)
		{
			// We have found the hostname for the user.
			user->WriteNotice("*** Found your hostname (" + this->question.name + (query->cached ? ") -- cached" : ")"));
			bool display_is_real = user->GetDisplayedHost() == user->GetRealHost();
			user->ChangeRealHost(this->question.name, display_is_real);
			dl->Unset(user);
		}
		else if (hasrecord)
		{
			// The hostname has A/AAAA records but none of them are the IP address of the user.
			HandleError(user, "Your hostname does not match up with your IP address");
		}
		else
		{
			// The hostname has no A/AAAA records.
			HandleError(user, "Could not resolve your hostname: No " + this->manager->GetTypeStr(this->question.type) + " records found");
		}
	}
};

// Handles retrieving the PTR record for users.
class UserHostResolver final
	: public UserResolver
{
public:
	UserHostResolver(DNS::Manager* mgr, Module* me, LocalUser* user)
		: UserResolver(mgr, me, user, user->GetAddress(), DNS::QUERY_PTR)
	{
	}

	void OnLookupComplete(const DNS::Query* query) override
	{
		LocalUser* user = ServerInstance->Users.FindUUID<LocalUser>(uuid);
		if (!user || user->client_sa != sa)
			return;

		// An IP can have multiple PTR records but it is considered bad practise to have
		// more than one so to simplify the lookup logic we only use the first.
		const DNS::ResourceRecord* rr = query->FindAnswerOfType(DNS::QUERY_PTR);
		if (!rr)
		{
			HandleError(user, "Could not resolve your hostname: No " + this->manager->GetTypeStr(this->question.type) + " records found");
			return;
		}

		LogLookup(*rr, query->cached);
		auto* res_forward = new UserIPResolver(this->manager, this->creator, user, rr->rdata);
		try
		{
			this->manager->Process(res_forward);
		}
		catch (const DNS::Exception& e)
		{
			delete res_forward;
			ServerInstance->Logs.Debug(MODNAME, "Error in resolver: " + e.GetReason());

			HandleError(user, "There was an internal error resolving your host");
		}
	}
};

class ModuleHostnameLookup final
	: public Module
{
private:
	BoolExtItem dnsLookup;
	DNS::ManagerRef DNS;

public:
	ModuleHostnameLookup()
		: Module(VF_CORE | VF_VENDOR, "Provides support for DNS lookups on connecting clients")
		, dnsLookup(this, "dns-lookup", ExtensionType::USER)
		, DNS(this)
	{
		dl = &dnsLookup;
	}

	void OnChangeRemoteAddress(LocalUser* user) override
	{
		// If core_dns is not loaded or hostname resolution is disabled for the user's
		// connect class then the logic in this function does not apply.
		if (!DNS || user->quitting || !user->GetClass()->resolvehostnames)
			return;

		// Clients can't have a DNS hostname if they aren't connected via IPv4 or IPv6.
		if (!user->client_sa.is_ip())
			return;

		user->WriteNotice("*** Looking up your hostname...");

		auto* res_reverse = new UserHostResolver(*this->DNS, this, user);
		try
		{
			/* If both the reverse and forward queries are cached, the user will be able to pass DNS completely
			 * before Process() completes, which is why dnsLookup.set() is here, before Process()
			 */
			this->dnsLookup.Set(user);
			this->DNS->Process(res_reverse);
		}
		catch (const DNS::Exception& e)
		{
			this->dnsLookup.Unset(user);
			delete res_reverse;
			ServerInstance->Logs.Debug(MODNAME, "Error in resolver: " + e.GetReason());
		}
	}

	ModResult OnCheckReady(LocalUser* user) override
	{
		return this->dnsLookup.Get(user) ? MOD_RES_DENY : MOD_RES_PASSTHRU;
	}
};

MODULE_INIT(ModuleHostnameLookup)
