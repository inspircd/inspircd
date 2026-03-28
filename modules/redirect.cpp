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
#include "modules/extban.h"
#include "numerichelper.h"

enum
{
	// From UnrealIRCd?
	ERR_LINKCHANNEL = 470,

	// InspIRCd-specific.
	ERR_REDIRECT = 690,
};

namespace
{
	Channel* CheckRedirect(User* source, Channel* channel, const std::string& parameter)
	{
		if (!ServerInstance->Channels.IsChannel(parameter))
		{
			source->WriteNumeric(Numerics::NoSuchChannel(parameter));
			return nullptr;
		}

		auto* c = ServerInstance->Channels.Find(parameter);
		if (!source->IsOper())
		{
			if (!c)
			{
				source->WriteNumeric(ERR_REDIRECT, parameter, FMT::format("Target channel {} must exist to be set as a redirect.", parameter));
				return nullptr;
			}
			else if (c->GetPrefixValue(source) < OP_VALUE)
			{
				source->WriteNumeric(ERR_REDIRECT, c->name, FMT::format("You must be opped on {} to set it as a redirect.", c->name));
				return nullptr;
			}
		}
		return c;
	}
}

class RedirectExtBan final
	: public ExtBan::Acting
{
private:
	bool SplitBan(const std::string& text, std::string& chan, std::string& mask)
	{
		auto sep = text.find(':');
		if (sep == std::string::npos || sep == 0 || sep+1 >= text.length())
			return false; // Malformed.

		chan.assign(text, 0, sep);
		mask.assign(text, sep + 1);
		return true;
	}

public:
	std::string matchchan;

	RedirectExtBan(const WeakModulePtr& mod)
		: ExtBan::Acting(mod, "redirect", 'd')
	{
	}

	bool IsMatch(ListModeBase* lm, User* user, Channel* channel, const std::string& text, const ExtBan::MatchConfig& config) override
	{
		std::string target, mask;
		if (!SplitBan(text, target, mask))
			return false; // Malformed.

		matchchan = target;
		return ExtBan::Acting::IsMatch(lm, user, channel, mask, config);
	}

	bool Validate(ListModeBase* lm, LocalUser* user, Channel* channel, std::string& text) override
	{
		std::string target, mask;
		if (!SplitBan(text, target, mask))
		{
			user->WriteNumeric(ERR_REDIRECT, text, "Redirect extban must be in the format <chan>:<mask>.");
			return false; // Malformed.
		}

		auto* targetchan = CheckRedirect(user, channel, target);
		if (!targetchan)
			return false; // Bad target.

		Canonicalize(mask);
		text = FMT::format("{}:{}", targetchan->name, mask);
		return true;
	}
};

class RedirectMode final
	: public ParamMode<RedirectMode, StringExtItem>
{
public:
	RedirectMode(const WeakModulePtr& mod)
		: ParamMode<RedirectMode, StringExtItem>(mod, "redirect", 'L')
	{
		syntax = "<target>";
	}

	bool OnSet(User* source, Channel* channel, std::string& parameter) override
	{
		if (source->IsLocal())
		{
			auto* targetchan = CheckRedirect(source, channel, parameter);
			if (!targetchan)
				return false;

			parameter = targetchan->name;
		}

		ext.Set(channel, parameter);
		return true;
	}

	void SerializeParam(Channel* chan, const std::string* str, std::string& out)
	{
		out += *str;
	}
};

class BanRedirect final
	: public ModeWatcher
{
private:
	RedirectExtBan& redirectextban;

public:
	BanRedirect(const WeakModulePtr& mod, RedirectExtBan& extban)
		: ModeWatcher(mod, "ban", MODETYPE_CHANNEL)
		, redirectextban(extban)
	{
	}

	bool BeforeMode(User* source, User* dest, Channel* channel, Modes::Change& change) override
	{
		if (!change.adding || change.param.empty() || !source->IsLocal())
			return true; // Nothing to do.

		bool xbinverted; // Unused
		std::string xbname, xbvalue; // Unused
		if (ExtBan::Parse(change.param, xbname, xbvalue, xbinverted))
			return true; // Don't touch extbans

		const auto pos_of_pling = change.param.find_first_of('!');
		const auto pos_of_at = change.param.find_first_of('@', pos_of_pling == std::string::npos ? 0 : pos_of_pling);
		const auto pos_of_hash = change.param.find_first_of('#', pos_of_at == std::string::npos ? 0 : pos_of_at);
		if (pos_of_hash != std::string::npos)
		{
			// Rewrite old banredirect entries from foo!bar@baz#chan to redirect:#chan:foo!bar@baz
			change.param = FMT::format("{}:{}:{}", redirectextban.service_name,
				change.param.substr(pos_of_hash), change.param.substr(0, pos_of_hash));
		}
		return true;
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
	ChanModeReference banmode;
	ChanModeReference inviteonlymode;
	ChanModeReference keymode;
	ChanModeReference limitmode;
	RedirectExtBan redirectextban;
	RedirectMode redirectmode;
	BanRedirect banredirect;

	ModResult HandleRedirect(LocalUser* user, Channel* chan, const std::string& reason, bool param = true)
	{
		const auto* channel = param ? redirectmode.ext.Get(chan) : &redirectextban.matchchan;
		if (!channel)
			return MOD_RES_PASSTHRU; // Should never happen.

		// Sometimes broken services can make circular or chained +L, avoid this.
		if (!activechan.empty())
		{
			user->WriteNumeric(ERR_LINKCHANNEL, activechan, channel, "You may not join this channel. A redirect is set, but you cannot be redirected as it is a circular loop.");
			return MOD_RES_PASSTHRU;
		}

		if (user->IsModeSet(antiredirectmode))
		{
			user->WriteNumeric(ERR_LINKCHANNEL, chan->name, *channel,
				FMT::format("You cannot join {} ({}) and you cannot be redirected to {} as you have user mode +{} ({}) set.",
					chan->name, reason, *channel, antiredirectmode.GetModeChar(), antiredirectmode.service_name));
			return MOD_RES_DENY;
		}

		user->WriteNumeric(ERR_LINKCHANNEL, chan->name, *channel,
			FMT::format("You cannot join {} ({}) so you are being automatically transferred to {}.",
				chan->name, reason, *channel));

		activechan = chan->name;
		Channel::JoinUser(user, *channel);
		activechan.clear();

		return MOD_RES_DENY;
	}

public:
	ModuleRedirect()
		: Module(VF_VENDOR, "Allows users to be redirected to another channel.")
		, antiredirectmode(weak_from_this(), "antiredirect", 'L')
		, banmode(weak_from_this(), "ban")
		, inviteonlymode(weak_from_this(), "inviteonly")
		, keymode(weak_from_this(), "key")
		, limitmode(weak_from_this(), "limit")
		, redirectextban(weak_from_this())
		, redirectmode(weak_from_this())
		, banredirect(weak_from_this(), redirectextban)
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

	ModResult OnUserPreJoin(LocalUser* user, Channel* chan, const std::string& cname, PrefixMode::Set& privs, const std::string& keygiven, bool override) override
	{
		if (override || !chan)
			return MOD_RES_PASSTHRU; // No redirect possible.

		auto modres = redirectextban.GetStatus(user, chan);
		if (modres == MOD_RES_DENY)
			return HandleRedirect(user, chan, "you're extbanned", false);

		if (!chan->IsModeSet(redirectmode))
			return MOD_RES_PASSTHRU; // All others require the mode.

		if (action_ban && chan->CheckList(*banmode, user))
			return HandleRedirect(user, chan, "you're banned");

		if (action_inviteonly && chan->IsModeSet(inviteonlymode))
		{
			FIRST_MOD_RESULT(OnCheckInvite, modres, (user, chan));
			if (modres != MOD_RES_ALLOW)
				return HandleRedirect(user, chan, "invite only");
		}

		if (action_key)
		{
			const auto key = chan->GetModeParameter(keymode);
			if (!key.empty())
			{
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
				FIRST_MOD_RESULT(OnCheckLimit, modres, (user, chan));
				if (!modres.check(chan->GetUsers().size() < ConvToNum<size_t>(limit)))
					return HandleRedirect(user, chan, "limit reached");
			}
		}

		return MOD_RES_PASSTHRU;
	}
};

MODULE_INIT(ModuleRedirect)
