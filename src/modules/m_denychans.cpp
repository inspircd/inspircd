/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2019 Matt Schatz <genius3000@g3k.solutions>
 *   Copyright (C) 2018-2024 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2013 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2008 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2007 Craig Edwards <brain@inspircd.org>
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

enum
{
	// InspIRCd-specific.
	ERR_BADCHANNEL = 926
};

struct BadChannel final
{
	bool allowopers;
	std::string name;
	std::string reason;
	std::string redirect;

	BadChannel(const std::string& Name, const std::string& Redirect, const std::string& Reason, bool AllowOpers)
		: allowopers(AllowOpers)
		, name(Name)
		, reason(Reason)
		, redirect(Redirect)
	{
	}
};

typedef std::vector<BadChannel> BadChannels;
typedef std::vector<std::string> GoodChannels;

class ModuleDenyChannels final
	: public Module
{
private:
	BadChannels badchannels;
	GoodChannels goodchannels;
	UserModeReference antiredirectmode;
	ChanModeReference redirectmode;

public:
	ModuleDenyChannels()
		: Module(VF_VENDOR, "Allows the server administrator to prevent users from joining channels matching a glob.")
		, antiredirectmode(this, "antiredirect")
		, redirectmode(this, "redirect")
	{
	}

	void ReadConfig(ConfigStatus& status) override
	{
		GoodChannels goodchans;

		for (const auto& [_, tag] : ServerInstance->Config->ConfTags("goodchan"))
		{
			// Ensure that we have the <goodchan:name> parameter.
			const std::string name = tag->getString("name");
			if (name.empty())
				throw ModuleException(this, "<goodchan:name> is a mandatory field, at " + tag->source.str());

			goodchans.push_back(name);
		}

		BadChannels badchans;
		for (const auto& [_, tag] : ServerInstance->Config->ConfTags("badchan"))
		{
			// Ensure that we have the <badchan:name> parameter.
			const std::string name = tag->getString("name");
			if (name.empty())
				throw ModuleException(this, "<badchan:name> is a mandatory field, at " + tag->source.str());

			// Ensure that we have the <badchan:reason> parameter.
			const std::string reason = tag->getString("reason");
			if (reason.empty())
				throw ModuleException(this, "<badchan:reason> is a mandatory field, at " + tag->source.str());

			const std::string redirect = tag->getString("redirect");
			if (!redirect.empty())
			{
				// Ensure that <badchan:redirect> contains a channel name.
				if (!ServerInstance->Channels.IsChannel(redirect))
					throw ModuleException(this, "<badchan:redirect> is not a valid channel name, at " + tag->source.str());

				// We defer the rest of the validation of the redirect channel until we have
				// finished parsing all of the badchans.
			}

			badchans.emplace_back(name, redirect, reason, tag->getBool("allowopers"));
		}

		// Now we have all of the badchan information recorded we can check that all redirect
		// channels can actually be redirected to.
		for (const auto& badchan : badchans)
		{
			// If there is no redirect channel we have nothing to do.
			if (badchan.redirect.empty())
				continue;

			// If the redirect channel is whitelisted then it is okay.
			bool whitelisted = false;
			for (const auto& goodchan : goodchans)
			{
				if (InspIRCd::Match(badchan.redirect, goodchan))
				{
					whitelisted = true;
					break;
				}
			}

			if (whitelisted)
				continue;

			// If the redirect channel is not blacklisted then it is okay.
			for (const auto& badchanredir : badchans)
			{
				if (InspIRCd::Match(badchan.redirect, badchanredir.name))
					throw ModuleException(this, "<badchan:redirect> cannot be a blacklisted channel name");
			}
		}

		// The config file contained no errors so we can apply the new configuration.
		badchannels.swap(badchans);
		goodchannels.swap(goodchans);
	}

	ModResult OnUserPreJoin(LocalUser* user, Channel* chan, const std::string& cname, std::string& privs, const std::string& keygiven, bool override) override
	{
		if (override)
			return MOD_RES_PASSTHRU;

		for (const auto& badchan : badchannels)
		{
			// If the channel does not match the current entry we have nothing else to do.
			if (!InspIRCd::Match(cname, badchan.name))
				continue;

			// If the user is an oper and opers are allowed to enter this blacklisted channel
			// then allow the join.
			if (user->IsOper() && badchan.allowopers)
				return MOD_RES_PASSTHRU;

			// If the channel matches a whitelist then allow the join.
			for (const auto& goodchan : goodchannels)
				if (InspIRCd::Match(cname, goodchan))
					return MOD_RES_PASSTHRU;

			// If there is no redirect chan, the user has enabled the antiredirect mode, or
			// the target channel redirects elsewhere we just tell the user and deny the join.
			Channel* target = nullptr;
			if (badchan.redirect.empty() || user->IsModeSet(antiredirectmode)
				|| ((target = ServerInstance->Channels.Find(badchan.redirect)) && target->IsModeSet(redirectmode)))
			{
				user->WriteNumeric(ERR_BADCHANNEL, cname, INSP_FORMAT("Channel {} is forbidden: {}", cname,
					badchan.reason));
				return MOD_RES_DENY;
			}

			// Redirect the user to the target channel.
			user->WriteNumeric(ERR_BADCHANNEL, cname, INSP_FORMAT("Channel {} is forbidden, redirecting to {}: {}",
				cname, badchan.redirect, badchan.reason));
			Channel::JoinUser(user, badchan.redirect);
			return MOD_RES_DENY;
		}
		return MOD_RES_PASSTHRU;
	}
};

MODULE_INIT(ModuleDenyChannels)
