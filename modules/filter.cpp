/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2021 Herman <GermanAizek@yandex.ru>
 *   Copyright (C) 2019 Matt Schatz <genius3000@g3k.solutions>
 *   Copyright (C) 2019 Filippo Cortigiani <simos@simosnap.org>
 *   Copyright (C) 2018-2019 linuxdaemon <linuxdaemon.irc@gmail.com>
 *   Copyright (C) 2018 Michael Hazell <michaelhazell@hotmail.com>
 *   Copyright (C) 2017 B00mX0r <b00mx0r@aureus.pw>
 *   Copyright (C) 2012-2014, 2016 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012-2013, 2017-2024 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012, 2018-2019 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2011 Adam <Adam@anope.org>
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2009 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2009 Matt Smith <dz@inspircd.org>
 *   Copyright (C) 2009 John Brooks <john@jbrooks.io>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2006-2009 Craig Edwards <brain@inspircd.org>
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
#include "modules/account.h"
#include "modules/regex.h"
#include "modules/server.h"
#include "modules/shun.h"
#include "modules/stats.h"
#include "numerichelper.h"
#include "timeutils.h"
#include "utility/string.h"
#include "xline.h"

#include <fstream>

enum FilterFlags
{
	FLAG_PART = 2,
	FLAG_QUIT = 4,
	FLAG_PRIVMSG = 8,
	FLAG_NOTICE = 16
};

enum FilterAction
{
	FA_GLINE,
	FA_ZLINE,
	FA_WARN,
	FA_BLOCK,
	FA_SILENT,
	FA_KILL,
	FA_SHUN,
	FA_NONE
};

static Module* thismod;

class FilterResult final
{
public:
	Regex::PatternPtr regex;
	std::string freeform;
	std::string reason;
	FilterAction action;
	unsigned long duration;
	bool from_config;

	bool flag_no_opers;
	bool flag_part_message;
	bool flag_quit_message;
	bool flag_privmsg;
	bool flag_notice;
	bool flag_strip_color;
	bool flag_no_registered;

	FilterResult(Regex::EngineReference& RegexEngine, const std::string& free, const std::string& rea, FilterAction act, unsigned long gt, const std::string& fla, bool cfg)
		: freeform(free)
		, reason(rea)
		, action(act)
		, duration(gt)
		, from_config(cfg)
	{
		if (!RegexEngine)
			throw ModuleException(thismod, "Regex module implementing '"+RegexEngine.GetProvider()+"' is not loaded!");
		regex = RegexEngine->CreateHuman(free);
		this->FillFlags(fla);
	}

	char FillFlags(const std::string& flags)
	{
		flag_no_opers = flag_part_message = flag_quit_message = flag_privmsg =
			flag_notice = flag_strip_color = flag_no_registered = false;

		for (const auto flag : flags)
		{
			switch (flag)
			{
				case 'o':
					flag_no_opers = true;
				break;
				case 'P':
					flag_part_message = true;
				break;
				case 'q':
					flag_quit_message = true;
				break;
				case 'p':
					flag_privmsg = true;
				break;
				case 'n':
					flag_notice = true;
				break;
				case 'c':
					flag_strip_color = true;
				break;
				case 'r':
					flag_no_registered = true;
				break;
				case '*':
					flag_no_opers = flag_part_message = flag_quit_message =
						flag_privmsg = flag_notice = flag_strip_color = true;
				break;
				default:
					return flag;
			}
		}
		return 0;
	}

	std::string GetFlags() const
	{
		std::string flags;
		if (flag_no_opers)
			flags.push_back('o');
		if (flag_part_message)
			flags.push_back('P');
		if (flag_quit_message)
			flags.push_back('q');
		if (flag_privmsg)
			flags.push_back('p');
		if (flag_notice)
			flags.push_back('n');

		/* Order is important here, as the logic in FillFlags() stops parsing when it encounters
		 * an unknown character. So the following characters must be last in the string.
		 * 'c' is unsupported on < 2.0.10
		 * 'r' is unsupported on < 3.2.0
		 */
		if (flag_strip_color)
			flags.push_back('c');
		if (flag_no_registered)
			flags.push_back('r');

		if (flags.empty())
			flags.push_back('-');

		return flags;
	}

	FilterResult() = default;
};

class CommandFilter final
	: public Command
{
public:
	CommandFilter(Module* f)
		: Command(f, "FILTER", 1, 5)
	{
		access_needed = CmdAccess::OPERATOR;
		this->syntax = { "<pattern> [<action> <flags> [<duration>] :<reason>]" };
	}

	CmdResult Handle(User* user, const Params& parameters) override;

	RouteDescriptor GetRouting(User* user, const Params& parameters) override
	{
		return ROUTE_BROADCAST;
	}
};

class ModuleFilter final
	: public Module
	, public ServerProtocol::SyncEventListener
	, public Stats::EventListener
	, public Timer
{
private:
	typedef insp::flat_set<std::string, irc::insensitive_swo> ExemptTargetSet;

	Account::API accountapi;
	bool initing = true;
	bool notifyuser;
	bool warnonselfmsg;
	bool dirty = false;
	std::string filterconf;
	Regex::Engine* factory;
	unsigned long saveperiod;
	unsigned long maxbackoff;
	unsigned char backoff;
	void FreeFilters();

public:
	CommandFilter filtcommand;
	Regex::EngineReference RegexEngine;

	std::vector<FilterResult> filters;
	int flags;

	// List of channel names excluded from filtering.
	ExemptTargetSet exemptedchans;

	// List of target nicknames excluded from filtering.
	ExemptTargetSet exemptednicks;

	ModuleFilter();
	void init() override;
	Cullable::Result Cull() override;
	ModResult OnUserPreMessage(User* user, MessageTarget& target, MessageDetails& details) override;
	const FilterResult* FilterMatch(User* user, const std::string& text, int flags);
	bool DeleteFilter(const std::string& freeform, std::string& reason);
	std::pair<bool, std::string> AddFilter(const std::string& freeform, FilterAction type, const std::string& reason, unsigned long duration, const std::string& flags, bool config = false);
	void ReadConfig(ConfigStatus& status) override;
	void CompareLinkData(const LinkData& otherdata, LinkDataDiff& diffs) override;
	void GetLinkData(LinkData& data) override;
	static std::string EncodeFilter(const FilterResult& filter);
	FilterResult DecodeFilter(const std::string& data);
	void OnSyncNetwork(Server& server) override;
	void OnDecodeMetadata(Extensible* target, const std::string& extname, const std::string& extdata) override;
	ModResult OnStats(Stats::Context& stats) override;
	ModResult OnPreCommand(std::string& command, CommandBase::Params& parameters, LocalUser* user, bool validated) override;
	void OnUnloadModule(Module* mod) override;
	bool Tick() override;
	bool WriteDatabase();
	bool AppliesToMe(User* user, const FilterResult& filter, int flags);
	void ReadFilters();
	static bool StringToFilterAction(const std::string& str, FilterAction& fa);
	static std::string FilterActionToString(FilterAction fa);
};

CmdResult CommandFilter::Handle(User* user, const Params& parameters)
{
	if (parameters.size() == 1)
	{
		/* Deleting a filter */
		Module* me = creator;
		std::string reason;

		if (static_cast<ModuleFilter*>(me)->DeleteFilter(parameters[0], reason))
		{
			user->WriteNotice("*** Removed filter '" + parameters[0] + "': " + reason);
			ServerInstance->SNO.WriteToSnoMask(IS_LOCAL(user) ? 'f' : 'F', "{} removed filter '{}': {}",
				user->nick, parameters[0], reason);
			return CmdResult::SUCCESS;
		}
		else
		{
			user->WriteNotice("*** Filter '" + parameters[0] + "' not found on the list.");
			return CmdResult::FAILURE;
		}
	}
	else
	{
		/* Adding a filter */
		if (parameters.size() >= 4)
		{
			const std::string& freeform = parameters[0];
			FilterAction type;
			const std::string& flags = parameters[2];
			unsigned int reasonindex;
			unsigned long duration = 0;

			if (!ModuleFilter::StringToFilterAction(parameters[1], type))
			{
				if (ServerInstance->XLines->GetFactory("SHUN"))
					user->WriteNotice("*** Invalid filter type '" + parameters[1] + "'. Supported types are 'gline', 'zline', 'none', 'warn', 'block', 'silent', 'kill', and 'shun'.");
				else
					user->WriteNotice("*** Invalid filter type '" + parameters[1] + "'. Supported types are 'gline', 'zline', 'none', 'warn', 'block', 'silent', and 'kill'.");
				return CmdResult::FAILURE;
			}

			if (type == FA_GLINE || type == FA_ZLINE || type == FA_SHUN)
			{
				if (parameters.size() >= 5)
				{
					if (!Duration::TryFrom(parameters[3], duration))
					{
						user->WriteNotice("*** Invalid duration for filter");
						return CmdResult::FAILURE;
					}
					reasonindex = 4;
				}
				else
				{
					user->WriteNotice("*** Not enough parameters: When setting a '" + parameters[1] + "' type filter, a duration must be specified as the third parameter.");
					return CmdResult::FAILURE;
				}
			}
			else
			{
				reasonindex = 3;
			}

			Module* me = creator;
			std::pair<bool, std::string> result = static_cast<ModuleFilter*>(me)->AddFilter(freeform, type, parameters[reasonindex], duration, flags);
			if (result.first)
			{
				const std::string message = FMT::format("'{}', type '{}'{}, flags '{}', reason: {}", freeform, parameters[1],
					(duration ? FMT::format(", duration '{}'", Duration::ToString(duration)) : ""),
					flags, parameters[reasonindex]);

				user->WriteNotice("*** Added filter " + message);
				ServerInstance->SNO.WriteToSnoMask(IS_LOCAL(user) ? 'f' : 'F', "{} added filter {}", user->nick, message);

				return CmdResult::SUCCESS;
			}
			else
			{
				user->WriteNotice("*** Filter '" + freeform + "' could not be added: " + result.second);
				return CmdResult::FAILURE;
			}
		}
		else
		{
			user->WriteNotice("*** Not enough parameters.");
			return CmdResult::FAILURE;
		}

	}
}

bool ModuleFilter::AppliesToMe(User* user, const FilterResult& filter, int iflags)
{
	if ((filter.flag_no_opers) && user->IsOper())
		return false;
	if ((filter.flag_no_registered) && accountapi && accountapi->GetAccountName(user))
		return false;
	if ((iflags & FLAG_PRIVMSG) && (!filter.flag_privmsg))
		return false;
	if ((iflags & FLAG_NOTICE) && (!filter.flag_notice))
		return false;
	if ((iflags & FLAG_QUIT)   && (!filter.flag_quit_message))
		return false;
	if ((iflags & FLAG_PART)   && (!filter.flag_part_message))
		return false;
	return true;
}

ModuleFilter::ModuleFilter()
	: Module(VF_VENDOR | VF_COMMON, "Adds the /FILTER command which allows server operators to define regex matches for inappropriate phrases that are not allowed to be used in channel messages, private messages, part messages, or quit messages.")
	, ServerProtocol::SyncEventListener(this)
	, Stats::EventListener(this)
	, Timer(0, true)
	, accountapi(this)
	, filtcommand(this)
	, RegexEngine(this)
{
	thismod = this;
}

void ModuleFilter::init()
{
	ServerInstance->SNO.EnableSnomask('f', "FILTER");
}

Cullable::Result ModuleFilter::Cull()
{
	FreeFilters();
	return Module::Cull();
}

void ModuleFilter::FreeFilters()
{
	filters.clear();
	dirty = true;
}

ModResult ModuleFilter::OnUserPreMessage(User* user, MessageTarget& msgtarget, MessageDetails& details)
{
	// Leave remote users and servers alone
	if (!IS_LOCAL(user))
		return MOD_RES_PASSTHRU;

	flags = (details.type == MessageType::PRIVMSG) ? FLAG_PRIVMSG : FLAG_NOTICE;

	const FilterResult* f = this->FilterMatch(user, details.text, flags);
	if (f)
	{
		bool is_selfmsg = false;
		switch (msgtarget.type)
		{
			case MessageTarget::TYPE_USER:
			{
				const auto* t = msgtarget.Get<User>();
				// Check if the target nick is exempted, if yes, ignore this message
				if (exemptednicks.count(t->nick))
					return MOD_RES_PASSTHRU;

				if (user == t)
					is_selfmsg = true;
				break;
			}
			case MessageTarget::TYPE_CHANNEL:
			{
				const auto* t = msgtarget.Get<Channel>();
				if (exemptedchans.count(t->name))
					return MOD_RES_PASSTHRU;
				break;
			}
			case MessageTarget::TYPE_SERVER:
				return MOD_RES_PASSTHRU;
		}

		if (is_selfmsg && warnonselfmsg)
		{
			ServerInstance->SNO.WriteGlobalSno('f', "WARNING: {}'s self message matched {} ({})",
				user->nick, f->freeform, f->reason);
			return MOD_RES_PASSTHRU;
		}
		else if (f->action == FA_WARN)
		{
			ServerInstance->SNO.WriteGlobalSno('f', "WARNING: {}'s message to {} matched {} ({})",
				user->nick, msgtarget.GetName(), f->freeform, f->reason);
			return MOD_RES_PASSTHRU;
		}
		else if (f->action == FA_BLOCK)
		{
			ServerInstance->SNO.WriteGlobalSno('f', "{} had their message to {} filtered as it matched {} ({})",
				user->nick, msgtarget.GetName(), f->freeform, f->reason);
			if (notifyuser)
			{
				if (msgtarget.type == MessageTarget::TYPE_CHANNEL)
					user->WriteNumeric(Numerics::CannotSendTo(msgtarget.Get<Channel>(), FMT::format("Your message to this channel was blocked: {}.", f->reason)));
				else
					user->WriteNumeric(Numerics::CannotSendTo(msgtarget.Get<User>(), FMT::format("Your message to this user was blocked: {}.", f->reason)));
			}
			else
				details.echo_original = true;
		}
		else if (f->action == FA_SILENT)
		{
			if (notifyuser)
			{
				if (msgtarget.type == MessageTarget::TYPE_CHANNEL)
					user->WriteNumeric(Numerics::CannotSendTo(msgtarget.Get<Channel>(), FMT::format("Your message to this channel was blocked: {}.", f->reason)));
				else
					user->WriteNumeric(Numerics::CannotSendTo(msgtarget.Get<User>(), FMT::format("Your message to this user was blocked: {}.", f->reason)));
			}
			else
				details.echo_original = true;
		}
		else if (f->action == FA_KILL)
		{
			ServerInstance->SNO.WriteGlobalSno('f', "{} was killed because their message to {} matched {} ({})",
				user->nick, msgtarget.GetName(), f->freeform, f->reason);
			ServerInstance->Users.QuitUser(user, "Filtered: " + f->reason);
		}
		else if (f->action == FA_SHUN && (ServerInstance->XLines->GetFactory("SHUN")))
		{
			auto* sh = new Shun(ServerInstance->Time(), f->duration, MODNAME "@" + ServerInstance->Config->ServerName, f->reason, user->GetAddress());
			ServerInstance->SNO.WriteGlobalSno('f', "{} ({}) was shunned for {} (expires on {}) because their message to {} matched {} ({})",
				user->nick, sh->Displayable(), Duration::ToString(f->duration),
				Time::FromNow(f->duration),
				msgtarget.GetName(), f->freeform, f->reason);
			if (ServerInstance->XLines->AddLine(sh, nullptr))
			{
				ServerInstance->XLines->ApplyLines();
			}
			else
				delete sh;
		}
		else if (f->action == FA_GLINE)
		{
			auto* gl = new GLine(ServerInstance->Time(), f->duration, MODNAME "@" + ServerInstance->Config->ServerName, f->reason, "*", user->GetAddress());
			ServerInstance->SNO.WriteGlobalSno('f', "{} ({}) was G-lined for {} (expires on {}) because their message to {} matched {} ({})",
				user->nick, gl->Displayable(), Duration::ToString(f->duration),
				Time::FromNow(f->duration),
				msgtarget.GetName(), f->freeform, f->reason);
			if (ServerInstance->XLines->AddLine(gl, nullptr))
			{
				ServerInstance->XLines->ApplyLines();
			}
			else
				delete gl;
		}
		else if (f->action == FA_ZLINE)
		{
			auto* zl = new ZLine(ServerInstance->Time(), f->duration, MODNAME "@" + ServerInstance->Config->ServerName, f->reason, user->GetAddress());
			ServerInstance->SNO.WriteGlobalSno('f', "{} ({}) was Z-lined for {} (expires on {}) because their message to {} matched {} ({})",
				user->nick, zl->Displayable(), Duration::ToString(f->duration),
				Time::FromNow(f->duration),
				msgtarget.GetName(), f->freeform, f->reason);
			if (ServerInstance->XLines->AddLine(zl, nullptr))
			{
				ServerInstance->XLines->ApplyLines();
			}
			else
				delete zl;
		}

		ServerInstance->Logs.Normal(MODNAME, user->nick + " had their message filtered, target was " + msgtarget.GetName() + ": " + f->reason + " Action: " + ModuleFilter::FilterActionToString(f->action));
		return MOD_RES_DENY;
	}
	return MOD_RES_PASSTHRU;
}

ModResult ModuleFilter::OnPreCommand(std::string& command, CommandBase::Params& parameters, LocalUser* user, bool validated)
{
	if (validated)
	{
		flags = 0;
		bool parting;

		if (command == "QUIT")
		{
			/* QUIT with no reason: nothing to do */
			if (parameters.empty())
				return MOD_RES_PASSTHRU;

			parting = false;
			flags = FLAG_QUIT;
		}
		else if (command == "PART")
		{
			/* PART with no reason: nothing to do */
			if (parameters.size() < 2)
				return MOD_RES_PASSTHRU;

			if (exemptedchans.count(parameters[0]))
				return MOD_RES_PASSTHRU;

			parting = true;
			flags = FLAG_PART;
		}
		else
			/* We're only messing with PART and QUIT */
			return MOD_RES_PASSTHRU;

		const FilterResult* f = this->FilterMatch(user, parameters[parting ? 1 : 0], flags);
		if (!f)
			/* PART or QUIT reason doesnt match a filter */
			return MOD_RES_PASSTHRU;

		/* We cant block a part or quit, so instead we change the reason to 'Reason filtered' */
		parameters[parting ? 1 : 0] = "Reason filtered";

		/* We're warning or blocking, OR they're quitting and its a KILL action
		 * (we cant kill someone who's already quitting, so filter them anyway)
		 */
		if ((f->action == FA_WARN) || (f->action == FA_BLOCK) || (((!parting) && (f->action == FA_KILL))) || (f->action == FA_SILENT))
		{
			return MOD_RES_PASSTHRU;
		}
		else
		{
			/* Are they parting, if so, kill is applicable */
			if ((parting) && (f->action == FA_KILL))
			{
				user->WriteNotice("*** Your PART message was filtered: " + f->reason);
				ServerInstance->Users.QuitUser(user, "Filtered: " + f->reason);
			}
			if (f->action == FA_GLINE)
			{
				/* Note: We G-line *@IP so that if their host doesn't resolve the G-line still applies. */
				auto* gl = new GLine(ServerInstance->Time(), f->duration, MODNAME "@" + ServerInstance->Config->ServerName, f->reason, "*", user->GetAddress());
				ServerInstance->SNO.WriteGlobalSno('f', "{} ({}) was G-lined for {} (expires on {}) because their {} message matched {} ({})",
					user->nick, gl->Displayable(),
					Duration::ToString(f->duration),
					Time::FromNow(f->duration),
					command, f->freeform, f->reason);

				if (ServerInstance->XLines->AddLine(gl, nullptr))
				{
					ServerInstance->XLines->ApplyLines();
				}
				else
					delete gl;
			}
			if (f->action == FA_ZLINE)
			{
				auto* zl = new ZLine(ServerInstance->Time(), f->duration, MODNAME "@" + ServerInstance->Config->ServerName, f->reason, user->GetAddress());
				ServerInstance->SNO.WriteGlobalSno('f', "{} ({}) was Z-lined for {} (expires on {}) because their {} message matched {} ({})",
					user->nick, zl->Displayable(),
					Duration::ToString(f->duration),
					Time::FromNow(f->duration),
					command, f->freeform, f->reason);

				if (ServerInstance->XLines->AddLine(zl, nullptr))
				{
					ServerInstance->XLines->ApplyLines();
				}
				else
					delete zl;
			}
			else if (f->action == FA_SHUN && (ServerInstance->XLines->GetFactory("SHUN")))
			{
				/* Note: We shun *!*@IP so that if their host doesnt resolve the shun still applies. */
				auto* sh = new Shun(ServerInstance->Time(), f->duration, MODNAME "@" + ServerInstance->Config->ServerName, f->reason, user->GetAddress());
				ServerInstance->SNO.WriteGlobalSno('f', "{} ({}) was shunned for {} (expires on {}) because their {} message matched {} ({})",
					user->nick, sh->Displayable(),
					Duration::ToString(f->duration),
					Time::FromNow(f->duration),
					command, f->freeform, f->reason);

				if (ServerInstance->XLines->AddLine(sh, nullptr))
				{
					ServerInstance->XLines->ApplyLines();
				}
				else
					delete sh;
			}
			return MOD_RES_DENY;
		}
	}
	return MOD_RES_PASSTHRU;
}

void ModuleFilter::ReadConfig(ConfigStatus& status)
{
	exemptedchans.clear();
	exemptednicks.clear();

	for (const auto& [_, tag] : ServerInstance->Config->ConfTags("exemptfromfilter"))
	{
		const std::string target = tag->getString("target");
		if (!target.empty())
		{
			if (ServerInstance->Channels.IsPrefix(target[0]))
				exemptedchans.insert(target);
			else
				exemptednicks.insert(target);
		}
	}

	const auto& tag = ServerInstance->Config->ConfValue("filteropts");
	std::string newrxengine = tag->getString("engine");
	notifyuser = tag->getBool("notifyuser", true);
	warnonselfmsg = tag->getBool("warnonselfmsg");
	filterconf = tag->getString("filename");
	if (!filterconf.empty())
		filterconf = ServerInstance->Config->Paths.PrependConfig(filterconf);
	saveperiod = tag->getDuration("saveperiod", 5);
	backoff = tag->getNum<uint8_t>("backoff", 0);
	maxbackoff = tag->getDuration("maxbackoff", saveperiod * 120, saveperiod);
	SetInterval(saveperiod);

	factory = RegexEngine ? (RegexEngine.operator->()) : nullptr;

	RegexEngine.SetEngine(newrxengine);
	if (!RegexEngine)
	{
		if (newrxengine.empty())
			ServerInstance->SNO.WriteGlobalSno('f', "WARNING: No regex engine loaded - Filter functionality disabled until this is corrected.");
		else
			ServerInstance->SNO.WriteGlobalSno('f', "WARNING: Regex engine '{}' is not loaded - Filter functionality disabled until this is corrected.", newrxengine);

		initing = false;
		FreeFilters();
		return;
	}

	if ((!initing) && (RegexEngine.operator->() != factory))
	{
		ServerInstance->SNO.WriteGlobalSno('f', "Dumping all filters due to regex engine change");
		FreeFilters();
	}

	initing = false;
	ReadFilters();
}

void ModuleFilter::CompareLinkData(const LinkData& otherdata, LinkDataDiff& diffs)
{
	Module::CompareLinkData(otherdata, diffs);

	auto it = diffs.find("flags");
	if (it == diffs.end())
		return; // Should never happen.

	if (!it->second.first)
		it->second.first = "no";
	if (!it->second.second)
		it->second.second = "no";
}

void ModuleFilter::GetLinkData(LinkData& data)
{
	if (RegexEngine)
		data["regex"] = RegexEngine->GetName(); // e.g. pcre
	else
		data["regex"] = "broken";

	data["flags"] = "yes";
}

std::string ModuleFilter::EncodeFilter(const FilterResult& filter)
{
	std::ostringstream stream;

	/* Hax to allow spaces in the freeform without changing the design of the irc protocol */
	std::string freeform = filter.freeform;
	for (auto& chr : freeform)
	{
		if (chr == ' ')
			chr = '\7';
	}

	stream << freeform << " " << FilterActionToString(filter.action) << " " << filter.GetFlags() << " " << filter.duration << " :" << filter.reason;
	return stream.str();
}

FilterResult ModuleFilter::DecodeFilter(const std::string& data)
{
	std::string filteraction;
	FilterResult res;
	irc::tokenstream tokens(data);
	tokens.GetMiddle(res.freeform);
	tokens.GetMiddle(filteraction);
	if (!StringToFilterAction(filteraction, res.action))
		throw ModuleException(this, "Invalid action: " + filteraction);

	std::string filterflags;
	tokens.GetMiddle(filterflags);
	char c = res.FillFlags(filterflags);
	if (c != 0)
		throw ModuleException(this, "Invalid flag: '" + std::string(1, c) + "'");

	std::string duration;
	tokens.GetMiddle(duration);
	res.duration = ConvToNum<unsigned long>(duration);

	tokens.GetTrailing(res.reason);

	/* Hax to allow spaces in the freeform without changing the design of the irc protocol */
	for (auto& chr : res.freeform)
	{
		if (chr == '\7')
			chr = ' ';
	}
	return res;
}

void ModuleFilter::OnSyncNetwork(Server& server)
{
	for (const auto& filter : filters)
	{
		if (filter.from_config)
			continue;

		server.SendMetadata("filter", EncodeFilter(filter));
	}
}

void ModuleFilter::OnDecodeMetadata(Extensible* target, const std::string& extname, const std::string& extdata)
{
	if (!target && irc::equals(extname, "filter"))
	{
		try
		{
			FilterResult data = DecodeFilter(extdata);
			this->AddFilter(data.freeform, data.action, data.reason, data.duration, data.GetFlags());
		}
		catch (const ModuleException& e)
		{
			ServerInstance->Logs.Debug(MODNAME, "Error when unserializing filter: {}", e.GetReason());
		}
	}
}

const FilterResult* ModuleFilter::FilterMatch(User* user, const std::string& text, int flgs)
{
	static std::string stripped_text;
	stripped_text.clear();

	for (const auto& filter : filters)
	{
		/* Skip ones that dont apply to us */
		if (!AppliesToMe(user, filter, flgs))
			continue;

		if ((filter.flag_strip_color) && (stripped_text.empty()))
		{
			stripped_text = text;
			InspIRCd::StripColor(stripped_text);
		}

		if (filter.regex->IsMatch(filter.flag_strip_color ? stripped_text : text))
			return &filter;
	}
	return nullptr;
}

bool ModuleFilter::DeleteFilter(const std::string& freeform, std::string& reason)
{
	for (std::vector<FilterResult>::iterator i = filters.begin(); i != filters.end(); i++)
	{
		if (i->freeform == freeform)
		{
			reason.assign(i->reason);
			filters.erase(i);
			dirty = true;
			return true;
		}
	}
	return false;
}

std::pair<bool, std::string> ModuleFilter::AddFilter(const std::string& freeform, FilterAction type, const std::string& reason, unsigned long duration, const std::string& flgs, bool config)
{
	for (const auto& filter : filters)
	{
		if (filter.freeform == freeform)
		{
			return std::make_pair(false, "Filter already exists");
		}
	}

	try
	{
		filters.emplace_back(RegexEngine, freeform, reason, type, duration, flgs, config);
		dirty = true;
	}
	catch (const ModuleException& e)
	{
		ServerInstance->Logs.Normal(MODNAME, "Error in regular expression '{}': {}", freeform, e.GetReason());
		return std::make_pair(false, e.GetReason());
	}
	return std::make_pair(true, "");
}

bool ModuleFilter::StringToFilterAction(const std::string& str, FilterAction& fa)
{
	if (insp::equalsci(str, "gline"))
		fa = FA_GLINE;
	else if (insp::equalsci(str, "zline"))
		fa = FA_ZLINE;
	else if (insp::equalsci(str, "warn"))
		fa = FA_WARN;
	else if (insp::equalsci(str, "block"))
		fa = FA_BLOCK;
	else if (insp::equalsci(str, "silent"))
		fa = FA_SILENT;
	else if (insp::equalsci(str, "kill"))
		fa = FA_KILL;
	else if (insp::equalsci(str, "shun") && (ServerInstance->XLines->GetFactory("SHUN")))
		fa = FA_SHUN;
	else if (insp::equalsci(str, "none"))
		fa = FA_NONE;
	else
		return false;

	return true;
}

std::string ModuleFilter::FilterActionToString(FilterAction fa)
{
	switch (fa)
	{
		case FA_GLINE:  return "gline";
		case FA_ZLINE:  return "zline";
		case FA_WARN:   return "warn";
		case FA_BLOCK:  return "block";
		case FA_SILENT: return "silent";
		case FA_KILL:   return "kill";
		case FA_SHUN:   return "shun";
		default:		return "none";
	}
}

void ModuleFilter::ReadFilters()
{
	insp::flat_set<std::string> removedfilters;

	for (std::vector<FilterResult>::iterator filter = filters.begin(); filter != filters.end(); )
	{
		if (filter->from_config)
		{
			removedfilters.insert(filter->freeform);
			filter = filters.erase(filter);
			continue;
		}

		// The filter is not from the config.
		filter++;
	}

	for (const auto& [_, tag] : ServerInstance->Config->ConfTags("keyword"))
	{
		std::string pattern = tag->getString("pattern");
		std::string reason = tag->getString("reason");
		std::string action = tag->getString("action");
		std::string flgs = tag->getString("flags", "*", 1);
		bool generated = tag->getBool("generated");
		unsigned long duration = tag->getDuration("duration", 10*60, 1);

		FilterAction fa;
		if (!StringToFilterAction(action, fa))
			fa = FA_NONE;

		std::pair<bool, std::string> result = static_cast<ModuleFilter*>(this)->AddFilter(pattern, fa, reason, duration, flgs, !generated);
		if (result.first)
			removedfilters.erase(pattern);
		else
			ServerInstance->Logs.Warning(MODNAME, "Filter '{}' could not be added: {}", pattern, result.second);
	}

	if (!removedfilters.empty())
	{
		for (const auto& removedfilter : removedfilters)
			ServerInstance->SNO.WriteGlobalSno('f', "Removing filter '" + removedfilter + "' due to config rehash.");
	}
}

ModResult ModuleFilter::OnStats(Stats::Context& stats)
{
	if (stats.GetSymbol() == 's')
	{
		for (const auto& filter : filters)
		{
			stats.AddRow(223, RegexEngine.GetProvider(), filter.freeform, filter.GetFlags(), FilterActionToString(filter.action), filter.duration, filter.reason);
		}
		for (const auto& exemptedchan : exemptedchans)
		{
			stats.AddRow(223, "EXEMPT " + exemptedchan);
		}
		for (const auto& exemptednick : exemptednicks)
		{
			stats.AddRow(223, "EXEMPT " + exemptednick);
		}
		return MOD_RES_DENY;
	}
	return MOD_RES_PASSTHRU;
}

void ModuleFilter::OnUnloadModule(Module* mod)
{
	// If the regex engine became unavailable or has changed, remove all filters
	if (!RegexEngine)
	{
		FreeFilters();
	}
	else if (RegexEngine.operator->() != factory)
	{
		factory = RegexEngine.operator->();
		FreeFilters();
	}
}

bool ModuleFilter::Tick()
{
	if (dirty)
	{
		if (WriteDatabase())
		{
			// If we were previously unable to write but now can then reset the time interval.
			if (GetInterval() != saveperiod)
				SetInterval(saveperiod, false);

			dirty = false;
		}
		else
		{
			// Back off a bit to avoid spamming opers.
			if (backoff > 1)
				SetInterval(std::min(GetInterval() * backoff, maxbackoff), false);
			ServerInstance->Logs.Debug(MODNAME, "Trying again in {} seconds", GetInterval());
		}
	}
	return true;
};

bool ModuleFilter::WriteDatabase()
{
		if (filterconf.empty()) // Nothing to write to.
		{
			dirty = false;
			return true;
		}

		const std::string newfilterconf = filterconf + ".new." + ConvToStr(ServerInstance->Time());
		std::ofstream stream(newfilterconf.c_str());
		if (!stream.is_open()) // Filesystem probably not writable.
		{
			ServerInstance->SNO.WriteToSnoMask('f', "Unable to save filters to \"{}\": {} ({})",
					newfilterconf, strerror(errno), errno);
			return false;
		}

		stream
			<< "# This file was automatically generated by the " << INSPIRCD_VERSION << " filter module on " << Time::ToString(ServerInstance->Time()) << "." << std::endl
			<< "# Any changes to this file will be automatically overwritten." << std::endl
			<< "# If you want to convert this to a normal config file you *MUST* remove the generated=\"yes\" keys!" << std::endl
			<< std::endl;

		for (const auto& filter : filters)
		{
			if (filter.from_config)
				continue;

			stream << "<keyword generated=\"yes"
			<< "\" pattern=\"" << ServerConfig::Escape(filter.freeform)
			<< "\" reason=\"" << ServerConfig::Escape(filter.reason)
			<< "\" action=\"" << FilterActionToString(filter.action)
			<< "\" flags=\"" << filter.GetFlags();
			if (filter.duration)
				stream << "\" duration=\"" << Duration::ToString(filter.duration);
			stream << "\">" << std::endl;
		}

		if (stream.fail()) // Filesystem probably not writable.
		{
			ServerInstance->SNO.WriteToSnoMask('f', "Unable to save filters to \"{}\": {} ({})",
				newfilterconf, strerror(errno), errno);
			return false;
		}
		stream.close();

#ifdef _WIN32
		remove(filterconf.c_str());
#endif

		// Use rename to move temporary to new db - this is guaranteed not to fuck up, even in case of a crash.
		if (rename(newfilterconf.c_str(), filterconf.c_str()) < 0)
		{
			ServerInstance->SNO.WriteToSnoMask('f', "Unable to replace old filter config \"{}\" with \"{}\": {} ({})",
				filterconf, newfilterconf, strerror(errno), errno);
			return false;
		}

		dirty = false;
		return true;
}

MODULE_INIT(ModuleFilter)
