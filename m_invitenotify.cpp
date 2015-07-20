/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2013 Adam <Adam@anope.org>
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

/* $ModAuthor: Adam */
/* $ModAuthorMail: Adam@anope.org */
/* $ModDesc: Implements invite-notify */
/* $ModDepends: core 2.0 */

#include "inspircd.h"
#include "m_cap.h"

class ModuleInviteNotify : public Module
{
	GenericCap invite_notify;

 public:
	ModuleInviteNotify() : invite_notify(this, "invite-notify")
	{
	}

	void init()
	{
		Implementation eventlist[] = { I_OnEvent, I_OnUserInvite };
		ServerInstance->Modules->Attach(eventlist, this, sizeof(eventlist) / sizeof(Implementation));
	}

	Version GetVersion()
	{
		return Version("Implements invite-notify");
	}

	void OnEvent(Event& ev)
	{
		this->invite_notify.HandleEvent(ev);
	}

	void OnUserInvite(User* source, User* dest, Channel* channel, time_t)
	{
		const UserMembList* cl = channel->GetUsers();
		for (UserMembCIter it = cl->begin(); it != cl->end(); ++it)
		{
			User* u = it->first;

			if (u == source || u == dest || !this->invite_notify.ext.get(u))
				continue;

			u->WriteFrom(source, "INVITE " + dest->nick + " :" + channel->name);
		}
	}
};

MODULE_INIT(ModuleInviteNotify)
