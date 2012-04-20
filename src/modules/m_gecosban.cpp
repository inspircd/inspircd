/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2008 Robin Burchell <robin+git@viroteck.net>
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

/* $ModDesc: Implements extban +b r: - realname (gecos) bans */

class ModuleGecosBan : public Module
{
 private:
 public:
	ModuleGecosBan(InspIRCd* Me) : Module(Me)
	{
		Implementation eventlist[] = { I_OnCheckBan, I_On005Numeric };
		ServerInstance->Modules->Attach(eventlist, this, 2);
	}

	virtual ~ModuleGecosBan()
	{
	}

	virtual Version GetVersion()
	{
		return Version("$Id$", VF_COMMON|VF_VENDOR, API_VERSION);
	}

	virtual int OnCheckBan(User *user, Channel *c)
	{
		return c->GetExtBanStatus(user->fullname, 'r');
	}

	virtual void On005Numeric(std::string &output)
	{
		ServerInstance->AddExtBanChar('r');
	}
};


MODULE_INIT(ModuleGecosBan)

