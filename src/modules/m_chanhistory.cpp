/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2009 InspIRCd Development Team
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
	unsigned int max;
	HistoryList(unsigned int Max) : max(Max) {}
};

class HistoryMode : public ModeHandler
{
 public:
	SimpleExtItem<HistoryList> ext;
	HistoryMode(Module* Creator) : ModeHandler(Creator, "history", 'H', PARAM_SETONLY, MODETYPE_CHANNEL),
		ext("history", Creator) { }

	ModeAction OnModeChange(User* source, User* dest, Channel* channel, std::string &parameter, bool adding)
	{
		int max = atoi(parameter.c_str());
		if (adding && max == 0)
			return MODEACTION_DENY;
		if (adding)
		{
			ext.set(channel, new HistoryList(max));
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

		Implementation eventlist[] = { I_OnUserJoin, I_OnUserMessage };
		ServerInstance->Modules->Attach(eventlist, this, 2);
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
				if (list->lines.size() > list->max)
					list->lines.pop_front();
			}
		}
	}

	void OnUserJoin(Membership* memb, bool sync, bool created, CUList& except_list)
	{
		HistoryList* list = m.ext.get(memb->chan);
		if (!list)
			return;
		for(std::deque<HistoryItem>::iterator i = list->lines.begin(); i != list->lines.end(); ++i)
			memb->user->Write(i->line);
	}

	Version GetVersion()
	{
		return Version("Provides channel history replayed on join", VF_COMMON | VF_VENDOR);
	}
};

MODULE_INIT(ModuleChanHistory)
