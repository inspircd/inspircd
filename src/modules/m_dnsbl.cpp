/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2018-2020 Matt Schatz <genius3000@g3k.solutions>
 *   Copyright (C) 2018-2019 linuxdaemon <linuxdaemon.irc@gmail.com>
 *   Copyright (C) 2013, 2017-2023 Sadie Powell <sadie@witchery.services>
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
#include "modules/dns.h"
#include "modules/shun.h"
#include "modules/stats.h"
#include "numerichelper.h"
#include "timeutils.h"
#include "xline.h"

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

		// Shun users who's IP address is in the DNSBL.
		SHUN,

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

	// If action is set to mark then a new username to set on users who's IP address is in this DNSBL.
	std::string markuser;

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

	DNSBLEntry(const Module* mod, const std::shared_ptr<ConfigTag>& tag)
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
			{ "shun",  Action::SHUN  },
			{ "zline", Action::ZLINE },
		});

		const std::string typestr = tag->getString("type");
		if (stdalgo::string::equalsci(typestr, "bitmask"))
		{
			type = Type::BITMASK;

			bitmask = tag->getNum<unsigned int>("bitmask", 0);
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
		markuser = tag->getString("user", tag->getString("ident"));
		markhost = tag->getString("host");
		xlineduration = tag->getDuration("duration", 60*60, 1);
	}
};

class DNSBLMask final
{
public:
	// The username to give a user because they were in a DNSBL (<dnsbl:user>).
	std::string user;

	// The hostname to give a user because they were in a DNSBL (<dnsbl:host>).
	std::string host;

	// The reason why the user was given this user@host (<dnsbl:reason>).
	std::string reason;

	DNSBLMask(const std::shared_ptr<DNSBLEntry>& cfg, const std::string& msg)
		: user(cfg->markuser)
		, host(cfg->markhost)
		, reason(msg)
	{
	}
};

typedef std::vector<std::shared_ptr<DNSBLEntry>> DNSBLEntries;
typedef SimpleExtItem<DNSBLMask> MaskExtItem;
typedef ListExtItem<std::vector<std::string>> MarkExtItem;

// Data which is shared with DNS lookup classes.
class SharedData final
{
private:
	std::string ReverseIP(const irc::sockets::sockaddrs& sa)
	{
		switch (sa.family())
		{
			case AF_INET:
			{
				auto a = (unsigned int) sa.in4.sin_addr.s_addr & 0xFF;
				auto b = (unsigned int) (sa.in4.sin_addr.s_addr >> 8) & 0xFF;
				auto c = (unsigned int) (sa.in4.sin_addr.s_addr >> 16) & 0xFF;
				auto d = (unsigned int) (sa.in4.sin_addr.s_addr >> 24) & 0xFF;
				return INSP_FORMAT("{}.{}.{}.{}", d, c, b, a);
			}

			case AF_INET6:
			{
				const std::string hexaddr = Hex::Encode(sa.in6.sin6_addr.s6_addr, 16);
				std::string reversedip;
				reversedip.reserve(hexaddr.length() * 2);
				for (const auto hexchr : insp::reverse_range(hexaddr))
				{
					reversedip.push_back(hexchr);
					reversedip.push_back('.');
				}
				reversedip.pop_back();
				return reversedip;
			}

			default:
				break;
		}

		// Clients can't be in a DNSBL if they aren't connected via IPv4 or IPv6.
		return {};
	}

public:
	// Counts the number of DNSBL lookups waiting for this user.
	IntExtItem countext;

	// Reference to the DNS manager.
	DNS::ManagerRef dns;

	// The DNSBL entries from the config.
	DNSBLEntries dnsbls;

	// The DNSBL marks which are set on a user.
	MarkExtItem markext;

	// The user@host to set on a marked user when they are connected.
	MaskExtItem maskext;

	SharedData(Module* mod)
		: countext(mod, "dnsbl-pending", ExtensionType::USER)
		, dns(mod)
		, markext(mod, "dnsbl-match", ExtensionType::USER)
		, maskext(mod, "dnsbl-mask", ExtensionType::USER)
	{
	}

	// Performs one or more DNSBL lookups on the specified user.
	void Lookup(LocalUser* user);
};

class DNSBLResolver final
	: public DNS::Request
{
private:
	std::shared_ptr<DNSBLEntry> config;
	SharedData& data;
	const irc::sockets::sockaddrs sa;
	const std::string uuid;

	template <typename Line, typename... Extra>
	void AddLine(const char* type, const std::string& reason, unsigned long duration, LocalUser* user, Extra&&... extra)
	{
		if (user->exempt)
			return; // This user shouldn't be banned.

		auto line = new Line(ServerInstance->Time(), duration, MODNAME "@" + ServerInstance->Config->ServerName, reason, std::forward<Extra>(extra)...);
		if (!ServerInstance->XLines->AddLine(line, nullptr))
		{
			ServerInstance->Users.QuitUser(user, "Killed (" + reason + ")");
			delete line;
			return;
		}

		ServerInstance->SNO.WriteToSnoMask('x', "{} added a timed {} on {}, expires in {} (on {}): {}",
			line->source, type, line->Displayable(), Duration::ToString(line->duration),
			Time::ToString(line->expiry), line->reason);
		ServerInstance->XLines->ApplyLines();
	}

public:
	DNSBLResolver(Module* mod, SharedData& sd, const std::string& hostname, LocalUser* u, const std::shared_ptr<DNSBLEntry>& cfg)
		: DNS::Request(*sd.dns, mod, hostname, DNS::QUERY_A, true, cfg->timeout)
		, config(cfg)
		, data(sd)
		, sa(u->client_sa)
		, uuid(u->uuid)
	{
	}

	/* Note: This may be called multiple times for multiple A record results */
	void OnLookupComplete(const DNS::Query* r) override
	{
		/* Check the user still exists */
		LocalUser* them = ServerInstance->Users.FindUUID<LocalUser>(uuid);
		if (!them || them->client_sa != sa)
		{
			config->stats_misses++;
			return;
		}

		intptr_t i = data.countext.Get(them);
		if (i)
			data.countext.Set(them, i - 1);

		// The DNSBL reply must contain an A result.
		const DNS::ResourceRecord* const ans_record = r->FindAnswerOfType(DNS::QUERY_A);
		if (!ans_record)
		{
			config->stats_errors++;
			ServerInstance->SNO.WriteGlobalSno('d', "{} returned an result with no IPv4 address.",
				config->name);
			return;
		}

		// The DNSBL reply must be a valid IPv4 address.
		in_addr resultip;
		if (inet_pton(AF_INET, ans_record->rdata.c_str(), &resultip) != 1)
		{
			config->stats_errors++;
			ServerInstance->SNO.WriteGlobalSno('d', "{} returned an invalid IPv4 address: {}",
				config->name, ans_record->rdata);
			return;
		}

		// The DNSBL reply should be in the 127.0.0.0/8 range.
		if ((resultip.s_addr & 0xFF) != 127)
		{
			config->stats_errors++;
			ServerInstance->SNO.WriteGlobalSno('d', "{} returned an IPv4 address which is outside of the 127.0.0.0/8 subnet: {}",
				config->name, ans_record->rdata);
			return;
		}

		bool match = false;
		unsigned int result = 0;
		switch (config->type)
		{
			case DNSBLEntry::Type::BITMASK:
			{
				result = (resultip.s_addr >> 24) & config->bitmask;
				match = (result != 0);
				break;
			}
			case DNSBLEntry::Type::RECORD:
			{
				result = resultip.s_addr >> 24;
				match = (config->records[result] == 1);
				break;
			}
		}

		if (match)
		{
			const std::string reason = Template::Replace(config->reason, {
				{ "dnsbl",  config->name       },
				{ "ip",     them->GetAddress() },
				{ "result", ConvToStr(result)  },
			});

			config->stats_hits++;

			switch (config->action)
			{
				case DNSBLEntry::Action::KILL:
				{
					if (!them->exempt)
						ServerInstance->Users.QuitUser(them, "Killed (" + reason + ")");
					break;
				}
				case DNSBLEntry::Action::MARK:
				{
					if (!config->markuser.empty() || !config->markhost.empty())
					{
						// Store the u@h mask for later to avoid being overwritten by username/hostname lookups.
						data.maskext.SetFwd(them, config, reason);

						// If the user is already connected we should just do this now.
						if (them->IsFullyConnected())
							creator->OnUserConnect(them);
					}

					data.markext.GetRef(them).push_back(config->name);
					break;
				}
				case DNSBLEntry::Action::KLINE:
				{
					AddLine<KLine>("K-line", reason, config->xlineduration, them, them->GetBanUser(true), them->GetAddress());
					break;
				}
				case DNSBLEntry::Action::GLINE:
				{
					AddLine<GLine>("G-line", reason, config->xlineduration, them, them->GetBanUser(true), them->GetAddress());
					break;
				}
				case DNSBLEntry::Action::ZLINE:
				{
					AddLine<ZLine>("Z-line", reason, config->xlineduration, them, them->GetAddress());
					break;
				}
				case DNSBLEntry::Action::SHUN:
				{
					AddLine<Shun>("Shun", reason, config->xlineduration, them, them->GetAddress());
					break;
				}
			}

			ServerInstance->SNO.WriteGlobalSno('d', "{} {} ({}) detected as being on the '{}' DNSBL with result {}{}",
				them->IsFullyConnected() ? "User" : "Connecting user", them->GetRealMask(), them->GetAddress(),
				config->name, result, them->exempt ? " -- exempt" : "");
		}
		else
			config->stats_misses++;
	}

	void OnError(const DNS::Query* q) override
	{
		bool is_miss = true;
		switch (q->error)
		{
			case DNS::ERROR_NO_RECORDS:
			case DNS::ERROR_DOMAIN_NOT_FOUND:
				config->stats_misses++;
				break;

			default:
				config->stats_errors++;
				is_miss = false;
				break;
		}

		LocalUser* them = ServerInstance->Users.FindUUID<LocalUser>(uuid);
		if (!them || them->client_sa != sa)
			return;

		intptr_t i = data.countext.Get(them);
		if (i)
			data.countext.Set(them, i - 1);

		if (is_miss)
			return;

		ServerInstance->SNO.WriteGlobalSno('d', "An error occurred whilst checking whether {} ({}) is on the '{}' DNSBL: {}",
			them->GetRealMask(), them->GetAddress(), config->name, data.dns->GetErrorStr(q->error));
	}
};

class CommandDNSBL final
	: public Command
{
private:
	SharedData& data;

public:
	CommandDNSBL(Module* mod, SharedData& sd)
		: Command(mod, "DNSBL", 1, 2)
		, data(sd)
	{
		access_needed = CmdAccess::OPERATOR;
		syntax = { "[<nick> [<reason>]]" };
		translation = { TR_NICK, TR_TEXT };
	}

	CmdResult Handle(User* user, const Params& parameters) override
	{
		User* target;
		if (IS_LOCAL(user))
			target = ServerInstance->Users.FindNick(parameters[0], true);
		else
			target = ServerInstance->Users.FindUUID(parameters[0], true);

		// We couldn't find the user so just give up.
		if (!target)
		{
			user->WriteNumeric(Numerics::NoSuchNick(parameters[0]));
			return CmdResult::FAILURE;
		}

		auto* ltarget = IS_LOCAL(target);
		if (!ltarget)
			return CmdResult::SUCCESS; // The user is on another server so just let it forward.

		intptr_t count = data.countext.Get(ltarget);
		if (count)
		{
			// TODO: replace this with a FAIL stdrpl when we can network those.
			user->WriteRemoteNotice(INSP_FORMAT("*** DNSBL: Unable to recheck {}: still waiting on {} DNSBLs from the previous check.",
				ltarget->nick, count));
			return CmdResult::FAILURE;
		}

		// TODO: replace this with a NOTE stdrpl when we can network those.
		user->WriteRemoteNotice(INSP_FORMAT("*** DNSBL: Rechecking {} against {} DNSBLs.", ltarget->nick, data.dnsbls.size()));

		const bool has_reason = parameters.size() > 1;
		ServerInstance->SNO.WriteGlobalSno('d', "{} is rechecking whether {} ({}) is in a DNSBL{}{}", user->nick,
			ltarget->nick, ltarget->GetAddress(),  has_reason ? ": " : "", has_reason ? parameters[1] : ".");

		data.Lookup(ltarget);
		return CmdResult::SUCCESS;
	}

	RouteDescriptor GetRouting(User* user, const Params& parameters) override
	{
		return ROUTE_OPT_UCAST(parameters[0]);
	}
};

void SharedData::Lookup(LocalUser* user)
{
	if (!dns)
		return; // The core_dns module is not loaded.

	if (!user->GetClass()->config->getBool("usednsbl", true))
		return; // The user's class is exempt from DNSBL lookups.

	const std::string reversedip = ReverseIP(user->client_sa);
	ServerInstance->Logs.Debug(MODNAME, "Reversed IP {} => {}", user->GetAddress(), reversedip);

	countext.Set(user, dnsbls.size());

	// For each DNSBL, we will run through this lookup
	for (const auto& dnsbl : dnsbls)
	{
		// Fill hostname with a dnsbl style host (d.c.b.a.domain.tld)
		const std::string hostname = reversedip + "." + dnsbl->domain;

		// Try to do the DNSBL lookup.
		auto* r = new DNSBLResolver(dns->creator, *this, hostname, user, dnsbl);
		try
		{
			dns->Process(r);
		}
		catch (const DNS::Exception& ex)
		{
			delete r;
			ServerInstance->Logs.Debug(MODNAME, "DNSBL lookup error: {}", ex.GetReason());
		}

		if (user->quitting)
			break; // DNS resolver found a cached hit.
	}
}

class ModuleDNSBL final
	: public Module
	, public Stats::EventListener
{
private:
	SharedData data;
	CommandDNSBL cmd;

public:
	ModuleDNSBL()
		: Module(VF_VENDOR, "Allows the server administrator to check the IP address of connecting users against a DNSBL.")
		, Stats::EventListener(this)
		, data(this)
		, cmd(this, data)
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
		data.dnsbls.swap(newdnsbls);
	}

	void OnChangeRemoteAddress(LocalUser* user) override
	{
		if (!user->quitting && user->GetClass())
			data.Lookup(user);
	}

	ModResult OnPreChangeConnectClass(LocalUser* user, const std::shared_ptr<ConnectClass>& klass, std::optional<Numeric::Numeric>& errnum) override
	{
		const std::string dnsbl = klass->config->getString("dnsbl");
		if (dnsbl.empty())
			return MOD_RES_PASSTHRU;

		MarkExtItem::List* match = data.markext.Get(user);
		if (!match)
		{
			ServerInstance->Logs.Debug("CONNECTCLASS", "The {} connect class is not suitable as it requires a DNSBL mark.",
				klass->GetName());
			return MOD_RES_DENY;
		}

		for (const auto& mark : *match)
		{
			if (InspIRCd::Match(mark, dnsbl))
				return MOD_RES_PASSTHRU;
		}

		const std::string marks = stdalgo::string::join(dnsbl);
		ServerInstance->Logs.Debug("CONNECTCLASS", "The {} connect class is not suitable as the DNSBL marks ({}) do not match {}.",
			klass->GetName(), marks, dnsbl);
		return MOD_RES_DENY;
	}

	ModResult OnCheckReady(LocalUser* user) override
	{
		// Block until all of the DNSBL lookups are complete.
		return data.countext.Get(user) ? MOD_RES_DENY : MOD_RES_PASSTHRU;
	}

	void OnUserConnect(LocalUser* user) override
	{
		DNSBLMask* ih = data.maskext.Get(user);
		if (ih)
		{
			if (!ih->user.empty())
			{
				user->ChangeDisplayedUser(ih->user);
				user->WriteNotice("Your username has been set to " + ih->user + ": " + ih->reason);
			}

			if (!ih->host.empty())
			{
				user->ChangeDisplayedHost(ih->host);
				user->WriteNotice("Your hostname has been set to " + ih->host + ": " + ih->reason);
			}

			data.maskext.Unset(user);
		}
	}

	ModResult OnStats(Stats::Context& stats) override
	{
		if (stats.GetSymbol() != 'd')
			return MOD_RES_PASSTHRU;

		unsigned long total_hits = 0;
		unsigned long total_misses = 0;
		unsigned long total_errors = 0;
		for (const auto& dnsbl : data.dnsbls)
		{
			total_hits += dnsbl->stats_hits;
			total_misses += dnsbl->stats_misses;
			total_errors += dnsbl->stats_errors;

			stats.AddGenericRow(INSP_FORMAT("The \"{}\" DNSBL had {} hits, {} misses, and {} errors",
				dnsbl->name, dnsbl->stats_hits, dnsbl->stats_misses, dnsbl->stats_errors));
		}

		stats.AddGenericRow("Total DNSBL hits: " + ConvToStr(total_hits));
		stats.AddGenericRow("Total DNSBL misses: " + ConvToStr(total_misses));
		stats.AddGenericRow("Total DNSBL errors: " + ConvToStr(total_errors));
		return MOD_RES_DENY;
	}
};

MODULE_INIT(ModuleDNSBL)
