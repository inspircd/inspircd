/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2018-2024 Sadie Powell <sadie@witchery.services>
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


#include <cmath>

#include "inspircd.h"
#include "extension.h"
#include "modules/exemption.h"
#include "numerichelper.h"

enum class AntiCapsMethod
	: uint8_t
{
	BAN,
	BLOCK,
	MUTE,
	KICK,
	KICK_BAN,
};

class AntiCapsSettings final
{
private:
	bool ParseMethod(irc::sepstream& stream)
	{
		std::string methodstr;
		if (!stream.GetToken(methodstr))
			return false;

		if (irc::equals(methodstr, "ban"))
			method = AntiCapsMethod::BAN;
		else if (irc::equals(methodstr, "block"))
			method = AntiCapsMethod::BLOCK;
		else if (irc::equals(methodstr, "mute"))
			method = AntiCapsMethod::MUTE;
		else if (irc::equals(methodstr, "kick"))
			method = AntiCapsMethod::KICK;
		else if (irc::equals(methodstr, "kickban"))
			method = AntiCapsMethod::KICK_BAN;
		else
			return false;

		return true;
	}

	bool ParseMinimumLength(irc::sepstream& stream)
	{
		std::string minlenstr;
		if (!stream.GetToken(minlenstr))
			return false;

		uint16_t result = ConvToNum<uint16_t>(minlenstr);
		if (result < 1 || result > ServerInstance->Config->Limits.MaxLine)
			return false;

		minlen = result;
		return true;
	}

	bool ParsePercent(irc::sepstream& stream)
	{
		std::string percentstr;
		if (!stream.GetToken(percentstr))
			return false;

		uint8_t result = ConvToNum<uint8_t>(percentstr);
		if (result < 1 || result > 100)
			return false;

		percent = result;
		return true;
	}

public:
	AntiCapsMethod method = AntiCapsMethod::BLOCK;
	uint16_t minlen = 0;
	uint8_t percent = 0;

	bool Parse(const std::string& str)
	{
		irc::sepstream stream(str, ':');
		return ParseMethod(stream) && ParseMinimumLength(stream) && ParsePercent(stream);
	}
};

class AntiCapsMode final
	: public ParamMode<AntiCapsMode, SimpleExtItem<AntiCapsSettings>>
{
public:
	AntiCapsMode(Module* Creator)
		: ParamMode<AntiCapsMode, SimpleExtItem<AntiCapsSettings>>(Creator, "anticaps", 'B')
	{
		syntax = "{ban|block|mute|kick|kickban}:<minlen>:<percent>";
	}

	bool OnSet(User* source, Channel* channel, std::string& parameter) override
	{
		AntiCapsSettings settings;
		if (!settings.Parse(parameter))
		{
			source->WriteNumeric(Numerics::InvalidModeParameter(channel, this, parameter));
			return false;
		}

		ext.Set(channel, settings);
		return true;
	}

	void SerializeParam(Channel* chan, const AntiCapsSettings* acs, std::string& out)
	{
		switch (acs->method)
		{
			case AntiCapsMethod::BAN:
				out.append("ban");
				break;
			case AntiCapsMethod::BLOCK:
				out.append("block");
				break;
			case AntiCapsMethod::MUTE:
				out.append("mute");
				break;
			case AntiCapsMethod::KICK:
				out.append("kick");
				break;
			case AntiCapsMethod::KICK_BAN:
				out.append("kickban");
				break;
		}
		out.push_back(':');
		out.append(ConvToStr(acs->minlen));
		out.push_back(':');
		out.append(ConvToStr<uint16_t>(acs->percent));
	}
};

class ModuleAntiCaps final
	: public Module
{
private:
	ChanModeReference banmode;
	CheckExemption::EventProvider exemptionprov;
	CharState uppercase;
	CharState lowercase;
	AntiCapsMode mode;

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

	static void InformUser(Channel* channel, User* user, const std::string& message)
	{
		user->WriteNumeric(Numerics::CannotSendTo(channel, message + " and was blocked."));
	}

public:
	ModuleAntiCaps()
		: Module(VF_VENDOR | VF_COMMON, "Adds channel mode B (anticaps) which allows channels to block messages which are excessively capitalised.")
		, banmode(this, "ban")
		, exemptionprov(this)
		, mode(this)
	{
	}

	void ReadConfig(ConfigStatus&) override
	{
		const auto& tag = ServerInstance->Config->ConfValue("anticaps");

		uppercase.reset();
		for (const auto chr : tag->getString("uppercase", "ABCDEFGHIJKLMNOPQRSTUVWXYZ", 1))
			uppercase.set(static_cast<unsigned char>(chr));

		lowercase.reset();
		for (const auto chr : tag->getString("lowercase", "abcdefghijklmnopqrstuvwxyz", 1))
			lowercase.set(static_cast<unsigned char>(chr));
	}

	ModResult OnUserPreMessage(User* user, MessageTarget& target, MessageDetails& details) override
	{
		// We only want to operate on messages from local users.
		if (!IS_LOCAL(user))
			return MOD_RES_PASSTHRU;

		// The mode can only be applied to channels.
		if (target.type != MessageTarget::TYPE_CHANNEL)
			return MOD_RES_PASSTHRU;

		// We only act if the channel has the mode set.
		auto* channel = target.Get<Channel>();
		if (!channel->IsModeSet(&mode))
			return MOD_RES_PASSTHRU;

		// If the user is exempt from anticaps then we don't need
		// to do anything else.
		ModResult result = exemptionprov.Check(user, channel, "anticaps");
		if (result == MOD_RES_ALLOW)
			return MOD_RES_PASSTHRU;

		// If the message is a CTCP then we skip it unless it is
		// an ACTION in which case we just check against the body.
		std::string_view ctcpname;
		std::string_view msgbody(details.text);
		if (details.IsCTCP(ctcpname, msgbody))
		{
			// If the CTCP is not an action then skip it.
			if (!irc::equals(ctcpname, "ACTION"))
				return MOD_RES_PASSTHRU;
		}

		// Retrieve the anticaps config. This should never be
		// null but its better to be safe than sorry.
		AntiCapsSettings* config = mode.ext.Get(channel);
		if (!config)
			return MOD_RES_PASSTHRU;

		// If the message is shorter than the minimum length then
		// we don't need to do anything else.
		size_t length = msgbody.length();
		if (length < config->minlen)
			return MOD_RES_PASSTHRU;

		// Count the characters to see how many upper case and
		// ignored (non upper or lower) characters there are.
		size_t upper = 0;
		for (const auto chr : msgbody)
		{
			if (uppercase.test(static_cast<unsigned char>(chr)))
				upper += 1;
			else if (!lowercase.test(static_cast<unsigned char>(chr)))
				length -= 1;
		}

		// If the message was entirely symbols then the message
		// can't contain any upper case letters.
		if (length == 0)
			return MOD_RES_PASSTHRU;

		// Calculate the percentage.
		double percent = round((upper * 100) / length);
		if (percent < config->percent)
			return MOD_RES_PASSTHRU;

		const std::string message = fmt::format("Your message exceeded the {}% upper case character threshold for {}",
			config->percent, channel->name);

		switch (config->method)
		{
			case AntiCapsMethod::BAN:
				InformUser(channel, user, message);
				CreateBan(channel, user, false);
				break;

			case AntiCapsMethod::BLOCK:
				InformUser(channel, user, message);
				break;

			case AntiCapsMethod::MUTE:
				InformUser(channel, user, message);
				CreateBan(channel, user, true);
				break;

			case AntiCapsMethod::KICK:
				channel->KickUser(ServerInstance->FakeClient, user, message);
				break;

			case AntiCapsMethod::KICK_BAN:
				CreateBan(channel, user, false);
				channel->KickUser(ServerInstance->FakeClient, user, message);
				break;
		}
		return MOD_RES_DENY;
	}
};

MODULE_INIT(ModuleAntiCaps)
