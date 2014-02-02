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

void JoinChannels(LocalUser* u, const std::string& chanlist)
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
	LocalUser* user;
	std::string channels;
	SimpleExtItem<JoinTimer>& ext;

 public:
	JoinTimer(LocalUser* u, SimpleExtItem<JoinTimer>& ex, const std::string& chans, int delay) : Timer(delay, ServerInstance->Time(), false), user(u), channels(chans), ext(ex)
	{
		ServerInstance->Timers->AddTimer(this);
	}

	bool Tick(time_t time)
	{
		if (user->chans.empty())
			JoinChannels(user, channels);

		ext.unset(user);
		return true;
	}
};

class ModuleConnJoin : public Module
{
 private:
	SimpleExtItem<JoinTimer> ext;

 public:
	ModuleConnJoin() : ext("join_timer", this)
	{
	}

	void Prioritize()
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
		int chandelay = localuser->GetClass()->config->getInt("autojoindelay");

		if (chanlist.empty())
		{
			ConfigTag* tag = ServerInstance->Config->ConfValue("autojoin");
			chanlist = tag->getString("channel");
			chandelay = tag->getInt("delay");
		}

		if (chanlist.empty())
			return;

		if (!chandelay)
			JoinChannels(localuser, chanlist);
		else
			ext.set(localuser, new JoinTimer(localuser, ext, chanlist, chandelay));
	}
};

MODULE_INIT(ModuleConnJoin)
