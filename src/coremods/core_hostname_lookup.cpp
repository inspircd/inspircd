/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2013-2016 Adam <Adam@anope.org>
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
#include "modules/dns.h"

namespace
{
	IntExtItem* dl;
}

/** Derived from Resolver, and performs user forward/reverse lookups.
 */
class UserResolver : public DNS::Request
{
 private:
	/** UUID we are looking up */
	const std::string uuid;

	/** Handles errors which happen during DNS resolution. */
	static void HandleError(LocalUser* user, const std::string& message)
	{
		user->WriteNotice("*** " + message + "; using your IP address (" + user->GetIPString() + ") instead.");

		bool display_is_real = user->GetDisplayedHost() == user->GetRealHost();
		user->ChangeRealHost(user->GetIPString(), display_is_real);

		dl->unset(user);
	}

 public:
	/** Create a resolver.
	 * @param mgr DNS Manager
	 * @param me this module
	 * @param user The user to begin lookup on
	 * @param to_resolve The IP or host to resolve
	 * @param qt The query type
	 */
	UserResolver(DNS::Manager* mgr, Module* me, LocalUser* user, const std::string& to_resolve, DNS::QueryType qt)
		: DNS::Request(mgr, me, to_resolve, qt)
		, uuid(user->uuid)
	{
	}

	/** Called on successful lookup
	 * if a previous result has already come back.
	 * @param r The finished query
	 */
	void OnLookupComplete(const DNS::Query* r) override
	{
		LocalUser* bound_user = IS_LOCAL(ServerInstance->FindUUID(uuid));
		if (!bound_user)
		{
			ServerInstance->Logs.Log(MODNAME, LOG_DEBUG, "Resolution finished for user '%s' who is gone", uuid.c_str());
			return;
		}

		const DNS::ResourceRecord* ans_record = r->FindAnswerOfType(this->question.type);
		if (ans_record == NULL)
		{
			HandleError(bound_user, "Could not resolve your hostname: No " + this->manager->GetTypeStr(this->question.type) + " records found");
			return;
		}

		ServerInstance->Logs.Log(MODNAME, LOG_DEBUG, "DNS %s result for %s: '%s' -> '%s'%s",
			this->manager->GetTypeStr(question.type).c_str(), uuid.c_str(),
			ans_record->name.c_str(), ans_record->rdata.c_str(),
			r->cached ? " (cached)" : "");

		if (this->question.type == DNS::QUERY_PTR)
		{
			UserResolver* res_forward;
			if (bound_user->client_sa.family() == AF_INET6)
			{
				/* IPV6 forward lookup */
				res_forward = new UserResolver(this->manager, this->creator, bound_user, ans_record->rdata, DNS::QUERY_AAAA);
			}
			else
			{
				/* IPV4 lookup */
				res_forward = new UserResolver(this->manager, this->creator, bound_user, ans_record->rdata, DNS::QUERY_A);
			}
			try
			{
				this->manager->Process(res_forward);
			}
			catch (DNS::Exception& e)
			{
				delete res_forward;
				ServerInstance->Logs.Log(MODNAME, LOG_DEBUG, "Error in resolver: " + e.GetReason());

				HandleError(bound_user, "There was an internal error resolving your host");
			}
		}
		else if (this->question.type == DNS::QUERY_A || this->question.type == DNS::QUERY_AAAA)
		{
			/* Both lookups completed */

			irc::sockets::sockaddrs* user_ip = &bound_user->client_sa;
			bool rev_match = false;
			if (user_ip->family() == AF_INET6)
			{
				struct in6_addr res_bin;
				if (inet_pton(AF_INET6, ans_record->rdata.c_str(), &res_bin))
				{
					rev_match = !memcmp(&user_ip->in6.sin6_addr, &res_bin, sizeof(res_bin));
				}
			}
			else
			{
				struct in_addr res_bin;
				if (inet_pton(AF_INET, ans_record->rdata.c_str(), &res_bin))
				{
					rev_match = !memcmp(&user_ip->in4.sin_addr, &res_bin, sizeof(res_bin));
				}
			}

			if (rev_match)
			{
				bound_user->WriteNotice("*** Found your hostname (" + this->question.name + (r->cached ? ") -- cached" : ")"));
				bound_user->ChangeRealHost(this->question.name, true);
				dl->unset(bound_user);
			}
			else
			{
				HandleError(bound_user, "Your hostname does not match up with your IP address");
			}
		}
	}

	/** Called on failed lookup
	 * @param query The errored query
	 */
	void OnError(const DNS::Query* query) override
	{
		LocalUser* bound_user = IS_LOCAL(ServerInstance->FindUUID(uuid));
		if (bound_user)
			HandleError(bound_user, "Could not resolve your hostname: " + this->manager->GetErrorStr(query->error));
	}
};

class ModuleHostnameLookup : public Module
{
 private:
	IntExtItem dnsLookup;
	dynamic_reference<DNS::Manager> DNS;

 public:
	ModuleHostnameLookup()
		: dnsLookup(this, "dnsLookup", ExtensionItem::EXT_USER)
		, DNS(this, "DNS")
	{
		dl = &dnsLookup;
	}

	void OnSetUserIP(LocalUser* user) override
	{
		// If core_dns is not loaded or hostname resolution is disabled for the user's
		// connect class then the logic in this function does not apply.
		if (!DNS || !user->MyClass->resolvehostnames)
			return;

		// Clients can't have a DNS hostname if they aren't connected via IPv4 or IPv6.
		if (user->client_sa.family() != AF_INET && user->client_sa.family() != AF_INET6)
			return;

		user->WriteNotice("*** Looking up your hostname...");

		UserResolver* res_reverse = new UserResolver(*this->DNS, this, user, user->GetIPString(), DNS::QUERY_PTR);
		try
		{
			/* If both the reverse and forward queries are cached, the user will be able to pass DNS completely
			 * before Process() completes, which is why dnsLookup.set() is here, before Process()
			 */
			this->dnsLookup.set(user, 1);
			this->DNS->Process(res_reverse);
		}
		catch (DNS::Exception& e)
		{
			this->dnsLookup.unset(user);
			delete res_reverse;
			ServerInstance->Logs.Log(MODNAME, LOG_DEBUG, "Error in resolver: " + e.GetReason());
		}
	}

	ModResult OnCheckReady(LocalUser* user) override
	{
		return this->dnsLookup.get(user) ? MOD_RES_DENY : MOD_RES_PASSTHRU;
	}

	Version GetVersion() override
	{
		return Version("Provides support for DNS lookups on connecting clients", VF_CORE|VF_VENDOR);
	}
};

MODULE_INIT(ModuleHostnameLookup)
