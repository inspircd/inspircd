/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2021 Dominic Hamon
 *   Copyright (C) 2021 David Schultz <me@zpld.me>
 *   Copyright (C) 2018 linuxdaemon <linuxdaemon.irc@gmail.com>
 *   Copyright (C) 2018 Matt Schatz <genius3000@g3k.solutions>
 *   Copyright (C) 2017-2023 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2013-2014 Attila Molnar <attilamolnar@hush.com>
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
#include "extension.h"
#include "modules/exemption.h"
#include "numerichelper.h"
#include "timeutils.h"

class ChannelSettings final
{
public:
	enum RepeatAction
	{
		ACT_BAN,
		ACT_BLOCK,
		ACT_KICK,
		ACT_KICK_BAN,
		ACT_MUTE,
	};

	RepeatAction Action;
	unsigned int Backlog;
	unsigned int Lines;
	unsigned int Diff;
	unsigned long Seconds;

	void serialize(bool extended, std::string& out) const
	{
		if (extended)
		{
			switch (Action)
			{
				case ACT_BAN:
					out.append("ban");
					break;
				case ACT_BLOCK:
					out.append("block");
					break;
				case ACT_MUTE:
					out.append("mute");
					break;
				case ACT_KICK:
					out.append("kick");
					break;
				case ACT_KICK_BAN:
					out.append("kickban");
					break;
			}
			out.push_back(':');
		}
		else
		{
			switch (Action)
			{
				case ACT_KICK_BAN:
					out.push_back('*');
					break;

				case ACT_BLOCK:
					out.push_back('~');
					break;

				default:
					break; // No other types are supported in the old mode.
			}
		}

		out.append(ConvToStr(Lines)).push_back(':');
		out.append(ConvToStr(Seconds));
		if (Diff)
		{
			out.push_back(':');
			out.append(ConvToStr(Diff));
			if (Backlog)
			{
				out.push_back(':');
				out.append(ConvToStr(Backlog));
			}
		}
	}
};

class RepeatMode final
	: public ParamMode<RepeatMode, SimpleExtItem<ChannelSettings>>
{
private:
	struct RepeatItem final
	{
		time_t ts;
		std::string line;
		RepeatItem(time_t TS, const std::string& Line)
			: ts(TS)
			, line(Line)
		{
		}
	};

	typedef std::deque<RepeatItem> RepeatItemList;

	struct MemberInfo final
	{
		RepeatItemList ItemList;
		unsigned int Counter = 0;
	};

	struct ModuleSettings final
	{
		bool Extended = false;
		unsigned long MaxLines = 0;
		unsigned long MaxSecs = 0;
		unsigned long MaxBacklog = 0;
		unsigned int MaxDiff = 0;
		size_t MaxMessageSize = 0;
		std::string Message;
	};

	std::vector<size_t> mx[2];

	bool CompareLines(const std::string& message, const std::string& historyline, unsigned long trigger)
	{
		if (message == historyline)
			return true;
		else if (trigger)
			return (Levenshtein(message, historyline) <= trigger);

		return false;
	}

	size_t Levenshtein(const std::string& s1, const std::string& s2)
	{
		size_t l1 = s1.size();
		size_t l2 = s2.size();

		for (size_t i = 0; i < l2; i++)
			mx[0][i] = i;
		for (size_t i = 0; i < l1; i++)
		{
			mx[1][0] = i + 1;
			for (size_t j = 0; j < l2; j++)
				mx[1][j + 1] = std::min(std::min(mx[1][j] + 1, mx[0][j + 1] + 1), mx[0][j] + ((s1[i] == s2[j]) ? 0 : 1));

			mx[0].swap(mx[1]);
		}
		return mx[0][l2];
	}

public:
	ModuleSettings ms;
	SimpleExtItem<MemberInfo> MemberInfoExt;

	RepeatMode(Module* Creator)
		: ParamMode<RepeatMode, SimpleExtItem<ChannelSettings>>(Creator, "repeat", 'E')
		, MemberInfoExt(Creator, "repeat", ExtensionType::MEMBERSHIP)
	{
	}

	void OnUnset(User* source, Channel* chan) override
	{
		// Unset the per-membership extension when the mode is removed
		for (const auto& [_, memb] : chan->GetUsers())
			MemberInfoExt.Unset(memb);
	}

	bool OnSet(User* source, Channel* channel, std::string& parameter) override
	{
		ChannelSettings settings;
		if (!ParseSettings(source, parameter, settings))
		{
			source->WriteNumeric(Numerics::InvalidModeParameter(channel, this, parameter));
			return false;
		}

		if ((settings.Backlog > 0) && (settings.Lines > settings.Backlog))
		{
			source->WriteNumeric(Numerics::InvalidModeParameter(channel, this, parameter,
				"You can't set lines higher than backlog."));
			return false;
		}

		LocalUser* localsource = IS_LOCAL(source);
		if ((localsource) && (!ValidateSettings(localsource, channel, parameter, settings)))
			return false;

		ext.Set(channel, settings);

		return true;
	}

	bool MatchLine(Membership* memb, ChannelSettings* rs, std::string message)
	{
		// If the message is larger than whatever size it's set to,
		// let's pretend it isn't. If the first 512 (def. setting) match, it's probably spam.
		if (message.size() > ms.MaxMessageSize)
			message.erase(ms.MaxMessageSize);

		MemberInfo* rp = MemberInfoExt.Get(memb);
		if (!rp)
		{
			rp = new MemberInfo;
			MemberInfoExt.Set(memb, rp);
		}

		unsigned int matches = 0;
		if (!rs->Backlog)
			matches = rp->Counter;

		RepeatItemList& items = rp->ItemList;
		const unsigned long trigger = (message.size() * rs->Diff / 100);
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
					if (rs->Action != ChannelSettings::ACT_BLOCK)
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

	void SerializeParam(Channel* chan, const ChannelSettings* chset, std::string& out)
	{
		chset->serialize(ms.Extended, out);
	}

	void SetSyntax()
	{
		if (ms.Extended)
			syntax = "{ban|block|mute|kick|kickban}:<lines>:<duration>[:<difference>][:<backlog>]";
		else
			syntax = "[~|*]<lines>:<duration>[:<difference>][:<backlog>]";

	}

private:
	bool ParseAction(irc::sepstream& stream, ChannelSettings::RepeatAction& action)
	{
		std::string actionstr;
		if (!stream.GetToken(actionstr))
			return false;

		if (irc::equals(actionstr, "ban"))
			action = ChannelSettings::ACT_BAN;
		else if (irc::equals(actionstr, "kick"))
			action = ChannelSettings::ACT_KICK;
		else if (irc::equals(actionstr, "block"))
			action = ChannelSettings::ACT_BLOCK;
		else if (irc::equals(actionstr, "mute"))
			action = ChannelSettings::ACT_MUTE;
		else if (irc::equals(actionstr, "kickban"))
			action = ChannelSettings::ACT_KICK_BAN;
		else
			return false;

		return true;
	}

	bool ParseSettings(User* source, std::string& parameter, ChannelSettings& settings)
	{
		irc::sepstream stream(parameter, ':');
		std::string item;

		if (ms.Extended)
		{
			if (!ParseAction(stream, settings.Action) || !stream.GetToken(item))
				return false;
		}
		else
		{
			if (!stream.GetToken(item))
				return false; // Required parameter missing

			settings.Action = ChannelSettings::ACT_KICK;
			if ((item[0] == '*') || (item[0] == '~'))
			{
				settings.Action = ((item[0] == '*') ? ChannelSettings::ACT_KICK_BAN : ChannelSettings::ACT_BLOCK);
				item.erase(item.begin());
			}
		}

		if ((settings.Lines = ConvToNum<unsigned int>(item)) == 0)
			return false;

		if ((!stream.GetToken(item)) || !Duration::TryFrom(item, settings.Seconds) || (settings.Seconds == 0))
			// Required parameter missing
			return false;

		// The diff and backlog parameters are optional
		settings.Diff = settings.Backlog = 0;
		if (stream.GetToken(item))
		{
			// There is a diff parameter, see if it's valid (> 0)
			if ((settings.Diff = ConvToNum<unsigned int>(item)) == 0)
				return false;

			if (stream.GetToken(item))
			{
				// There is a backlog parameter, see if it's valid
				if ((settings.Backlog = ConvToNum<unsigned int>(item)) == 0)
					return false;

				// If there are still tokens, then it's invalid because we allow only 4
				if (stream.GetToken(item))
					return false;
			}
		}

		return true;
	}

	bool ValidateSettings(LocalUser* source, Channel* channel, const std::string& parameter, const ChannelSettings& settings)
	{
		if (ms.MaxLines && settings.Lines > ms.MaxLines)
		{
			source->WriteNumeric(Numerics::InvalidModeParameter(channel, this, parameter, fmt::format(
				"The line number you specified is too big. Maximum allowed is {}.", ms.MaxLines)));
			return false;
		}

		if (ms.MaxSecs && settings.Seconds > ms.MaxSecs)
		{
			source->WriteNumeric(Numerics::InvalidModeParameter(channel, this, parameter, fmt::format(
				"The seconds you specified are too big. Maximum allowed is {}.", ms.MaxSecs)));
			return false;
		}

		if (settings.Diff && settings.Diff > ms.MaxDiff)
		{
			if (ms.MaxDiff == 0)
				source->WriteNumeric(Numerics::InvalidModeParameter(channel, this, parameter,
					"The server administrator has disabled matching on edit distance."));
			else
				source->WriteNumeric(Numerics::InvalidModeParameter(channel, this, parameter, fmt::format(
					"The distance you specified is too big. Maximum allowed is {}.", ms.MaxDiff)));
			return false;
		}

		if (settings.Backlog && settings.Backlog > ms.MaxBacklog)
		{
			if (ms.MaxBacklog == 0)
				source->WriteNumeric(Numerics::InvalidModeParameter(channel, this, parameter,
					"The server administrator has disabled backlog matching."));
			else
				source->WriteNumeric(Numerics::InvalidModeParameter(channel, this, parameter, fmt::format(
					"The backlog you specified is too big. Maximum allowed is {}.", ms.MaxBacklog)));
			return false;
		}

		return true;
	}
};

class RepeatModule final
	: public Module
{
private:
	ChanModeReference banmode;
	CheckExemption::EventProvider exemptionprov;
	RepeatMode rm;

	void CreateBan(Channel* channel, User* user, bool mute)
	{
		std::string banmask(mute ? "m:*!" : "*!");
		banmask.append(user->GetBanUser(false));
		banmask.append("@");
		banmask.append(user->GetDisplayedHost());

		Modes::ChangeList changelist;
		changelist.push_add(*banmode, banmask);
		ServerInstance->Modes.Process(ServerInstance->FakeClient, channel, nullptr, changelist);
	}

public:
	RepeatModule()
		: Module(VF_VENDOR | VF_COMMON, "Adds channel mode E (repeat) which helps protect against spammers which spam the same message repeatedly.")
		, banmode(this, "ban")
		, exemptionprov(this)
		, rm(this)
	{
	}

	void ReadConfig(ConfigStatus& status) override
	{
		const auto& tag = ServerInstance->Config->ConfValue("repeat");
		rm.ms.Message = tag->getString("message", "Repeat flood (trigger is %lines% messages in %duration%)", 1);
		rm.ms.Extended = tag->getBool("extended");
		rm.ms.MaxBacklog = tag->getNum<unsigned long>("maxbacklog", 20);
		rm.ms.MaxDiff = tag->getNum<unsigned int>("maxdistance", 50, 0, 100);
		rm.ms.MaxLines = tag->getNum<unsigned long>("maxlines", 20);
		rm.ms.MaxSecs = tag->getDuration("maxtime", 0);

		rm.Resize(tag->getNum<size_t>("size", 512, 1, ServerInstance->Config->Limits.MaxLine));
	}

	ModResult OnUserPreMessage(User* user, MessageTarget& target, MessageDetails& details) override
	{
		if (target.type != MessageTarget::TYPE_CHANNEL || !IS_LOCAL(user))
			return MOD_RES_PASSTHRU;

		Channel* chan = target.Get<Channel>();
		ChannelSettings* settings = rm.ext.Get(chan);
		if (!settings)
			return MOD_RES_PASSTHRU;

		Membership* memb = chan->GetUser(user);
		if (!memb)
			return MOD_RES_PASSTHRU;

		ModResult res = exemptionprov.Check(user, chan, "repeat");
		if (res == MOD_RES_ALLOW)
			return MOD_RES_PASSTHRU;

		if (user->HasPrivPermission("channels/ignore-repeat"))
			return MOD_RES_PASSTHRU;

		if (rm.MatchLine(memb, settings, details.text))
		{
			const std::string message = Template::Replace(rm.ms.Message, {
				{ "diff",     ConvToStr(settings->Diff)             },
				{ "duration", Duration::ToString(settings->Seconds) },
				{ "lines",    ConvToStr(settings->Lines)            },
				{ "seconds",  ConvToStr(settings->Seconds)          },
			});

			switch (settings->Action)
			{
				case ChannelSettings::ACT_BAN:
					memb->WriteNotice(message);
					CreateBan(chan, user, false);
					break;

				case ChannelSettings::ACT_BLOCK:
					memb->WriteNotice(message);
					break;

				case ChannelSettings::ACT_KICK:
					chan->KickUser(ServerInstance->FakeClient, user, message);
					break;

				case ChannelSettings::ACT_KICK_BAN:
					CreateBan(chan, user, false);
					chan->KickUser(ServerInstance->FakeClient, user, message);
					break;

				case ChannelSettings::ACT_MUTE:
					memb->WriteNotice(message);
					CreateBan(chan, user, true);
					break;
			}
			return MOD_RES_DENY;
		}
		return MOD_RES_PASSTHRU;
	}

	void Prioritize() override
	{
		ServerInstance->Modules.SetPriority(this, I_OnUserPreMessage, PRIORITY_LAST);
	}

	void GetLinkData(LinkData& data, std::string& compatdata) override
	{
		data["actions"] = rm.ms.Extended ? "ban block kick kickban mute" : "block kick kickban";
		data["max-lines"] = ConvToStr(rm.ms.MaxLines);
		data["max-secs"] = ConvToStr(rm.ms.MaxSecs);
		data["max-diff"] = ConvToStr(rm.ms.MaxDiff);
		data["max-backlog"] = ConvToStr(rm.ms.MaxBacklog);

		compatdata = fmt::format("{}:{}:{}:{}{}", rm.ms.Extended ? "extended:" : "",
			rm.ms.MaxLines, rm.ms.MaxSecs, rm.ms.MaxDiff, rm.ms.MaxBacklog);
	}
};

MODULE_INIT(RepeatModule)
