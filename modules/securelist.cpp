/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2021 Dominic Hamon
 *   Copyright (C) 2018-2023 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2018 linuxdaemon <linuxdaemon.irc@gmail.com>
 *   Copyright (C) 2013, 2016 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2007-2008 Craig Edwards <brain@inspircd.org>
 *   Copyright (C) 2007 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
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
#include "timeutils.h"

typedef std::vector<std::string> AllowList;

class ModuleSecureList final
	: public Module
	, public ISupport::EventListener
{
private:
	Account::API accountapi;
	AllowList allowlist;
	bool exemptregistered;
	unsigned long fakechans;
	std::string fakechanprefix;
	std::string fakechantopic;
	size_t hidesmallchans;
	bool sendingfakelist = false;
	bool showmsg;
	unsigned long waittime;

	bool IsExempt(User* user)
	{
		// Allow if the source is a privileged server operator.
		if (user->HasPrivPermission("servers/ignore-securelist"))
			return true;

		// Allow if the source is logged in and <securelist:exemptregistered> is set.
		if (exemptregistered && accountapi && accountapi->GetAccountName(user))
			return true;

		// Allow if the source matches an <securehost> entry.
		for (const auto& allowhost : allowlist)
		{
			if (InspIRCd::Match(user->GetRealUserHost(), allowhost, ascii_case_insensitive_map))
				return true;

			if (InspIRCd::Match(user->GetUserAddress(), allowhost, ascii_case_insensitive_map))
				return true;
		}

		// The user does not appear to be exempt.
		return false;
	}

public:
	ModuleSecureList()
		: Module(VF_VENDOR, "Prevents users from using the /LIST command until a predefined period has passed.")
		, ISupport::EventListener(this)
		, accountapi(this)
	{
	}

	void ReadConfig(ConfigStatus& status) override
	{
		AllowList newallows;
		for (const auto& [_, tag] : ServerInstance->Config->ConfTags("securehost"))
		{
			const std::string host = tag->getString("exception");
			if (host.empty())
				throw ModuleException(this, "<securehost:exception> is a required field at " + tag->source.str());

			newallows.push_back(host);
		}

		const auto& tag = ServerInstance->Config->ConfValue("securelist");
		exemptregistered = tag->getBool("exemptregistered", true);
		fakechans = tag->getNum<unsigned long>("fakechans", 5, 0);
		fakechanprefix = tag->getString("fakechanprefix", "#", 1, ServerInstance->Config->Limits.MaxChannel - 1);
		fakechantopic = tag->getString("fakechantopic", "Fake channel for confusing spambots", 1, ServerInstance->Config->Limits.MaxTopic - 1);
		hidesmallchans = tag->getNum<size_t>("hidesmallchans", 0);
		showmsg = tag->getBool("showmsg", true);
		waittime = tag->getDuration("waittime", 60, !exemptregistered, 60*60*24);

		allowlist.swap(newallows);
	}

	ModResult OnPreCommand(std::string& command, CommandBase::Params& parameters, LocalUser* user, bool validated) override
	{
		// Ignore unless the command is a validated LIST command from a non-exempt user.
		if (!validated || command != "LIST" || IsExempt(user))
			return MOD_RES_PASSTHRU;

		// Allow if the wait time has passed.
		time_t maxwaittime = user->signon + waittime;
		if (waittime && ServerInstance->Time() > maxwaittime)
			return MOD_RES_PASSTHRU;

		// If <securehost:showmsg> is set then tell the user that they need to wait.
		if (showmsg)
		{
			if (waittime)
			{
				user->WriteNotice("*** You cannot view the channel list right now. Please {}try again in {}.",
					exemptregistered ? "log in to an account or " : "",
					Duration::ToString(maxwaittime - ServerInstance->Time()));
			}
			else
			{
				user->WriteNotice("*** You must be logged into an account to view the channel list.");
			}
		}

		// The client might be waiting on a response to do something so send them an
		// fake list response to satisfy that.
		size_t maxfakesuffix = ServerInstance->Config->Limits.MaxChannel - fakechanprefix.size();
		sendingfakelist = true;

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

		sendingfakelist = false;
		return MOD_RES_DENY;
	}

	ModResult OnNumeric(User* user, const Numeric::Numeric& numeric) override
	{
		if (numeric.GetNumeric() != RPL_LIST || numeric.GetParams().size() < 2)
			return MOD_RES_PASSTHRU; // The numeric isn't the one we care about.

		if (sendingfakelist || IsExempt(user))
			return MOD_RES_PASSTHRU; // This numeric should be shown even if too small.

		// If the channel has less than the minimum amount of users then hide it from /LIST.
		auto usercount = ConvToNum<size_t>(numeric.GetParams()[1]);
		return usercount < hidesmallchans ? MOD_RES_DENY : MOD_RES_PASSTHRU;
	}

	void OnBuildISupport(ISupport::TokenMap& tokens) override
	{
		if (showmsg)
			tokens["SECURELIST"] = ConvToStr(waittime);
	}
};

MODULE_INIT(ModuleSecureList)
