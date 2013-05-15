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

/* $ModDesc: Implements extban +b p: - part message bans */

class ModulePartMsgBan : public Module
{
 public:
	void init() CXX11_OVERRIDE
	{
		Implementation eventlist[] = { I_OnUserPart, I_On005Numeric };
		ServerInstance->Modules->Attach(eventlist, this, sizeof(eventlist)/sizeof(Implementation));
	}

	Version GetVersion() CXX11_OVERRIDE
	{
		return Version("Implements extban +b p: - part message bans", VF_OPTCOMMON|VF_VENDOR);
	}

	void OnUserPart(Membership* memb, std::string &partmessage, CUList& excepts) CXX11_OVERRIDE
	{
		if (!IS_LOCAL(memb->user))
			return;

		if (memb->chan->GetExtBanStatus(memb->user, 'p') == MOD_RES_DENY)
			partmessage.clear();
	}

	void On005Numeric(std::map<std::string, std::string>& tokens) CXX11_OVERRIDE
	{
		tokens["EXTBAN"].push_back('p');
	}
};

MODULE_INIT(ModulePartMsgBan)
