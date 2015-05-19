/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2014 Attila Molnar <attilamolnar@hush.com>
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

 public:
	CoreModChannel()
		: cmdinvite(this), cmdjoin(this), cmdkick(this), cmdnames(this), cmdtopic(this)
	{
	}

	void OnPostJoin(Membership* memb) CXX11_OVERRIDE
	{
		Channel* const chan = memb->chan;
		LocalUser* const localuser = IS_LOCAL(memb->user);
		if (localuser)
		{
			if (chan->topicset)
				Topic::ShowTopic(localuser, chan);

			// Show all members of the channel, including invisible (+i) users
			cmdnames.SendNames(localuser, chan, true);
		}
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
