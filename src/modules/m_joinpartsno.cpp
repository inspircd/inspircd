/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2012 Shawn Smith <ShawnSmith0828@gmail.com>
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

/* $ModAuthor: Shawn Smith */
/* $ModDesc: Sends server notices when a user joins/parts a channel. */

class ModuleJoinPartSNO : public Module
{
 public:
	void init()
	{
		ServerInstance->SNO->EnableSnomask('e', "JOIN");
		ServerInstance->SNO->EnableSnomask('p', "PART");
		Implementation eventlist[] = { I_OnUserJoin, I_OnUserPart };
		ServerInstance->Modules->Attach(eventlist, this, sizeof(eventlist)/sizeof(Implementation));
	}

	Version GetVersion()
	{
		return Version("Creates SNOMask for user joins/parts", VF_VENDOR);
	}


	void OnUserJoin(Membership* memb, bool sync, bool created, CUList& except)
	{
		if (IS_LOCAL(memb->user))
		{
			ServerInstance->SNO->WriteToSnoMask('e', "User %s!%s@%s joined %s",
				memb->user->nick.c_str(), memb->user->ident.c_str(), memb->user->host.c_str(), memb->chan->name.c_str());
		}
		else
		{
			ServerInstance->SNO->WriteGlobalSno('E', "User %s!%s@%s joined %s",
				memb->user->nick.c_str(), memb->user->ident.c_str(), memb->user->host.c_str(), memb->chan->name.c_str());
		}
	}

	void OnUserPart(Membership* memb, std::string &partmessage, CUList &except)
	{
		if (IS_LOCAL(memb->user))
		{
			ServerInstance->SNO->WriteToSnoMask('p', "User %s!%s@%s parted %s",
				memb->user->nick.c_str(), memb->user->ident.c_str(), memb->user->host.c_str(), memb->chan->name.c_str());
		}
		else
		{
			ServerInstance->SNO->WriteGlobalSno('P', "User %s!%s@%s parted %s",
				memb->user->nick.c_str(), memb->user->ident.c_str(), memb->user->host.c_str(), memb->chan->name.c_str());
		}
	}
};

MODULE_INIT(ModuleJoinPartSNO)
