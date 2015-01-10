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

struct HistoryItem
{
	time_t ts;
	std::string line;
	HistoryItem(const std::string& Line) : ts(ServerInstance->Time()), line(Line) {}
};

struct HistoryList
{
	std::deque<HistoryItem> lines;
	unsigned int maxlen, maxtime;
	std::string param;

	HistoryList(unsigned int len, unsigned int time, const std::string& oparam)
		: maxlen(len), maxtime(time), param(oparam) { }
};

class HistoryMode : public ParamMode<HistoryMode, SimpleExtItem<HistoryList> >
{
	bool IsValidDuration(const std::string& duration)
	{
		for (std::string::const_iterator i = duration.begin(); i != duration.end(); ++i)
		{
			unsigned char c = *i;
			if (((c >= '0') && (c <= '9')) || (c == 's') || (c == 'S'))
				continue;

			if (duration_multi[c] == 1)
				return false;
		}
		return true;
	}

 public:
	unsigned int maxlines;
	HistoryMode(Module* Creator)
		: ParamMode<HistoryMode, SimpleExtItem<HistoryList> >(Creator, "history", 'H')
	{
	}

	ModeAction OnSet(User* source, Channel* channel, std::string& parameter)
	{
		std::string::size_type colon = parameter.find(':');
		if (colon == std::string::npos)
			return MODEACTION_DENY;

		std::string duration(parameter, colon+1);
		if ((IS_LOCAL(source)) && ((duration.length() > 10) || (!IsValidDuration(duration))))
			return MODEACTION_DENY;

		unsigned int len = ConvToInt(parameter.substr(0, colon));
		int time = InspIRCd::Duration(duration);
		if (len == 0 || time < 0)
			return MODEACTION_DENY;
		if (len > maxlines && IS_LOCAL(source))
			return MODEACTION_DENY;
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
			history->param = parameter;
		}
		else
		{
			ext.set(channel, new HistoryList(len, time, parameter));
		}
		return MODEACTION_ALLOW;
	}

	void SerializeParam(Channel* chan, const HistoryList* history, std::string& out)
	{
		out.append(history->param);
	}
};

class ModuleChanHistory : public Module
{
	HistoryMode m;
	bool sendnotice;
	UserModeReference botmode;
	bool dobots;
 public:
	ModuleChanHistory() : m(this), botmode(this, "bot")
	{
	}

	void ReadConfig(ConfigStatus& status) CXX11_OVERRIDE
	{
		ConfigTag* tag = ServerInstance->Config->ConfValue("chanhistory");
		m.maxlines = tag->getInt("maxlines", 50);
		sendnotice = tag->getBool("notice", true);
		dobots = tag->getBool("bots", true);
	}

	void OnUserMessage(User* user, void* dest, int target_type, const std::string &text, char status, const CUList&, MessageType msgtype) CXX11_OVERRIDE
	{
		if ((target_type == TYPE_CHANNEL) && (status == 0) && (msgtype == MSG_PRIVMSG))
		{
			Channel* c = (Channel*)dest;
			HistoryList* list = m.ext.get(c);
			if (list)
			{
				const std::string line = ":" + user->GetFullHost() + " PRIVMSG " + c->name + " :" + text;
				list->lines.push_back(HistoryItem(line));
				if (list->lines.size() > list->maxlen)
					list->lines.pop_front();
			}
		}
	}

	void OnPostJoin(Membership* memb) CXX11_OVERRIDE
	{
		if (IS_REMOTE(memb->user))
			return;

		if (memb->user->IsModeSet(botmode) && !dobots)
			return;

		HistoryList* list = m.ext.get(memb->chan);
		if (!list)
			return;
		time_t mintime = 0;
		if (list->maxtime)
			mintime = ServerInstance->Time() - list->maxtime;

		if (sendnotice)
		{
			memb->user->WriteNotice("Replaying up to " + ConvToStr(list->maxlen) + " lines of pre-join history spanning up to " + ConvToStr(list->maxtime) + " seconds");
		}

		for(std::deque<HistoryItem>::iterator i = list->lines.begin(); i != list->lines.end(); ++i)
		{
			if (i->ts >= mintime)
				memb->user->Write(i->line);
		}
	}

	Version GetVersion() CXX11_OVERRIDE
	{
		return Version("Provides channel history replayed on join", VF_VENDOR);
	}
};

MODULE_INIT(ModuleChanHistory)
