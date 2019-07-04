/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
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
#include "modules/ircv3_servertime.h"
#include "modules/ircv3_batch.h"
#include "modules/server.h"

struct HistoryItem
{
	time_t ts;
	std::string text;
	std::string sourcemask;

	HistoryItem(User* source, const std::string& Text)
		: ts(ServerInstance->Time())
		, text(Text)
		, sourcemask(source->GetFullHost())
	{
	}
};

struct HistoryList
{
	std::deque<HistoryItem> lines;
	unsigned int maxlen;
	unsigned int maxtime;

	HistoryList(unsigned int len, unsigned int time)
		: maxlen(len)
		, maxtime(time)
	{
	}
};

class HistoryMode : public ParamMode<HistoryMode, SimpleExtItem<HistoryList> >
{
 public:
	unsigned int maxlines;
	HistoryMode(Module* Creator)
		: ParamMode<HistoryMode, SimpleExtItem<HistoryList> >(Creator, "history", 'H')
	{
		syntax = "<max-messages>:<max-duration>";
	}

	ModeAction OnSet(User* source, Channel* channel, std::string& parameter) CXX11_OVERRIDE
	{
		std::string::size_type colon = parameter.find(':');
		if (colon == std::string::npos)
		{
			source->WriteNumeric(Numerics::InvalidModeParameter(channel, this, parameter));
			return MODEACTION_DENY;
		}

		std::string duration(parameter, colon+1);
		if ((IS_LOCAL(source)) && ((duration.length() > 10) || (!InspIRCd::IsValidDuration(duration))))
		{
			source->WriteNumeric(Numerics::InvalidModeParameter(channel, this, parameter));
			return MODEACTION_DENY;
		}

		unsigned int len = ConvToNum<unsigned int>(parameter.substr(0, colon));
		unsigned long time;
		if (!InspIRCd::Duration(duration, time) || len == 0 || (len > maxlines && IS_LOCAL(source)))
		{
			source->WriteNumeric(Numerics::InvalidModeParameter(channel, this, parameter));
			return MODEACTION_DENY;
		}
		if (len > maxlines)
			len = maxlines;

		HistoryList* history = ext.get(channel);
		if (history)
		{
			// Shrink the list if the new line number limit is lower than the old one
			if (len < history->lines.size())
				history->lines.erase(history->lines.begin(), history->lines.begin() + (history->lines.size() - len));

			history->maxlen = len;
			history->maxtime = time;
		}
		else
		{
			ext.set(channel, new HistoryList(len, time));
		}
		return MODEACTION_ALLOW;
	}

	void SerializeParam(Channel* chan, const HistoryList* history, std::string& out)
	{
		out.append(ConvToStr(history->maxlen));
		out.append(":");
		out.append(InspIRCd::DurationString(history->maxtime));
	}
};

class ModuleChanHistory
	: public Module
	, public ServerEventListener
{
	HistoryMode m;
	bool sendnotice;
	UserModeReference botmode;
	bool dobots;
	IRCv3::Batch::CapReference batchcap;
	IRCv3::Batch::API batchmanager;
	IRCv3::Batch::Batch batch;
	IRCv3::ServerTime::API servertimemanager;

 public:
	ModuleChanHistory()
		: ServerEventListener(this)
		, m(this)
		, botmode(this, "bot")
		, batchcap(this)
		, batchmanager(this)
		, batch("chathistory")
		, servertimemanager(this)
	{
	}

	void ReadConfig(ConfigStatus& status) CXX11_OVERRIDE
	{
		ConfigTag* tag = ServerInstance->Config->ConfValue("chanhistory");
		m.maxlines = tag->getUInt("maxlines", 50, 1);
		sendnotice = tag->getBool("notice", true);
		dobots = tag->getBool("bots", true);
	}

	ModResult OnBroadcastMessage(Channel* channel, const Server* server) CXX11_OVERRIDE
	{
		return channel->IsModeSet(m) ? MOD_RES_ALLOW : MOD_RES_PASSTHRU;
	}

	void OnUserPostMessage(User* user, const MessageTarget& target, const MessageDetails& details) CXX11_OVERRIDE
	{
		if ((target.type == MessageTarget::TYPE_CHANNEL) && (target.status == 0) && (details.type == MSG_PRIVMSG))
		{
			Channel* c = target.Get<Channel>();
			HistoryList* list = m.ext.get(c);
			if (list)
			{
				list->lines.push_back(HistoryItem(user, details.text));
				if (list->lines.size() > list->maxlen)
					list->lines.pop_front();
			}
		}
	}

	void OnPostJoin(Membership* memb) CXX11_OVERRIDE
	{
		LocalUser* localuser = IS_LOCAL(memb->user);
		if (!localuser)
			return;

		if (memb->user->IsModeSet(botmode) && !dobots)
			return;

		HistoryList* list = m.ext.get(memb->chan);
		if (!list)
			return;
		time_t mintime = 0;
		if (list->maxtime)
			mintime = ServerInstance->Time() - list->maxtime;

		if ((sendnotice) && (!batchcap.get(localuser)))
		{
			std::string message("Replaying up to " + ConvToStr(list->maxlen) + " lines of pre-join history");
			if (list->maxtime > 0)
				message.append(" spanning up to " + InspIRCd::DurationString(list->maxtime));
			memb->WriteNotice(message);
		}

		if (batchmanager)
		{
			batchmanager->Start(batch);
			batch.GetBatchStartMessage().PushParamRef(memb->chan->name);
		}

		for(std::deque<HistoryItem>::iterator i = list->lines.begin(); i != list->lines.end(); ++i)
		{
			const HistoryItem& item = *i;
			if (item.ts >= mintime)
			{
				ClientProtocol::Messages::Privmsg msg(ClientProtocol::Messages::Privmsg::nocopy, item.sourcemask, memb->chan, item.text);
				if (servertimemanager)
					servertimemanager->Set(msg, item.ts);
				batch.AddToBatch(msg);
				localuser->Send(ServerInstance->GetRFCEvents().privmsg, msg);
			}
		}

		if (batchmanager)
			batchmanager->End(batch);
	}

	Version GetVersion() CXX11_OVERRIDE
	{
		return Version("Provides channel mode +H, allows for the channel message history to be replayed on join", VF_VENDOR);
	}
};

MODULE_INIT(ModuleChanHistory)
