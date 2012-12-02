/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
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
#include "account.h"

/* $ModDesc: Prevents users whose nicks are not registered from creating new channels */

class ModuleRegOnlyCreate : public Module
{
 public:
	void init()
	{
		Implementation eventlist[] = { I_OnUserPreJoin };
		ServerInstance->Modules->Attach(eventlist, this, sizeof(eventlist)/sizeof(Implementation));
	}

	ModResult OnUserPreJoin(User* user, Channel* chan, const char* cname, std::string &privs, const std::string &keygiven)
	{
		if (chan)
			return MOD_RES_PASSTHRU;

		if (IS_OPER(user))
			return MOD_RES_PASSTHRU;

		if (user->IsModeSet('r'))
			return MOD_RES_PASSTHRU;

		const AccountExtItem* ext = GetAccountExtItem();
		if (ext && ext->get(user))
			return MOD_RES_PASSTHRU;

		// XXX. there may be a better numeric for this..
		user->WriteNumeric(ERR_CHANOPRIVSNEEDED, "%s %s :You must have a registered nickname to create a new channel", user->nick.c_str(), cname);
		return MOD_RES_DENY;
	}

	~ModuleRegOnlyCreate()
	{
	}

	Version GetVersion()
	{
		return Version("Prevents users whose nicks are not registered from creating new channels", VF_VENDOR);
	}
};

MODULE_INIT(ModuleRegOnlyCreate)
