/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2005-2008 Craig Edwards <craigedwards@brainbox.cc>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2006 Robin Burchell <robin+git@viroteck.net>
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

/** Holds a timed ban
 */
class TimedBan
{
 public:
	std::string mask;
	time_t expire;
	Channel* chan;
};

typedef std::vector<TimedBan> timedbans;
timedbans TimedBanList;

/** Handle /TBAN
 */
class CommandTban : public Command
{
	ChanModeReference banmode;

	bool IsBanSet(Channel* chan, const std::string& mask)
	{
		ListModeBase* banlm = static_cast<ListModeBase*>(*banmode);
		const ListModeBase::ModeList* bans = banlm->GetList(chan);
		if (bans)
		{
			for (ListModeBase::ModeList::const_iterator i = bans->begin(); i != bans->end(); ++i)
			{
				const ListModeBase::ListItem& ban = *i;
				if (!strcasecmp(ban.mask.c_str(), mask.c_str()))
					return true;
			}
		}

		return false;
	}

 public:
	CommandTban(Module* Creator) : Command(Creator,"TBAN", 3)
		, banmode(Creator, "ban")
	{
		syntax = "<channel> <duration> <banmask>";
	}

	CmdResult Handle (const std::vector<std::string> &parameters, User *user)
	{
		Channel* channel = ServerInstance->FindChan(parameters[0]);
		if (!channel)
		{
			user->WriteNumeric(Numerics::NoSuchNick(parameters[0]));
			return CMD_FAILURE;
		}
		int cm = channel->GetPrefixValue(user);
		if (cm < HALFOP_VALUE)
		{
			user->WriteNumeric(ERR_CHANOPRIVSNEEDED, channel->name, "You do not have permission to set bans on this channel");
			return CMD_FAILURE;
		}

		TimedBan T;
		unsigned long duration = InspIRCd::Duration(parameters[1]);
		unsigned long expire = duration + ServerInstance->Time();
		if (duration < 1)
		{
			user->WriteNotice("Invalid ban time");
			return CMD_FAILURE;
		}
		std::string mask = parameters[2];
		bool isextban = ((mask.size() > 2) && (mask[1] == ':'));
		if (!isextban && !InspIRCd::IsValidMask(mask))
			mask.append("!*@*");

		if (IsBanSet(channel, mask))
		{
			user->WriteNotice("Ban already set");
			return CMD_FAILURE;
		}

		Modes::ChangeList setban;
		setban.push_add(ServerInstance->Modes->FindMode('b', MODETYPE_CHANNEL), mask);
		// Pass the user (instead of ServerInstance->FakeClient) to ModeHandler::Process() to
		// make it so that the user sets the mode themselves
		ServerInstance->Modes->Process(user, channel, NULL, setban);
		if (ServerInstance->Modes->GetLastParse().empty())
		{
			user->WriteNotice("Invalid ban mask");
			return CMD_FAILURE;
		}

		CUList tmp;
		T.mask = mask;
		T.expire = expire + (IS_REMOTE(user) ? 5 : 0);
		T.chan = channel;
		TimedBanList.push_back(T);

		// If halfop is loaded, send notice to halfops and above, otherwise send to ops and above
		ModeHandler* mh = ServerInstance->Modes->FindMode('h', MODETYPE_CHANNEL);
		char pfxchar = (mh && mh->name == "halfop") ? '%' : '@';

		channel->WriteAllExcept(ServerInstance->FakeClient, true, pfxchar, tmp, "NOTICE %s :%s added a timed ban on %s lasting for %ld seconds.", channel->name.c_str(), user->nick.c_str(), mask.c_str(), duration);
		return CMD_SUCCESS;
	}

	RouteDescriptor GetRouting(User* user, const std::vector<std::string>& parameters)
	{
		return ROUTE_BROADCAST;
	}
};

class BanWatcher : public ModeWatcher
{
 public:
	BanWatcher(Module* parent)
		: ModeWatcher(parent, "ban", MODETYPE_CHANNEL)
	{
	}

	void AfterMode(User* source, User* dest, Channel* chan, const std::string& banmask, bool adding)
	{
		if (adding)
			return;

		for (timedbans::iterator i = TimedBanList.begin(); i != TimedBanList.end(); ++i)
		{
			if (i->chan != chan)
				continue;

			const std::string& target = i->mask;
			if (irc::equals(banmask, target))
			{
				TimedBanList.erase(i);
				break;
			}
		}
	}
};

class ChannelMatcher
{
	Channel* const chan;

 public:
	ChannelMatcher(Channel* ch)
		: chan(ch)
	{
	}

	bool operator()(const TimedBan& tb) const
	{
		return (tb.chan == chan);
	}
};

class ModuleTimedBans : public Module
{
	CommandTban cmd;
	BanWatcher banwatcher;

 public:
	ModuleTimedBans()
		: cmd(this)
		, banwatcher(this)
	{
	}

	void OnBackgroundTimer(time_t curtime) CXX11_OVERRIDE
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

		for (timedbans::iterator i = expired.begin(); i != expired.end(); i++)
		{
			std::string mask = i->mask;
			Channel* cr = i->chan;
			{
				CUList empty;
				std::string expiry = "*** Timed ban on " + cr->name + " expired.";
				cr->WriteAllExcept(ServerInstance->FakeClient, true, '@', empty, "NOTICE %s :%s", cr->name.c_str(), expiry.c_str());
				ServerInstance->PI->SendChannelNotice(cr, '@', expiry);

				Modes::ChangeList setban;
				setban.push_remove(ServerInstance->Modes->FindMode('b', MODETYPE_CHANNEL), mask);
				ServerInstance->Modes->Process(ServerInstance->FakeClient, cr, NULL, setban);
			}
		}
	}

	void OnChannelDelete(Channel* chan) CXX11_OVERRIDE
	{
		// Remove all timed bans affecting the channel from internal bookkeeping
		TimedBanList.erase(std::remove_if(TimedBanList.begin(), TimedBanList.end(), ChannelMatcher(chan)), TimedBanList.end());
	}

	Version GetVersion() CXX11_OVERRIDE
	{
		return Version("Adds timed bans", VF_COMMON | VF_VENDOR);
	}
};

MODULE_INIT(ModuleTimedBans)
