/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2007 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2007 John Brooks <john.brooks@dereferenced.net>
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

/* $ModDesc: Creates a snomask with notices whenever a new channel is created */

class ModuleChanCreate : public Module
{
 private:
 public:
	ModuleChanCreate(InspIRCd* Me)
		: Module(Me)
	{
		ServerInstance->SNO->EnableSnomask('j', "CHANCREATE");
		ServerInstance->SNO->EnableSnomask('J', "REMOTECHANCREATE");
		Implementation eventlist[] = { I_OnUserJoin };
		ServerInstance->Modules->Attach(eventlist, this, 1);
	}

	virtual ~ModuleChanCreate()
	{
		ServerInstance->SNO->DisableSnomask('j');
		ServerInstance->SNO->DisableSnomask('J');
	}

	virtual Version GetVersion()
	{
		return Version("$Id$",VF_VENDOR,API_VERSION);
	}


	virtual void OnUserJoin(User* user, Channel* channel, bool sync, bool &silent)
	{
		if (channel->GetUserCounter() == 1 && !channel->IsModeSet('P'))
		{
			if (IS_LOCAL(user))
				ServerInstance->SNO->WriteToSnoMask('j', "Channel %s created by %s!%s@%s", channel->name.c_str(), user->nick.c_str(), user->ident.c_str(), user->host.c_str());
			else
				ServerInstance->SNO->WriteToSnoMask('J', "Channel %s created by %s!%s@%s", channel->name.c_str(), user->nick.c_str(), user->ident.c_str(), user->host.c_str());
		}
	}
};

MODULE_INIT(ModuleChanCreate)
