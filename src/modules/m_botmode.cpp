/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2017-2024 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012, 2015 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2004 Craig Edwards <brain@inspircd.org>
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
#include "modules/isupport.h"
#include "modules/who.h"
#include "modules/whois.h"

class BotTag final
	: public CTCTags::TagProvider
{
private:
	SimpleUserMode& botmode;

public:
	BotTag(Module* mod, SimpleUserMode& bm)
		: CTCTags::TagProvider(mod)
		, botmode(bm)
	{
	}

	void OnPopulateTags(ClientProtocol::Message& msg) override
	{
		User* const user = msg.GetSourceUser();
		if (user && user->IsModeSet(botmode))
			msg.AddTag("bot", this, "");
	}
};

class ModuleBotMode final
	: public Module
	, public ISupport::EventListener
	, public Who::EventListener
	, public Whois::EventListener
{
private:
	SimpleUserMode bm;
	BotTag tag;
	bool forcenotice;

public:
	ModuleBotMode()
		: Module(VF_VENDOR, "Adds user mode B (bot) which marks users with it set as bots.")
		, ISupport::EventListener(this)
		, Who::EventListener(this)
		, Whois::EventListener(this)
		, bm(this, "bot", 'B')
		, tag(this, bm)
	{
	}

	void ReadConfig(ConfigStatus& status) override
	{
		forcenotice = ServerInstance->Config->ConfValue("botmode")->getBool("forcenotice");
	}

	void OnBuildISupport(ISupport::TokenMap& tokens) override
	{
		tokens["BOT"] = ConvToStr(bm.GetModeChar());
	}

	ModResult OnUserPreMessage(User* user, MessageTarget& target, MessageDetails& details) override
	{
		// Allow sending if forcenotice is off, the user is not a bot, or if the message is a notice.
		if (!forcenotice || !user->IsModeSet(bm) || details.type == MessageType::NOTICE)
			return MOD_RES_PASSTHRU;

		// Allow sending PRIVMSGs to services pseudoclients.
		if (target.type == MessageTarget::TYPE_USER && target.Get<User>()->server->IsService())
			return MOD_RES_PASSTHRU;

		// Force the message to be broadcast as a NOTICE.
		details.type = MessageType::NOTICE;
		return MOD_RES_PASSTHRU;
	}

	ModResult OnWhoLine(const Who::Request& request, LocalUser* source, User* user, Membership* memb, Numeric::Numeric& numeric) override
	{
		size_t flag_index;
		if (!request.GetFieldIndex('f', flag_index))
			return MOD_RES_PASSTHRU;

		if (user->IsModeSet(bm))
			numeric.GetParams()[flag_index].push_back('B');

		return MOD_RES_PASSTHRU;
	}

	void OnWhois(Whois::Context& whois) override
	{
		if (whois.GetTarget()->IsModeSet(bm))
		{
			whois.SendLine(RPL_WHOISBOT, "is a bot");
		}
	}
};

MODULE_INIT(ModuleBotMode)
