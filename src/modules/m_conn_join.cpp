/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2007 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2007 Craig Edwards <craigedwards@brainbox.cc>
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

static void JoinChannels(LocalUser* u, const std::string& chanlist)
{
	irc::commasepstream chans(chanlist);
	std::string chan;

	while (chans.GetToken(chan))
	{
		if (ServerInstance->IsChannel(chan))
			Channel::JoinUser(u, chan);
	}
}

class JoinTimer : public Timer
{
 private:
	LocalUser* const user;
	const std::string channels;
	SimpleExtItem<JoinTimer>& ext;

 public:
	JoinTimer(LocalUser* u, SimpleExtItem<JoinTimer>& ex, const std::string& chans, unsigned int delay)
		: Timer(delay, false)
		, user(u), channels(chans), ext(ex)
	{
		ServerInstance->Timers.AddTimer(this);
	}

	bool Tick(time_t time) CXX11_OVERRIDE
	{
		if (user->chans.empty())
			JoinChannels(user, channels);

		ext.unset(user);
		return false;
	}
};

class ModuleConnJoin : public Module
{
	SimpleExtItem<JoinTimer> ext;
	std::string defchans;
	unsigned int defdelay;

 public:
	ModuleConnJoin()
		: ext("join_timer", ExtensionItem::EXT_USER, this)
	{
	}

	void ReadConfig(ConfigStatus& status) CXX11_OVERRIDE
	{
		ConfigTag* tag = ServerInstance->Config->ConfValue("autojoin");
		defchans = tag->getString("channel");
		defdelay = tag->getInt("delay", 0, 0, 60);
	}

	void Prioritize() CXX11_OVERRIDE
	{
		ServerInstance->Modules->SetPriority(this, I_OnPostConnect, PRIORITY_LAST);
	}

	Version GetVersion() CXX11_OVERRIDE
	{
		return Version("Forces users to join the specified channel(s) on connect", VF_VENDOR);
	}

	void OnPostConnect(User* user) CXX11_OVERRIDE
	{
		LocalUser* localuser = IS_LOCAL(user);
		if (!localuser)
			return;

		std::string chanlist = localuser->GetClass()->config->getString("autojoin");
		unsigned int chandelay = localuser->GetClass()->config->getInt("autojoindelay", 0, 0, 60);

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
			ext.set(localuser, new JoinTimer(localuser, ext, chanlist, chandelay));
	}
};

MODULE_INIT(ModuleConnJoin)
