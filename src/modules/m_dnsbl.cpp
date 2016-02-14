/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2006-2008 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2007 Craig Edwards <craigedwards@brainbox.cc>
 *   Copyright (C) 2006-2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2007 John Brooks <john.brooks@dereferenced.net>
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
#include "modules/dns.h"

/* Class holding data for a single entry */
class DNSBLConfEntry : public refcountbase
{
	public:
		enum EnumBanaction { I_UNKNOWN, I_KILL, I_ZLINE, I_KLINE, I_GLINE, I_MARK };
		enum EnumType { A_RECORD, A_BITMASK };
		std::string name, ident, host, domain, reason;
		EnumBanaction banaction;
		EnumType type;
		long duration;
		int bitmask;
		unsigned char records[256];
		unsigned long stats_hits, stats_misses;
		DNSBLConfEntry(): type(A_BITMASK),duration(86400),bitmask(0),stats_hits(0), stats_misses(0) {}
};


/** Resolver for CGI:IRC hostnames encoded in ident/GECOS
 */
class DNSBLResolver : public DNS::Request
{
	std::string theiruid;
	LocalStringExt& nameExt;
	LocalIntExt& countExt;
	reference<DNSBLConfEntry> ConfEntry;

 public:

	DNSBLResolver(DNS::Manager *mgr, Module *me, LocalStringExt& match, LocalIntExt& ctr, const std::string &hostname, LocalUser* u, reference<DNSBLConfEntry> conf)
		: DNS::Request(mgr, me, hostname, DNS::QUERY_A, true), theiruid(u->uuid), nameExt(match), countExt(ctr), ConfEntry(conf)
	{
	}

	/* Note: This may be called multiple times for multiple A record results */
	void OnLookupComplete(const DNS::Query *r) CXX11_OVERRIDE
	{
		/* Check the user still exists */
		LocalUser* them = (LocalUser*)ServerInstance->FindUUID(theiruid);
		if (!them)
			return;

		const DNS::ResourceRecord* const ans_record = r->FindAnswerOfType(DNS::QUERY_A);
		if (!ans_record)
			return;

		// All replies should be in 127.0.0.0/8
		if (ans_record->rdata.compare(0, 4, "127.") != 0)
		{
			ServerInstance->SNO->WriteGlobalSno('a', "DNSBL: %s returned address outside of acceptable subnet 127.0.0.0/8: %s", ConfEntry->domain.c_str(), ans_record->rdata.c_str());
			ConfEntry->stats_misses++;
			return;
		}

		int i = countExt.get(them);
		if (i)
			countExt.set(them, i - 1);

		// Now we calculate the bitmask: 256*(256*(256*a+b)+c)+d

		unsigned int bitmask = 0, record = 0;
		bool match = false;
		in_addr resultip;

		inet_pton(AF_INET, ans_record->rdata.c_str(), &resultip);

		switch (ConfEntry->type)
		{
			case DNSBLConfEntry::A_BITMASK:
				bitmask = resultip.s_addr >> 24; /* Last octet (network byte order) */
				bitmask &= ConfEntry->bitmask;
				match = (bitmask != 0);
			break;
			case DNSBLConfEntry::A_RECORD:
				record = resultip.s_addr >> 24; /* Last octet */
				match = (ConfEntry->records[record] == 1);
			break;
		}

		if (match)
		{
			std::string reason = ConfEntry->reason;
			std::string::size_type x = reason.find("%ip%");
			while (x != std::string::npos)
			{
				reason.erase(x, 4);
				reason.insert(x, them->GetIPString());
				x = reason.find("%ip%");
			}

			ConfEntry->stats_hits++;

			switch (ConfEntry->banaction)
			{
				case DNSBLConfEntry::I_KILL:
				{
					ServerInstance->Users->QuitUser(them, "Killed (" + reason + ")");
					break;
				}
				case DNSBLConfEntry::I_MARK:
				{
					if (!ConfEntry->ident.empty())
					{
						them->WriteNumeric(304, "Your ident has been set to " + ConfEntry->ident + " because you matched " + reason);
						them->ChangeIdent(ConfEntry->ident);
					}

					if (!ConfEntry->host.empty())
					{
						them->WriteNumeric(304, "Your host has been set to " + ConfEntry->host + " because you matched " + reason);
						them->ChangeDisplayedHost(ConfEntry->host);
					}

					nameExt.set(them, ConfEntry->name);
					break;
				}
				case DNSBLConfEntry::I_KLINE:
				{
					KLine* kl = new KLine(ServerInstance->Time(), ConfEntry->duration, ServerInstance->Config->ServerName.c_str(), reason.c_str(),
							"*", them->GetIPString());
					if (ServerInstance->XLines->AddLine(kl,NULL))
					{
						std::string timestr = InspIRCd::TimeString(kl->expiry);
						ServerInstance->SNO->WriteGlobalSno('x',"K:line added due to DNSBL match on *@%s to expire on %s: %s",
							them->GetIPString().c_str(), timestr.c_str(), reason.c_str());
						ServerInstance->XLines->ApplyLines();
					}
					else
					{
						delete kl;
						return;
					}
					break;
				}
				case DNSBLConfEntry::I_GLINE:
				{
					GLine* gl = new GLine(ServerInstance->Time(), ConfEntry->duration, ServerInstance->Config->ServerName.c_str(), reason.c_str(),
							"*", them->GetIPString());
					if (ServerInstance->XLines->AddLine(gl,NULL))
					{
						std::string timestr = InspIRCd::TimeString(gl->expiry);
						ServerInstance->SNO->WriteGlobalSno('x',"G:line added due to DNSBL match on *@%s to expire on %s: %s",
							them->GetIPString().c_str(), timestr.c_str(), reason.c_str());
						ServerInstance->XLines->ApplyLines();
					}
					else
					{
						delete gl;
						return;
					}
					break;
				}
				case DNSBLConfEntry::I_ZLINE:
				{
					ZLine* zl = new ZLine(ServerInstance->Time(), ConfEntry->duration, ServerInstance->Config->ServerName.c_str(), reason.c_str(),
							them->GetIPString());
					if (ServerInstance->XLines->AddLine(zl,NULL))
					{
						std::string timestr = InspIRCd::TimeString(zl->expiry);
						ServerInstance->SNO->WriteGlobalSno('x',"Z:line added due to DNSBL match on %s to expire on %s: %s",
							them->GetIPString().c_str(), timestr.c_str(), reason.c_str());
						ServerInstance->XLines->ApplyLines();
					}
					else
					{
						delete zl;
						return;
					}
					break;
				}
				case DNSBLConfEntry::I_UNKNOWN:
				default:
					break;
			}

			ServerInstance->SNO->WriteGlobalSno('a', "Connecting user %s%s detected as being on a DNS blacklist (%s) with result %d", them->nick.empty() ? "<unknown>" : "", them->GetFullRealHost().c_str(), ConfEntry->domain.c_str(), (ConfEntry->type==DNSBLConfEntry::A_BITMASK) ? bitmask : record);
		}
		else
			ConfEntry->stats_misses++;
	}

	void OnError(const DNS::Query *q) CXX11_OVERRIDE
	{
		LocalUser* them = (LocalUser*)ServerInstance->FindUUID(theiruid);
		if (!them)
			return;

		int i = countExt.get(them);
		if (i)
			countExt.set(them, i - 1);

		if (q->error == DNS::ERROR_NO_RECORDS || q->error == DNS::ERROR_DOMAIN_NOT_FOUND)
			ConfEntry->stats_misses++;
	}
};

class ModuleDNSBL : public Module
{
	std::vector<reference<DNSBLConfEntry> > DNSBLConfEntries;
	dynamic_reference<DNS::Manager> DNS;
	LocalStringExt nameExt;
	LocalIntExt countExt;

	/*
	 *	Convert a string to EnumBanaction
	 */
	DNSBLConfEntry::EnumBanaction str2banaction(const std::string &action)
	{
		if(action.compare("KILL")==0)
			return DNSBLConfEntry::I_KILL;
		if(action.compare("KLINE")==0)
			return DNSBLConfEntry::I_KLINE;
		if(action.compare("ZLINE")==0)
			return DNSBLConfEntry::I_ZLINE;
		if(action.compare("GLINE")==0)
			return DNSBLConfEntry::I_GLINE;
		if(action.compare("MARK")==0)
			return DNSBLConfEntry::I_MARK;

		return DNSBLConfEntry::I_UNKNOWN;
	}
 public:
	ModuleDNSBL()
		: DNS(this, "DNS")
		, nameExt("dnsbl_match", ExtensionItem::EXT_USER, this)
		, countExt("dnsbl_pending", ExtensionItem::EXT_USER, this)
	{
	}

	Version GetVersion() CXX11_OVERRIDE
	{
		return Version("Provides handling of DNS blacklists", VF_VENDOR);
	}

	/** Fill our conf vector with data
	 */
	void ReadConfig(ConfigStatus& status) CXX11_OVERRIDE
	{
		DNSBLConfEntries.clear();

		ConfigTagList dnsbls = ServerInstance->Config->ConfTags("dnsbl");
		for(ConfigIter i = dnsbls.first; i != dnsbls.second; ++i)
		{
			ConfigTag* tag = i->second;
			reference<DNSBLConfEntry> e = new DNSBLConfEntry();

			e->name = tag->getString("name");
			e->ident = tag->getString("ident");
			e->host = tag->getString("host");
			e->reason = tag->getString("reason");
			e->domain = tag->getString("domain");

			if (tag->getString("type") == "bitmask")
			{
				e->type = DNSBLConfEntry::A_BITMASK;
				e->bitmask = tag->getInt("bitmask");
			}
			else
			{
				memset(e->records, 0, sizeof(e->records));
				e->type = DNSBLConfEntry::A_RECORD;
				irc::portparser portrange(tag->getString("records"), false);
				long item = -1;
				while ((item = portrange.GetToken()))
					e->records[item] = 1;
			}

			e->banaction = str2banaction(tag->getString("action"));
			e->duration = tag->getDuration("duration", 60, 1);

			/* Use portparser for record replies */

			/* yeah, logic here is a little messy */
			if ((e->bitmask <= 0) && (DNSBLConfEntry::A_BITMASK == e->type))
			{
				std::string location = tag->getTagLocation();
				ServerInstance->SNO->WriteGlobalSno('a', "DNSBL(%s): invalid bitmask", location.c_str());
			}
			else if (e->name.empty())
			{
				std::string location = tag->getTagLocation();
				ServerInstance->SNO->WriteGlobalSno('a', "DNSBL(%s): Invalid name", location.c_str());
			}
			else if (e->domain.empty())
			{
				std::string location = tag->getTagLocation();
				ServerInstance->SNO->WriteGlobalSno('a', "DNSBL(%s): Invalid domain", location.c_str());
			}
			else if (e->banaction == DNSBLConfEntry::I_UNKNOWN)
			{
				std::string location = tag->getTagLocation();
				ServerInstance->SNO->WriteGlobalSno('a', "DNSBL(%s): Invalid banaction", location.c_str());
			}
			else
			{
				if (e->reason.empty())
				{
					std::string location = tag->getTagLocation();
					ServerInstance->SNO->WriteGlobalSno('a', "DNSBL(%s): empty reason, using defaults", location.c_str());
					e->reason = "Your IP has been blacklisted.";
				}

				/* add it, all is ok */
				DNSBLConfEntries.push_back(e);
			}
		}
	}

	void OnSetUserIP(LocalUser* user) CXX11_OVERRIDE
	{
		if ((user->exempt) || !DNS)
			return;

		if (user->MyClass)
		{
			if (!user->MyClass->config->getBool("usednsbl", true))
				return;
		}
		else
			ServerInstance->Logs->Log(MODNAME, LOG_DEBUG, "User has no connect class in OnSetUserIP");

		std::string reversedip;
		if (user->client_sa.sa.sa_family == AF_INET)
		{
			unsigned int a, b, c, d;
			d = (unsigned int) (user->client_sa.in4.sin_addr.s_addr >> 24) & 0xFF;
			c = (unsigned int) (user->client_sa.in4.sin_addr.s_addr >> 16) & 0xFF;
			b = (unsigned int) (user->client_sa.in4.sin_addr.s_addr >> 8) & 0xFF;
			a = (unsigned int) user->client_sa.in4.sin_addr.s_addr & 0xFF;

			reversedip = ConvToStr(d) + "." + ConvToStr(c) + "." + ConvToStr(b) + "." + ConvToStr(a);
		}
		else if (user->client_sa.sa.sa_family == AF_INET6)
		{
			const unsigned char* ip = user->client_sa.in6.sin6_addr.s6_addr;

			std::string buf = BinToHex(ip, 16);
			for (std::string::const_reverse_iterator it = buf.rbegin(); it != buf.rend(); ++it)
			{
				reversedip.push_back(*it);
				reversedip.push_back('.');
			}
		}
		else
			return;

		ServerInstance->Logs->Log(MODNAME, LOG_DEBUG, "Reversed IP %s -> %s", user->GetIPString().c_str(), reversedip.c_str());

		countExt.set(user, DNSBLConfEntries.size());

		// For each DNSBL, we will run through this lookup
		for (unsigned i = 0; i < DNSBLConfEntries.size(); ++i)
		{
			// Fill hostname with a dnsbl style host (d.c.b.a.domain.tld)
			std::string hostname = reversedip + "." + DNSBLConfEntries[i]->domain;

			/* now we'd need to fire off lookups for `hostname'. */
			DNSBLResolver *r = new DNSBLResolver(*this->DNS, this, nameExt, countExt, hostname, user, DNSBLConfEntries[i]);
			try
			{
				this->DNS->Process(r);
			}
			catch (DNS::Exception &ex)
			{
				delete r;
				ServerInstance->Logs->Log(MODNAME, LOG_DEBUG, ex.GetReason());
			}

			if (user->quitting)
				break;
		}
	}

	ModResult OnSetConnectClass(LocalUser* user, ConnectClass* myclass) CXX11_OVERRIDE
	{
		std::string dnsbl;
		if (!myclass->config->readString("dnsbl", dnsbl))
			return MOD_RES_PASSTHRU;
		std::string* match = nameExt.get(user);
		std::string myname = match ? *match : "";
		if (dnsbl == myname)
			return MOD_RES_PASSTHRU;
		return MOD_RES_DENY;
	}

	ModResult OnCheckReady(LocalUser *user) CXX11_OVERRIDE
	{
		if (countExt.get(user))
			return MOD_RES_DENY;
		return MOD_RES_PASSTHRU;
	}

	ModResult OnStats(Stats::Context& stats) CXX11_OVERRIDE
	{
		if (stats.GetSymbol() != 'd')
			return MOD_RES_PASSTHRU;

		unsigned long total_hits = 0, total_misses = 0;

		for (std::vector<reference<DNSBLConfEntry> >::const_iterator i = DNSBLConfEntries.begin(); i != DNSBLConfEntries.end(); ++i)
		{
			total_hits += (*i)->stats_hits;
			total_misses += (*i)->stats_misses;

			stats.AddRow(304, "DNSBLSTATS DNSbl \"" + (*i)->name + "\" had " +
					ConvToStr((*i)->stats_hits) + " hits and " + ConvToStr((*i)->stats_misses) + " misses");
		}

		stats.AddRow(304, "DNSBLSTATS Total hits: " + ConvToStr(total_hits));
		stats.AddRow(304, "DNSBLSTATS Total misses: " + ConvToStr(total_misses));

		return MOD_RES_PASSTHRU;
	}
};

MODULE_INIT(ModuleDNSBL)
