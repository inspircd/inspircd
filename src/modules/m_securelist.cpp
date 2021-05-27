/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2018 linuxdaemon <linuxdaemon.irc@gmail.com>
 *   Copyright (C) 2013, 2018, 2020 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012-2013, 2016 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012, 2019 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2007-2008 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2006-2008, 2010 Craig Edwards <brain@inspircd.org>
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
#include "modules/isupport.h"

typedef std::vector<std::string> AllowList;

class ModuleSecureList final
	: public Module
	, public ISupport::EventListener
{
 private:
	AllowList allowlist;
	bool exemptregistered;
	unsigned long fakechans;
	std::string fakechanprefix;
	std::string fakechantopic;
	bool showmsg;
	unsigned long waittime;

 public:
	ModuleSecureList()
		: Module(VF_VENDOR, "Prevents users from using the /LIST command until a predefined period has passed.")
		, ISupport::EventListener(this)
	{
	}

	void ReadConfig(ConfigStatus& status) override
	{
		AllowList newallows;
		for (const auto& [_, tag] : ServerInstance->Config->ConfTags("securehost"))
		{
			const std::string host = tag->getString("exception");
			if (host.empty())
				throw ModuleException("<securehost:exception> is a required field at " + tag->source.str());

			newallows.push_back(host);
		}

		auto tag = ServerInstance->Config->ConfValue("securelist");
		exemptregistered = tag->getBool("exemptregistered", true);
		fakechans = tag->getUInt("fakechans", 5, 0);
		fakechanprefix = tag->getString("fakechanprefix", "#", 1, ServerInstance->Config->Limits.MaxChannel - 1);
		fakechantopic = tag->getString("fakechantopic", "Fake channel for confusing spambots", 1, ServerInstance->Config->Limits.MaxTopic - 1);
		showmsg = tag->getBool("showmsg", true);
		waittime = tag->getDuration("waittime", 60, 1, UINT_MAX);

		allowlist.swap(newallows);
	}

	ModResult OnPreCommand(std::string& command, CommandBase::Params& parameters, LocalUser* user, bool validated) override
	{
		// Ignore unless the command is a validated LIST command.
		if (!validated || command != "LIST")
			return MOD_RES_PASSTHRU;

		// Allow if the wait time has passed.
		time_t maxwaittime = user->signon + waittime;
		if (ServerInstance->Time() > maxwaittime || user->HasPrivPermission("servers/ignore-securelist"))
			return MOD_RES_PASSTHRU;

		// Allow if the source matches an <securehost> entry.
		for (const auto& allowhost : allowlist)
		{
			if (InspIRCd::Match(user->MakeHost(), allowhost, ascii_case_insensitive_map))
				return MOD_RES_PASSTHRU;

			if (InspIRCd::Match(user->MakeHostIP(), allowhost, ascii_case_insensitive_map))
				return MOD_RES_PASSTHRU;
		}

		// Allow if the source is logged in and <securelist:exemptregistered> is set.
		const AccountExtItem* ext = GetAccountExtItem();
		if (exemptregistered && ext && ext->Get(user))
			return MOD_RES_PASSTHRU;

		// If <securehost:showmsg> is set then tell the user that they need to wait.
		if (showmsg)
		{
			user->WriteNotice(InspIRCd::Format("*** You cannot view the channel list right now. Please %stry again in %s.",
				exemptregistered ? "log in to an account or " : "",
				InspIRCd::DurationString(maxwaittime - ServerInstance->Time()).c_str()));
		}

		// The client might be waiting on a response to do something so send them an
		// fake list response to satisfy that.
		size_t maxfakesuffix = ServerInstance->Config->Limits.MaxChannel - fakechanprefix.size();
		user->WriteNumeric(RPL_LISTSTART, "Channel", "Users Name");
		for (unsigned long fakechan = 0; fakechan < fakechans; ++fakechan)
		{
			// Generate the fake channel name.
			unsigned long chansuffixsize = ServerInstance->GenRandomInt(maxfakesuffix) + 1;
			const std::string chansuffix = ServerInstance->GenRandomStr(chansuffixsize);

			// Generate the fake channel size.
			unsigned long chanusers = ServerInstance->GenRandomInt(ServerInstance->Users.GetUsers().size()) + 1;

			// Generate the fake channel topic.
			std::string chantopic(fakechantopic);
			chantopic.insert(ServerInstance->GenRandomInt(chantopic.size()), 1, "\x02\x1D\x11\x1E\x1F"[fakechan % 5]);

			// Send the fake channel list entry.
			user->WriteNumeric(RPL_LIST, fakechanprefix + chansuffix, chanusers, chantopic);
		}
		user->WriteNumeric(RPL_LISTEND, "End of channel list.");
		return MOD_RES_DENY;
	}

	void OnBuildISupport(ISupport::TokenMap& tokens) override
	{
		if (showmsg)
			tokens["SECURELIST"] = ConvToStr(waittime);
	}
};

MODULE_INIT(ModuleSecureList)
