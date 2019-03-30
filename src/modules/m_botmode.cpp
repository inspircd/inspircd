/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2004, 2008 Craig Edwards <craigedwards@brainbox.cc>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2007 Robin Burchell <robin+git@viroteck.net>
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
#include "modules/cap.h"
#include "modules/whois.h"

enum
{
	// From UnrealIRCd.
	RPL_WHOISBOT = 335
};

class BotTag : public ClientProtocol::MessageTagProvider
{
 private:
	SimpleUserModeHandler& botmode;
	Cap::Reference ctctagcap;

 public:
	BotTag(Module* mod, SimpleUserModeHandler& bm)
		: ClientProtocol::MessageTagProvider(mod)
		, botmode(bm)
		, ctctagcap(mod, "message-tags")
	{
	}

	void OnPopulateTags(ClientProtocol::Message& msg) CXX11_OVERRIDE
	{
		User* const user = msg.GetSourceUser();
		if (user && user->IsModeSet(botmode))
			msg.AddTag("inspircd.org/bot", this, "");
	}

	bool ShouldSendTag(LocalUser* user, const ClientProtocol::MessageTagData& tagdata) CXX11_OVERRIDE
	{
		return ctctagcap.get(user);
	}
};

class ModuleBotMode : public Module, public Whois::EventListener
{
 private:
	SimpleUserModeHandler bm;
	BotTag tag;

 public:
	ModuleBotMode()
		: Whois::EventListener(this)
		, bm(this, "bot", 'B')
		, tag(this, bm)
	{
	}

	Version GetVersion() CXX11_OVERRIDE
	{
		return Version("Provides user mode +B to mark the user as a bot",VF_VENDOR);
	}

	void OnWhois(Whois::Context& whois) CXX11_OVERRIDE
	{
		if (whois.GetTarget()->IsModeSet(bm))
		{
			whois.SendLine(RPL_WHOISBOT, "is a bot on " + ServerInstance->Config->Network);
		}
	}
};

MODULE_INIT(ModuleBotMode)
