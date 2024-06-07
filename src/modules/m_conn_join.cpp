/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2021 Dominic Hamon
 *   Copyright (C) 2019-2023 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2014 Daniel Vassdal <shutter@canternet.org>
 *   Copyright (C) 2013-2015 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2009 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2007 Craig Edwards <brain@inspircd.org>
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

static void JoinChannels(LocalUser* u, const std::string& chanlist)
{
	irc::commasepstream chans(chanlist);
	std::string chan;

	while (chans.GetToken(chan))
	{
		if (ServerInstance->Channels.IsChannel(chan))
			Channel::JoinUser(u, chan);
	}
}

class JoinTimer final
	: public Timer
{
private:
	LocalUser* const user;
	const std::string channels;
	SimpleExtItem<JoinTimer>& ext;

public:
	JoinTimer(LocalUser* u, SimpleExtItem<JoinTimer>& ex, const std::string& chans, unsigned int delay)
		: Timer(delay, false)
		, user(u)
		, channels(chans)
		, ext(ex)
	{
		ServerInstance->Timers.AddTimer(this);
	}

	bool Tick() override
	{
		if (user->chans.empty())
			JoinChannels(user, channels);

		ext.Unset(user);
		return false;
	}
};

class ModuleConnJoin final
	: public Module
{
private:
	SimpleExtItem<JoinTimer> ext;
	std::string defchans;
	unsigned int defdelay;

public:
	ModuleConnJoin()
		: Module(VF_VENDOR, "Allows the server administrator to force users to join one or more channels on connect.")
		, ext(this, "join-timer", ExtensionType::USER)
	{
	}

	void ReadConfig(ConfigStatus& status) override
	{
		const auto& tag = ServerInstance->Config->ConfValue("autojoin");
		defchans = tag->getString("channel");
		defdelay = static_cast<unsigned int>(tag->getDuration("delay", 0, 0, 60*15));
	}

	void Prioritize() override
	{
		ServerInstance->Modules.SetPriority(this, I_OnPostConnect, PRIORITY_LAST);
	}

	void OnPostConnect(User* user) override
	{
		LocalUser* localuser = IS_LOCAL(user);
		if (!localuser)
			return;

		std::string chanlist = localuser->GetClass()->config->getString("autojoin");
		unsigned int chandelay = static_cast<unsigned int>(localuser->GetClass()->config->getDuration("autojoindelay", 0, 0, 60*15));

		if (chanlist.empty())
		{
			if (defchans.empty())
				return;
			chanlist = defchans;
			chandelay = defdelay;
		}

		if (!chandelay)
			JoinChannels(localuser, chanlist);
		else
			ext.SetFwd(localuser, localuser, ext, chanlist, chandelay);
	}
};

MODULE_INIT(ModuleConnJoin)
