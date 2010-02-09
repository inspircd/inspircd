/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2010 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
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
	HistoryMode(Module* Creator) : ModeHandler(Creator, "history", 'H', PARAM_SETONLY, MODETYPE_CHANNEL),
		ext("history", Creator) { }

	ModeAction OnModeChange(User* source, User* dest, Channel* channel, std::string &parameter, bool adding)
	{
		if (adding)
		{
			std::string::size_type colon = parameter.find(':');
			if (colon == std::string::npos)
				return MODEACTION_DENY;
			int len = atoi(parameter.substr(0, colon).c_str());
			int time = ServerInstance->Duration(parameter.substr(colon+1));
			if (len <= 0 || time < 0 || len > 50)
				return MODEACTION_DENY;
			ext.set(channel, new HistoryList(len, time));
			channel->SetModeParam('H', parameter);
		}
		else
		{
			ext.unset(channel);
			channel->SetModeParam('H', "");
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
		if (!ServerInstance->Modes->AddMode(&m))
			throw ModuleException("Could not add new modes!");

		Implementation eventlist[] = { I_OnPostJoin, I_OnUserMessage };
		ServerInstance->Modules->Attach(eventlist, this, 2);
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
