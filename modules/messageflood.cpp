/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2017-2024 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012-2014 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2008 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2007 John Brooks <john@jbrooks.io>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2006, 2008 Craig Edwards <brain@inspircd.org>
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
#include "modules/ctctags.h"
#include "modules/exemption.h"
#include "numerichelper.h"
#include "timeutils.h"

enum class MsgFloodAction
	: uint8_t
{
	BAN,
	BLOCK,
	MUTE,
	KICK,
	KICK_BAN,
};

class MsgFloodSettings final
{
private:
	insp::flat_map<User*, double> counters;
	time_t reset;

public:
	MsgFloodAction action;
	unsigned int messages;
	unsigned long period;


	MsgFloodSettings(MsgFloodAction a, unsigned int m, unsigned long p)
		: reset(ServerInstance->Time() + p)
		, action(a)
		, messages(m)
		, period(p)
	{
	}

	bool Add(User* who, double weight)
	{
		if (ServerInstance->Time() > reset)
		{
			counters.clear();
			reset = ServerInstance->Time() + period;
		}

		counters[who] += weight;
		return (counters[who] >= this->messages);
	}

	void Clear(User* who)
	{
		counters.erase(who);
	}
};

class MsgFlood final
	: public ParamMode<MsgFlood, SimpleExtItem<MsgFloodSettings>>
{
private:
	static bool ParseAction(irc::sepstream& stream, MsgFloodAction& action)
	{
		std::string actionstr;
		if (!stream.GetToken(actionstr))
			return false;

		if (irc::equals(actionstr, "ban"))
			action = MsgFloodAction::BAN;
		else if (irc::equals(actionstr, "block"))
			action = MsgFloodAction::BLOCK;
		else if (irc::equals(actionstr, "mute"))
			action = MsgFloodAction::MUTE;
		else if (irc::equals(actionstr, "kick"))
			action = MsgFloodAction::KICK;
		else if (irc::equals(actionstr, "kickban"))
			action = MsgFloodAction::KICK_BAN;
		else
			return false;

		return true;
	}

	static bool ParseMessages(irc::sepstream& stream, unsigned int& messages)
	{
		std::string messagestr;
		if (!stream.GetToken(messagestr))
			return false;

		messages = ConvToNum<unsigned int>(messagestr);
		return true;
	}

	static bool ParsePeriod(irc::sepstream& stream, unsigned long& period)
	{
		std::string periodstr;
		if (!stream.GetToken(periodstr))
			return false;

		return Duration::TryFrom(periodstr, period);
	}

public:
	bool extended;

	MsgFlood(Module* Creator)
		: ParamMode<MsgFlood, SimpleExtItem<MsgFloodSettings>>(Creator, "flood", 'f')
	{
	}

	bool OnSet(User* source, Channel* channel, std::string& parameter) override
	{
		MsgFloodAction action;
		unsigned int messages;
		unsigned long period;
		if (extended)
		{
			irc::sepstream stream(parameter, ':');
			if (!ParseAction(stream, action) || !ParseMessages(stream, messages) || !ParsePeriod(stream, period))
			{
				source->WriteNumeric(Numerics::InvalidModeParameter(channel, this, parameter));
				return false;
			}
		}
		else
		{
			std::string::size_type colon = parameter.find(':');
			if (colon == std::string::npos || parameter.find('-') != std::string::npos)
			{
				source->WriteNumeric(Numerics::InvalidModeParameter(channel, this, parameter));
				return false;
			}

			bool kickban = parameter[0] == '*';
			action = kickban ? MsgFloodAction::KICK_BAN : MsgFloodAction::BLOCK;
			messages = ConvToNum<unsigned int>(parameter.substr(kickban ? 1 : 0, kickban ? colon - 1 : colon));
			period = ConvToNum<unsigned int>(parameter.substr(colon + 1));
		}

		if (messages < 2 || period < 1)
		{
			source->WriteNumeric(Numerics::InvalidModeParameter(channel, this, parameter));
			return false;
		}

		ext.SetFwd(channel, action, messages, period);
		return true;
	}

	void SerializeParam(Channel* chan, const MsgFloodSettings* fs, std::string& out)
	{
		if (extended)
		{
			switch (fs->action)
			{
				case MsgFloodAction::BAN:
					out.append("ban");
					break;
				case MsgFloodAction::BLOCK:
					out.append("block");
					break;
				case MsgFloodAction::MUTE:
					out.append("mute");
					break;
				case MsgFloodAction::KICK:
					out.append("kick");
					break;
				case MsgFloodAction::KICK_BAN:
					out.append("kickban");
					break;
			}
			out.push_back(':');
			out.append(ConvToStr(fs->messages));
			out.push_back(':');
			out.append(Duration::ToString(fs->period));
		}
		else
		{
			if (fs->action == MsgFloodAction::KICK_BAN)
				out.push_back('*');
			out.append(ConvToStr(fs->messages)).push_back(':');
			out.append(ConvToStr(fs->period));
		}
	}

	void SetSyntax()
	{
		if (extended)
			syntax = "{ban|block|mute|kick|kickban}:<messages>:<period>";
		else
			syntax = "[*]<messages>:<period>";
	}
};

class ModuleMsgFlood final
	: public Module
	, public CTCTags::EventListener
{
private:
	ChanModeReference banmode;
	CheckExemption::EventProvider exemptionprov;
	MsgFlood mf;
	double notice;
	double privmsg;
	double tagmsg;
	std::string message;

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

	static void InformUser(Channel* chan, User* user, const std::string& message)
	{
		Membership* memb = chan->GetUser(user);
		if (memb)
			memb->WriteNotice(message);
		else
			user->WriteNotice("[{}] {}", chan->name, message);
	}

public:
	ModuleMsgFlood()
		: Module(VF_VENDOR, "Adds channel mode f (flood) which helps protect against spammers which mass-message channels.")
		, CTCTags::EventListener(this)
		, banmode(this, "ban")
		, exemptionprov(this)
		, mf(this)
	{
	}

	void ReadConfig(ConfigStatus&) override
	{
		const auto& tag = ServerInstance->Config->ConfValue("messageflood");
		notice = tag->getNum<double>("notice", 1.0);
		privmsg = tag->getNum<double>("privmsg", 1.0);
		tagmsg = tag->getNum<double>("tagmsg", 0.2);
		message = tag->getString("message", "Message flood (trigger is %messages% messages in %duration%)", 1);
		mf.extended = tag->getBool("extended");
		mf.SetSyntax();
	}

	void GetLinkData(LinkData& data) override
	{
		data["actions"] = mf.extended ? "ban block kick kickban mute" : "block kickban";
	}

	ModResult HandleMessage(User* user, const MessageTarget& target, double weight)
	{
		if (target.type != MessageTarget::TYPE_CHANNEL)
			return MOD_RES_PASSTHRU;

		auto* dest = target.Get<Channel>();
		if ((!IS_LOCAL(user)) || !dest->IsModeSet(mf))
			return MOD_RES_PASSTHRU;

		ModResult res = exemptionprov.Check(user, dest, "flood");
		if (res == MOD_RES_ALLOW)
			return MOD_RES_PASSTHRU;

		auto* f = mf.ext.Get(dest);
		if (f)
		{
			if (f->Add(user, weight))
			{
				/* You're outttta here! */
				f->Clear(user);

				const std::string msg = Template::Replace(message, {
					{ "duration", Duration::ToString(f->period) },
					{ "messages", ConvToStr(f->messages)        },
					{ "seconds",  ConvToStr(f->period)          },
				});
				switch (f->action)
				{
					case MsgFloodAction::BAN:
						InformUser(dest, user, msg);
						CreateBan(dest, user, false);
						break;

					case MsgFloodAction::BLOCK:
						InformUser(dest, user, msg);
						break;

					case MsgFloodAction::KICK:
						dest->KickUser(ServerInstance->FakeClient, user, msg);
						break;

					case MsgFloodAction::KICK_BAN:
						CreateBan(dest, user, false);
						dest->KickUser(ServerInstance->FakeClient, user, msg);
						break;

					case MsgFloodAction::MUTE:
						InformUser(dest, user, msg);
						CreateBan(dest, user, true);
						break;
				}

				return MOD_RES_DENY;
			}
		}

		return MOD_RES_PASSTHRU;
	}

	ModResult OnUserPreMessage(User* user, MessageTarget& target, MessageDetails& details) override
	{
		return HandleMessage(user, target, (details.type == MessageType::PRIVMSG ? privmsg : notice));
	}

	ModResult OnUserPreTagMessage(User* user, MessageTarget& target, CTCTags::TagMessageDetails& details) override
	{
		return HandleMessage(user, target, tagmsg);
	}

	void Prioritize() override
	{
		// we want to be after all modules that might deny the message (e.g. m_muteban, m_noctcp, m_blockcolor, etc.)
		ServerInstance->Modules.SetPriority(this, I_OnUserPreMessage, PRIORITY_LAST);
	}
};

MODULE_INIT(ModuleMsgFlood)
