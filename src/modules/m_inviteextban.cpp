/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2010 Jackmcbarn <jackmcbarn@jackmcbarn.no-ip.org>
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

/* $ModDesc: Implements extban +b/+e i: - invite requirements/exemptions */

class ModuleInviteExtban : public Module
{
public:
	ModuleInviteExtban()
	{
	}

	void init()
	{
		Implementation eventlist[] = { I_On005Numeric, I_OnCheckJoin };
		ServerInstance->Modules->Attach(eventlist, this, sizeof(eventlist)/sizeof(Implementation));
	}

	void On005Numeric(std::string &output)
	{
		ServerInstance->AddExtBanChar('i');
	}

	void OnCheckJoin(ChannelPermissionData& join)
	{
		if(!join.chan)
			return;
		join.needs_invite = !join.chan->GetExtBanStatus(join.user, 'i').check(!join.needs_invite);
	}

	Version GetVersion()
	{
		return Version("Implements extban +b/+e i: - invite requirements/exemptions", VF_OPTCOMMON|VF_VENDOR);
	}
};

MODULE_INIT(ModuleInviteExtban)
