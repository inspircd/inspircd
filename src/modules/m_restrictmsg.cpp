/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2007 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2005 Craig Edwards <craigedwards@brainbox.cc>
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

/* $ModDesc: Forbids users from messaging each other. Users may still message opers and opers may message other opers. */


class ModuleRestrictMsg : public Module
{

 public:

	ModuleRestrictMsg(InspIRCd* Me)
		: Module(Me)
	{

		Implementation eventlist[] = { I_OnUserPreMessage, I_OnUserPreNotice };
		ServerInstance->Modules->Attach(eventlist, this, 2);
	}


	virtual int OnUserPreMessage(User* user,void* dest,int target_type, std::string &text, char status, CUList &exempt_list)
	{
		if ((target_type == TYPE_USER) && (IS_LOCAL(user)))
		{
			User* u = (User*)dest;

			// message allowed if:
			// (1) the sender is opered
			// (2) the recipient is opered
			// anything else, blocked.
			if (IS_OPER(u) || IS_OPER(user))
			{
				return 0;
			}
			user->WriteNumeric(ERR_CANTSENDTOUSER, "%s %s :You are not permitted to send private messages to this user",user->nick.c_str(),u->nick.c_str());
			return 1;
		}

		// however, we must allow channel messages...
		return 0;
	}

	virtual int OnUserPreNotice(User* user,void* dest,int target_type, std::string &text, char status, CUList &exempt_list)
	{
		return this->OnUserPreMessage(user,dest,target_type,text,status,exempt_list);
	}

	virtual ~ModuleRestrictMsg()
	{
	}

	virtual Version GetVersion()
	{
		return Version("$Id$",VF_VENDOR,API_VERSION);
	}
};

MODULE_INIT(ModuleRestrictMsg)
