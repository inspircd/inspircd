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
#include "u_listmode.h"

/* $ModDep: ../../include/u_listmode.h */

/* $ModDesc: Implements extban/invex +I O: - opertype bans */

class ModuleOperInvex : public Module
{
 private:
 public:
	ModuleOperInvex(InspIRCd* Me) : Module(Me)
	{
		Implementation eventlist[] = { I_OnCheckBan, I_On005Numeric, I_OnCheckInvite };
		ServerInstance->Modules->Attach(eventlist, this, 3);
	}

	virtual ~ModuleOperInvex()
	{
	}

	virtual Version GetVersion()
	{
		return Version("$Id$", VF_COMMON|VF_VENDOR, API_VERSION);
	}

	virtual int OnCheckInvite(User *user, Channel *c)
	{
		if (!IS_LOCAL(user) || !IS_OPER(user))
			return 0;

		Module* ExceptionModule = ServerInstance->Modules->Find("m_inviteexception.so");
		if (ExceptionModule)
		{
			if (ListModeRequest(this, ExceptionModule, user->oper, 'O', c).Send())
			{
				// Oper type is exempt
				return 1;
			}
		}

		return 0;
	}

	virtual int OnCheckBan(User *user, Channel *c)
	{
		if (!IS_OPER(user))
			return 0;
		return c->GetExtBanStatus(user->oper, 'O');
	}

	virtual void On005Numeric(std::string &output)
	{
		ServerInstance->AddExtBanChar('O');
	}
};


MODULE_INIT(ModuleOperInvex)

