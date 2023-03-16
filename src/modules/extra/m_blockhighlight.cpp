/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2017 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2015 Attila Molnar <attilamolnar@hush.com>
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

/// $ModAuthor: Sadie Powell
/// $ModAuthorMail: sadie@witchery.services
/// $ModConfig: <blockhighlight ignoreextmsg="yes" minlen="50" minusernum="10" reason="Mass highlight spam is not allowed" stripcolor="yes" modechar="V">
/// $ModDesc: Adds a channel mode which kills clients that mass highlight spam.
/// $ModDepends: core 3

#include "inspircd.h"
#include "modules/exemption.h"

class ModuleBlockHighlight : public Module
{
	SimpleChannelModeHandler mode;
	ChanModeReference noextmsgmode;
	CheckExemption::EventProvider exemptionprov;

	bool ignoreextmsg;
	unsigned int minlen;
	unsigned int minusers;
	std::string reason;
	bool stripcolor;

public:
	ModuleBlockHighlight()
		: mode(this, "blockhighlight", ServerInstance->Config->ConfValue("blockhighlight")->getString("modechar", "V", 1, 1)[0])
		, noextmsgmode(this, "noextmsg")
		, exemptionprov(this)
	{
	}

	void ReadConfig(ConfigStatus&) CXX11_OVERRIDE
	{
		ConfigTag* tag = ServerInstance->Config->ConfValue("blockhighlight");
		ignoreextmsg = tag->getBool("ignoreextmsg", true);
		minlen = tag->getUInt("minlen", 50, 1, ServerInstance->Config->Limits.MaxLine);
		minusers = tag->getUInt("minusernum", 10, 2);
		reason = tag->getString("reason", "Mass highlight spam is not allowed");
		stripcolor = tag->getBool("stripcolor", true);
	}

	ModResult OnUserPreMessage(User* user, const MessageTarget& target, MessageDetails& details) CXX11_OVERRIDE
	{
		if ((target.type != MessageTarget::TYPE_CHANNEL) || (!IS_LOCAL(user)))
			return MOD_RES_PASSTHRU;

		// Must be at least minlen long
		if (details.text.length() < minlen)
			return MOD_RES_PASSTHRU;

		Channel* const chan = target.Get<Channel>();
		if (chan->GetUsers().size() < minusers)
			return MOD_RES_PASSTHRU;

		// We only work if the channel mode is enabled.
		if (!chan->IsModeSet(mode))
			return MOD_RES_PASSTHRU;

		// Exempt operators with the channels/mass-highlight privilege.
		if (user->HasPrivPermission("channels/mass-highlight"))
			return MOD_RES_PASSTHRU;

		// Exempt users who match a blockhighlight entry.
		if (CheckExemption::Call(exemptionprov, user, chan, "blockhighlight") == MOD_RES_ALLOW)
			return MOD_RES_PASSTHRU;

		// Prevent the enumeration of channel members if enabled.
		if (!chan->IsModeSet(noextmsgmode) && !chan->HasUser(user) && ignoreextmsg)
			return MOD_RES_PASSTHRU;

		std::string message(details.text);
		if (stripcolor)
			InspIRCd::StripColor(message);

		irc::spacesepstream ss(message);
		unsigned int count = 0;
		for (std::string token; ss.GetToken(token); )
		{
			// Chop off trailing :
			if ((token.length() > 1) && (token[token.length()-1] == ':'))
				token.erase(token.length()-1);

			User* const highlighted = ServerInstance->FindNickOnly(token);
			if (!highlighted)
				continue;

			if (!chan->HasUser(highlighted))
				continue;

			// Highlighted someone
			count++;
			if (count >= minusers)
			{
				ServerInstance->Users->QuitUser(user, reason);
				return MOD_RES_DENY;
			}
		}

		return MOD_RES_PASSTHRU;
	}

	Version GetVersion() CXX11_OVERRIDE
	{
		return Version("Adds a channel mode which kills clients that mass highlight spam.");
	}
};

MODULE_INIT(ModuleBlockHighlight)
