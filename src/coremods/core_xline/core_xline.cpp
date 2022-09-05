/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2019 linuxdaemon <linuxdaemon.irc@gmail.com>
 *   Copyright (C) 2019 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2019 Matt Schatz <genius3000@g3k.solutions>
 *   Copyright (C) 2017-2021 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2014, 2016 Attila Molnar <attilamolnar@hush.com>
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
#include "core_xline.h"

bool InsaneBan::MatchesEveryone(const std::string& mask, MatcherBase& test, User* user, const char* bantype, const char* confkey)
{
	auto insane = ServerInstance->Config->ConfValue("insane");

	if (insane->getBool(confkey))
		return false;

	float itrigger = insane->getFloat("trigger", 95.5, 0.0, 100.0);

	long matches = test.Run(mask);

	if (!matches)
		return false;

	float percent = ((float)matches / (float)ServerInstance->Users.GetUsers().size()) * 100;
	if (percent > itrigger)
	{
		ServerInstance->SNO.WriteToSnoMask('a', "\002WARNING\002: %s tried to set a %s-line mask of %s, which covers %.2f%% of the network!", user->nick.c_str(), bantype, mask.c_str(), percent);
		return true;
	}
	return false;
}

bool InsaneBan::IPHostMatcher::Check(User* user, const std::string& mask) const
{
	return ((InspIRCd::MatchCIDR(user->MakeHost(), mask, ascii_case_insensitive_map)) ||
			(InspIRCd::MatchCIDR(user->MakeHostIP(), mask, ascii_case_insensitive_map)));
}

class CoreModXLine final
	: public Module
{
private:
	CommandEline cmdeline;
	CommandGline cmdgline;
	CommandKline cmdkline;
	CommandQline cmdqline;
	CommandZline cmdzline;

	static void ReadXLine(const std::string& tag, const std::string& key, const std::string& type)
	{
		XLineFactory* make = ServerInstance->XLines->GetFactory(type);
		if (!make)
			throw CoreException("BUG: Unable to find the " + type + "-line factory!");

		insp::flat_set<std::string> configlines;
		for (const auto& [_, ctag] : ServerInstance->Config->ConfTags(tag))
		{
			const std::string mask = ctag->getString(key);
			if (mask.empty())
				throw CoreException("<" + tag + ":" + key + "> missing at " + ctag->source.str());

			const std::string reason = ctag->getString("reason");
			if (reason.empty())
				throw CoreException("<" + tag + ":reason> missing at " + ctag->source.str());

			XLine* xl = make->Generate(ServerInstance->Time(), 0, ServerInstance->Config->ServerName, reason, mask);
			xl->from_config = true;
			configlines.insert(xl->Displayable());
			if (!ServerInstance->XLines->AddLine(xl, nullptr))
				delete xl;
		}

		ServerInstance->XLines->ExpireRemovedConfigLines(make->GetType(), configlines);
	}

public:
	CoreModXLine()
		: Module(VF_CORE | VF_VENDOR, "Provides the ELINE, GLINE, KLINE, QLINE, and ZLINE commands")
		, cmdeline(this)
		, cmdgline(this)
		, cmdkline(this)
		, cmdqline(this)
		, cmdzline(this)
	{
	}

	void init() override
	{
		ServerInstance->SNO.EnableSnomask('x', "XLINE");
	}

	void ReadConfig(ConfigStatus& status) override
	{
		ReadXLine("badip", "ipmask", "Z");
		ReadXLine("badnick", "nick", "Q");
		ReadXLine("badhost", "host", "K");
		ReadXLine("exception", "host", "E");

		ServerInstance->XLines->CheckELines();
		ServerInstance->XLines->ApplyLines();
	}

	void OnChangeRemoteAddress(LocalUser* user) override
	{
		if (user->quitting)
			return;

		user->exempt = (ServerInstance->XLines->MatchesLine("E", user) != nullptr);
		user->CheckLines(true);
	}

	void OnPostChangeRealHost(User* user) override
	{
		LocalUser* luser = IS_LOCAL(user);
		if (!luser || luser->quitting)
			return;

		luser->exempt = (ServerInstance->XLines->MatchesLine("E", user) != nullptr);
		luser->CheckLines(false);
	}

	ModResult OnUserPreNick(LocalUser* user, const std::string& newnick) override
	{
		// Check Q-lines (for local nick changes only, remote servers have our Q-lines to enforce themselves)

		XLine* xline = ServerInstance->XLines->MatchesLine("Q", newnick);
		if (!xline)
			return MOD_RES_PASSTHRU; // No match

		// A Q-line matched the new nick, tell opers if the user is registered
		if (user->registered == REG_ALL)
		{
			ServerInstance->SNO.WriteGlobalSno('x', "Q-lined nickname %s from %s: %s",
				newnick.c_str(), user->GetFullRealHost().c_str(), xline->reason.c_str());
		}

		// Send a numeric because if we deny then the core doesn't reply anything
		user->WriteNumeric(ERR_ERRONEUSNICKNAME, newnick, InspIRCd::Format("Invalid nickname: %s", xline->reason.c_str()));
		return MOD_RES_DENY;
	}

	void OnGarbageCollect() override
	{
		// HACK: ELines are not expired properly at the moment but it can't be fixed
		// as the XLine system is a spaghetti nightmare. Instead we skip over expired
		// ELines in XLineManager::CheckELines() and expire them here instead.
		ServerInstance->XLines->GetAll("E");
	}
};

MODULE_INIT(CoreModXLine)
