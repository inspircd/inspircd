/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2023-2024 Sadie Powell <sadie@witchery.services>
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
#include "modules/stats.h"
#include "modules/whois.h"
#include "timeutils.h"

class HelpOp final
	: public SimpleUserMode
{
public:
	std::vector<User*> helpers;

	HelpOp(Module* mod)
		: SimpleUserMode(mod, "helpop", 'h', true)
	{
	}

	bool OnModeChange(User* source, User* dest, Channel* channel, Modes::Change& change) override
	{
		if (!SimpleUserMode::OnModeChange(source, dest, channel, change))
			return false;

		if (change.adding)
			helpers.push_back(dest);
		else
			std::erase(helpers, dest);

		return true;
	}
};

class ModuleHelpMode final
	: public Module
	, public Stats::EventListener
	, public Whois::EventListener
{
private:
	bool ignorehideoper;
	HelpOp helpop;
	UserModeReference hideoper;
	bool markhelpers;
	std::string helpchanmodes;
	insp::flat_map<std::string, std::string> helpchans;

public:
	ModuleHelpMode()
		: Module(VF_VENDOR, "Adds user mode h (helpop) which marks a user as being available for help.")
		, Stats::EventListener(this, 50)
		, Whois::EventListener(this)
		, helpop(this)
		, hideoper(this, "hideoper")
	{
	}

	void ReadConfig(ConfigStatus& status) override
	{
		for (const auto& [_, tag] : ServerInstance->Config->ConfTags("helpchan"))
		{
			const auto name = tag->getString("name");
			if (name.empty())
				throw ModuleException(this, "<helpchan:name> must not be empty at " + tag->source.str());

			helpchans[name] = tag->getString("prefix", "o", 1);
		}

		const auto& tag = ServerInstance->Config->ConfValue("helpmode");
		ignorehideoper = tag->getBool("ignorehideoper");
		markhelpers = tag->getBool("markhelpers", true);
	}

	ModResult OnStats(Stats::Context& stats) override
	{
		if (stats.GetSymbol() != 'P')
			return MOD_RES_PASSTHRU;

		for (auto* helper : helpop.helpers)
		{
			if (helper->server->IsService())
				continue; // Ignore services.

			if (helper->IsOper() && (!ignorehideoper || !helper->IsModeSet(hideoper)))
				continue; // Ignore opers.

			std::string extra;
			if (helper->IsAway())
			{
				const std::string awayperiod = Duration::ToHuman(ServerInstance->Time() - helper->away->time, true);
				const std::string awaytime = Time::ToString(helper->away->time);

				extra = FMT::format(": away for {} [since {}] ({})", awayperiod, awaytime, helper->away->message);
			}

			auto* lhelper = IS_LOCAL(helper);
			if (lhelper)
			{
				const std::string idleperiod = Duration::ToHuman(ServerInstance->Time() - lhelper->idle_lastmsg, true);
				const std::string idletime = Time::ToString(lhelper->idle_lastmsg);

				extra += FMT::format("{} idle for {} [since {}]",  extra.empty() ? ':' : ',', idleperiod, idletime);
			}

			stats.AddGenericRow(FMT::format("\x02{}\x02{} ({}){}", helper->nick, markhelpers ? " [helper]" : "",
				helper->GetUserHost(), extra));
		}

		// Allow the core to add normal opers.
		return MOD_RES_PASSTHRU;
	}

	ModResult OnUserPreJoin(LocalUser* user, Channel* chan, const std::string& cname, std::string& privs, const std::string& keygiven, bool override) override
	{
		if (!user->IsModeSet(helpop))
			return MOD_RES_PASSTHRU;

		for (const auto& [helpchan, prefix] : helpchans)
		{
			if (InspIRCd::Match(cname, helpchan))
			{
				privs.append(prefix);
				break;
			}
		}
		return MOD_RES_PASSTHRU;
	}

	void OnUserQuit(User* user, const std::string& message, const std::string& opermessage) override
	{
		if (user->IsModeSet(helpop))
			std::erase(helpop.helpers, user);
	}

	void OnWhois(Whois::Context& whois) override
	{
		if (whois.GetTarget()->IsModeSet(helpop))
			whois.SendLine(RPL_WHOISHELPOP, "is available for help.");
	}
};

MODULE_INIT(ModuleHelpMode)
