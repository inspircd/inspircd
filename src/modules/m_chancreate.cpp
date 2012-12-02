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

/* $ModDesc: Provides snomasks 'j' and 'J', to which notices about newly created channels are sent */

class ModuleChanCreate : public Module
{
 private:
 public:
	void init()
	{
		ServerInstance->SNO->EnableSnomask('j', "CHANCREATE");
		Implementation eventlist[] = { I_OnUserJoin };
		ServerInstance->Modules->Attach(eventlist, this, sizeof(eventlist)/sizeof(Implementation));
	}

	Version GetVersion()
	{
		return Version("Provides snomasks 'j' and 'J', to which notices about newly created channels are sent",VF_VENDOR);
	}


	void OnUserJoin(Membership* memb, bool sync, bool created, CUList& except)
	{
		if ((created) && (IS_LOCAL(memb->user)))
		{
			ServerInstance->SNO->WriteGlobalSno('j', "Channel %s created by %s", memb->chan->name.c_str(), memb->user->GetFullRealHost().c_str());
		}
	}
};

MODULE_INIT(ModuleChanCreate)
