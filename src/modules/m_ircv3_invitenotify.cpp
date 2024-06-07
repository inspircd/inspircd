/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2018-2022 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2015, 2018 Attila Molnar <attilamolnar@hush.com>
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
#include "clientprotocolmsg.h"
#include "modules/cap.h"

class ModuleIRCv3InviteNotify final
	: public Module
{
	Cap::Capability cap;

public:
	ModuleIRCv3InviteNotify()
		: Module(VF_VENDOR, "Provides the IRCv3 invite-notify client capability.")
		, cap(this, "invite-notify")
	{
	}

	void OnUserInvite(User* source, User* dest, Channel* chan, time_t expiry, ModeHandler::Rank notifyrank, CUList& notifyexcepts) override
	{
		ClientProtocol::Messages::Invite invitemsg(source, dest, chan);
		ClientProtocol::Event inviteevent(ServerInstance->GetRFCEvents().invite, invitemsg);
		for (const auto& [user, memb] : chan->GetUsers())
		{
			// Skip members who don't use this extension or were excluded by other modules
			if ((!cap.IsEnabled(user)) || (notifyexcepts.count(user)))
				continue;

			// Check whether the member has a high enough rank to see the notification
			if (memb->GetRank() < notifyrank)
				continue;

			// Caps are only set on local users
			LocalUser* const localuser = static_cast<LocalUser*>(user);
			// Send and add the user to the exceptions so they won't get the NOTICE invite announcement message
			localuser->Send(inviteevent);
			notifyexcepts.insert(user);
		}
	}

	void Prioritize() override
	{
		// Prioritize after all modules to see all excepted users
		ServerInstance->Modules.SetPriority(this, I_OnUserInvite, PRIORITY_LAST);
	}
};

MODULE_INIT(ModuleIRCv3InviteNotify)
