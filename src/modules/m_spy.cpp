/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2006-2007 Craig Edwards <craigedwards@brainbox.cc>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2006-2007 Robin Burchell <robin+git@viroteck.net>
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


/* $ModDesc: Provides the ability to see the complete names list of channels an oper is not a member of */

#include "inspircd.h"

class ModuleSpy : public Module
{
 public:
	ModuleSpy(InspIRCd* Me) : Module(Me)
	{
		ServerInstance->Modules->Attach(I_OnUserList, this);
	}

	virtual int OnUserList(User* user, Channel* Ptr, CUList* &nameslist)
	{
		/* User has priv and is NOT on the channel */
		if (user->HasPrivPermission("channels/auspex") && !Ptr->HasUser(user))
			return -1;

		return 0;
	}

	void Prioritize()
	{
		/* To ensure that we get priority over namesx and delayjoin for names list generation */
		Module* list[] = { ServerInstance->Modules->Find("m_namesx.so"), ServerInstance->Modules->Find("m_delayjoin.so") };
		ServerInstance->Modules->SetPriority(this, I_OnUserList, PRIORITY_BEFORE, list, 2);
	}

	virtual ~ModuleSpy()
	{
	}

	virtual Version GetVersion()
	{
		return Version("$Id$", VF_VENDOR, API_VERSION);
	}
};

MODULE_INIT(ModuleSpy)

