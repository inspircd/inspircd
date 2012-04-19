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
 public:
	SimpleExtItem<HistoryList> ext;
	int maxlines;
	HistoryMode(Module* Creator) : ModeHandler(Creator, "history", 'H', PARAM_SETONLY, MODETYPE_CHANNEL),
		ext(EXTENSIBLE_CHANNEL, "history", Creator) { fixed_letter = false; }

	ModeAction OnModeChange(User* source, User* dest, Channel* channel, std::string &parameter, bool adding)
	{
		if (adding)
		{
			std::string::size_type colon = parameter.find(':');
			if (colon == std::string::npos)
				return MODEACTION_DENY;
			int len = atoi(parameter.substr(0, colon).c_str());
			int time = ServerInstance->Duration(parameter.substr(colon+1));
			if (len <= 0 || time < 0)
				return MODEACTION_DENY;
			if (len > maxlines && IS_LOCAL(source))
				return MODEACTION_DENY;
			if (len > maxlines)
				len = maxlines;
			if (parameter == channel->GetModeParameter(this))
				return MODEACTION_DENY;
			ext.set(channel, new HistoryList(len, time));
			channel->SetModeParam(this, parameter);
		}
		else
		{
			ext.unset(channel);
			channel->SetModeParam(this, "");
		}
		return MODEACTION_ALLOW;
	}
};

class ModuleChanHistory : public Module
{
	HistoryMode m;
 public:
	ModuleChanHistory() : m(this)
	{
	}

	void init()
	{
		ServerInstance->Modules->AddService(m);
		ServerInstance->Modules->AddService(m.ext);

		Implementation eventlist[] = { I_OnPostJoin, I_OnUserMessage };
		ServerInstance->Modules->Attach(eventlist, this, sizeof(eventlist)/sizeof(Implementation));
	}

	void ReadConfig(ConfigReadStatus&)
	{
		m.maxlines = ServerInstance->Config->GetTag("chanhistory")->getInt("maxlines", 50);
	}

	~ModuleChanHistory()
	{
		ServerInstance->Modes->DelMode(&m);
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
		HistoryList* list = m.ext.get(memb->chan);
		if (!list)
			return;
		time_t mintime = 0;
		if (list->maxtime)
			mintime = ServerInstance->Time() - list->maxtime;
		memb->user->WriteServ("NOTICE %s :Replaying up to %d lines of pre-join history spanning up to %d seconds",
			memb->chan->name.c_str(), list->maxlen, list->maxtime);
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
