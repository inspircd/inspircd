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
	LocalIntExt* dl;
	LocalStringExt* ph;
}

/** Derived from Resolver, and performs user forward/reverse lookups.
 */
class UserResolver : public DNS::Request
{
	/** UUID we are looking up */
	const std::string uuid;

	/** True if the lookup is forward, false if is a reverse lookup
	 */
	const bool fwd;

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
		, fwd(qt == DNS::QUERY_A || qt == DNS::QUERY_AAAA)
	{
	}

	/** Called on successful lookup
	 * if a previous result has already come back.
	 * @param r The finished query
	 */
	void OnLookupComplete(const DNS::Query* r)
	{
		LocalUser* bound_user = (LocalUser*)ServerInstance->FindUUID(uuid);
		if (!bound_user)
		{
			ServerInstance->Logs->Log(MODNAME, LOG_DEBUG, "Resolution finished for user '%s' who is gone", uuid.c_str());
			return;
		}

		const DNS::ResourceRecord* ans_record = r->FindAnswerOfType(this->question.type);
		if (ans_record == NULL)
		{
			OnError(r);
			return;
		}

		ServerInstance->Logs->Log(MODNAME, LOG_DEBUG, "DNS result for %s: '%s' -> '%s'", uuid.c_str(), ans_record->name.c_str(), ans_record->rdata.c_str());

		if (!fwd)
		{
			// first half of resolution is done. We now need to verify that the host matches.
			ph->set(bound_user, ans_record->rdata);

			UserResolver* res_forward;
			if (bound_user->client_sa.sa.sa_family == AF_INET6)
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
				ServerInstance->Logs->Log(MODNAME, LOG_DEBUG, "Error in resolver: " + e.GetReason());

				bound_user->WriteNotice("*** There was an internal error resolving your host, using your IP address (" + bound_user->GetIPString() + ") instead.");
				dl->set(bound_user, 0);
			}
		}
		else
		{
			/* Both lookups completed */

			irc::sockets::sockaddrs* user_ip = &bound_user->client_sa;
			bool rev_match = false;
			if (user_ip->sa.sa_family == AF_INET6)
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

			dl->set(bound_user, 0);

			if (rev_match)
			{
				std::string* hostname = ph->get(bound_user);

				if (hostname == NULL)
				{
					ServerInstance->Logs->Log(MODNAME, LOG_DEFAULT, "ERROR: User has no hostname attached when doing a forward lookup");
					bound_user->WriteNotice("*** There was an internal error resolving your host, using your IP address (" + bound_user->GetIPString() + ") instead.");
					return;
				}
				else if (hostname->length() <= ServerInstance->Config->Limits.MaxHost)
				{
					/* Hostnames starting with : are not a good thing (tm) */
					if ((*hostname)[0] == ':')
						hostname->insert(0, "0");

					bound_user->WriteNotice("*** Found your hostname (" + *hostname + (r->cached ? ") -- cached" : ")"));
					bound_user->host.assign(*hostname, 0, ServerInstance->Config->Limits.MaxHost);
					bound_user->dhost = bound_user->host;

					/* Invalidate cache */
					bound_user->InvalidateCache();
				}
				else
				{
					bound_user->WriteNotice("*** Your hostname is longer than the maximum of " + ConvToStr(ServerInstance->Config->Limits.MaxHost) + " characters, using your IP address (" + bound_user->GetIPString() + ") instead.");
				}

				ph->unset(bound_user);
			}
			else
			{
				bound_user->WriteNotice("*** Your hostname does not match up with your IP address. Sorry, using your IP address (" + bound_user->GetIPString() + ") instead.");
			}
		}
	}

	/** Called on failed lookup
	 * @param query The errored query
	 */
	void OnError(const DNS::Query* query)
	{
		LocalUser* bound_user = (LocalUser*)ServerInstance->FindUUID(uuid);
		if (bound_user)
		{
			bound_user->WriteNotice("*** Could not resolve your hostname: " + this->manager->GetErrorStr(query->error) + "; using your IP address (" + bound_user->GetIPString() + ") instead.");
			dl->set(bound_user, 0);
		}
	}
};

class ModuleHostnameLookup : public Module
{
	LocalIntExt dnsLookup;
	LocalStringExt ptrHosts;
	dynamic_reference<DNS::Manager> DNS;

 public:
	ModuleHostnameLookup()
		: dnsLookup("dnsLookup", ExtensionItem::EXT_USER, this)
		, ptrHosts("ptrHosts", ExtensionItem::EXT_USER, this)
		, DNS(this, "DNS")
	{
		dl = &dnsLookup;
		ph = &ptrHosts;
	}

	void OnUserInit(LocalUser *user)
	{
		if (!DNS || !user->MyClass->resolvehostnames)
		{
			user->WriteNotice("*** Skipping host resolution (disabled by server administrator)");
			return;
		}

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
			this->dnsLookup.set(user, 0);
			delete res_reverse;
			ServerInstance->Logs->Log(MODNAME, LOG_DEBUG, "Error in resolver: " + e.GetReason());
		}
	}

	ModResult OnCheckReady(LocalUser* user)
	{
		return this->dnsLookup.get(user) ? MOD_RES_DENY : MOD_RES_PASSTHRU;
	}

	Version GetVersion()
	{
		return Version("Provides support for DNS lookups on connecting clients", VF_CORE|VF_VENDOR);
	}
};

MODULE_INIT(ModuleHostnameLookup)
