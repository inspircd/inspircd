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

#ifdef _WIN32
// windows.h defines this
#undef min
#endif

class RepeatMode : public ModeHandler
{
 private:
	struct RepeatItem
	{
		time_t ts;
		std::string line;
		RepeatItem(time_t TS, const std::string& Line) : ts(TS), line(Line) { }
	};

	typedef std::deque<RepeatItem> RepeatItemList;

	struct MemberInfo
	{
		RepeatItemList ItemList;
		unsigned int Counter;
		MemberInfo() : Counter(0) {}
	};

	struct ModuleSettings
	{
		unsigned int MaxLines;
		unsigned int MaxSecs;
		unsigned int MaxBacklog;
		unsigned int MaxDiff;
		unsigned int MaxMessageSize;
		ModuleSettings() : MaxLines(0), MaxSecs(0), MaxBacklog(0), MaxDiff() { }
	};

	std::vector<unsigned int> mx[2];
	ModuleSettings ms;

	bool CompareLines(const std::string& message, const std::string& historyline, unsigned int trigger)
	{
		if (message == historyline)
			return true;
		else if (trigger)
			return (Levenshtein(message, historyline) <= trigger);

		return false;
	}

	unsigned int Levenshtein(const std::string& s1, const std::string& s2)
	{
		unsigned int l1 = s1.size();
		unsigned int l2 = s2.size();

		for (unsigned int i = 0; i < l2; i++)
			mx[0][i] = i;
		for (unsigned int i = 0; i < l1; i++)
		{
			mx[1][0] = i + 1;
			for (unsigned int j = 0; j < l2; j++)
	            mx[1][j + 1] = std::min(std::min(mx[1][j] + 1, mx[0][j + 1] + 1), mx[0][j] + ((s1[i] == s2[j]) ? 0 : 1));

			mx[0].swap(mx[1]);
		}
		return mx[0][l2];
	}

 public:
	enum RepeatAction
	{
		ACT_KICK,
		ACT_BLOCK,
		ACT_BAN
	};

	class ChannelSettings
	{
	 public:
		RepeatAction Action;
		unsigned int Backlog;
		unsigned int Lines;
		unsigned int Diff;
		unsigned int Seconds;

		std::string serialize()
		{
			std::string ret = ((Action == ACT_BAN) ? "*" : (Action == ACT_BLOCK ? "~" : "")) + ConvToStr(Lines) + ":" + ConvToStr(Seconds);
			if (Diff)
			{
				ret += ":" + ConvToStr(Diff);
				if (Backlog)
					ret += ":" + ConvToStr(Backlog);
			}
			return ret;
		}
	};

	SimpleExtItem<MemberInfo> MemberInfoExt;
	SimpleExtItem<ChannelSettings> ChanSet;

	RepeatMode(Module* Creator)
		: ModeHandler(Creator, "repeat", 'E', PARAM_SETONLY, MODETYPE_CHANNEL)
		, MemberInfoExt("repeat_memb", Creator)
		, ChanSet("repeat", Creator)
	{
	}

	ModeAction OnModeChange(User* source, User* dest, Channel* channel, std::string& parameter, bool adding)
	{
		if (!adding)
		{
			if (!channel->IsModeSet(this))
				return MODEACTION_DENY;

			// Unset the per-membership extension when the mode is removed
			const UserMembList* users = channel->GetUsers();
			for (UserMembCIter i = users->begin(); i != users->end(); ++i)
				MemberInfoExt.unset(i->second);

			ChanSet.unset(channel);
			return MODEACTION_ALLOW;
		}

		if (channel->GetModeParameter(this) == parameter)
			return MODEACTION_DENY;

		ChannelSettings settings;
		if (!ParseSettings(source, parameter, settings))
		{
			source->WriteNotice("*** Invalid syntax. Syntax is {[~*]}[lines]:[time]{:[difference]}{:[backlog]}");
			return MODEACTION_DENY;
		}

		if ((settings.Backlog > 0) && (settings.Lines > settings.Backlog))
		{
			source->WriteNotice("*** You can't set needed lines higher than backlog");
			return MODEACTION_DENY;
		}

		LocalUser* localsource = IS_LOCAL(source);
		if ((localsource) && (!ValidateSettings(localsource, settings)))
			return MODEACTION_DENY;

		ChanSet.set(channel, settings);

		return MODEACTION_ALLOW;
	}

	bool MatchLine(Membership* memb, ChannelSettings* rs, std::string message)
	{
		// If the message is larger than whatever size it's set to,
		// let's pretend it isn't. If the first 512 (def. setting) match, it's probably spam.
		if (message.size() > ms.MaxMessageSize)
			message.erase(ms.MaxMessageSize);

		MemberInfo* rp = MemberInfoExt.get(memb);
		if (!rp)
		{
			rp = new MemberInfo;
			MemberInfoExt.set(memb, rp);
		}

		unsigned int matches = 0;
		if (!rs->Backlog)
			matches = rp->Counter;

		RepeatItemList& items = rp->ItemList;
		const unsigned int trigger = (message.size() * rs->Diff / 100);
		const time_t now = ServerInstance->Time();

		std::transform(message.begin(), message.end(), message.begin(), ::tolower);

		for (std::deque<RepeatItem>::iterator it = items.begin(); it != items.end(); ++it)
		{
			if (it->ts < now)
			{
				items.erase(it, items.end());
				matches = 0;
				break;
			}

			if (CompareLines(message, it->line, trigger))
			{
				if (++matches >= rs->Lines)
				{
					if (rs->Action != ACT_BLOCK)
						rp->Counter = 0;
					return true;
				}
			}
			else if ((ms.MaxBacklog == 0) || (rs->Backlog == 0))
			{
				matches = 0;
				items.clear();
				break;
			}
		}

		unsigned int max_items = (rs->Backlog ? rs->Backlog : 1);
		if (items.size() >= max_items)
			items.pop_back();

		items.push_front(RepeatItem(now + rs->Seconds, message));
		rp->Counter = matches;
		return false;
	}

	void Resize(size_t size)
	{
		size_t newsize = size+1;
		if (newsize <= mx[0].size())
			return;
		ms.MaxMessageSize = size;
		mx[0].resize(newsize);
		mx[1].resize(newsize);
	}

	void ReadConfig()
	{
		ConfigTag* conf = ServerInstance->Config->ConfValue("repeat");
		ms.MaxLines = conf->getInt("maxlines", 20);
		ms.MaxBacklog = conf->getInt("maxbacklog", 20);
		ms.MaxSecs = conf->getInt("maxsecs", 0);

		ms.MaxDiff = conf->getInt("maxdistance", 50);
		if (ms.MaxDiff > 100)
			ms.MaxDiff = 100;

		unsigned int newsize = conf->getInt("size", 512);
		if (newsize > ServerInstance->Config->Limits.MaxLine)
			newsize = ServerInstance->Config->Limits.MaxLine;
		Resize(newsize);
	}

	std::string GetModuleSettings() const
	{
		return ConvToStr(ms.MaxLines) + ":" + ConvToStr(ms.MaxSecs) + ":" + ConvToStr(ms.MaxDiff) + ":" + ConvToStr(ms.MaxBacklog);
	}

 private:
	bool ParseSettings(User* source, std::string& parameter, ChannelSettings& settings)
	{
		irc::sepstream stream(parameter, ':');
		std::string	item;
		if (!stream.GetToken(item))
			// Required parameter missing
			return false;

		if ((item[0] == '*') || (item[0] == '~'))
		{
			settings.Action = ((item[0] == '*') ? ACT_BAN : ACT_BLOCK);
			item.erase(item.begin());
		}
		else
			settings.Action = ACT_KICK;

		if ((settings.Lines = ConvToInt(item)) == 0)
			return false;

		if ((!stream.GetToken(item)) || ((settings.Seconds = InspIRCd::Duration(item)) == 0))
			// Required parameter missing
			return false;

		// The diff and backlog parameters are optional
		settings.Diff = settings.Backlog = 0;
		if (stream.GetToken(item))
		{
			// There is a diff parameter, see if it's valid (> 0)
			if ((settings.Diff = ConvToInt(item)) == 0)
				return false;

			if (stream.GetToken(item))
			{
				// There is a backlog parameter, see if it's valid
				if ((settings.Backlog = ConvToInt(item)) == 0)
					return false;

				// If there are still tokens, then it's invalid because we allow only 4
				if (stream.GetToken(item))
					return false;
			}
		}

		parameter = settings.serialize();
		return true;
	}

	bool ValidateSettings(LocalUser* source, const ChannelSettings& settings)
	{
		if (settings.Backlog && !ms.MaxBacklog)
		{
			source->WriteNotice("*** The server administrator has disabled backlog matching");
			return false;
		}

		if (settings.Diff)
		{
			if (settings.Diff > ms.MaxDiff)
			{
				if (ms.MaxDiff == 0)
					source->WriteNotice("*** The server administrator has disabled matching on edit distance");
				else
					source->WriteNotice("*** The distance you specified is too great. Maximum allowed is " + ConvToStr(ms.MaxDiff));
				return false;
			}

			if (ms.MaxLines && settings.Lines > ms.MaxLines)
			{
				source->WriteNotice("*** The line number you specified is too great. Maximum allowed is " + ConvToStr(ms.MaxLines));
				return false;
			}

			if (ms.MaxSecs && settings.Seconds > ms.MaxSecs)
			{
				source->WriteNotice("*** The seconds you specified is too great. Maximum allowed is " + ConvToStr(ms.MaxSecs));
				return false;
			}
		}

		return true;
	}
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
		ServerInstance->Modules->AddService(rm.MemberInfoExt);
		rm.ReadConfig();
	}

	void OnRehash(User* user) CXX11_OVERRIDE
	{
		rm.ReadConfig();
	}

	ModResult OnUserPreMessage(User* user, void* dest, int target_type, std::string& text, char status, CUList& exempt_list, MessageType msgtype) CXX11_OVERRIDE
	{
		if (target_type != TYPE_CHANNEL || !IS_LOCAL(user))
			return MOD_RES_PASSTHRU;

		Membership* memb = ((Channel*)dest)->GetUser(user);
		if (!memb || !memb->chan->IsModeSet(&rm))
			return MOD_RES_PASSTHRU;

		if (ServerInstance->OnCheckExemption(user, memb->chan, "repeat") == MOD_RES_ALLOW)
			return MOD_RES_PASSTHRU;

		RepeatMode::ChannelSettings* settings = rm.ChanSet.get(memb->chan);
		if (!settings)
			return MOD_RES_PASSTHRU;

		if (rm.MatchLine(memb, settings, text))
		{
			if (settings->Action == RepeatMode::ACT_BLOCK)
			{
				user->WriteNotice("*** This line is too similiar to one of your last lines.");
				return MOD_RES_DENY;
			}

			if (settings->Action == RepeatMode::ACT_BAN)
			{
				std::vector<std::string> parameters;
				parameters.push_back(memb->chan->name);
				parameters.push_back("+b");
				parameters.push_back("*!*@" + user->dhost);
				ServerInstance->Modes->Process(parameters, ServerInstance->FakeClient);
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
		return Version("Provides the +E channel mode - for blocking of similiar messages", VF_COMMON|VF_VENDOR, rm.GetModuleSettings());
	}
};

MODULE_INIT(RepeatModule)
