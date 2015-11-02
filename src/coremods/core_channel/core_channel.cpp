/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2014-2015 Attila Molnar <attilamolnar@hush.com>
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
#include "core_channel.h"

class CoreModChannel : public Module
{
	CommandInvite cmdinvite;
	CommandJoin cmdjoin;
	CommandKick cmdkick;
	CommandNames cmdnames;
	CommandTopic cmdtopic;

	ModResult IsInvited(User* user, Channel* chan)
	{
		LocalUser* localuser = IS_LOCAL(user);
		if ((localuser) && (localuser->IsInvited(chan)))
			return MOD_RES_ALLOW;
		return MOD_RES_PASSTHRU;
	}

 public:
	CoreModChannel()
		: cmdinvite(this), cmdjoin(this), cmdkick(this), cmdnames(this), cmdtopic(this)
	{
	}

	void ReadConfig(ConfigStatus& status) CXX11_OVERRIDE
	{
		Implementation events[] = { I_OnCheckKey, I_OnCheckLimit, I_OnCheckChannelBan };
		if (ServerInstance->Config->InvBypassModes)
			ServerInstance->Modules.Attach(events, this, sizeof(events)/sizeof(Implementation));
		else
		{
			for (unsigned int i = 0; i < sizeof(events)/sizeof(Implementation); i++)
				ServerInstance->Modules.Detach(events[i], this);
		}
	}

	void OnPostJoin(Membership* memb) CXX11_OVERRIDE
	{
		Channel* const chan = memb->chan;
		LocalUser* const localuser = IS_LOCAL(memb->user);
		if (localuser)
		{
			// Remove existing invite, if any
			localuser->RemoveInvite(chan);

			if (chan->topicset)
				Topic::ShowTopic(localuser, chan);

			// Show all members of the channel, including invisible (+i) users
			cmdnames.SendNames(localuser, chan, true);
		}
	}

	ModResult OnCheckKey(User* user, Channel* chan, const std::string& keygiven) CXX11_OVERRIDE
	{
		// Hook only runs when being invited bypasses +bkl
		return IsInvited(user, chan);
	}

	ModResult OnCheckChannelBan(User* user, Channel* chan) CXX11_OVERRIDE
	{
		// Hook only runs when being invited bypasses +bkl
		return IsInvited(user, chan);
	}

	ModResult OnCheckLimit(User* user, Channel* chan) CXX11_OVERRIDE
	{
		// Hook only runs when being invited bypasses +bkl
		return IsInvited(user, chan);
	}

	ModResult OnCheckInvite(User* user, Channel* chan) CXX11_OVERRIDE
	{
		// Hook always runs
		return IsInvited(user, chan);
	}

	void Prioritize() CXX11_OVERRIDE
	{
		ServerInstance->Modules.SetPriority(this, I_OnPostJoin, PRIORITY_FIRST);
	}

	Version GetVersion() CXX11_OVERRIDE
	{
		return Version("Provides the INVITE, JOIN, KICK, NAMES, and TOPIC commands", VF_VENDOR|VF_CORE);
	}
};

MODULE_INIT(CoreModChannel)
