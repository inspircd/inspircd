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
	ModuleChanCreate()
			{
		ServerInstance->SNO->EnableSnomask('j', "CHANCREATE");
		Implementation eventlist[] = { I_OnUserJoin };
		ServerInstance->Modules->Attach(eventlist, this, 1);
	}

	Version GetVersion()
	{
		return Version("Creates a snomask with notices whenever a new channel is created",VF_VENDOR);
	}


	void OnUserJoin(Membership* memb, bool sync, bool created, CUList& except)
	{
		if (created)
		{
			if (IS_LOCAL(memb->user))
				ServerInstance->SNO->WriteToSnoMask('j', "Channel %s created by %s!%s@%s",
					memb->chan->name.c_str(), memb->user->nick.c_str(),
					memb->user->ident.c_str(), memb->user->host.c_str());
			else
				ServerInstance->SNO->WriteGlobalSno('J', "Channel %s created by %s!%s@%s",
					memb->chan->name.c_str(), memb->user->nick.c_str(),
					memb->user->ident.c_str(), memb->user->host.c_str());
		}
	}
};

MODULE_INIT(ModuleChanCreate)
