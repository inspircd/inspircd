/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2019-2021, 2023 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2012 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2008 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2007 John Brooks <john@jbrooks.io>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
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

class ModuleChanCreate final
	: public Module
{
public:
	ModuleChanCreate()
		: Module(VF_VENDOR, "Sends a notice to snomasks j (local) and J (remote) when a channel is created.")
	{
	}

	void init() override
	{
		ServerInstance->SNO.EnableSnomask('j', "CHANCREATE");
	}

	void OnUserJoin(Membership* memb, bool sync, bool created, CUList& except) override
	{
		if ((created) && (IS_LOCAL(memb->user)))
		{
			ServerInstance->SNO.WriteGlobalSno('j', "Channel {} created by {}", memb->chan->name, memb->user->GetRealMask());
		}
	}
};

MODULE_INIT(ModuleChanCreate)
