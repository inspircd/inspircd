/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2009 Uli Schlachter <psychon@znc.in>
 *   Copyright (C) 2007-2009 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2007-2008 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2008 Pippijn van Steenhoven <pip88nl@gmail.com>
 *   Copyright (C) 2008 Geoff Bricker <geoff.bricker@gmail.com>
 *   Copyright (C) 2004-2006 Craig Edwards <craigedwards@brainbox.cc>
 *   Copyright (C) 2006 Oliver Lupton <oliverlupton@gmail.com>
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
#include "modules/invite.h"

class ModuleOverride : public Module
{
	bool RequireKey;
	bool NoisyOverride;
	ChanModeReference topiclock;
	ChanModeReference inviteonly;
	ChanModeReference key;
	ChanModeReference limit;
	Invite::API invapi;

	static bool IsOverride(unsigned int userlevel, const Modes::ChangeList::List& list)
	{
		for (Modes::ChangeList::List::const_iterator i = list.begin(); i != list.end(); ++i)
		{
			ModeHandler* mh = i->mh;
			if (mh->GetLevelRequired() > userlevel)
				return true;
		}
		return false;
	}

	ModResult HandleJoinOverride(LocalUser* user, Channel* chan, const std::string& keygiven, const char* bypasswhat, const char* mode)
	{
		if (RequireKey && keygiven != "override")
		{
			// Can't join normally -- must use a special key to bypass restrictions
			user->WriteNotice("*** You may not join normally. You must join with a key of 'override' to oper override.");
			return MOD_RES_PASSTHRU;
		}

		if (NoisyOverride)
			chan->WriteNotice(InspIRCd::Format("%s used oper override to bypass %s", user->nick.c_str(), bypasswhat));
		ServerInstance->SNO->WriteGlobalSno('v', user->nick+" used oper override to bypass " + mode + " on " + chan->name);
		return MOD_RES_ALLOW;
	}

 public:
	ModuleOverride()
		: topiclock(this, "topiclock")
		, inviteonly(this, "inviteonly")
		, key(this, "key")
		, limit(this, "limit")
		, invapi(this)
	{
	}

	void init() CXX11_OVERRIDE
	{
		ServerInstance->SNO->EnableSnomask('v', "OVERRIDE");
	}

	void ReadConfig(ConfigStatus& status) CXX11_OVERRIDE
	{
		// re-read our config options
		ConfigTag* tag = ServerInstance->Config->ConfValue("override");
		NoisyOverride = tag->getBool("noisy");
		RequireKey = tag->getBool("requirekey");
	}

	void On005Numeric(std::map<std::string, std::string>& tokens) CXX11_OVERRIDE
	{
		tokens["OVERRIDE"];
	}

	bool CanOverride(User* source, const char* token)
	{
		std::string tokenlist = source->oper->getConfig("override");

		// its defined or * is set, return its value as a boolean for if the token is set
		return ((tokenlist.find(token, 0) != std::string::npos) || (tokenlist.find("*", 0) != std::string::npos));
	}


	ModResult OnPreTopicChange(User *source, Channel *channel, const std::string &topic) CXX11_OVERRIDE
	{
		if (IS_LOCAL(source) && source->IsOper() && CanOverride(source, "TOPIC"))
		{
			if (!channel->HasUser(source) || (channel->IsModeSet(topiclock) && channel->GetPrefixValue(source) < HALFOP_VALUE))
			{
				ServerInstance->SNO->WriteGlobalSno('v',source->nick+" used oper override to change a topic on "+channel->name);
			}

			// Explicit allow
			return MOD_RES_ALLOW;
		}

		return MOD_RES_PASSTHRU;
	}

	ModResult OnUserPreKick(User* source, Membership* memb, const std::string &reason) CXX11_OVERRIDE
	{
		if (source->IsOper() && CanOverride(source,"KICK"))
		{
			// If the kicker's status is less than the target's,			or	the kicker's status is less than or equal to voice
			if ((memb->chan->GetPrefixValue(source) < memb->getRank()) || (memb->chan->GetPrefixValue(source) <= VOICE_VALUE) ||
			    (memb->chan->GetPrefixValue(source) == HALFOP_VALUE && memb->getRank() == HALFOP_VALUE))
			{
				ServerInstance->SNO->WriteGlobalSno('v',source->nick+" used oper override to kick "+memb->user->nick+" on "+memb->chan->name+" ("+reason+")");
				return MOD_RES_ALLOW;
			}
		}
		return MOD_RES_PASSTHRU;
	}

	ModResult OnPreMode(User* source, User* dest, Channel* channel, Modes::ChangeList& modes) CXX11_OVERRIDE
	{
		if (!channel)
			return MOD_RES_PASSTHRU;
		if (!source->IsOper() || !IS_LOCAL(source))
			return MOD_RES_PASSTHRU;

		const Modes::ChangeList::List& list = modes.getlist();
		unsigned int mode = channel->GetPrefixValue(source);

		if (!IsOverride(mode, list))
			return MOD_RES_PASSTHRU;

		if (CanOverride(source, "MODE"))
		{
			std::string msg = source->nick + " overriding modes: ";

			// Construct a MODE string in the old format for sending it as a snotice
			std::string params;
			char pm = 0;
			for (Modes::ChangeList::List::const_iterator i = list.begin(); i != list.end(); ++i)
			{
				const Modes::Change& item = *i;
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
			ServerInstance->SNO->WriteGlobalSno('v',msg);
			return MOD_RES_ALLOW;
		}
		return MOD_RES_PASSTHRU;
	}

	ModResult OnUserPreJoin(LocalUser* user, Channel* chan, const std::string& cname, std::string& privs, const std::string& keygiven) CXX11_OVERRIDE
	{
		if (user->IsOper())
		{
			if (chan)
			{
				if (chan->IsModeSet(inviteonly) && (CanOverride(user,"INVITE")))
				{
					if (!invapi->IsInvited(user, chan))
						return HandleJoinOverride(user, chan, keygiven, "invite-only", "+i");
					return MOD_RES_ALLOW;
				}

				if (chan->IsModeSet(key) && (CanOverride(user,"KEY")) && keygiven != chan->GetModeParameter(key))
					return HandleJoinOverride(user, chan, keygiven, "the channel key", "+k");

				if (chan->IsModeSet(limit) && (chan->GetUserCounter() >= ConvToInt(chan->GetModeParameter(limit))) && (CanOverride(user,"LIMIT")))
					return HandleJoinOverride(user, chan, keygiven, "the channel limit", "+l");

				if (chan->IsBanned(user) && CanOverride(user,"BANWALK"))
					return HandleJoinOverride(user, chan, keygiven, "channel ban", "channel ban");
			}
		}
		return MOD_RES_PASSTHRU;
	}

	Version GetVersion() CXX11_OVERRIDE
	{
		return Version("Provides support for allowing opers to override certain things",VF_VENDOR);
	}
};

MODULE_INIT(ModuleOverride)
