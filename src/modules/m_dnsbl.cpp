/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2018-2020 Matt Schatz <genius3000@g3k.solutions>
 *   Copyright (C) 2018-2019 linuxdaemon <linuxdaemon.irc@gmail.com>
 *   Copyright (C) 2013, 2017-2022 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2013, 2015-2016 Adam <Adam@anope.org>
 *   Copyright (C) 2012-2016 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012, 2018 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2007 Craig Edwards <brain@inspircd.org>
 *   Copyright (C) 2006-2009 Robin Burchell <robin+git@viroteck.net>
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
#include "xline.h"
#include "modules/dns.h"
#include "modules/stats.h"

class DNSBLEntry final
{
public:
	enum class Action
		: uint8_t
	{
		// G-line users who's IP address is in the DNSBL.
		GLINE,

		// Kill users who's IP address is in the DNSBL.
		KILL,

		// K-line users who's IP address is in the DNSBL.
		KLINE,

		// Mark users who's IP address is in the DNSBL.
		MARK,

		// Z-line users who's IP address is in the DNSBL.
		ZLINE,
	};

	enum class Type
		: uint8_t
	{
		// DNSBL results will be compared against the specified bit mask.
		BITMASK,

		// DNSBL results will be compared against a numeric range of values.
		RECORD,
	};

	// The action to take against users who's IP address is in this DNSBL.
	Action action;

	// A bitmask of DNSBL result types to match against.
	unsigned int bitmask;

	// The domain name of this DNSBL.
	std::string domain;

	// If action is set to mark then a new hostname to set on users who's IP address is in this DNSBL.
	std::string markhost;

	// If action is set to mark then a new username (ident) to set on users who's IP address is in this DNSBL.
	std::string markident;

	// The human readable name of this DNSBL.
	std::string name;

	// A range of DNSBL result types to match against.
	CharState records;

	// The message to send to users who's IP address is in a DNSBL.
	std::string reason;

	// The number of seconds to wait for a DNSBL request before assuming it has failed.
	unsigned int timeout;

	// The type of result that this DNSBL will provide.
	Type type;

	// The number of errors which have occurred when querying this DNSBL.
	unsigned long stats_errors = 0;

	// The number of hits which have occurred when querying this DNSBL.
	unsigned long stats_hits = 0;

	// The number of misses which have occurred when querying this DNSBL.
	unsigned long stats_misses = 0;

	// If action is set to gline, kline, or zline then the duration for an X-line to last for.
	unsigned long xlineduration;

	DNSBLEntry(const Module* mod, std::shared_ptr<ConfigTag> tag)
	{
		name = tag->getString("name");
		if (name.empty())
			throw ModuleException(mod, "<dnsbl:name> can not be empty at " + tag->source.str());

		domain = tag->getString("domain");
		if (domain.empty())
			throw ModuleException(mod, "<dnsbl:domain> can not be empty at " + tag->source.str());

		action = tag->getEnum("action", Action::KILL, {
			{ "gline", Action::GLINE },
			{ "kill",  Action::KILL  },
			{ "kline", Action::KLINE },
			{ "mark",  Action::MARK  },
			{ "zline", Action::ZLINE },
		});

		const std::string typestr = tag->getString("type");
		if (stdalgo::string::equalsci(typestr, "bitmask"))
		{
			type = Type::BITMASK;

			bitmask = static_cast<unsigned int>(tag->getUInt("bitmask", 0, 0, UINT_MAX));
			records = 0;
		}
		else if (stdalgo::string::equalsci(typestr, "record"))
		{
			type = Type::RECORD;

			irc::portparser recordrange(tag->getString("records"), false);
			for (long record = 0; (record = recordrange.GetToken()); )
			{
				if (record < 0 || record > UCHAR_MAX)
					throw ModuleException(mod, "<dnsbl:records> can only hold records between 0 and 255 at " + tag->source.str());

				records.set(record);
			}
		}
		else
		{
			throw ModuleException(mod, typestr + " is an invalid value for <dnsbl:type>; acceptable values are 'bitmask' or 'records' at "
				+ tag->source.str());
		}

		reason = tag->getString("reason", "Your IP (%ip%) has been blacklisted by the %dnsbl% DNSBL.", 1, ServerInstance->Config->Limits.MaxLine);
		timeout = static_cast<unsigned int>(tag->getDuration("timeout", 0, 1, 60));
		markident = tag->getString("ident");
		markhost = tag->getString("host");
		xlineduration = tag->getDuration("duration", 60*60, 1);
	}
};

typedef ListExtItem<std::vector<std::string>> MarkExtItem;

// Data which is shared with DNS lookup classes.
class SharedData final
{
public:
	// Reference to the DNS manager.
	dynamic_reference<DNS::Manager> dns;

	// Counts the number of DNSBL lookups waiting for this user.
	IntExtItem countext;

	// The DNSBL marks which are set on a user.
	MarkExtItem markext;

	SharedData(Module* mod)
		: dns(mod, "DNS")
		, countext(mod, "dnsbl-pending", ExtensionType::USER)
		, markext(mod, "dnsbl-match", ExtensionType::USER)
	{
	}
};

class DNSBLResolver final
	: public DNS::Request
{
private:
	SharedData& data;
	irc::sockets::sockaddrs theirsa;
	std::string theiruid;
	std::shared_ptr<DNSBLEntry> ConfEntry;


	template <typename Line, typename... Extra>
	void AddLine(const char* type, const std::string& reason, unsigned long duration, Extra&&... extra)
	{
		auto line = new Line(ServerInstance->Time(), duration, MODNAME "@" + ServerInstance->Config->ServerName, reason, std::forward<Extra>(extra)...);
		if (!ServerInstance->XLines->AddLine(line, nullptr))
		{
			delete line;
			return;
		}

		ServerInstance->SNO.WriteToSnoMask('x', "%s added a timed %s on %s, expires in %s (on %s): %s",
			line->source.c_str(), type, line->Displayable().c_str(), InspIRCd::DurationString(line->duration).c_str(),
			InspIRCd::TimeString(line->expiry).c_str(), line->reason.c_str());
		ServerInstance->XLines->ApplyLines();
	}

public:
	DNSBLResolver(Module* me, SharedData& sd, const std::string& hostname, LocalUser* u, std::shared_ptr<DNSBLEntry> conf)
		: DNS::Request(*sd.dns, me, hostname, DNS::QUERY_A, true, conf->timeout)
		, data(sd)
		, theirsa(u->client_sa)
		, theiruid(u->uuid)
		, ConfEntry(conf)
	{
	}

	/* Note: This may be called multiple times for multiple A record results */
	void OnLookupComplete(const DNS::Query* r) override
	{
		/* Check the user still exists */
		LocalUser* them = IS_LOCAL(ServerInstance->Users.FindUUID(theiruid));
		if (!them || them->client_sa != theirsa)
		{
			ConfEntry->stats_misses++;
			return;
		}

		intptr_t i = data.countext.Get(them);
		if (i)
			data.countext.Set(them, i - 1);

		// The DNSBL reply must contain an A result.
		const DNS::ResourceRecord* const ans_record = r->FindAnswerOfType(DNS::QUERY_A);
		if (!ans_record)
		{
			ConfEntry->stats_errors++;
			ServerInstance->SNO.WriteGlobalSno('d', "%s returned an result with no IPv4 address.",
				ConfEntry->name.c_str());
			return;
		}

		// The DNSBL reply must be a valid IPv4 address.
		in_addr resultip;
		if (inet_pton(AF_INET, ans_record->rdata.c_str(), &resultip) != 1)
		{
			ConfEntry->stats_errors++;
			ServerInstance->SNO.WriteGlobalSno('d', "%s returned an invalid IPv4 address: %s",
				ConfEntry->name.c_str(), ans_record->rdata.c_str());
			return;
		}

		// The DNSBL reply should be in the 127.0.0.0/8 range.
		if ((resultip.s_addr & 0xFF) != 127)
		{
			ConfEntry->stats_errors++;
			ServerInstance->SNO.WriteGlobalSno('d', "%s returned an IPv4 address which is outside of the 127.0.0.0/8 subnet: %s",
				ConfEntry->name.c_str(), ans_record->rdata.c_str());
			return;
		}

		bool match = false;
		unsigned int result = 0;
		switch (ConfEntry->type)
		{
			case DNSBLEntry::Type::BITMASK:
			{
				result = (resultip.s_addr >> 24) & ConfEntry->bitmask;
				match = (result != 0);
				break;
			}
			case DNSBLEntry::Type::RECORD:
			{
				result = resultip.s_addr >> 24;
				match = (ConfEntry->records[result] == 1);
				break;
			}
		}

		if (match)
		{
			const std::string reason = Template::Replace(ConfEntry->reason, {
				{ "dnsbl",  ConfEntry->name     },
				{ "ip",     them->GetIPString() },
				{ "result", ConvToStr(result)   },
			});

			ConfEntry->stats_hits++;

			switch (ConfEntry->action)
			{
				case DNSBLEntry::Action::KILL:
				{
					ServerInstance->Users.QuitUser(them, "Killed (" + reason + ")");
					break;
				}
				case DNSBLEntry::Action::MARK:
				{
					if (!ConfEntry->markident.empty())
					{
						them->WriteNotice("Your ident has been set to " + ConfEntry->markident + " because you matched " + reason);
						them->ChangeIdent(ConfEntry->markident);
					}

					if (!ConfEntry->markhost.empty())
					{
						them->WriteNotice("Your host has been set to " + ConfEntry->markhost + " because you matched " + reason);
						them->ChangeDisplayedHost(ConfEntry->markhost);
					}

					data.markext.GetRef(them).push_back(ConfEntry->name);
					break;
				}
				case DNSBLEntry::Action::KLINE:
				{
					AddLine<KLine>("K-line", reason, ConfEntry->xlineduration, them->GetBanIdent(), them->GetIPString());
					break;
				}
				case DNSBLEntry::Action::GLINE:
				{
					AddLine<GLine>("G-line", reason, ConfEntry->xlineduration, them->GetBanIdent(), them->GetIPString());
					break;
				}
				case DNSBLEntry::Action::ZLINE:
				{
					AddLine<ZLine>("Z-line", reason, ConfEntry->xlineduration, them->GetIPString());
					break;
				}
			}

			ServerInstance->SNO.WriteGlobalSno('d', "Connecting user %s (%s) detected as being on the '%s' DNS blacklist with result %d",
				them->GetFullRealHost().c_str(), them->GetIPString().c_str(), ConfEntry->name.c_str(), result);
		}
		else
			ConfEntry->stats_misses++;
	}

	void OnError(const DNS::Query* q) override
	{
		bool is_miss = true;
		switch (q->error)
		{
			case DNS::ERROR_NO_RECORDS:
			case DNS::ERROR_DOMAIN_NOT_FOUND:
				ConfEntry->stats_misses++;
				break;

			default:
				ConfEntry->stats_errors++;
				is_miss = false;
				break;
		}

		LocalUser* them = IS_LOCAL(ServerInstance->Users.FindUUID(theiruid));
		if (!them || them->client_sa != theirsa)
			return;

		intptr_t i = data.countext.Get(them);
		if (i)
			data.countext.Set(them, i - 1);

		if (is_miss)
			return;

		ServerInstance->SNO.WriteGlobalSno('d', "An error occurred whilst checking whether %s (%s) is on the '%s' DNS blacklist: %s",
			them->GetFullRealHost().c_str(), them->GetIPString().c_str(), ConfEntry->name.c_str(), data.dns->GetErrorStr(q->error).c_str());
	}
};

typedef std::vector<std::shared_ptr<DNSBLEntry>> DNSBLEntries;

class ModuleDNSBL final
	: public Module
	, public Stats::EventListener
{
private:
	SharedData data;
	DNSBLEntries dnsbls;

public:
	ModuleDNSBL()
		: Module(VF_VENDOR, "Allows the server administrator to check the IP address of connecting users against a DNSBL.")
		, Stats::EventListener(this)
		, data(this)
	{
	}

	void init() override
	{
		ServerInstance->SNO.EnableSnomask('d', "DNSBL");
	}

	void Prioritize() override
	{
		Module* corexline = ServerInstance->Modules.Find("core_xline");
		ServerInstance->Modules.SetPriority(this, I_OnChangeRemoteAddress, PRIORITY_AFTER, corexline);
	}

	void ReadConfig(ConfigStatus& status) override
	{
		DNSBLEntries newdnsbls;
		for (const auto& [_, tag] : ServerInstance->Config->ConfTags("dnsbl"))
		{
			auto entry = std::make_shared<DNSBLEntry>(this, tag);
			newdnsbls.push_back(entry);
		}
		dnsbls.swap(newdnsbls);
	}

	void OnChangeRemoteAddress(LocalUser* user) override
	{
		if (user->exempt || user->quitting || !data.dns || !user->GetClass())
			return;

		// Clients can't be in a DNSBL if they aren't connected via IPv4 or IPv6.
		if (user->client_sa.family() != AF_INET && user->client_sa.family() != AF_INET6)
			return;

		if (!user->GetClass()->config->getBool("usednsbl", true))
			return;

		std::string reversedip;
		if (user->client_sa.family() == AF_INET)
		{
			unsigned int a = (unsigned int) user->client_sa.in4.sin_addr.s_addr & 0xFF;
			unsigned int b = (unsigned int) (user->client_sa.in4.sin_addr.s_addr >> 8) & 0xFF;
			unsigned int c = (unsigned int) (user->client_sa.in4.sin_addr.s_addr >> 16) & 0xFF;
			unsigned int d = (unsigned int) (user->client_sa.in4.sin_addr.s_addr >> 24) & 0xFF;

			reversedip = ConvToStr(d) + "." + ConvToStr(c) + "." + ConvToStr(b) + "." + ConvToStr(a);
		}
		else if (user->client_sa.family() == AF_INET6)
		{
			const unsigned char* ip = user->client_sa.in6.sin6_addr.s6_addr;

			const std::string buf = Hex::Encode(ip, 16);
			for (const auto& chr : insp::reverse_range(buf))
			{
				reversedip.push_back(chr);
				reversedip.push_back('.');
			}
			reversedip.erase(reversedip.length() - 1, 1);
		}
		else
			return;

		ServerInstance->Logs.Debug(MODNAME, "Reversed IP %s -> %s", user->GetIPString().c_str(), reversedip.c_str());

		data.countext.Set(user, dnsbls.size());

		// For each DNSBL, we will run through this lookup
		for (const auto& dnsbl : dnsbls)
		{
			// Fill hostname with a dnsbl style host (d.c.b.a.domain.tld)
			std::string hostname = reversedip + "." + dnsbl->domain;

			/* now we'd need to fire off lookups for `hostname'. */
			auto r = new DNSBLResolver(this, data, hostname, user, dnsbl);
			try
			{
				data.dns->Process(r);
			}
			catch (const DNS::Exception& ex)
			{
				delete r;
				ServerInstance->Logs.Debug(MODNAME, ex.GetReason());
			}

			if (user->quitting)
				break;
		}
	}

	ModResult OnSetConnectClass(LocalUser* user, const ConnectClass::Ptr& myclass) override
	{
		std::string dnsbl;
		if (!myclass->config->readString("dnsbl", dnsbl))
			return MOD_RES_PASSTHRU;

		MarkExtItem::List* match = data.markext.Get(user);
		if (!match)
		{
			ServerInstance->Logs.Debug("CONNECTCLASS", "The %s connect class is not suitable as it requires a DNSBL mark",
					myclass->GetName().c_str());
			return MOD_RES_DENY;
		}

		for (const auto& mark : *match)
		{
			if (InspIRCd::Match(mark, dnsbl))
				return MOD_RES_PASSTHRU;
		}

		const std::string marks = stdalgo::string::join(dnsbl);
		ServerInstance->Logs.Debug("CONNECTCLASS", "The %s connect class is not suitable as the DNSBL marks (%s) do not match %s",
				myclass->GetName().c_str(), marks.c_str(), dnsbl.c_str());
		return MOD_RES_DENY;
	}

	ModResult OnCheckReady(LocalUser* user) override
	{
		// Block until all of the DNSBL lookups are complete.
		return data.countext.Get(user) ? MOD_RES_DENY : MOD_RES_PASSTHRU;
	}

	ModResult OnStats(Stats::Context& stats) override
	{
		if (stats.GetSymbol() != 'd')
			return MOD_RES_PASSTHRU;

		unsigned long total_hits = 0;
		unsigned long total_misses = 0;
		unsigned long total_errors = 0;
		for (const auto& dnsbl : dnsbls)
		{
			total_hits += dnsbl->stats_hits;
			total_misses += dnsbl->stats_misses;
			total_errors += dnsbl->stats_errors;

			stats.AddGenericRow(InspIRCd::Format("The \"%s\" DNSBL had %lu hits, %lu misses, and %lu errors",
				dnsbl->name.c_str(), dnsbl->stats_hits, dnsbl->stats_misses, dnsbl->stats_errors));
		}

		stats.AddGenericRow("Total DNSBL hits: " + ConvToStr(total_hits));
		stats.AddGenericRow("Total DNSBL misses: " + ConvToStr(total_misses));
		stats.AddGenericRow("Total DNSBL errors: " + ConvToStr(total_errors));
		return MOD_RES_PASSTHRU;
	}
};

MODULE_INIT(ModuleDNSBL)
