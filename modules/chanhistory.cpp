/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2021 Dominic Hamon
 *   Copyright (C) 2017-2024 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2013 Daniel Vassdal <shutter@canternet.org>
 *   Copyright (C) 2012, 2014, 2018 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
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
		, sourcemask(source->GetMask())
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
private:
	bool ParseDuration(User* user, irc::sepstream& stream, unsigned long& duration)
	{
		std::string durationstr;
		if (!stream.GetToken(durationstr))
			return false;

		if (!Duration::TryFrom(durationstr, duration) || duration == 0)
			return false;

		if (IS_LOCAL(user) && maxduration && duration > maxduration)
			duration = maxduration; // Clamp for local users.

		return true;
	}

	bool ParseLines(User* user, irc::sepstream& stream, unsigned long& lines)
	{
		std::string linesstr;
		if (!stream.GetToken(linesstr))
			return false;

		lines = ConvToNum<unsigned long>(linesstr);
		if (lines == 0)
			return false;

		if (IS_LOCAL(user) && maxlines && lines > maxlines)
			lines = maxlines; // Clamp for local users.

		return true;
	}

public:
	unsigned long maxduration;
	unsigned long maxlines;

	HistoryMode(Module* Creator)
		: ParamMode<HistoryMode, SimpleExtItem<HistoryList>>(Creator, "history", 'H')
	{
		syntax = "<max-messages>:<max-duration>";
	}

	bool OnSet(User* source, Channel* channel, std::string& parameter) override
	{
		irc::sepstream stream(parameter, ':');

		unsigned long lines;
		unsigned long duration;
		if (!ParseLines(source, stream, lines) || !ParseDuration(source, stream, duration))
		{
			source->WriteNumeric(Numerics::InvalidModeParameter(channel, this, parameter));
			return false;
		}

		HistoryList* history = ext.Get(channel);
		if (history)
		{
			// Shrink the list if the new line number limit is lower than the old one
			if (lines < history->lines.size())
				history->lines.erase(history->lines.begin(), history->lines.begin() + (history->lines.size() - lines));

			history->maxlen = lines;
			history->maxtime = duration;
			history->Prune();
		}
		else
		{
			ext.SetFwd(channel, lines, duration);
		}
		return true;
	}

	void SerializeParam(Channel* chan, const HistoryList* history, std::string& out)
	{
		out.append(ConvToStr(history->maxlen));
		out.append(":");
		out.append(Duration::ToString(history->maxtime));
	}
};

class ModuleChanHistory final
	: public Module
	, public ServerProtocol::RouteEventListener
{
private:
	HistoryMode historymode;
	SimpleUserMode nohistorymode;
	UserModeReference botmode;
	IRCv3::Batch::CapReference batchcap;
	IRCv3::Batch::API batchmanager;
	IRCv3::Batch::Batch batch;
	IRCv3::ServerTime::API servertimemanager;
	ClientProtocol::MessageTagEvent tagevent;
	bool prefixmsg;
	bool savefrombots;
	bool sendtobots;

	void AddTag(ClientProtocol::Message& msg, const std::string& tagkey, std::string& tagval)
	{
		for (auto* subscriber : tagevent.GetSubscribers())
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
		, ServerProtocol::RouteEventListener(this)
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
		const auto& tag = ServerInstance->Config->ConfValue("chanhistory");
		historymode.maxduration = tag->getDuration("maxduration", 60*60*24*28);
		historymode.maxlines = tag->getNum<unsigned long>("maxlines", 50);
		prefixmsg = tag->getBool("prefixmsg", true);
		savefrombots = tag->getBool("savefrombots", true);
		sendtobots = tag->getBool("sendtobots", true);
	}

	ModResult OnRouteMessage(const Channel* channel, const Server* server) override
	{
		return channel->IsModeSet(historymode) && !server->IsService() ? MOD_RES_ALLOW : MOD_RES_PASSTHRU;
	}

	void OnUserPostMessage(User* user, const MessageTarget& target, const MessageDetails& details) override
	{
		if (target.type != MessageTarget::TYPE_CHANNEL || target.status)
			return;

		if (user->IsModeSet(botmode) && !savefrombots)
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

		if (memb->user->IsModeSet(botmode) && !sendtobots)
			return;

		if (memb->user->IsModeSet(nohistorymode))
			return;

		HistoryList* list = historymode.ext.Get(memb->chan);
		if (!list || !list->Prune())
			return;

		if ((prefixmsg) && (!batchcap.IsEnabled(localuser)))
		{
			auto message = FMT::format("Replaying up to {} lines of pre-join history", list->maxlen);
			if (list->maxtime > 0)
				message += FMT::format(" from the last {}", Duration::ToString(list->maxtime));
			memb->WriteNotice(message);
		}

		SendHistory(localuser, memb->chan, list);
	}
};

MODULE_INIT(ModuleChanHistory)
