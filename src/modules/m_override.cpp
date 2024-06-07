/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2020 satmd <satmd@satmd.de>
 *   Copyright (C) 2017 B00mX0r <b00mx0r@aureus.pw>
 *   Copyright (C) 2013, 2019-2023 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2013 Daniel Vassdal <shutter@canternet.org>
 *   Copyright (C) 2012-2015 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2008 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2007-2008 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2006, 2008 Craig Edwards <brain@inspircd.org>
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
#include "modules/invite.h"
#include "modules/isupport.h"

class UnsetTimer final
	: public Timer
{
private:
	ModeHandler& overridemode;
	LocalUser* user;

public:
	UnsetTimer(LocalUser* u, unsigned long timeout, ModeHandler& om)
		: Timer(timeout, false)
		, overridemode(om)
		, user(u)
	{
		ServerInstance->Timers.AddTimer(this);
	}

	bool Tick() override
	{
		if (!user->quitting && user->IsModeSet(overridemode))
		{
			Modes::ChangeList changelist;
			changelist.push_remove(&overridemode);
			ServerInstance->Modes.Process(ServerInstance->FakeClient, nullptr, user, changelist);
		}
		return false;
	}
};

class Override final
	: public SimpleUserMode
{
public:
	SimpleExtItem<UnsetTimer> ext;
	unsigned long timeout;

	Override(Module* Creator)
		: SimpleUserMode(Creator, "override", 'O', true)
		, ext(Creator, "override-timer", ExtensionType::USER)
	{
	}

	bool OnModeChange(User* source, User* dest, Channel* channel, Modes::Change& change) override
	{
		bool res = SimpleUserMode::OnModeChange(source, dest, channel, change);
		if (change.adding && res && IS_LOCAL(dest) && timeout)
			ext.Set(dest, new UnsetTimer(IS_LOCAL(dest), timeout, *this));
		return res;
	}
};

class ModuleOverride final
	: public Module
	, public ISupport::EventListener
{
private:
	bool RequireKey;
	bool NoisyOverride;
	Override ou;
	ChanModeReference topiclock;
	ChanModeReference inviteonly;
	ChanModeReference key;
	ChanModeReference limit;
	Invite::API invapi;

	static bool IsOverride(ModeHandler::Rank userlevel, const Modes::ChangeList::List& list)
	{
		for (const auto& change : list)
		{
			if (change.mh->GetLevelRequired(change.adding) > userlevel)
				return true;
		}
		return false;
	}

	ModResult HandleJoinOverride(LocalUser* user, Channel* chan, const std::string& keygiven, const char* bypasswhat, const char* mode) const
	{
		if (RequireKey && keygiven != "override")
		{
			// Can't join normally -- must use a special key to bypass restrictions
			user->WriteNotice("*** You may not join normally. You must join with a key of 'override' to oper override.");
			return MOD_RES_PASSTHRU;
		}

		if (NoisyOverride)
			chan->WriteRemoteNotice(INSP_FORMAT("{} used oper override to bypass {}", user->nick, bypasswhat));
		ServerInstance->SNO.WriteGlobalSno('v', user->nick+" used oper override to bypass " + mode + " on " + chan->name);
		return MOD_RES_ALLOW;
	}

public:
	ModuleOverride()
		: Module(VF_VENDOR, "Allows server operators to be given privileges that allow them to ignore various channel-level restrictions.")
		, ISupport::EventListener(this)
		, ou(this)
		, topiclock(this, "topiclock")
		, inviteonly(this, "inviteonly")
		, key(this, "key")
		, limit(this, "limit")
		, invapi(this)
	{
	}

	void init() override
	{
		ServerInstance->SNO.EnableSnomask('v', "OVERRIDE");
	}

	void ReadConfig(ConfigStatus& status) override
	{
		// re-read our config options
		const auto& tag = ServerInstance->Config->ConfValue("override");
		NoisyOverride = tag->getBool("noisy");
		RequireKey = tag->getBool("requirekey");
		ou.timeout = tag->getDuration("timeout", 0);
	}

	void OnBuildISupport(ISupport::TokenMap& tokens) override
	{
		tokens["OVERRIDE"] = ConvToStr(ou.GetModeChar());
	}

	bool CanOverride(User* source, const char* token)
	{
		// The oper override umode (+O) is not set
		if (!source->IsModeSet(ou))
			return false;

		return TokenList(source->oper->GetConfig()->getString("override")).Contains(token);
	}

	ModResult OnPreTopicChange(User* source, Channel* channel, const std::string& topic) override
	{
		if (IS_LOCAL(source) && source->IsOper() && CanOverride(source, "TOPIC"))
		{
			if (!channel->HasUser(source) || (channel->IsModeSet(topiclock) && channel->GetPrefixValue(source) < HALFOP_VALUE))
			{
				ServerInstance->SNO.WriteGlobalSno('v', source->nick + " used oper override to change a topic on " + channel->name);
			}

			// Explicit allow
			return MOD_RES_ALLOW;
		}

		return MOD_RES_PASSTHRU;
	}

	ModResult OnUserPreKick(User* source, Membership* memb, const std::string& reason) override
	{
		if (source->IsOper() && CanOverride(source, "KICK"))
		{
			// If the kicker's status is less than the target's,			or	the kicker's status is less than or equal to voice
			if ((memb->chan->GetPrefixValue(source) < memb->GetRank()) || (memb->chan->GetPrefixValue(source) <= VOICE_VALUE) ||
				(memb->chan->GetPrefixValue(source) == HALFOP_VALUE && memb->GetRank() == HALFOP_VALUE))
			{
				ServerInstance->SNO.WriteGlobalSno('v', source->nick + " used oper override to kick " + memb->user->nick + " on " + memb->chan->name + " (" + reason + ")");
				return MOD_RES_ALLOW;
			}
		}
		return MOD_RES_PASSTHRU;
	}

	ModResult OnPreMode(User* source, User* dest, Channel* channel, Modes::ChangeList& modes) override
	{
		if (!channel)
			return MOD_RES_PASSTHRU;
		if (!source->IsOper() || !IS_LOCAL(source))
			return MOD_RES_PASSTHRU;

		const Modes::ChangeList::List& list = modes.getlist();
		ModeHandler::Rank mode = channel->GetPrefixValue(source);

		if (!IsOverride(mode, list))
			return MOD_RES_PASSTHRU;

		if (CanOverride(source, "MODE"))
		{
			std::string msg = source->nick + " used oper override to set modes on " + channel->name + ": ";

			// Construct a MODE string in the old format for sending it as a snotice
			std::string params;
			char pm = 0;
			for (const auto& item : list)
			{
				if (!item.param.empty())
					params.append(1, ' ').append(item.param);

				char wanted_pm = (item.adding ? '+' : '-');
				if (wanted_pm != pm)
				{
					pm = wanted_pm;
					msg += pm;
				}

				msg += item.mh->GetModeChar();
			}
			msg += params;
			ServerInstance->SNO.WriteGlobalSno('v', msg);
			return MOD_RES_ALLOW;
		}
		return MOD_RES_PASSTHRU;
	}

	ModResult OnUserPreJoin(LocalUser* user, Channel* chan, const std::string& cname, std::string& privs, const std::string& keygiven, bool override) override
	{
		if (user->IsOper() && !override)
		{
			if (chan)
			{
				if (chan->IsModeSet(inviteonly) && (CanOverride(user, "INVITE")))
				{
					if (!invapi->IsInvited(user, chan))
						return HandleJoinOverride(user, chan, keygiven, "invite-only", "+i");
					return MOD_RES_ALLOW;
				}

				if (chan->IsModeSet(key) && (CanOverride(user, "KEY")) && keygiven != chan->GetModeParameter(key))
					return HandleJoinOverride(user, chan, keygiven, "the channel key", "+k");

				if (chan->IsModeSet(limit) && (chan->GetUsers().size() >= ConvToNum<size_t>(chan->GetModeParameter(limit))) && (CanOverride(user, "LIMIT")))
					return HandleJoinOverride(user, chan, keygiven, "the channel limit", "+l");

				if (chan->IsBanned(user) && CanOverride(user, "BANWALK"))
					return HandleJoinOverride(user, chan, keygiven, "channel ban", "channel ban");
			}
		}
		return MOD_RES_PASSTHRU;
	}
};

MODULE_INIT(ModuleOverride)
