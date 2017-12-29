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

/* $ModDesc: Provides channel history for a given number of lines */

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
	HistoryList(unsigned int len, unsigned int time) : maxlen(len), maxtime(time) {}
};

class HistoryMode : public ModeHandler
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
	SimpleExtItem<HistoryList> ext;
	unsigned int maxlines;
	HistoryMode(Module* Creator) : ModeHandler(Creator, "history", 'H', PARAM_SETONLY, MODETYPE_CHANNEL),
		ext("history", Creator) { }

	ModeAction OnModeChange(User* source, User* dest, Channel* channel, std::string &parameter, bool adding)
	{
		if (adding)
		{
			std::string::size_type colon = parameter.find(':');
			if (colon == std::string::npos)
				return MODEACTION_DENY;

			std::string duration = parameter.substr(colon+1);
			if ((IS_LOCAL(source)) && ((duration.length() > 10) || (!IsValidDuration(duration))))
				return MODEACTION_DENY;

			unsigned int len = ConvToInt(parameter.substr(0, colon));
			int time = ServerInstance->Duration(duration);
			if (len == 0 || time < 0)
				return MODEACTION_DENY;
			if (len > maxlines && IS_LOCAL(source))
				return MODEACTION_DENY;
			if (len > maxlines)
				len = maxlines;
			if (parameter == channel->GetModeParameter(this))
				return MODEACTION_DENY;

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
			channel->SetModeParam('H', parameter);
		}
		else
		{
			if (!channel->IsModeSet('H'))
				return MODEACTION_DENY;
			ext.unset(channel);
			channel->SetModeParam('H', "");
		}
		return MODEACTION_ALLOW;
	}
};

class ModuleChanHistory : public Module
{
	HistoryMode m;
	bool sendnotice;
	bool dobots;
 public:
	ModuleChanHistory() : m(this)
	{
	}

	void init()
	{
		ServerInstance->Modules->AddService(m);
		ServerInstance->Modules->AddService(m.ext);

		Implementation eventlist[] = { I_OnPostJoin, I_OnUserMessage, I_OnRehash };
		ServerInstance->Modules->Attach(eventlist, this, sizeof(eventlist)/sizeof(Implementation));
		OnRehash(NULL);
	}

	void OnRehash(User*)
	{
		ConfigTag* tag = ServerInstance->Config->ConfValue("chanhistory");
		m.maxlines = tag->getInt("maxlines", 50);
		sendnotice = tag->getBool("notice", true);
		dobots = tag->getBool("bots", true);
	}

	void OnUserMessage(User* user,void* dest,int target_type, const std::string &text, char status, const CUList&)
	{
		if (target_type == TYPE_CHANNEL && status == 0)
		{
			Channel* c = (Channel*)dest;
			HistoryList* list = m.ext.get(c);
			if (list)
			{
				char buf[MAXBUF];
				snprintf(buf, MAXBUF, ":%s PRIVMSG %s :%s",
					user->GetFullHost().c_str(), c->name.c_str(), text.c_str());
				list->lines.push_back(HistoryItem(buf));
				if (list->lines.size() > list->maxlen)
					list->lines.pop_front();
			}
		}
	}

	void OnPostJoin(Membership* memb)
	{
		if (IS_REMOTE(memb->user))
			return;

		if (!dobots && ServerInstance->Modules->Find("m_botmode.so") && memb->user->IsModeSet('B'))
			return;

		HistoryList* list = m.ext.get(memb->chan);
		if (!list)
			return;
		time_t mintime = 0;
		if (list->maxtime)
			mintime = ServerInstance->Time() - list->maxtime;

		if (sendnotice)
		{
			memb->user->WriteServ("NOTICE %s :Replaying up to %d lines of pre-join history spanning up to %d seconds",
				memb->chan->name.c_str(), list->maxlen, list->maxtime);
		}

		for(std::deque<HistoryItem>::iterator i = list->lines.begin(); i != list->lines.end(); ++i)
		{
			if (i->ts >= mintime)
				memb->user->Write(i->line);
		}
	}

	Version GetVersion()
	{
		return Version("Provides channel history replayed on join", VF_VENDOR);
	}
};

MODULE_INIT(ModuleChanHistory)
