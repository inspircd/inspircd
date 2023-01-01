/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2018 linuxdaemon <linuxdaemon.irc@gmail.com>
 *   Copyright (C) 2013, 2017-2021 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2013 Daniel Vassdal <shutter@canternet.org>
 *   Copyright (C) 2012-2015, 2018 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012, 2019 Robby <robby@chatbelgie.be>
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
#include "clientprotocolmsg.h"
#include "modules/ircv3_batch.h"
#include "modules/ircv3_servertime.h"
#include "modules/server.h"
#include "numerichelper.h"

typedef insp::flat_map<std::string, std::string> HistoryTagMap;

struct HistoryItem final
{
	time_t ts;
	std::string text;
	MessageType type;
	HistoryTagMap tags;
	std::string sourcemask;

	HistoryItem(User* source, const MessageDetails& details)
		: ts(ServerInstance->Time())
		, text(details.text)
		, type(details.type)
		, sourcemask(source->GetFullHost())
	{
		tags.reserve(details.tags_out.size());
		for (const auto& [tagname, tagvalue] : details.tags_out)
			tags[tagname] = tagvalue.value;
	}
};

struct HistoryList final
{
	std::deque<HistoryItem> lines;
	unsigned long maxlen;
	unsigned long maxtime;

	HistoryList(unsigned long len, unsigned long time)
		: maxlen(len)
		, maxtime(time)
	{
	}

	size_t Prune()
	{
		// Prune expired entries from the list.
		if (maxtime)
		{
			time_t mintime = ServerInstance->Time() - maxtime;
			while (!lines.empty() && lines.front().ts < mintime)
				lines.pop_front();
		}
		return lines.size();
	}
};

class HistoryMode final
	: public ParamMode<HistoryMode, SimpleExtItem<HistoryList>>
{
public:
	unsigned long maxlines;
	HistoryMode(Module* Creator)
		: ParamMode<HistoryMode, SimpleExtItem<HistoryList>>(Creator, "history", 'H')
	{
		syntax = "<max-messages>:<max-duration>";
	}

	ModeAction OnSet(User* source, Channel* channel, std::string& parameter) override
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

		unsigned long len = ConvToNum<unsigned long>(parameter.substr(0, colon));
		unsigned long time;
		if (!InspIRCd::Duration(duration, time) || len == 0 || (len > maxlines && IS_LOCAL(source)))
		{
			source->WriteNumeric(Numerics::InvalidModeParameter(channel, this, parameter));
			return MODEACTION_DENY;
		}
		if (len > maxlines)
			len = maxlines;

		HistoryList* history = ext.Get(channel);
		if (history)
		{
			// Shrink the list if the new line number limit is lower than the old one
			if (len < history->lines.size())
				history->lines.erase(history->lines.begin(), history->lines.begin() + (history->lines.size() - len));

			history->maxlen = len;
			history->maxtime = time;
			history->Prune();
		}
		else
		{
			ext.SetFwd(channel, len, time);
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

class ModuleChanHistory final
	: public Module
	, public ServerProtocol::BroadcastEventListener
{
private:
	HistoryMode historymode;
	SimpleUserMode nohistorymode;
	bool prefixmsg;
	UserModeReference botmode;
	bool dobots;
	IRCv3::Batch::CapReference batchcap;
	IRCv3::Batch::API batchmanager;
	IRCv3::Batch::Batch batch;
	IRCv3::ServerTime::API servertimemanager;
	ClientProtocol::MessageTagEvent tagevent;

	void AddTag(ClientProtocol::Message& msg, const std::string& tagkey, std::string& tagval)
	{
		for (const auto& subscriber : tagevent.GetSubscribers())
		{
			ClientProtocol::MessageTagProvider* const tagprov = static_cast<ClientProtocol::MessageTagProvider*>(subscriber);
			const ModResult res = tagprov->OnProcessTag(ServerInstance->FakeClient, tagkey, tagval);
			if (res == MOD_RES_ALLOW)
				msg.AddTag(tagkey, tagprov, tagval);
			else if (res == MOD_RES_DENY)
				break;
		}
	}

	void SendHistory(LocalUser* user, Channel* channel, HistoryList* list)
	{
		if (batchmanager)
		{
			batchmanager->Start(batch);
			batch.GetBatchStartMessage().PushParamRef(channel->name);
		}

		for (auto& item : list->lines)
		{
			ClientProtocol::Messages::Privmsg msg(ClientProtocol::Messages::Privmsg::nocopy, item.sourcemask, channel, item.text, item.type);
			for (auto& [tagname, tagvalue] : item.tags)
				AddTag(msg, tagname, tagvalue);
			if (servertimemanager)
				servertimemanager->Set(msg, item.ts);
			batch.AddToBatch(msg);
			user->Send(ServerInstance->GetRFCEvents().privmsg, msg);
		}

		if (batchmanager)
			batchmanager->End(batch);
	}

public:
	ModuleChanHistory()
		: Module(VF_VENDOR, "Adds channel mode H (history) which allows message history to be viewed on joining the channel.")
		, ServerProtocol::BroadcastEventListener(this)
		, historymode(this)
		, nohistorymode(this, "nohistory", 'N')
		, botmode(this, "bot")
		, batchcap(this)
		, batchmanager(this)
		, batch("chathistory")
		, servertimemanager(this)
		, tagevent(this)
	{
	}

	void ReadConfig(ConfigStatus& status) override
	{
		auto tag = ServerInstance->Config->ConfValue("chanhistory");
		historymode.maxlines = tag->getUInt("maxlines", 50, 1);
		prefixmsg = tag->getBool("prefixmsg", true);
		dobots = tag->getBool("bots", true);
	}

	ModResult OnBroadcastMessage(const Channel* channel, const Server* server) override
	{
		return channel->IsModeSet(historymode) ? MOD_RES_ALLOW : MOD_RES_PASSTHRU;
	}

	void OnUserPostMessage(User* user, const MessageTarget& target, const MessageDetails& details) override
	{
		if (target.type != MessageTarget::TYPE_CHANNEL || target.status)
			return;

		std::string_view ctcpname;
		if (details.IsCTCP(ctcpname) && !irc::equals(ctcpname, "ACTION"))
			return;

		HistoryList* list = historymode.ext.Get(target.Get<Channel>());
		if (!list)
			return;

		list->lines.emplace_back(user, details);
		if (list->lines.size() > list->maxlen)
			list->lines.pop_front();
	}

	void OnPostJoin(Membership* memb) override
	{
		LocalUser* localuser = IS_LOCAL(memb->user);
		if (!localuser)
			return;

		if (memb->user->IsModeSet(botmode) && !dobots)
			return;

		if (memb->user->IsModeSet(nohistorymode))
			return;

		HistoryList* list = historymode.ext.Get(memb->chan);
		if (!list || !list->Prune())
			return;

		if ((prefixmsg) && (!batchcap.IsEnabled(localuser)))
		{
			std::string message("Replaying up to " + ConvToStr(list->maxlen) + " lines of pre-join history");
			if (list->maxtime > 0)
				message.append(" from the last " + InspIRCd::DurationString(list->maxtime));
			memb->WriteNotice(message);
		}

		SendHistory(localuser, memb->chan, list);
	}
};

MODULE_INIT(ModuleChanHistory)
