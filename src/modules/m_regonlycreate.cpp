/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2007 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2007 Craig Edwards <craigedwards@brainbox.cc>
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

/* $ModDesc: Prevents users who's nicks are not registered from creating new channels */

class ModuleRegOnlyCreate : public Module
{
 public:
	ModuleRegOnlyCreate(InspIRCd* Me)
		: Module(Me)
	{
		Implementation eventlist[] = { I_OnUserPreJoin };
		ServerInstance->Modules->Attach(eventlist, this, 1);
	}


	virtual int OnUserPreJoin(User* user, Channel* chan, const char* cname, std::string &privs, const std::string &keygiven)
	{
		if (chan)
			return 0;

		if (IS_OPER(user))
			return 0;

		if ((!user->IsModeSet('r')) && (!user->GetExt("accountname")))
		{
			// XXX. there may be a better numeric for this..
			user->WriteNumeric(ERR_CHANOPRIVSNEEDED, "%s %s :You must have a registered nickname to create a new channel", user->nick.c_str(), cname);
			return 1;
		}

		return 0;
	}

	virtual ~ModuleRegOnlyCreate()
	{
	}

	virtual Version GetVersion()
	{
		return Version("$Id$", VF_VENDOR, API_VERSION);
	}
};

MODULE_INIT(ModuleRegOnlyCreate)
