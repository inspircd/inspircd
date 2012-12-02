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

/* $ModDesc: Provides support for seeing local and remote nickchanges via snomasks 'n' and 'N'. */

class ModuleSeeNicks : public Module
{
 public:
	void init()
	{
		ServerInstance->SNO->EnableSnomask('n',"NICK");
		Implementation eventlist[] = { I_OnUserPostNick };
		ServerInstance->Modules->Attach(eventlist, this, sizeof(eventlist)/sizeof(Implementation));
	}

	virtual Version GetVersion()
	{
		return Version("Provides support for seeing local and remote nickchanges via snomasks", VF_VENDOR);
	}

	virtual void OnUserPostNick(User* user, const std::string &oldnick)
	{
		ServerInstance->SNO->WriteToSnoMask(IS_LOCAL(user) ? 'n' : 'N',"User %s changed their nickname to %s", oldnick.c_str(), user->nick.c_str());
	}
};

MODULE_INIT(ModuleSeeNicks)
