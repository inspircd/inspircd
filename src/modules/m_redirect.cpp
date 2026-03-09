/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2019-2023 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2017 Dylan Frank <b00mx0r@aureus.pw>
 *   Copyright (C) 2012-2014, 2016 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012, 2019 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2012, 2014 Shawn Smith <ShawnSmith0828@gmail.com>
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2008 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2006 Craig Edwards <brain@inspircd.org>
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
#include "extension.h"
#include "numerichelper.h"

enum
{
	// From UnrealIRCd?
	ERR_LINKCHANNEL = 470,

	// InspIRCd-specific.
	ERR_REDIRECT = 690,
};

class RedirectMode final
	: public ParamMode<RedirectMode, StringExtItem>
{
public:
	RedirectMode(Module* mod)
		: ParamMode<RedirectMode, StringExtItem>(mod, "redirect", 'L')
	{
		syntax = "<target>";
	}

	bool OnSet(User* source, Channel* channel, std::string& parameter) override
	{
		if (IS_LOCAL(source))
		{
			if (!ServerInstance->Channels.IsChannel(parameter))
			{
				source->WriteNumeric(Numerics::NoSuchChannel(parameter));
				return false;
			}
			if (!source->IsOper())
			{
				auto* c = ServerInstance->Channels.Find(parameter);
				if (!c)
				{
					source->WriteNumeric(ERR_REDIRECT, parameter, INSP_FORMAT("Target channel {} must exist to be set as a redirect.", parameter));
					return false;
				}
				else if (c->GetPrefixValue(source) < OP_VALUE)
				{
					source->WriteNumeric(ERR_REDIRECT, c->name, INSP_FORMAT("You must be opped on {} to set it as a redirect.", c->name));
					return false;
				}
			}
		}

		ext.Set(channel, parameter);
		return true;
	}

	void SerializeParam(Channel* chan, const std::string* str, std::string& out)
	{
		out += *str;
	}
};

class ModuleRedirect final
	: public Module
{
private:
	bool action_ban;
	bool action_inviteonly;
	bool action_key;
	bool action_limit;
	std::string activechan;
	SimpleUserMode antiredirectmode;
	ChanModeReference inviteonlymode;
	ChanModeReference keymode;
	ChanModeReference limitmode;
	RedirectMode redirectmode;

	ModResult HandleRedirect(LocalUser* user, Channel* chan, const std::string& reason)
	{
		const auto* channel = redirectmode.ext.Get(chan);
		if (!channel)
			return MOD_RES_PASSTHRU; // Should never happen.

		if (user->IsModeSet(antiredirectmode))
		{
			user->WriteNumeric(ERR_LINKCHANNEL, chan->name, *channel,
				INSP_FORMAT("You cannot join {} ({}) and you cannot be redirected to {} as you have user mode +{} ({}) set.",
					chan->name, reason, *channel, antiredirectmode.GetModeChar(), antiredirectmode.name));
			return MOD_RES_DENY;
		}

		user->WriteNumeric(ERR_LINKCHANNEL, chan->name, *channel,
			INSP_FORMAT("You cannot join {} ({}) so you are being automatically transferred to {}.",
				chan->name, reason, *channel));

		activechan = chan->name;
		Channel::JoinUser(user, *channel);
		activechan.clear();

		return MOD_RES_DENY;
	}

public:
	ModuleRedirect()
		: Module(VF_VENDOR, "Allows users to be redirected to another channel.")
		, antiredirectmode(this, "antiredirect", 'L')
		, inviteonlymode(this, "inviteonly")
		, keymode(this, "key")
		, limitmode(this, "limit")
		, redirectmode(this)
	{
	}

	void ReadConfig(ConfigStatus&) override
	{
		const auto& tag = ServerInstance->Config->ConfValue("redirect");
		action_ban = tag->getBool("ban", false);
		action_inviteonly = tag->getBool("inviteonly", true);
		action_key = tag->getBool("key", true);
		action_limit = tag->getBool("limit", true);
	}

	ModResult OnUserPreJoin(LocalUser* user, Channel* chan, const std::string& cname, std::string& privs, const std::string& keygiven, bool override) override
	{
		if (override || !chan || !chan->IsModeSet(redirectmode))
			return MOD_RES_PASSTHRU; // No redirect possible.

		// Sometimes broken services can make circular or chained +L, avoid this.
		if (!activechan.empty())
		{
			user->WriteNumeric(ERR_LINKCHANNEL, activechan, cname, "You may not join this channel. A redirect is set, but you cannot be redirected as it is a circular loop.");
			return MOD_RES_PASSTHRU;
		}

		if (action_ban && chan->IsBanned(user))
			return HandleRedirect(user, chan, "you're banned");

		if (action_inviteonly && chan->IsModeSet(inviteonlymode))
		{
			ModResult modres;
			FIRST_MOD_RESULT(OnCheckInvite, modres, (user, chan));
			if (modres != MOD_RES_ALLOW)
				return HandleRedirect(user, chan, "invite only");
		}

		if (action_key)
		{
			const auto key = chan->GetModeParameter(keymode);
			if (!key.empty())
			{
				ModResult modres;
				FIRST_MOD_RESULT(OnCheckKey, modres, (user, chan, keygiven));
				if (!modres.check(InspIRCd::TimingSafeCompare(key, keygiven)))
					return HandleRedirect(user, chan, "incorrect channel key");
			}
		}

		if (action_limit)
		{
			const auto limit = chan->GetModeParameter(limitmode);
			if (!limit.empty())
			{
				ModResult modres;
				FIRST_MOD_RESULT(OnCheckLimit, modres, (user, chan));
				if (!modres.check(chan->GetUsers().size() < ConvToNum<size_t>(limit)))
					return HandleRedirect(user, chan, "limit reached");
			}
		}

		return MOD_RES_PASSTHRU;
	}
};

MODULE_INIT(ModuleRedirect)
