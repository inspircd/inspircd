/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2013 Daniel Vassdal <shutter@canternet.org>
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

class RepeatMode : public ModeHandler
{
 public:
	enum RepeatAction
	{
		ACT_KICK,
		ACT_BLOCK,
		ACT_BAN
	};

	struct ChannelSettings
	{
		RepeatAction Action;
		unsigned int
			Backlog,
			Lines,
			Diff,
			Seconds;
		ChannelSettings(RepeatAction ac, int li, int di, int se, int ba) :
			Action(ac), Backlog(ba), Lines(li), Diff(di), Seconds(se) {}
	};

	struct ModuleSettings
	{
		unsigned int
			MaxLines,
			MaxSecs,
			MaxBacklog,
			MaxDiff;
		unsigned int	MxSize;
		ModuleSettings() : MaxLines(0), MaxSecs(0), MaxBacklog(0), MaxDiff() {}
	};

	struct RepeatItem
	{
		time_t ts;
		std::string line;
		RepeatItem(time_t TS, const std::string& Line) : ts(TS), line(Line) {}
	};

	typedef std::deque<RepeatItem> RepeatItemList;

	struct RepeatItemListMatchCounterPair
	{
		RepeatItemList ItemList;
		unsigned int Counter;
		RepeatItemListMatchCounterPair() : Counter(0) {}
	};

	SimpleExtItem<RepeatItemListMatchCounterPair> RILMC;
	SimpleExtItem<ChannelSettings> ChanSet;

	RepeatMode(Module* Creator) : ModeHandler(Creator, "repeat", 'E', PARAM_SETONLY, MODETYPE_CHANNEL),
		RILMC("itemcounterpair", Creator), ChanSet("chanset", Creator) {}

	ModuleSettings ms;
	ModeAction OnModeChange(User* source, User* dest, Channel* channel, std::string& parameter, bool adding)
	{
		if (!adding)
		{
			if (!channel->IsModeSet(this))
				return MODEACTION_DENY;

			ChanSet.unset(channel);
			channel->SetModeParam(this, "");
			return MODEACTION_ALLOW;
		}

		if (!parameter.size())
		{
syntax:
			source->WriteNotice("*** Invalid syntax. Syntax is {[~*]}[lines]:[time]{:[difference]}{:[backlog]}");
			return MODEACTION_DENY;
		}

		if (channel->GetModeParameter(this) == parameter)
			return MODEACTION_DENY;

		RepeatAction action = ACT_KICK;

		unsigned int
			lines = 0,
			backlog = 0,
			distance = 0,
			seconds = 0;
		std::string	distancestr,
				backlogstr;
		size_t colon1 = parameter.find(':');
		size_t colon2 = (colon1 != std::string::npos ? parameter.find(':', colon1 + 1) : std::string::npos);
		size_t colon3 = (colon2 != std::string::npos ? parameter.find(':', colon2 + 1) : std::string::npos);

		if (parameter[0] == '*')
			action = ACT_BAN;
		else if (parameter[0] == '~')
			action = ACT_BLOCK;

		lines = ConvToInt(parameter.substr((action != ACT_KICK ? 1 : 0), colon1));
		seconds = InspIRCd::Duration(parameter.substr(colon1 + 1, (colon2 != std::string::npos ? colon2 - colon1 - 1 : std::string::npos)));
		if (colon2 != std::string::npos)
			distancestr = parameter.substr(colon2 + 1,  (colon3 != std::string::npos ? colon3 - colon2 - 1 : std::string::npos));

		if (colon3 != std::string::npos)
			backlogstr = parameter.substr(colon3 + 1, std::string::npos);

		if ((colon1 == std::string::npos) || !lines || !seconds)
			goto syntax;

		if ((distancestr.size() && distancestr != "0" && !(distance = ConvToInt(distancestr))) ||
			(backlogstr.size() && backlogstr != "0" && !(backlog = ConvToInt(backlogstr))))
			goto syntax;

		if (backlog && !ms.MaxBacklog)
		{
			source->WriteNotice("*** The server administrator has disabled backlog matching");
			return MODEACTION_DENY;
		}

		if (distance && !ms.MaxDiff)
		{
			source->WriteNotice("*** The server administrator has disabled matching on edit distance");
			return MODEACTION_DENY;
		}
		else if (distance)
		{
			if (ms.MaxDiff && distance > ms.MaxDiff)
			{
				source->WriteNotice("*** The distance you specified is too great. Maximum allowed is " + ConvToStr(ms.MaxDiff));
				return MODEACTION_DENY;
			}

			if (ms.MaxLines && lines > ms.MaxLines)
			{
				source->WriteNotice("*** The line number you specified is too great. Maximum allowed is " + ConvToStr(ms.MaxLines));
				return MODEACTION_DENY;
			}

			if (ms.MaxSecs && seconds > ms.MaxSecs)
			{
				source->WriteNotice("*** The seconds you specified is too great. Maximum allowed is " + ConvToStr(ms.MaxSecs));
				return MODEACTION_DENY;
			}

			if (lines > backlog)
			{
				source->WriteNotice("*** You can't set needed lines higher than backlog");
				return MODEACTION_DENY;
			}
		}

		ChanSet.set(channel, new ChannelSettings(action, lines, distance, seconds, backlog));
		channel->SetModeParam(this, parameter);

		return MODEACTION_ALLOW;
	}

	std::vector<std::vector<unsigned int> > mx;
	unsigned int Levenshtein(std::string& s1, std::string& s2)
	{
		unsigned int l1 = s1.size(), l2 = s2.size();

		for (unsigned int i = 0; i <= l1; i++)
			mx[i][0] = i;
		for (unsigned int i = 0; i <= l2; i++)
			mx[0][i] = i;
		for (unsigned int i = 1; i <= l1; i++)
			for (unsigned int j = 1; j <= l2; j++)
				mx[i][j] = std::min(std::min(mx[i - 1][j] + 1, mx[i][j - 1] + 1), mx[i - 1][j - 1] + (s1[i - 1] == s2[j - 1] ? 0 : 1));
		return (mx[l1][l2]);
	}

	bool MatchLine(Membership* memb, std::string message)
	{
		// If the message is larger than whatever size it's set to,
		// let's pretend it isn't. If the first 512 (def. setting) match, it's probably spam.
		if (message.capacity() > ms.MxSize - 1)
			message.resize(ms.MxSize - 1);

		std::transform(message.begin(), message.end(), message.begin(), ::tolower);

		ChannelSettings* rs = ChanSet.get(memb->chan);
		if (!rs)
			return false;

		RepeatItemListMatchCounterPair* rp = RILMC.get(memb); 
		if (!rp)
		{
			rp = new RepeatItemListMatchCounterPair();
			RILMC.set(memb, rp);
		}

		unsigned int matches = 0;
		if (!rs->Backlog)
			matches = rp->Counter;

		std::transform(message.begin(), message.end(), message.begin(), ::tolower);
		time_t now = ServerInstance->Time();

		RepeatItem ri(now + rs->Seconds, message);

		RepeatItemList* ve = &rp->ItemList;
		if (ve->size() > (!rs->Backlog ? rs->Lines : rs->Backlog))
			ve->pop_back();

		unsigned int trigger = message.size() * rs->Diff / 100;
		for (std::deque<RepeatItem>::iterator it = ve->begin(); it != ve->end(); it++)
		{
			if (it->ts < now)
			{
				ve->erase(it, ve->end());
				matches = 0;
				rp->Counter = 0;
				break;
			}

			if (((rs->Diff == 0 || trigger == 0) ? (message == it->line) : (Levenshtein(message, it->line) <= trigger)))
			{
				if (++matches >= rs->Lines)
				{
					rp->Counter = 0;
					return true;
				}
			}
			else if (!ms.MaxBacklog || !rs->Backlog)
			{
				ve->clear();
				matches = 0;
				break;
			}
		}

		ve->push_front(ri);
		rp->Counter = 0;
		return false;
	}

	void Resize(size_t size)
	{
		if (size == mx.size())
			return;
		mx.resize(size);

		if (mx.size() > size)
		{
			mx.resize(size);
			for (unsigned int i = 0; i < mx.size(); i++)
				mx[i].resize(size);
		}
		else
		{
			for (unsigned int i = 0; i < mx.size(); i++)
			{
				mx[i].resize(size);
				std::vector<unsigned int>(mx[i]).swap(mx[i]);
			}
			std::vector<std::vector<unsigned int> >(mx).swap(mx);
		}
	}

	void Update()
	{
		ConfigTag* conf = ServerInstance->Config->ConfValue("repeat");
		ms.MaxLines = conf->getInt("maxlines", 20);
		ms.MaxBacklog = conf->getInt("maxbacklog", 20);
		ms.MaxDiff = conf->getInt("maxdistance", 50);
		ms.MaxSecs = conf->getInt("maxsecs", 0);
		Resize(conf->getInt("size", 512));
	};
};

class RepeatModule : public Module
{
	RepeatMode rm;
 public:

	RepeatModule() : rm(this) {}

	void init() CXX11_OVERRIDE
	{
		ServerInstance->Modules->AddService(rm);
		ServerInstance->Modules->AddService(rm.ChanSet);
		ServerInstance->Modules->AddService(rm.RILMC);
		Implementation eventlist[] = { I_OnUserPreMessage, I_OnRehash };
		ServerInstance->Modules->Attach(eventlist, this, sizeof(eventlist)/sizeof(Implementation));
		rm.Update();
	}

	void OnRehash(User* user) CXX11_OVERRIDE
	{
		rm.Update();
	}

	ModResult OnUserPreMessage(User* user,void* dest,int target_type, std::string& text,char status, CUList& exempt_list, MessageType msgtype) CXX11_OVERRIDE
	{
		if (target_type != TYPE_CHANNEL || !IS_LOCAL(user))
			return MOD_RES_PASSTHRU;

		Membership* memb = ((Channel*)dest)->GetUser(user);
		if (!memb || !memb->chan->IsModeSet(&rm))
			return MOD_RES_PASSTHRU;

		if (ServerInstance->OnCheckExemption(user, memb->chan, "repeat") == MOD_RES_ALLOW)
			return MOD_RES_PASSTHRU;

		RepeatMode::ChannelSettings* rh = rm.ChanSet.get(memb->chan);

		if (rm.MatchLine(memb, text))
		{
			if (rh->Action == RepeatMode::ACT_BLOCK)
			{
				user->WriteNotice("*** This line is too similiar to one of your last lines.");
				return MOD_RES_DENY;
			}

			if (rh->Action == RepeatMode::ACT_BAN)
			{
				std::vector<std::string> parameters;
				parameters.push_back(memb->chan->name);
				parameters.push_back("+b");
				parameters.push_back(user->MakeWildHost());
				ServerInstance->SendGlobalMode(parameters, ServerInstance->FakeClient);
			}

			memb->chan->KickUser(ServerInstance->FakeClient, user, "Repeat flood");
			return MOD_RES_DENY;
		}
		return MOD_RES_PASSTHRU;
	}

	void Prioritize() CXX11_OVERRIDE
	{
		ServerInstance->Modules->SetPriority(this, I_OnUserPreMessage, PRIORITY_LAST);
	}

	Version GetVersion() CXX11_OVERRIDE
	{
		return Version("Provides the E channel mode - For blocking of similiar messages", VF_VENDOR);
	}
};

MODULE_INIT(RepeatModule)
