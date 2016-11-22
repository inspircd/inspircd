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


/* $ModDesc: Adds timed bans */

#include "inspircd.h"

/** Holds a timed ban
 */
class TimedBan
{
 public:
	std::string channel;
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
	static bool IsBanSet(Channel* chan, const std::string& mask)
	{
		for (BanList::const_iterator i = chan->bans.begin(); i != chan->bans.end(); ++i)
		{
			if (!strcasecmp(i->data.c_str(), mask.c_str()))
				return true;
		}
		return false;
	}

 public:
	CommandTban(Module* Creator) : Command(Creator,"TBAN", 3)
	{
		syntax = "<channel> <duration> <banmask>";
		TRANSLATE4(TR_TEXT, TR_TEXT, TR_TEXT, TR_END);
	}

	CmdResult Handle (const std::vector<std::string> &parameters, User *user)
	{
		Channel* channel = ServerInstance->FindChan(parameters[0]);
		if (!channel)
		{
			user->WriteNumeric(401, "%s %s :No such channel",user->nick.c_str(), parameters[0].c_str());
			return CMD_FAILURE;
		}
		int cm = channel->GetPrefixValue(user);
		if (cm < HALFOP_VALUE)
		{
			user->WriteNumeric(482, "%s %s :You do not have permission to set bans on this channel",
				user->nick.c_str(), channel->name.c_str());
			return CMD_FAILURE;
		}		

		TimedBan T;
		std::string channelname = parameters[0];
		long duration = ServerInstance->Duration(parameters[1]);
		unsigned long expire = duration + ServerInstance->Time();
		if (duration < 1)
		{
			user->WriteServ("NOTICE "+user->nick+" :Invalid ban time");
			return CMD_FAILURE;
		}
		std::string mask = parameters[2];
		std::vector<std::string> setban;
		setban.push_back(parameters[0]);
		setban.push_back("+b");
		bool isextban = ((mask.size() > 2) && (mask[1] == ':'));
		if (!isextban && !ServerInstance->IsValidMask(mask))
			mask.append("!*@*");
		if ((mask.length() > 250) || (!ServerInstance->IsValidMask(mask) && !isextban))
		{
			user->WriteServ("NOTICE "+user->nick+" :Invalid ban mask");
			return CMD_FAILURE;
		}

		if (IsBanSet(channel, mask))
		{
			user->WriteServ("NOTICE %s :Ban already set", user->nick.c_str());
			return CMD_FAILURE;
		}

		setban.push_back(mask);
		// use CallHandler to make it so that the user sets the mode
		// themselves
		ServerInstance->Parser->CallHandler("MODE",setban,user);
		if (!IsBanSet(channel, mask))
			return CMD_FAILURE;

		CUList tmp;
		T.channel = channelname;
		T.mask = mask;
		T.expire = expire + (IS_REMOTE(user) ? 5 : 0);
		T.chan = channel;
		TimedBanList.push_back(T);

		const std::string addban = user->nick + " added a timed ban on " + mask + " lasting for " + ConvToStr(duration) + " seconds.";
		// If halfop is loaded, send notice to halfops and above, otherwise send to ops and above
		ModeHandler* mh = ServerInstance->Modes->FindMode('h', MODETYPE_CHANNEL);
		char pfxchar = (mh && mh->name == "halfop") ? mh->GetPrefix() : '@';

		channel->WriteAllExcept(ServerInstance->FakeClient, true, pfxchar, tmp, "NOTICE %s :%s", channel->name.c_str(), addban.c_str());
		ServerInstance->PI->SendChannelNotice(channel, pfxchar, addban);
		return CMD_SUCCESS;
	}

	RouteDescriptor GetRouting(User* user, const std::vector<std::string>& parameters)
	{
		return ROUTE_BROADCAST;
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
 public:
	ModuleTimedBans()
		: cmd(this)
	{
	}

	void init()
	{
		ServerInstance->Modules->AddService(cmd);
		Implementation eventlist[] = { I_OnDelBan, I_OnBackgroundTimer, I_OnChannelDelete };
		ServerInstance->Modules->Attach(eventlist, this, sizeof(eventlist)/sizeof(Implementation));
	}

	virtual ModResult OnDelBan(User* source, Channel* chan, const std::string &banmask)
	{
		irc::string listitem = banmask.c_str();
		irc::string thischan = chan->name.c_str();
		for (timedbans::iterator i = TimedBanList.begin(); i != TimedBanList.end(); i++)
		{
			irc::string target = i->mask.c_str();
			irc::string tchan = i->channel.c_str();
			if ((listitem == target) && (tchan == thischan))
			{
				TimedBanList.erase(i);
				break;
			}
		}
		return MOD_RES_PASSTHRU;
	}

	virtual void OnBackgroundTimer(time_t curtime)
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
			std::string chan = i->channel;
			std::string mask = i->mask;
			Channel* cr = ServerInstance->FindChan(chan);
			if (cr)
			{
				std::vector<std::string> setban;
				setban.push_back(chan);
				setban.push_back("-b");
				setban.push_back(mask);

				CUList empty;
				const std::string expiry = "*** Timed ban on " + chan + " expired.";
				// If halfop is loaded, send notice to halfops and above, otherwise send to ops and above
				ModeHandler* mh = ServerInstance->Modes->FindMode('h', MODETYPE_CHANNEL);
				char pfxchar = (mh && mh->name == "halfop") ? mh->GetPrefix() : '@';

				cr->WriteAllExcept(ServerInstance->FakeClient, true, pfxchar, empty, "NOTICE %s :%s", cr->name.c_str(), expiry.c_str());
				ServerInstance->PI->SendChannelNotice(cr, pfxchar, expiry);

				ServerInstance->SendGlobalMode(setban, ServerInstance->FakeClient);
			}
		}
	}

	void OnChannelDelete(Channel* chan)
	{
		// Remove all timed bans affecting the channel from internal bookkeeping
		TimedBanList.erase(std::remove_if(TimedBanList.begin(), TimedBanList.end(), ChannelMatcher(chan)), TimedBanList.end());
	}

	virtual Version GetVersion()
	{
		return Version("Adds timed bans", VF_COMMON | VF_VENDOR);
	}
};

MODULE_INIT(ModuleTimedBans)

