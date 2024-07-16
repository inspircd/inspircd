/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2020 iwalkalone <iwalkalone69@gmail.com>
 *   Copyright (C) 2018 linuxdaemon <linuxdaemon.irc@gmail.com>
 *   Copyright (C) 2017 B00mX0r <b00mx0r@aureus.pw>
 *   Copyright (C) 2016, 2019 Matt Schatz <genius3000@g3k.solutions>
 *   Copyright (C) 2013, 2017, 2019-2023 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2013 Daniel Vassdal <shutter@canternet.org>
 *   Copyright (C) 2012-2016 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2009 John Brooks <john@jbrooks.io>
 *   Copyright (C) 2008 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2005, 2007-2008 Craig Edwards <brain@inspircd.org>
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
#include "listmode.h"
#include "modules/extban.h"
#include "numerichelper.h"
#include "timeutils.h"

// Holds a timed ban
class TimedBan final
{
public:
	std::string mask;
	std::string setter;
	time_t expire;
	Channel* chan;
};

typedef std::vector<TimedBan> timedbans;
timedbans TimedBanList;

class CommandTban final
	: public Command
{
private:
	ChanModeReference banmode;
	ExtBan::ManagerRef extbanmgr;

	bool IsBanSet(Channel* chan, const std::string& mask)
	{
		ListModeBase* banlm = static_cast<ListModeBase*>(*banmode);
		if (!banlm)
			return false;

		const ListModeBase::ModeList* bans = banlm->GetList(chan);
		if (bans)
		{
			for (const auto& ban : *bans)
			{
				if (banlm->CompareEntry(ban.mask, mask))
					return true;
			}
		}

		return false;
	}

public:
	bool sendnotice;

	CommandTban(Module* Creator)
		: Command(Creator, "TBAN", 3)
		, banmode(Creator, "ban")
		, extbanmgr(Creator)
	{
		syntax = { "<channel> <duration> <banmask>" };
	}

	CmdResult Handle(User* user, const Params& parameters) override
	{
		auto* channel = ServerInstance->Channels.Find(parameters[0]);
		if (!channel)
		{
			user->WriteNumeric(Numerics::NoSuchChannel(parameters[0]));
			return CmdResult::FAILURE;
		}

		ModeHandler::Rank cm = channel->GetPrefixValue(user);
		if (cm < HALFOP_VALUE)
		{
			user->WriteNumeric(Numerics::ChannelPrivilegesNeeded(channel, HALFOP_VALUE, "set timed bans"));
			return CmdResult::FAILURE;
		}

		TimedBan T;
		unsigned long duration;
		if (!Duration::TryFrom(parameters[1], duration))
		{
			user->WriteNotice("Invalid ban time");
			return CmdResult::FAILURE;
		}
		unsigned long expire = duration + ServerInstance->Time();

		std::string mask = parameters[2];
		if (!extbanmgr || !extbanmgr->Canonicalize(mask))
			ModeParser::CleanMask(mask);

		if (IsBanSet(channel, mask))
		{
			user->WriteNotice("Ban already set");
			return CmdResult::FAILURE;
		}

		Modes::ChangeList setban;
		setban.push_add(*banmode, mask);

		// Pass the user (instead of ServerInstance->FakeClient) to ModeHandler::Process() to
		// make it so that the user sets the mode themselves
		ServerInstance->Modes.Process(user, channel, nullptr, setban);
		if (ServerInstance->Modes.GetLastChangeList().empty())
		{
			user->WriteNotice("Invalid ban mask");
			return CmdResult::FAILURE;
		}

		// Attempt to find the actual set ban mask.
		for (const auto& mc : ServerInstance->Modes.GetLastChangeList().getlist())
		{
			if (mc.mh == *banmode)
			{
				// We found the actual mask.
				mask = mc.param;
				break;
			}
		}

		T.mask = mask;
		T.setter = user->nick;
		T.expire = expire + (IS_REMOTE(user) ? 5 : 0);
		T.chan = channel;
		TimedBanList.push_back(T);

		if (sendnotice)
		{
			const std::string message = fmt::format("Timed ban {} added by {} on {} lasting for {}.",
				mask, user->nick, channel->name, Duration::ToString(duration));

			// If halfop is loaded, send notice to halfops and above, otherwise send to ops and above
			PrefixMode* mh = ServerInstance->Modes.FindNearestPrefixMode(HALFOP_VALUE);
			char pfxchar = mh ? mh->GetPrefix() : '@';

			channel->WriteRemoteNotice(message, pfxchar);
		}

		return CmdResult::SUCCESS;
	}

	RouteDescriptor GetRouting(User* user, const Params& parameters) override
	{
		return ROUTE_BROADCAST;
	}
};

class BanWatcher final
	: public ModeWatcher
{
public:
	BanWatcher(Module* parent)
		: ModeWatcher(parent, "ban", MODETYPE_CHANNEL)
	{
	}

	void AfterMode(User* source, User* dest, Channel* chan, const Modes::Change& change) override
	{
		if (change.adding)
			return;

		for (timedbans::iterator i = TimedBanList.begin(); i != TimedBanList.end(); ++i)
		{
			if (i->chan != chan)
				continue;

			const std::string& target = i->mask;
			if (irc::equals(change.param, target))
			{
				TimedBanList.erase(i);
				break;
			}
		}
	}
};

class ModuleTimedBans final
	: public Module
{
private:
	ChanModeReference banmode;
	CommandTban cmd;
	BanWatcher banwatcher;

public:
	ModuleTimedBans()
		: Module(VF_VENDOR | VF_COMMON, "Adds the /TBAN command which allows channel operators to add bans which will be expired after the specified period.")
		, banmode(this, "ban")
		, cmd(this)
		, banwatcher(this)
	{
	}

	void ReadConfig(ConfigStatus& status) override
	{
		const auto& tag = ServerInstance->Config->ConfValue("timedbans");
		cmd.sendnotice = tag->getBool("sendnotice", true);
	}

	void OnBackgroundTimer(time_t curtime) override
	{
		timedbans expired;
		for (timedbans::iterator i = TimedBanList.begin(); i != TimedBanList.end();)
		{
			if (curtime > i->expire)
			{
				expired.push_back(*i);
				i = TimedBanList.erase(i);
			}
			else
				++i;
		}

		for (const auto& timedban : expired)
		{
			if (cmd.sendnotice)
			{
				const std::string message = fmt::format("Timed ban {} set by {} on {} has expired.",
					timedban.mask, timedban.setter, timedban.chan->name);

				// If halfop is loaded, send notice to halfops and above, otherwise send to ops and above
				PrefixMode* mh = ServerInstance->Modes.FindNearestPrefixMode(HALFOP_VALUE);
				char pfxchar = mh ? mh->GetPrefix() : '@';

				timedban.chan->WriteRemoteNotice(message, pfxchar);
			}

			Modes::ChangeList setban;
			setban.push_remove(*banmode, timedban.mask);
			ServerInstance->Modes.Process(ServerInstance->FakeClient, timedban.chan, nullptr, setban);
		}
	}

	void OnChannelDelete(Channel* chan) override
	{
		// Remove all timed bans affecting the channel from internal bookkeeping
		std::erase_if(TimedBanList, [chan](const TimedBan& tb) { return tb.chan == chan; });
	}
};

MODULE_INIT(ModuleTimedBans)
